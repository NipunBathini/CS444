/**
 * elevator sstf
 */

/* Coders: Nipun Bathini and Parker Bruni
   Class: CS444
   Group: 20
   Date: 10/20/17
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/elevator.h>
#include <linux/blkdev.h>

struct sstf_data {
	struct list_head queue;
	sector_t head_pos;
};

static int sstf_init_queue(struct request_queue *q, struct elevator_type *e){
	struct sstf_data *s_data;
	struct elevator_queue *elev_q;
	elev_q = elevator_alloc(q, e);
	
	// memory related failure
	if (!elev_q){
		return -ENOMEM;
	}
	
	s_data = kmalloc_node(sizeof(*s_data), GFP_KERNEL, q->node);
	
	// memory related failure
	if(!s_data){
		kobject_put(&elev_q->kobj);
		return -ENOMEM;
	}
	
	elev_q->elevator_data = s_data;
	//initiate data for queue list
	INIT_LIST_HEAD(&s_data->queue);
	spin_lock_irq(q->queue_lock);
	q->elevator = elev_q;
	spin_unlock_irq(q->queue_lock);

	printk(KERN_DEBUG "Queue initiated\n");
	return 0;
}

//add request to request list in the infinite elevator mode "CLOOK"
static void sstf_add_request(struct request_queue *q, struct request *rqst){
	struct sstf_data *s_data = q->elevator->elevator_data;
	struct list_head *request_index_position;
	struct request *current_request;

	//if list is empty, simply add to list
	if(list_empty(&s_data->queue)){
		printk(KERN_DEBUG "Adding new item to empty queue.\n");
		list_add(&rqst->queuelist, &s_data->queue);
	}else{
		//iterate through list and seek until you find request for higher sector
		list_for_each(request_index_position, &s_data->queue){
			current_request = list_entry(request_index_position, struct request, queuelist);
			//if sector of current node is less than the new request node, put the request node in front
			if(blk_rq_pos(current_request) < blk_rq_pos(rqst)){
				list_add(&rqst->queuelist, &current_request->queuelist);
				printk(KERN_DEBUG "inserted request to sorted queue\n");
				//break out of queue iteration
				break;
			}
		}
	}
}

//merge request function
static void sstf_merged_requests(struct request_queue *q, struct request *rqst, struct request *next){
	list_del_init(&next->queuelist);
}

//dispatches the request to the request queue
static int sstf_dispatch(struct request_queue *q, int force){
	struct sstf_data *s_data = q->elevator->elevator_data;
	
	if(!list_empty(&s_data->queue)){	
		struct request *rqst = list_entry(s_data->queue.next, struct request, queuelist);
		
		printk(KERN_DEBUG "Sector request dispatched: %llu\n",blk_rq_pos(rqst));
		list_del_init(&rqst->queuelist);	
		//dispatch request to dispatch queue
		elv_dispatch_add_tail(q, rqst);													
		
		return 1;
	}
	return 0;
}

//gets former request
static struct request *
sstf_former_request(struct request_queue *q, struct request *rqst){
	struct sstf_data *s_data = q->elevator->elevator_data;

	if (rqst->queuelist.prev == &s_data->queue){
		return NULL;
	}
	return list_entry(rqst->queuelist.prev, struct request, queuelist);
}

//gets latter request
static struct request *
sstf_latter_request(struct request_queue *q, struct request *rqst){
	struct sstf_data *s_data = q->elevator->elevator_data;

	if (rqst->queuelist.next == &s_data->queue){
		return NULL;
	}
	return list_entry(rqst->queuelist.next, struct request, queuelist);
}

//frees the dynamically allocated list data on exit
static void sstf_exit_queue(struct elevator_queue *e){
	struct sstf_data *s_data = e->elevator_data;
	BUG_ON(!list_empty(&s_data->queue));
	kfree(s_data);
}


static struct elevator_type elevator_sstf = {
	.ops = {
		.elevator_merge_req_fn		= sstf_merged_requests,
		.elevator_dispatch_fn		= sstf_dispatch,
		.elevator_add_req_fn		= sstf_add_request,
		.elevator_former_req_fn		= sstf_former_request,
		.elevator_latter_req_fn		= sstf_latter_request,
		.elevator_init_fn		= sstf_init_queue,
		.elevator_exit_fn		= sstf_exit_queue,
	},
	.elevator_name = "sstf",
	.elevator_owner = THIS_MODULE,
};

static int __init sstf_init(void){
	return elv_register(&elevator_sstf);
}

static void __exit sstf_exit(void){
	elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);


MODULE_AUTHOR("Group 20");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF IO scheduler");