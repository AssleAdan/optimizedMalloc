#include <iostream>
#include <unistd.h>

#define LARGE_NUM 100000000

void* smalloc(size_t size){
    if(size == 0) 
		return nullptr;
    if(size > LARGE_NUM)
		return nullptr;
	void* start = sbrk(0);	
    int* result = (int*)sbrk(size);
    if(*result == -1) 
		return nullptr;
    return start;
}
