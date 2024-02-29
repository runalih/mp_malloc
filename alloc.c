/**
 * Malloc
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// /**
//  * Allocate space for array in memory
//  *
//  * Allocates a block of memory for an array of num elements, each of them size
//  * bytes long, and initializes all its bits to zero. The effective result is
//  * the allocation of an zero-initialized memory block of (num * size) bytes.
//  *
//  * @param num
//  *    Number of elements to be allocated.
//  * @param size
//  *    Size of elements.
//  *
//  * @return
//  *    A pointer to the memory block allocated by the function.
//  *
//  *    The type of this pointer is always void*, which can be cast to the
//  *    desired type of data pointer in order to be dereferenceable.
//  *
//  *    If the function failed to allocate the requested block of memory, a
//  *    NULL pointer is returned.
//  *
//  * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
//  */
void *calloc(size_t num, size_t size) {
   // implement calloc:
    void *mem = malloc(num*size);
    if (mem) {
      memset(mem, 0, num*size);
      return mem;
    }
    return NULL;
}
// /**
//  * Allocate memory block
//  *
//  * Allocates a block of size bytes of memory, returning a pointer to the
//  * beginning of the block.  The content of the newly allocated block of
//  * memory is not initialized, remaining with indeterminate values.
//  *
//  * @param size
//  *    Size of the memory block, in bytes.
//  *
//  * @return
//  *    On success, a pointer to the memory block allocated by the function.
//  *
//  *    The type of this pointer is always void*, which can be cast to the
//  *    desired type of data pointer in order to be dereferenceable.
//  *
//  *    If the function failed to allocate the requested block of memory,
//  *    a null pointer is returned.
//  *
//  * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
//  */


typedef struct _metadata_t metadata_t;
struct _metadata_t {
    uint32_t size;          
    uint32_t capacity;      
    uint8_t used;      
    uint32_t next;       
    uint32_t prev;       
    metadata_t *next_free; 
};


static uint32_t addrDist(void *meta1, void *meta2) {
    return (meta2 > meta1) ? (meta2 - meta1) : (meta1 - meta2);
}

void *startOfHeap = NULL;
metadata_t *head = NULL;  
metadata_t *tail = NULL;
metadata_t *free_head = NULL;

void printHeap() {
    metadata_t *curMeta = startOfHeap;
    void *endOfHeap = sbrk(0);
    printf("-- Start of Heap (%p) --\n", startOfHeap);
    while ((void *)curMeta < endOfHeap) {   // While we're before the end of the heap...
        printf("metadata for memory %p: (%p, size=%d, isUsed=%d)\n", 
            (void *)curMeta + sizeof(metadata_t), 
            curMeta, 
            curMeta->size, 
            curMeta->used
        );
        curMeta = (void *)curMeta + curMeta->size + sizeof(metadata_t);
    }
    printf("-- End of Heap (%p) --\n\n", endOfHeap);
}

void printFree() {
    metadata_t *curMeta = free_head;
    if (!curMeta) { 
        printf("no free list\n\n");
        return;
    }
    printf("-- Start of Free --\n");
    while (curMeta) {
        printf("metadata for free memory %p: (%p, size=%d, isUsed=%d)\n", 
            (void *)curMeta + sizeof(metadata_t), 
            curMeta, 
            curMeta->size, 
            curMeta->used
        );
        curMeta = curMeta->next_free;
    }
    printf("-- End of Free --\n\n");
}


void add_to_free_list(metadata_t *meta) {
    if (!meta) return;
    // insert as the first node in linked list of free blocks
    meta->next_free = free_head;
    free_head = meta;
}

void splitFreeBlock(metadata_t *meta, uint32_t size1, uint32_t size2) {
    //starting address of new block
    void *ptr2 = (char *)meta + sizeof(metadata_t) + size1;
    metadata_t *meta2 = (metadata_t *)ptr2;

    //creates new node
    meta2->size = size2;
    meta2->capacity = size2;
    meta2->used = 0;

    //first half of block
    meta->capacity = size1;
    meta->size = meta->capacity;

    if (meta->next) {
        meta2->next_free = meta->next_free;
    }
    else {
        tail = meta2;
    }
    //sets meta next to meta2
    meta->next = meta2->prev = addrDist(meta, meta2);
    if (tail == meta) tail = meta2;

    add_to_free_list(meta2);
}

void remove_from_free_list(metadata_t *meta) {
    if (!meta) return;
    // first node of linked list
    if (free_head == meta) {
        free_head = meta->next_free;
        meta->next_free = NULL;
        return;
    }
    // locate the node
    metadata_t *curr = free_head;
    while (curr->next_free != meta) curr = curr->next_free;
    // adjust pointers to remove the block from linked list
    curr->next_free = meta->next_free;
    meta->next_free = NULL;
}

void coalesce(metadata_t *meta1, metadata_t *meta2) {
    //removes 2 blocks from free
    remove_from_free_list(meta1);
    remove_from_free_list(meta2);

    //combines meta1 size
    meta1->size = meta1->capacity = (uint32_t)(meta1->capacity + meta2->capacity + sizeof(metadata_t));

    meta1->next = 0;
    //sets meta2's next to meta1's next
    if (meta2->next) {
        metadata_t* newNext = (metadata_t *)((char *)meta2 + meta2->next);
        meta1->next = newNext->prev = addrDist(meta1, newNext);
    }
    //if meta2 is tail
    else {
        tail = meta1;
    }
    add_to_free_list(meta1);
}

