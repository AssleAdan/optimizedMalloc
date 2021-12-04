#include <unistd.h>
#include <cmath>
#include <iostream>
#include <cstring>
#include <cstddef>



struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata *next;
    MallocMetadata *prev;
    void* block_pointer;
};

size_t _size_meta_data(){
    return sizeof(MallocMetadata);
}

struct metaDataList {
    MallocMetadata *firstMeta;
    MallocMetadata *lastMeta;
    size_t allocBlocks;
    size_t allocBytes;
    size_t usedBlocks;
    size_t usedBytes;
    size_t num_metaData;
    metaDataList() : firstMeta(), lastMeta(), allocBlocks(0), allocBytes(0), usedBlocks(0), usedBytes(0), num_metaData(0) {}

    void addToList(size_t size){
        allocBlocks++;
        usedBlocks++;
        allocBytes += size;
        allocBytes+= _size_meta_data();
        usedBytes += _size_meta_data();
        usedBytes += size;
        num_metaData++;
    }

    void reuseBlock(size_t size){
        usedBytes += size;
        usedBlocks++;
    }

    void freeBlock(size_t size){
        usedBlocks--;
        usedBytes -= size;
    }

};

metaDataList* meta_data_list = new metaDataList();


size_t _num_free_blocks() {
    return (meta_data_list->allocBlocks - meta_data_list->usedBlocks);
}

size_t _num_free_bytes() {
    return (meta_data_list->allocBytes - meta_data_list->usedBytes);
}

size_t _num_allocated_blocks(){
    return meta_data_list->allocBlocks;
}

size_t _num_meta_data_bytes(){
    return _size_meta_data() * (meta_data_list->num_metaData);
}

size_t _num_allocated_bytes(){
    return (meta_data_list->allocBytes - _num_meta_data_bytes());
}




///Searches for a block with ‘size’ bytes or allocates (sbrk()) one if none are found.
void* smalloc(size_t size) {

    if (size == 0 || size > pow(10, 8)) {
        return NULL;
    }

    void *pointer;
    void *res_pointer;
    MallocMetadata *curr = meta_data_list->firstMeta;
    //if list empty
    //check if first metaData:
    if (curr == NULL) {
        //allocate for metaData:
        pointer = sbrk(_size_meta_data());
        if (pointer == (void *) (-1)) {
            return NULL;
        }

        MallocMetadata *new_metaData = (struct MallocMetadata *) pointer;
        new_metaData->size = size;
        new_metaData->is_free = false;
        new_metaData->prev = NULL;
        new_metaData->next = NULL;
        meta_data_list->firstMeta = new_metaData;
        meta_data_list->lastMeta = new_metaData;

        //allocate size bytes for the user:
        res_pointer = sbrk(size);
        if (res_pointer == (void *) (-1)) {
            return NULL;
        }
        new_metaData->block_pointer = res_pointer;
        meta_data_list->addToList(size);
        return res_pointer;
    }
    //if got here then the list is not empty
    //first search for a freed block with enough size
    while (curr != NULL) {
        if (curr->is_free && curr->size >= size) {
            curr->is_free = false;
            meta_data_list->reuseBlock(curr->size);
            return curr->block_pointer;
        }
        curr = curr->next;
    }
    //if got here then no freed block available / enough size -> need to allocate
    //allocate for metaData:

    pointer = sbrk(_size_meta_data());
    if (pointer == (void *) (-1)) {
        return NULL;
    }
    MallocMetadata *new_metaData = (struct MallocMetadata *) pointer;
    new_metaData->size = size;
    new_metaData->is_free = false;
    new_metaData->prev = meta_data_list->lastMeta;
    new_metaData->next = NULL;
    meta_data_list->lastMeta->next = new_metaData;
    meta_data_list->lastMeta = new_metaData;

    //allocate size bytes for the user:
    res_pointer = sbrk(size);
    if (res_pointer == (void *) (-1)) {
        return NULL;
    }
    new_metaData->block_pointer = res_pointer;
    meta_data_list->addToList(size);
    return res_pointer;
}


void* scalloc(size_t num, size_t size){
    void* pointer = smalloc(num * size);
    if (pointer == NULL) {
        return NULL;
    }
    memset(pointer,0,num*size);
    return pointer;
}


void sfree(void* p){
    if (p == NULL) {
        return;
    }
    MallocMetadata* curr = meta_data_list->firstMeta;
    while (curr != NULL) {
        if (curr->block_pointer == p) {
            if(curr->is_free){
                return;
            }
            curr->is_free = true;
            meta_data_list->freeBlock(curr->size);
            return;
        }
        curr = curr->next;
    }
    return;
}

void* srealloc(void* oldp, size_t size){

    if (size == 0 || size > pow(10, 8)) {
        return NULL;
    }

    if (oldp == NULL) {
        return smalloc(size);
    }

    MallocMetadata* curr = meta_data_list->firstMeta;

    //search for oldp
    while(curr!= NULL && curr->block_pointer != oldp) {
        curr = curr->next;
    }

    //check if we can use the block
    if (curr != NULL && curr->size >= size){
        return  oldp;
    }

    //if we can not we have to alloc a new one
    void* pointer = smalloc(size);

    if (pointer == NULL) {
        return NULL;
    }
    //copy content
    memcpy(pointer,oldp,curr->size);
    sfree(oldp);
    return pointer;
}
