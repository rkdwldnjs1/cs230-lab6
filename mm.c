/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 * 
 * I will use implicit method and header, footer format. (boundary tag)
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

#define WS 4
#define DS 8
#define CHUNKSIZE (1<<8)

#define SUM(size, allocation) ((size|allocation))
#define PUT(p,val) (*(unsigned int *)(p) = (val))
#define GET(p) (*(unsigned int *)(p))
#define SIZE(p) ((*(unsigned int *)(p))&(~7))
#define ALLOC_BIT(p) ((*(unsigned int *)(p))&(1))
#define HEADP(bp) ((char *)(bp) - WS)
#define FOOTP(bp) ((char *)(bp) + SIZE(HEADP(bp)) - DS)
#define NEXT_BLP(bp) ((char *)(bp) + SIZE(HEADP(bp)))
#define PREV_BLP(bp) ((char *)(bp) - SIZE((char *)(bp) - DS))


/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */
static char *heap_listp;

static void *extend_heap(size_t words);
static void *find_space(size_t size);
static void *coalesce(char *bp);
static void insert(char *bp, size_t size);

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WS)) == (void *) -1){
        return -1; // fail
    }
    PUT(heap_listp, 0); // unused padding
    PUT(heap_listp + 4, SUM(DS,1)); //prologue
    PUT(heap_listp + 8, SUM(DS,1)); //prologue
    PUT(heap_listp + 12, 1); //epilogue
    heap_listp = heap_listp + DS;

    if(extend_heap(CHUNKSIZE/WS) == NULL) return -1;
    
    return 0;
}

static void *extend_heap(size_t words){

    char *bp;
    size_t size;
    
    if ((words%2)==0){
        size = words * WS;
    }
    else{
        size = (words+1)*WS;
    }
    
    if ((int)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    PUT(HEADP(bp), SUM(size,0)); // free header
    PUT(FOOTP(bp), SUM(size,0)); // free footer
    PUT(bp+size-WS, 1); //epilogue header 

    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

 
void *mm_malloc(size_t size)
{
    char *bp;
    size_t more_space;

    if (size == 0){
        return NULL;
    }
    if (size <= DS){
        size = 2*DS;
    }
    else{
        size = ALIGN(size+DS);
    }

    if ((bp = find_space(size)) != NULL){ 
        insert(bp, size); // when find proper free space
        return bp;
    }
    else {

        if( size > CHUNKSIZE ){ // MAX(size,CHUNKSIZE)
            more_space = size;
        }
        else{
            more_space = CHUNKSIZE;
        }

        if( (bp = extend_heap(more_space/WS)) == NULL){
            return NULL;
        }
        insert(bp, size);
        return bp;
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if ((ptr) == 0){
        return;
    }
    PUT(HEADP(ptr) , SIZE(HEADP(ptr)));
    PUT(FOOTP(ptr) , SIZE(FOOTP(ptr)));

    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL){
        return mm_malloc(size);
    }
    
    if (size == 0){
        mm_free(ptr);
        return 0;
    }

    void *newptr = mm_malloc(size);
    if (SIZE(HEADP(ptr)) - DS > size){
        memcpy(newptr, ptr, size); //copy by amount of payload (except header, footer)
    }
    else{
        memcpy(newptr, ptr, SIZE(HEADP(ptr)) - DS); //copy by amount of payload (except header, footer)
    }

    mm_free(ptr);

    return newptr;
}

static void *find_space(size_t size){

    char *bp = heap_listp;
    while (SIZE(HEADP(bp))>0){

        if ( (ALLOC_BIT(HEADP(bp)) == 0) && (SIZE(HEADP(bp)) >= size) ){
            return bp;
        }

        bp = NEXT_BLP(bp);
    }
    return NULL;
}

static void insert(char *bp, size_t size){

    size_t t;

    if ( (t = SIZE(HEADP(bp)) - size) < 2*DS ){ // left space is smaller than 4 words.
        PUT(HEADP(bp) , SUM(SIZE(HEADP(bp)),1));
        PUT(FOOTP(bp) , SUM(SIZE(HEADP(bp)),1));

    }
    else { // left space should be divided to allocated and free.
        PUT(HEADP(bp) , SUM(size,1));
        PUT(FOOTP(bp) , SUM(size,1));
        bp = NEXT_BLP(bp);
        PUT(HEADP(bp) , SUM(t,0));
        PUT(FOOTP(bp) , SUM(t,0));
    }
}

static void *coalesce(char *bp){
    
    size_t prev_alloc_bit = ALLOC_BIT(FOOTP(PREV_BLP(bp)));
    size_t next_alloc_bit = ALLOC_BIT(HEADP(NEXT_BLP(bp)));
    size_t size = SIZE(HEADP(bp));

    if (prev_alloc_bit && next_alloc_bit) { // prev and next are all allocated.
        return bp;
    }
    else if(!prev_alloc_bit && next_alloc_bit){ // prev is free
        size = size + SIZE(HEADP(PREV_BLP(bp)));
        PUT(FOOTP(bp), SUM(size,0));
        PUT(HEADP(PREV_BLP(bp)) , SUM(size,0));
        return PREV_BLP(bp);
    }
    else if(prev_alloc_bit && !next_alloc_bit){ // next is free
        size = size + SIZE(HEADP(NEXT_BLP(bp)));
        PUT(HEADP(bp) , SUM(size,0));
        PUT(FOOTP(bp) , SUM(size,0));
        return bp;
    }
    else{
        size = size + SIZE(HEADP(PREV_BLP(bp))) + SIZE(FOOTP(NEXT_BLP(bp))); // both are free
        PUT(HEADP(PREV_BLP(bp)) , SUM(size,0));
        PUT(FOOTP(NEXT_BLP(bp)) , SUM(size,0));
        return PREV_BLP(bp);
    }
    return bp;
}














