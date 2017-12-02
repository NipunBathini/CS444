#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

int main(void){
    long free_space = syscall(359);
    long used_space = syscall(360); 

    printf("free space: %lu\n", free_space);
    printf("used space: %lu\n\n",used_space);

    return 0;
}
