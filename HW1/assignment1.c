/*
Names: Nipun Bathini and Parker Bruni
CS 444
Assignment 1
10/1/2017
*/ 

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "mt19937ar.c" //source http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/emt19937ar.html

struct Container A;
int SystemToggle;

typedef struct {
	short numb;
	short wait; 
} DATA;


struct Container {
	short producer; 
	short consumer;
	
	DATA items[32];
	pthread_mutex_t Lock;
	pthread_cond_t producerCond; //conditions for both producer and consumer used to signal
	pthread_cond_t consumerCond;
	
};

//int SystemToggle;

int SystemType(){
	//registers
	unsigned int eax = 0x01;
	unsigned int ebx;
	unsigned int ecx;
	unsigned int edx;  
	
	//https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
		__asm__ __volatile__("cpuid;"
							 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
							 : "a"(eax)
							 );
		
	if(ecx & 0x40000000){//32 bit bitwise& operation
		SystemToggle = 1;
	}
	else{//64 bit
		SystemToggle = 0;
	}
	
	return SystemToggle;
}

void printData(DATA *Item){
	static int counter = 0;
	
	printf("%d:", counter);
	counter++;
	
	printf("\tItem #: %d\n", Item->numb);
	printf("Item wait: %d\n\n", Item->wait);
}

int getRandomNum(int min, int max){
	int randomNum = 0; // will store the random number
	
	if(SystemToggle == 0){
		randomNum = (int)genrand_int32();
	}
	else {
		__asm__ __volatile__("rdrand %0":"=r"(randomNum));
	}
	
	randomNum = abs(randomNum % (max - min));
	if(randomNum < min) {
		return min;
	}
	
	return randomNum;
}



void *consumerFoo(void *cons){
	while(1){
		pthread_mutex_lock(&A.Lock);
		DATA takeItem;
		
		if(A.consumer >= 32){
			A.consumer = 0;
		}
		
		pthread_cond_signal(&(A.producerCond)); //signal that consumer is ready
		pthread_cond_wait(&(A.consumerCond), &A.Lock);
		
		if(A.producer == 0){
			printf("Reached Max!!!!!!\n");
			pthread_cond_wait(&(A.consumerCond), &A.Lock);
		}
		
		takeItem = A.items[A.consumer];
		printf("--Consuming item: %d\n\n", takeItem.numb);
		sleep(takeItem.wait);
		printf("--Consumed item: %d\n\n", takeItem.numb);
		A.consumer++;
		
		pthread_mutex_unlock(&A.Lock);
	}
}

void *producerFoo(void *prods){
	while(1){
		pthread_mutex_lock(&A.Lock);
		
		DATA newItem;
		
		newItem.numb = getRandomNum(1,100);
		newItem.wait = getRandomNum(2,9);
		printf("Produced Item: ");
		printData(&newItem);
		
		if(A.producer == 32){
			printf("Reached Max, BUFFER FULL!!\n\n");
			pthread_cond_signal(&(A.consumerCond));
			pthread_cond_wait(&(A.producerCond), &A.Lock);
		}
		
		A.items[A.producer] = newItem;
		A.producer++; 
		
		pthread_cond_signal(&(A.consumerCond));
		pthread_cond_wait(&(A.producerCond), &A.Lock);
		
		if(A.producer >= 32){
			printf("Reached Max, BUFFER FULL!!\n\n");
			A.producer = 0;
		}
		
		pthread_mutex_unlock(&A.Lock);
	}
}

int main(int argc, char* argv[]){
	int pairs;
	
	if(argc <= 1){
		pairs = 1;
	}
	else{
		pairs = atoi(argv[1]);
	}
	
	SystemType();
	
	A.consumer = 0;
	A.producer = 0;
	
	pthread_t threads[2 * pairs];
	
	int i;
	
	for(i = 0; i < pairs; i++){
		pthread_create(&threads[i], NULL, consumerFoo, NULL);
		pthread_create(&threads[i+1], NULL, producerFoo, NULL);
	}
	
	for(i = 0; i < (2 * pairs); i++){
		pthread_join(threads[i], NULL);
	}
	
	return 0;
}
		
	
	
		
	
	