#include <iostream>
#include <cstdint>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define LARGE_NUM 100000000
#define USING_MMAP 128000
#define LARGE_ENOUGH_BLOCK 128

struct MallocMetadata{
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata* lastBlock = nullptr;
MallocMetadata* globalMemory = nullptr; /**the global memory list*/
MallocMetadata* globalMmap = nullptr; /**the mmap memory list*/

MallocMetadata* pointer_to_next(MallocMetadata* pointer){
    MallocMetadata* itr = globalMemory;
    
    MallocMetadata* temp = pointer + 1;
	void* help = ((bool*)temp) + (pointer->size);
	temp = (MallocMetadata*)help;
    for ( ; itr != nullptr; itr = itr->next){
        if(temp == itr){
			return itr;
        }
    }   	
    return nullptr;
}

MallocMetadata* pointer_to_prev(MallocMetadata* pointer){
    MallocMetadata* itr = globalMemory;

    for ( ; itr != nullptr; itr = itr->next){	
	    MallocMetadata* temp = itr + 1;
		void* help = ((bool*)temp) + (itr->size);
		temp = (MallocMetadata*)help;

        if(temp == pointer){
			return itr;
        }
    }   	
    return nullptr;
}

void* add_element_memory(MallocMetadata* element){
	if(element == nullptr){
		return nullptr;
	}
	
    MallocMetadata* itr = globalMemory;
    if(itr == nullptr){
		globalMemory = element;
	} else{
		for ( ; itr != nullptr; itr = itr->next){
			if((element->size == itr->size)	&& (element < itr)){
				if(itr->prev == nullptr){ /**change the head of the global list*/
					element->next = itr;
					itr->prev = element;
					globalMemory = element;
					element->prev = nullptr;
				}
				else{
					element->next = itr;
					element->prev =  itr->prev;
					itr->prev = element;
					if(element->prev != nullptr){
						element->prev->next = element;
					}
				}
				break;
			}
			
			if(element->size < itr->size){
				if(itr->prev == nullptr){ /**change the head of the global list*/
					element->next = itr;
					itr->prev = element;
					globalMemory = element;
					element->prev = nullptr;
				}
				else{
					element->next = itr;
					element->prev =  itr->prev;
					itr->prev = element;
					if(element->prev != nullptr){
						element->prev->next = element;
					}
				}
				break;
			}
			
			/** we reached the end of the list*/
			if(itr->next == nullptr){
				itr->next = element;
				element->prev = itr;
				element->next = nullptr;
				break;
			}
		}
	}

    return(element + 1);	
}

void* add_element_mmap(MallocMetadata* element){
	if(element == nullptr){
		return nullptr;
	}
	
    MallocMetadata* itr = globalMmap;
    if(itr == nullptr){
		globalMmap = element;
	} else{
		for ( ; itr != nullptr; itr = itr->next){
			if((element->size == itr->size)	&& (element < itr)){
				if(itr->prev == nullptr){ /**change the head of the global list*/
					element->next = itr;
					itr->prev = element;
					globalMmap = element;
					element->prev = nullptr;
				}
				else{
					element->next = itr;
					element->prev =  itr->prev;
					itr->prev = element;
					if(element->prev != nullptr){
						element->prev->next = element;
					}	
				}
				break;
			}			
			
			if(element->size < itr->size){
				if(itr->prev == nullptr){ /**change the head of the global list*/
					element->next = itr;
					itr->prev = element;
					globalMmap = element;
					element->prev = nullptr;
				}
				else{
					element->next = itr;
					element->prev =  itr->prev;
					itr->prev = element;
					if(element->prev != nullptr){
						element->prev->next = element;
					}						
				}
				break;
			}
			/** we reached the end of the list*/
			if(itr->next == nullptr){
				itr->next = element;
				element->prev = itr;
				element->next = nullptr;
				break;
			}
		}
	}

    return(element + 1);	
}

MallocMetadata* remove_element(MallocMetadata* global, MallocMetadata* element){

	if((global == globalMemory) && (element == lastBlock)){
		lastBlock = pointer_to_prev(element);
	}

	if(element->prev != nullptr){
		(element->prev)->next = element->next;
	} else{
		if(global == globalMemory){
			globalMemory = element->next;
			global = globalMemory;
		} else{
			globalMmap = element->next;
			global = globalMmap;
		}
		if(global != nullptr)
			global->prev = nullptr;
	}
	
	if(element->next != nullptr){
		(element->next)->prev = element->prev;
	}
	
    return(element);	
}

MallocMetadata* combine_free_blocks(MallocMetadata* pointer){
	if(pointer == nullptr)
		return nullptr;

	MallocMetadata* help = pointer;
	MallocMetadata* temp = pointer_to_next(pointer);		
	if((temp != nullptr) && (temp->is_free)){
		temp = remove_element(globalMemory, temp);
		MallocMetadata* help = lastBlock;
		pointer = remove_element(globalMemory, pointer);	
		lastBlock = help;
		pointer->size += sizeof(MallocMetadata) + (temp->size);		
		add_element_memory(pointer);
	}

	temp = pointer_to_prev(pointer);	
	if((temp != nullptr) && (temp->is_free)){
		pointer = remove_element(globalMemory, pointer);	
		MallocMetadata* help = lastBlock;
		temp = remove_element(globalMemory, temp);
		lastBlock = help;
		temp->size += sizeof(MallocMetadata) + (pointer->size);
		add_element_memory(temp);
		help = temp;
	}	
	return help;
}

MallocMetadata* combine_with_next(MallocMetadata* pointer){
	if(pointer == nullptr)
		return nullptr;

	MallocMetadata* help;
	MallocMetadata* next = pointer_to_next(pointer);	
	if((next != nullptr) && (next->is_free)){
		next = remove_element(globalMemory, next);	
		help = lastBlock;
		pointer = remove_element(globalMemory, pointer);
		lastBlock = help;
		pointer->size += sizeof(MallocMetadata) + (next->size);
		add_element_memory(pointer);
		pointer->is_free = true;
	}	
	return pointer;
}

MallocMetadata* combine_with_prev(MallocMetadata* pointer){
	if(pointer == nullptr)
		return nullptr;

	MallocMetadata* help;
	MallocMetadata* prev = pointer_to_prev(pointer);	
	if((prev != nullptr) && (prev->is_free)){
		pointer = remove_element(globalMemory, pointer);	
		help = lastBlock;
		prev = remove_element(globalMemory, prev);
		lastBlock = help;
		prev->size += sizeof(MallocMetadata) + (pointer->size);
		add_element_memory(prev);
		pointer = prev;
		pointer->is_free = true;
	}
	return pointer;		
}

void* smalloc_split_blocks_memory(MallocMetadata* curr, size_t size){
	
	bool isTrue = false;
	if(curr == lastBlock){
		isTrue = true;
	}	
	
    curr->is_free = false;
	/**the beginning of the new block*/
	MallocMetadata* newData = curr + 1;
	void* temp = ((bool*)newData)+size;
	newData = (MallocMetadata*)temp;
 
    newData->size = (curr->size) - size - sizeof(MallocMetadata);
    newData->next = nullptr;
    newData->prev = nullptr;
    curr->size = size;
    newData->is_free = true;

	curr = remove_element(globalMemory, curr);

	if(isTrue){
		lastBlock = newData;
	}

    /** add newData to sorted list*/
	add_element_memory(newData);
	newData = combine_with_next(newData);

    /** add curr to sorted list*/
	return (add_element_memory(curr));
}

void* smalloc_split_blocks_mmap(MallocMetadata* curr, size_t size){
    curr->is_free = false;
	/**the beginning of the new block*/
	MallocMetadata* newData = curr + 1;
	void* temp = ((bool*)newData)+size;
	newData = (MallocMetadata*)temp;
 
    newData->size = (curr->size) - size - sizeof(MallocMetadata);
    newData->next = nullptr;
    newData->prev = nullptr;
    curr->size = size;
    newData->is_free = true;

	curr = remove_element(globalMmap, curr);

    /** add newData to sorted list*/
	add_element_mmap(newData);
	
    /** add curr to sorted list*/
	return (add_element_mmap(curr));
}

void* smalloc_old(size_t size){      
    if(size == 0) 
		return nullptr;
    if(size > LARGE_NUM+sizeof(MallocMetadata))
		return nullptr;
	void* start = sbrk(0);	
    int* result = (int*)sbrk(size);
    if(*result == -1) 
		return nullptr;
    return start;
}

void* smmap(size_t size){
	size_t size2  = size - sizeof(MallocMetadata);
	if((size2 <= LARGE_NUM) && (size2 >= USING_MMAP)){	
		void* result = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	    if(result == MAP_FAILED){
			return nullptr;
		}
        return result;	
	}	
	return nullptr;	
}

void* smalloc_memory(size_t size){	
    MallocMetadata* curr = globalMemory;
    for ( ; curr != nullptr; curr = curr->next){
        /** we found a free block*/
       if(((curr->size) >= size) && (curr->is_free)){
            /** check if splitting is needed*/
			int temp = (curr->size) - size - sizeof(MallocMetadata);
            if((temp > 0) && (temp >= LARGE_ENOUGH_BLOCK)){
                return smalloc_split_blocks_memory(curr, size);
            }		   

            curr->is_free = false;
            return(curr+1);
       }
    }

	MallocMetadata* newData;
	if((lastBlock != nullptr) && (lastBlock->is_free)){
		newData = lastBlock;   		
        void* empty = smalloc_old(size - (newData->size));
        if(empty == nullptr)
            return nullptr;
        newData->is_free = false;
        newData->size = size; 
        MallocMetadata* help = lastBlock;
        remove_element(globalMemory, newData);
        lastBlock = help;     
	} else{
		/** no block is free*/
		void* empty = smalloc_old(sizeof(MallocMetadata)+size);
		if(empty == nullptr) 
			return nullptr;
		newData = (MallocMetadata*)empty;		
		newData->size = size;
		newData->is_free = false;
		newData->next = nullptr;
		newData->prev = nullptr;
		lastBlock =	newData;   		
	}

    return add_element_memory(newData);	
}

void* smalloc_mmap(size_t size){
	MallocMetadata* curr = globalMmap;
	for ( ; curr != nullptr; curr = curr->next){
		/** we found a free block*/
		if(((curr->size) >= size) && (curr->is_free)){
			/** check if splitting is needed*/
			int temp = (curr->size) - size - sizeof(MallocMetadata);
			if(temp >= LARGE_ENOUGH_BLOCK){
				return smalloc_split_blocks_mmap(curr, size);
			}					
			curr->is_free = false;
			return(curr+1);
		}
	}
		
	/** no block is free*/
	void* empty = smmap(sizeof(MallocMetadata)+size);
	if(empty == nullptr) 
		return nullptr;
		
	MallocMetadata* newData = (MallocMetadata*)empty;		
	newData->size = size;
	newData->is_free = false;
	newData->next = nullptr;
	newData->prev = nullptr;

	/** add block to sorted list*/
	return add_element_mmap(newData);
}

void* smalloc(size_t size){ /**Searches for a block with ‘size’ bytes or allocates (sbrk()) one if none are found. */
    if(size == 0)
		return nullptr;
    if(size > LARGE_NUM)
		return nullptr;
	if(size >= USING_MMAP){
		return smalloc_mmap(size);
	}
	return smalloc_memory(size);
}

void* scalloc(size_t num, size_t size){  
    if((size == 0) || (num == 0)) 
		return nullptr;
    if(size*num > LARGE_NUM)
		return nullptr; 

	void* pointer = smalloc(size*num); 

	if(pointer == nullptr)
		return nullptr;

	return (memset(pointer, 0, num*size));
}

void sfree(void* p) { /**Releases the usage of the block that starts with the pointer ‘p’*/        
    if(p == nullptr) 
		return;
		
    MallocMetadata* itr = globalMemory;
    for ( ; itr != nullptr; itr = itr->next){
        if((itr+1) == p){
			itr->is_free = true;
			combine_free_blocks(itr);
			return;
        }
    }   
     
    itr = globalMmap;  
    for ( ; itr != nullptr; itr = itr->next){
        if((itr + 1) == p){
			itr->is_free = true;
			if(itr->prev != nullptr){
				itr->prev->next = itr->next;
				if(itr->next !=nullptr){
					itr->next->prev = itr->prev;
				}
			} else{
				globalMmap = itr->next;
				if(globalMmap != nullptr){
					globalMmap->prev = nullptr;
				}
			}
	
			munmap(itr, (itr->size) + sizeof(MallocMetadata));			
			return;
        }
    }      
}

void* is_there_need_to_split(MallocMetadata* pointer, size_t size){
	int temp = (pointer->size) - size - sizeof(MallocMetadata);
	if((temp > 0) && (temp >= LARGE_ENOUGH_BLOCK)){
		return smalloc_split_blocks_memory(pointer, size);
	}
	return nullptr;		
}

void* merge_blocks_for_srealloc(MallocMetadata* pointer, size_t size){
	if(pointer == nullptr)
		return nullptr;
	
	MallocMetadata* help;	
	MallocMetadata* temp_prev = pointer_to_prev(pointer);	
	size_t all = pointer->size + sizeof(MallocMetadata);
	if((temp_prev != nullptr) && (temp_prev->is_free) && (size <= (all+(temp_prev->size)))){
		all += temp_prev->size;
		remove_element(globalMemory, pointer);
		
		help = lastBlock;
		remove_element(globalMemory, temp_prev);
		temp_prev->size = all;
		add_element_memory(temp_prev);
		lastBlock = help;		
		
		temp_prev = combine_with_prev(temp_prev);
		
		void* result = is_there_need_to_split(temp_prev, size);
		if(result != nullptr)
			return result;
		return (temp_prev+1); 	
	}
		 
	MallocMetadata* temp_next = pointer_to_next(pointer);	
	all = pointer->size + sizeof(MallocMetadata);
	if((temp_next != nullptr) && (temp_next->is_free) && (size <= (all+(temp_next->size)))){
		all += temp_next->size;
		remove_element(globalMemory, temp_next);
		
		help = lastBlock;
		remove_element(globalMemory, pointer);
		pointer->size = all;
		add_element_memory(pointer);
		lastBlock = help;		

		pointer = combine_with_next(pointer);
		
		void* result = is_there_need_to_split(pointer, size);
		if(result != nullptr)
			return result;
		return (pointer+1); 		
	}	 
	 	 
	all = pointer->size + 2*sizeof(MallocMetadata);
	if((temp_next != nullptr) && (temp_prev != nullptr) && (temp_next->is_free) && (temp_prev->is_free) && (size <= (all + (temp_next->size) + (temp_prev->size)))){
		all += temp_next->size + temp_prev->size;
		remove_element(globalMemory, temp_next);
		remove_element(globalMemory, pointer);
		
		help = lastBlock;
		remove_element(globalMemory, temp_prev);
		temp_prev->size = all;
		add_element_memory(temp_prev);
		lastBlock = help;		
		temp_prev = combine_with_next(temp_prev);	
		void* result = is_there_need_to_split(temp_prev, size);
		if(result != nullptr)
			return result;
		return (temp_prev+1); 		
	}
		
	return nullptr;
}

void* srealloc(void* oldp, size_t size){  
    if(size == 0) 
		return nullptr;
    if(size > LARGE_NUM)
		return nullptr; 
 
	if(oldp == nullptr){
		return(smalloc(size)); 		
	}
   
    MallocMetadata* itr = globalMemory;
    for ( ; itr != nullptr; itr = itr->next){
        if((itr+1) == oldp){
			if(size <= itr->size){
				itr->is_free = false;
				void* temp = is_there_need_to_split(itr, size);
				
				if(temp != nullptr){
					return temp;
				}	
            	return oldp;
			}

			MallocMetadata* newData;
			if((itr == lastBlock) && (lastBlock != nullptr)){
				newData = lastBlock;   		
				void* empty = smalloc_old(size - (newData->size));
				if(empty == nullptr)
					return nullptr;
				newData->is_free = false;
				newData->size = size; 
				remove_element(globalMemory, newData); 	
				lastBlock = newData;	 
				void* temp = newData+1;
				memcpy(temp, oldp, size);
				return add_element_memory(newData);			   
			}
		
			void* temp = merge_blocks_for_srealloc(itr, size);	
			if(temp != nullptr){
				(((MallocMetadata*)temp)-1)->is_free = false;
				memcpy(temp, oldp, size);
				return temp;
			}
		
			break;
		}
	}

	void* pointer = smalloc(size); 
	if(pointer == nullptr)
		return nullptr;

	memcpy(pointer, oldp, size);
	sfree(oldp);

	return pointer;
}

size_t _num_free_blocks(){ /**the number of allocated blocks in the heap that are currently free. */
    size_t counter = 0;
    MallocMetadata* curr = globalMemory;
    while (curr != nullptr){				
        if(curr->is_free == true)
            counter++;
        curr = curr->next;
    }      
    return counter;
}

size_t _num_free_bytes(){ 
    size_t counter = 0;
    MallocMetadata* curr = globalMemory;
    while (curr != nullptr){
        if(curr->is_free == true)
            counter += curr->size;
        curr = curr->next;
    }
    return counter;
}

size_t _num_allocated_blocks(){ /**the overall (free and used) number of allocated blocks in the heap. */
    size_t counter = 0;
    MallocMetadata* curr = globalMemory;
    while (curr != nullptr){
        counter++;
        curr = curr->next;
    }
    curr = globalMmap;
    while (curr != nullptr){
        counter++; 
        curr = curr->next;
    }    

    return counter;
}

size_t _num_allocated_bytes(){
	size_t counter = 0;
    MallocMetadata* curr = globalMemory;
    while (curr != nullptr){
        counter += curr->size;
        curr = curr->next;
    }
    
    curr = globalMmap;
    while (curr != nullptr){
        counter += curr->size;
        curr = curr->next;
    }    
    return counter;
}

size_t _num_meta_data_bytes(){ /**the overall number of meta-data bytes currently in the heap*/  
    size_t counter = 0;
    MallocMetadata* curr = globalMemory;
    while (curr != nullptr){
        counter += sizeof(MallocMetadata);
        curr = curr->next;
    }
    curr = globalMmap;
    while (curr != nullptr){
        counter += sizeof(MallocMetadata);
        curr = curr->next;
    }    
    return counter;
}

size_t _size_meta_data(){ 
    return (sizeof(MallocMetadata));	
}
