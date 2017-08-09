/*
 * mm.c
 *
 *    In this version, free blocks are linked in a explicit link list.
 * There are three link lists, which means that free blocks are devided into
 * three groups according to their size.
 *    Blocks which was allocated will not have footer in my implement.
 * And a free block will consist of a header and a footer.What's more, it will 
 * have two pointer which point to the last free block and the next free block
 * in the link list.
 *    Adjacent free blocks will coalesce into bigger free blocks, However, 
 * the block created by extend heap function will not coalesce with others 
 * immediately.
 *    The smallest block size is still 16 becase the free block have two pointer
 * 
 * Struct of free blocks:
 * 	[header|left ptr|right ptr|......|footer]
 * 
 * Strunt of allocated blocks:
 * 	[header|......]
 * 
 * Struct of header:
 * 	[XXX....XXX(size)| 0 | 0/1(pre_clock allocate bit) | 0/1 (allocate bit)]
 * 
 * Struct of footer:
 * 	[XXX...XXX(size) | 00 | 0/1(allcocate bit) ]
*/
/** 
 * check part
 *
 * Check for three aspects as below:
 * 1. every free block shall only apperead on one list once
 * 2. all free blocks on lists are marked as free.
 * 3. the information in header and footer should be consistent
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "515030910083",
    /* First member's full name */
    "Wang Tao",
    /* First member's email address */
    "thor.wang@sjtu.edu.cn",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// basic data size
#define WSIZE 4 /* word size */
#define DSIZE 8 /* double word size */
#define MIN_BLK_SIZE 16 /* minimal block size */

// max
#define MAX(x, y) ((x) > (y) ? (x) : (y))

// pack and pack_header
#define PACK(size, alloc) ((size) | (alloc))
#define PACK_HDR(size, pre_alloc, alloc) ((size) | ((pre_alloc)<<1) | (alloc & 0x1)) 

// read and write a word at position p 
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_PRE_ALLOC(p) (GET(p) & 0x2)

//take the blank pointer as argument, return the position of header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE((HDRP(bp))) - DSIZE) /* should set header correctly first */

//tack bp and return the adjcent bp
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// move and modify on list
#define LEFT(bp) ((void *)GET(bp))
#define RIGHT(bp) ((void *)GET((char *)(bp) + WSIZE))
#define PUT_LEFT(bp, val) (*((unsigned int *)(bp)) = (unsigned int)(val))
#define PUT_RIGHT(bp, val) (*((unsigned int *)((char *)(bp) + WSIZE)) = (unsigned int)(val))

// max block for each list
#define G1_MAX 56
#define G2_MAX 424
#define CHUNKSIZE 512

// check part
#define VISITED(bp) ((GET(HDRP(bp)) & 0x2) >> 1)
#define UNMARK(bp) PUT(HDRP(bp), GET(HDRP(bp)) - 0x2)
#define MARK(bp) PUT(HDRP(bp), GET(HDRP(bp)) + 0x2)

static void * heap_listp = NULL; // point to the begining of heap
static void * g1 = NULL; // point to group 1's header 
static void * g2 = NULL; // pointer to group 2's header */
static void * g3 = NULL; // pointer to group 3's header */


// all the defination of static function used in this file

// extend the heap, return the old brk
static void * extend_heap(size_t size);

// insert new free block at the begining of list
static void insert_free(void *new_bp);

// find the appropriate block to alloc 
static void *find_fit(size_t asize);

// alloc, place the allocate into free block find by find_fit
// devide the rest part of free block
static void * alloc(void * bp, size_t asize);

// remove allocated block from the link list
static void remove_free(void *bp);

// combine adjacent free blocks
static void * coalesce(void *bp);

// for check
static int check_list(void *header);
int mm_check();

/*			below is the implement of functions     */


static void * extend_heap(size_t size)
{
    size_t asize = ALIGN(size);
    char * bp = mem_sbrk(asize);
    if (bp == (char *)-1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK_HDR(size, 1, 0)); 
    PUT(FTRP(bp), PACK(size, 0)); 
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 

    return bp;
}


static void insert_free(void * new_bp)
{
	size_t new_size = GET_SIZE(HDRP(new_bp));
	void * header = NULL;
	if(new_size <= G1_MAX)
		header = g1;
	else if(new_size <= G2_MAX)
		header = g2;
	else 
		header = g3;
	
	PUT_LEFT(new_bp, header);
	PUT_RIGHT(new_bp, RIGHT(header));
    
	if(RIGHT(header) != (unsigned int)0)
		PUT_LEFT(RIGHT(header), new_bp);
	PUT_RIGHT(header, new_bp);

}