void *malloc(size_t size_) {
    // printf("%zu MALLOCING\n\n\n", size_);
    if (size_ == 0) {
        return NULL;
    }
    //starts heap
    if (!startOfHeap) {
        startOfHeap = sbrk(0);
        if (startOfHeap == (void *)-1) {
            return NULL;
        }
    }
    //iterates through free list and finds first available unused block of large enough size
    metadata_t *meta = NULL;
    metadata_t *curr = free_head;
    while (curr) {
      if (curr->size >= size_ && !curr->used)  {
        //sets meta to the pointer to the open block
        meta = curr;
        break;
      } 
      curr = curr->next_free;
    }

    void *ptr;

    //if no free block, add it to the end
    if (!meta) {

      //create new metadata block
      ptr = sbrk(size_ + sizeof(metadata_t));
      metadata_t *mdata = (metadata_t *)ptr;
      mdata->size = size_;
      mdata->capacity = size_;
      mdata->used = 1;

      //add to tail
      metadata_t *pointer = (metadata_t *)ptr;
      //if heap is empty
      if (!head) {
        head = tail = pointer;
      } else {
        pointer->prev = addrDist(tail, pointer);
        tail->next = pointer->prev;
        tail = pointer;
      }
    }

    // if size matches block size exactly, use block
    else if (size_ == meta->size) ptr = meta;

    // split block
    else {
        int extraBytes = meta->size - size_;
        int min_needed = sizeof(metadata_t) + 16;

        if (extraBytes > min_needed) {
            uint32_t secondBlock = extraBytes - sizeof(metadata_t);
            splitFreeBlock(meta, size_, secondBlock);
        }
        meta->size = size_;
        meta->used = 1;
        remove_from_free_list(meta);
        ptr = meta;
    }
    if (ptr) {
        // printHeap();
        // printFree();
      return ptr + sizeof(metadata_t);
    }
    return NULL;
}

// /**
//  * Deallocate space in memory
//  *
//  * A block of memory previously allocated using a call to malloc(),
//  * calloc() or realloc() is deallocated, making it available again for
//  * further allocations.
//  *
//  * Notice that this function leaves the value of ptr unchanged, hence
//  * it still points to the same (now invalid) location, and not to the
//  * null pointer.
//  *
//  * @param ptr
//  *    Pointer to a memory block previously allocated with malloc(),
//  *    calloc() or realloc() to be deallocated.  If a null pointer is
//  *    passed as argument, no action occurs.
//  */
void free(void *ptr) {
    if (ptr == NULL) return;

    metadata_t *meta = (metadata_t *)(ptr - sizeof(metadata_t));
    meta->used = 0;
    meta->size = meta->capacity;
    if (meta) {
        meta->next_free = free_head;
        free_head = meta;
    }

    // if the next block is free, merge
    metadata_t *metaNext = (metadata_t *)((char *)meta + meta->next);
    if (meta->next && metaNext->used == 0)
        coalesce(meta, metaNext);

    // if the previous block is free, merge
    metadata_t *metaPrev = (metadata_t *)((char *)meta - meta->prev);
    if (meta->prev && metaPrev->used == 0) {
        coalesce(metaPrev, meta);
    }
}

// /**
//  * Reallocate memory block
//  *
//  * The size of the memory block pointed to by the ptr parameter is changed
//  * to the size bytes, expanding or reducing the amount of memory available
//  * in the block.
//  *
//  * The function may move the memory block to a new location, in which case
//  * the new location is returned. The content of the memory block is preserved
//  * up to the lesser of the new and old sizes, even if the block is moved. If
//  * the new size is larger, the value of the newly allocated portion is
//  * indeterminate.
//  *
//  * In case that ptr is NULL, the function behaves exactly as malloc, assigning
//  * a new block of size bytes and returning a pointer to the beginning of it.
//  *
//  * In case that the size is 0, the memory previously allocated in ptr is
//  * deallocated as if a call to free was made, and a NULL pointer is returned.
//  *
//  * @param ptr
//  *    Pointer to a memory block previously allocated with malloc(), calloc()
//  *    or realloc() to be reallocated.
//  *
//  *    If this is NULL, a new block is allocated and a pointer to it is
//  *    returned by the function.
//  *
//  * @param size
//  *    New size for the memory block, in bytes.
//  *
//  *    If it is 0 and ptr points to an existing block of memory, the memory
//  *    block pointed by ptr is deallocated and a NULL pointer is returned.
//  *
//  * @return
//  *    A pointer to the reallocated memory block, which may be either the
//  *    same as the ptr argument or a new location.
//  *
//  *    The type of this pointer is void*, which can be cast to the desired
//  *    type of data pointer in order to be dereferenceable.
//  *
//  *    If the function failed to allocate the requested block of memory,
//  *    a NULL pointer is returned, and the memory block pointed to by
//  *    argument ptr is left unchanged.
//  *
//  * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
//  */
void *realloc(void *ptr, size_t size_) {
    if (ptr == NULL) return malloc(size_);

    if (size_ == 0) {
        free(ptr);
        return NULL;
    }

    metadata_t *meta = ptr - sizeof(metadata_t);
    
    if (size_ <= meta->capacity) {
        meta->size = size_;
        return ptr;
    }

    void *newMem = malloc(size_);
    if (newMem == NULL) return NULL;
    memcpy(newMem, ptr, meta->size);
    
    free(ptr);
    return newMem;
}
