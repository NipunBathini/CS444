/* block device module for cs444 assignment 3*/

/* reference 1: http://blog.superpat.com/2010/05/04/a-simple-block-driver-for-linux-kernel-2-6-31 */
/* reference 2: http://elixir.free-electrons.com/linux/latest/source/crypto/cipher.c#L63*/
/* reference 3: https://lwn.net/Kernel/LDD3/*/

/* Parker Bruni/Nipun Bathini*/
/* Group 20*/
/* Assignment 3*/
/* CS444 Fall 2017*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h> 
#include <linux/fs.h>    
#include <linux/errno.h>  
#include <linux/types.h>  
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/crypto.h> 

MODULE_LICENSE("Dual BSD/GPL");

static int major_num = 0;
module_param(major_num, int, 0);
static int logical_block_size = 512;
module_param(logical_block_size, int, 0);
static int nsectors = 1024;
module_param(nsectors, int, 0);

/* crypto cipher struct*/
static struct crypto_cipher *tfm;
/* encryption key*/
static char* key = "SuperSecretKey";
module_param(key, charp, 0);


#define KERNEL_SECTOR_SIZE 512

/*request queue.*/
static struct request_queue *Queue;

/*The internal representation the device.*/
static struct ebd_device{
	unsigned long size;
	spinlock_t lock; /* prevents collisions*/
	u8 *data; 
	struct gendisk *gd; 
} Device;

/*prints data of "length" from address ptr*/
static void print_data_func(u8 *ptr, int length){
    int i;
	for(i = 0; i < length; i++){
		printk("%02x ", ptr[i]);
	}
	printk("\n\n");
}

/*Handle I/O request.*/
static void ebd_transfer(struct ebd_device *dev, sector_t sector, unsigned long nsect, char *buffer, int write){
	unsigned long offset = sector * logical_block_size;
	unsigned long nbytes = nsect * logical_block_size;
	int i;
	u8 *data_str, *data_buffer, *data_disk;
 
	data_disk = dev->data + offset;
	data_buffer = buffer;
	printk("ebd: enc_key > %s\n",key);

	if((offset + nbytes) > dev->size){
		printk (KERN_NOTICE "ebd: write request out of bounds (%ld %ld)\n", offset, nbytes);
		return;
	}
	/*if its a write request*/
	if (write){
		printk("ebd: encrypting and writing information\n");
		
		/*print raw data*/
		printk("ebd: unencrypted data:\n");
		print_data_func(buffer, nbytes);
		
		/*encrypts a block at a time*/
		for(i = 0; i < nbytes; i += crypto_cipher_blocksize(tfm)){
			crypto_cipher_encrypt_one(tfm, data_disk + i, data_buffer + i);
		}

		/*print encrypted data*/
		printk("ebd: encrypted data:\n");
		data_str = dev->data + offset;
		print_data_func(data_str, nbytes);

	/*if its a read request*/
	} else{
		printk("ebd: decrypting and reading information\n");

		/* print encrypted data*/
		printk("ebd: printing original hex data\n");
		data_str = dev->data + offset;
		print_data_func(data_str, nbytes);
		
		/* decrypts a block at a time*/
		for(i = 0; i < nbytes; i += crypto_cipher_blocksize(tfm)){
			crypto_cipher_decrypt_one(tfm, data_buffer + i, data_disk + i);
		}

		/*print decrypted raw data*/
		printk("ebd: decrypted data:\n");
		print_data_func(buffer, nbytes);
	}
}

static void ebd_request(struct request_queue *q){
	struct request *req;
	req = blk_fetch_request(q);
	
	while (req != NULL){
		if (req == NULL || (req->cmd_type != REQ_TYPE_FS)){
			printk (KERN_NOTICE "Skip non-CMD request\n");
			__blk_end_request_all(req, -EIO);
			continue;
		}
		ebd_transfer(&Device, blk_rq_pos(req), blk_rq_cur_sectors(req),
			bio_data(req->bio), rq_data_dir(req));
		if (! __blk_end_request_cur(req, 0)){
			req = blk_fetch_request(q);
		}
	}
}

int ebd_getgeo(struct block_device * block_device, struct hd_geometry * geo){
	long size;

	size = Device.size * (logical_block_size / KERNEL_SECTOR_SIZE);
	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 0;
	return 0;
}

/* device operations.*/
static struct block_device_operations ebd_ops = {
		.owner  = THIS_MODULE,
		.getgeo = ebd_getgeo
};

static int __init ebd_init(void) {
	printk("ebd: initializing block device...\n");

	/* set up the internal device.*/
	Device.size = nsectors * logical_block_size;
	/* prevents collisions*/
	spin_lock_init(&Device.lock);
	Device.data = vmalloc(Device.size);
	if (Device.data == NULL)
		return -ENOMEM;
	
	/* get a request queue.*/
	Queue = blk_init_queue(ebd_request, &Device.lock);
	if (Queue == NULL)
		goto out;
	blk_queue_logical_block_size(Queue, logical_block_size);
	
	/* register device.*/
	major_num = register_blkdev(major_num, "ebd");
	if (major_num < 0) {
		printk(KERN_WARNING "ebd: unable to get major number\n");
		goto out;
	}

	/*allocate cipher */
	tfm = crypto_alloc_cipher("aes",0,0);
	if(!tfm){
		printk(KERN_WARNING "ebd: failure allocating cipher.\n");
		goto out;
	}

	printk("ebd: encryption key: %s\n", key);
	printk("ebd: setting key...\n");
	crypto_cipher_setkey(tfm, key, strlen(key));

	/* gendisk structure.*/
	Device.gd = alloc_disk(16);
	if (!Device.gd)
		goto out_unregister;
	Device.gd->major = major_num;
	Device.gd->first_minor = 0;
	Device.gd->fops = &ebd_ops;
	Device.gd->private_data = &Device;
	strcpy(Device.gd->disk_name, "ebd0");
	set_capacity(Device.gd, nsectors);
	Device.gd->queue = Queue;
	add_disk(Device.gd);

	printk("ebd: block device initialized.\n");

	return 0;

out_unregister:
	unregister_blkdev(major_num, "ebd");
out:
	vfree(Device.data);
	return -ENOMEM;
}

static void __exit ebd_exit(void)
{
	/* free cipher*/
	crypto_free_cipher(tfm);

	del_gendisk(Device.gd);
	put_disk(Device.gd);
	unregister_blkdev(major_num, "ebd");
	blk_cleanup_queue(Queue);

}

module_init(ebd_init);
module_exit(ebd_exit);