static void * find_fit(size_t asize)
{
	void *header = NULL;
	size_t max_size;
    if (asize <= G1_MAX) {
		header = g1;
		max_size = G1_MAX;
    } else if (asize <= G2_MAX) {
		header = g2;
		max_size = G2_MAX;
    } else {
		header = g3;
		max_size = CHUNKSIZE;
    }

    void * bp = RIGHT(header);
    for (;bp != 0; bp = RIGHT(bp)) {
        if (asize <= GET_SIZE((HDRP(bp)))) {
            return bp;
        }
    }

    bp = extend_heap(MAX(asize, max_size));
    insert_free(bp);
    return bp;
}

static void* alloc(void * bp, size_t asize)
{
    size_t bsize = GET_SIZE((HDRP(bp)));
    size_t rest = bsize - asize;

    // whether to split
    if( rest < MIN_BLK_SIZE) {
        PUT(HDRP(bp), PACK_HDR(bsize, GET_PRE_ALLOC(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(bsize, 1));
    }
	else {
        PUT(HDRP(bp), PACK_HDR(asize, GET_PRE_ALLOC(HDRP(bp)) , 1));
        PUT(FTRP(bp), PACK(asize, 1));
        //split

		void *next_bp = NEXT_BLKP(bp);
        PUT(HDRP(next_bp), PACK_HDR(bsize - asize, 1 , 0));
        PUT(FTRP(next_bp), PACK(bsize - asize, 0));
        
		insert_free(next_bp);
	}
	
	return bp;
}



static void remove_free(void * bp)
{
	//check the boundary
    PUT_RIGHT(LEFT(bp), RIGHT(bp));

	if(RIGHT(bp) !=(unsigned int) 0)
		PUT_LEFT(RIGHT(bp), LEFT(bp));
}


static void * coalesce(void * bp)
{
    size_t prev_alloc = GET_ALLOC((FTRP(PREV_BLKP(bp))));
    size_t next_alloc = GET_ALLOC((HDRP(NEXT_BLKP(bp))));
	size_t fsize = GET_SIZE(HDRP(bp)); 
	size_t csize = fsize; //combine size

	void * next_bp = NULL;
	void * pre_bp = NULL;

//	if(prev_alloc && next_alloc)
//		return bp;

	 if(prev_alloc && !next_alloc){
		next_bp = NEXT_BLKP(bp);
		remove_free(next_bp);

		csize += GET_SIZE(HDRP(next_bp));
		PUT(FTRP(next_bp), PACK(csize, 0));
		PUT(HDRP(bp), PACK_HDR(csize, 1, 0));
	}

	else if(!prev_alloc && next_alloc){
		pre_bp = PREV_BLKP(bp);
		remove_free(pre_bp);

		csize += GET_SIZE(HDRP(pre_bp));
		PUT(HDRP(pre_bp), PACK_HDR(csize, 1, 0));
		PUT(FTRP(bp), PACK(csize, 0));

		bp =  pre_bp;
	}

    else if (!prev_alloc && !next_alloc) { 
		pre_bp = PREV_BLKP(bp);
		next_bp = NEXT_BLKP(bp);
		remove_free(pre_bp);
		remove_free(next_bp);

		csize += GET_SIZE(HDRP(pre_bp)) + GET_SIZE(HDRP(next_bp));
		PUT(HDRP(pre_bp), PACK_HDR(csize, 1, 0));
		PUT(FTRP(pre_bp), PACK(csize, 0));
  	
		bp= pre_bp;
	}

    // prevent to forget insert_free after combine
    insert_free(bp);
    return bp;



}



/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //creat heap
    heap_listp = mem_sbrk(10 * WSIZE);
    if (heap_listp == (void *)-1) {
        return -1;
    }
    
	// initialize the heap
    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(WSIZE * 8, 1)); /* Prologue header */
    heap_listp += DSIZE;
    
	// group header
    g1 = heap_listp;
	g2 = heap_listp + DSIZE;
	g3 = heap_listp + 2 * DSIZE;
    
	
	PUT_LEFT(g1,0);
    PUT_RIGHT(g1, 0);

    PUT_LEFT(g2, 0);
    PUT_RIGHT(g2, 0);

    PUT_LEFT(g3, 0);
    PUT_RIGHT(g3, 0);

    PUT(heap_listp + (6 * WSIZE), PACK(WSIZE * 8, 1)); 
    PUT(heap_listp + (7 * WSIZE), PACK(0, 1)); // Epilogue header
    
	// extend a little
    void *bp = extend_heap(48);  // don't know why , just try many numbers....
    insert_free(bp);
	//bp = extend_heap(G2_MAX);
	//insert_free(bp);
	//bp = extend_heap(CHUNKSIZE);
	//insert_free(bp);
    
    return 0;
}

/* 
 * mm_malloc - allocates a block
 */
void *mm_malloc(size_t size)
{

    if (size == 0) {
        return NULL;
    }

    char * bp = NULL;
    // size should allocate
    size_t asize = ALIGN(size + DSIZE);
    bp = find_fit(asize);
    
	if (bp != NULL) {
        alloc(bp, asize);
		remove_free(bp);
        return bp;
    }

    return NULL;
}

/*
 * mm_free - Freeing a block.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE((HDRP(bp)));
    PUT(HDRP(bp), PACK_HDR(size, GET_PRE_ALLOC(HDRP(bp)),  0));
    PUT(FTRP(bp), PACK(size, 0));
    
	coalesce(bp);
}

/*
 * mm_realloc - only moves block when there is no other way
 */
void *mm_realloc(void *bp, size_t size)
{
    size_t old_size;
    size_t new_size = ALIGN(size + DSIZE);
	void * next_bp;
    if (bp == NULL) {
        return mm_malloc(size);
    }

    old_size = GET_SIZE((HDRP(bp)));

    if (size == 0) {
        mm_free(bp);
        return NULL;
    }
	
	if(new_size > old_size) { // old size is not enough
		
		next_bp = NEXT_BLKP(bp); 
		
        int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
		
		if (GET_SIZE(HDRP(next_bp)) == 0) { // is last block 
            extend_heap(new_size - old_size);
            PUT(HDRP(bp), PACK_HDR(new_size, GET_PRE_ALLOC(HDRP(bp)), 1));
            PUT(FTRP(bp), PACK(new_size, 1));
        } 
	    	
 
		else if (!next_alloc && (old_size + GET_SIZE(HDRP(next_bp)) >= new_size)) { // merge the next block
            remove_free(NEXT_BLKP(bp));
            old_size += GET_SIZE((HDRP(NEXT_BLKP(bp))));

			// alloc the new            
			size_t rest = old_size - new_size;
			if (rest >= MIN_BLK_SIZE) {
                PUT(HDRP(bp), PACK_HDR(new_size, GET_PRE_ALLOC(HDRP(bp)), 1));
                PUT(FTRP(bp), PACK(new_size, 1));
                //split
				next_bp = NEXT_BLKP(bp);

                PUT(HDRP(next_bp), PACK_HDR(old_size - new_size, 1,  0));
                PUT(FTRP(next_bp), PACK(old_size - new_size, 0));
                coalesce(next_bp);
            } 
			
			else {
                PUT(HDRP(bp), PACK_HDR(old_size, GET_PRE_ALLOC(HDRP(bp)), 1));
                PUT(FTRP(bp), PACK(old_size, 1));
            }
        } 
		
		
		else { // must create a new block and move the old block into it
            void *new_bp = mm_malloc(size);
            if (new_bp == NULL)
              return NULL;
			size_t copy = (size > old_size) ? old_size : size;
            memcpy(new_bp, bp, copy);
            mm_free(bp);
            bp = new_bp;
        }
    }
	else if(new_size <= old_size) { //do not need a new free block, old is enough
         
		if ((old_size - new_size) >= MIN_BLK_SIZE) { // could maintain a free clock
            PUT(HDRP(bp), PACK_HDR(new_size, GET_PRE_ALLOC(HDRP(bp)), 1));
            PUT(FTRP(bp), PACK(new_size, 1));
            
			next_bp = NEXT_BLKP(bp);
            PUT(HDRP(next_bp), PACK_HDR(old_size - new_size, 1, 0));
            PUT(FTRP(next_bp), PACK(old_size - new_size, 0));
            
			coalesce(next_bp);
        }
    } 
	
    return bp;
}


// check each list
static int check_list(void * header)
{
    void * bp = RIGHT(header);
    for (;bp != header; bp = RIGHT(bp)) {
        if (VISITED(bp)) {
            printf("block %p has appeared in another list\n", bp);
            return 0; // failed in check 2
		}
		if (GET_ALLOC((HDRP(bp)))) {
            printf("block %p is in list but not free\n", bp);
            return 0; // failed in check 3
        } 
        
		MARK(bp); // visited mark 
        
    }
    return 1;
}



int mm_check()
{
    void *bp = NULL;
    /* check for each free list */
	if((check_list(g1) && check_list(g2) && check_list(g3)) == 0)
		return 0;

    // header and footer
    for (bp = heap_listp; GET_SIZE((HDRP(bp))) > 0; bp = NEXT_BLKP(bp)) {
        
		if ((GET(HDRP(bp)) & 0x2)!= (GET(FTRP(bp)) & 0x2)) {
            
			printf("information are different at %p \n   header:size:%d, alloc:%d \n   footer:size:%d, alloc:%d\n",
                    bp, GET_SIZE((HDRP(bp))), GET_ALLOC((HDRP(bp))), GET_SIZE((FTRP(bp))), GET_ALLOC((FTRP(bp))));
            return 0; // failed in check 3

        }
        if (!GET_ALLOC((HDRP(bp)))) {  //check all free block in heap
            if (VISITED(bp)) {
                UNMARK(bp); // remove flag
            } 
			
			else {
				size_t waste = GET_SIZE(HDRP(bp));
                printf("block %p is not in any free list\n it's size is %d\n", bp, waste);
                return 0; // failed in check 2
            }
        }
    }
	// pass all check 
    return 1;
}


