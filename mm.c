/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define ALIGNMENT 8
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define setMask(size, alloc) ((size) | (alloc))
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//macros from book
//access pointer p
#define get(p) (*(unsigned int *)(p))
#define set(p, val) (*(unsigned int*)(p) = (val))
//get size of the block
#define getSize(p) (get(p) & ~0x7)
//isAllocated?
#define isAllocated(p) (get(p) & 0x1)
//getBlockHeader, getBlockFooter
#define getBlockHeader(bp) ((char *)(bp) - WSIZE)
#define getBlockFooter(bp) ((char *)(bp) + getSize(getBlockHeader(bp)) - DSIZE)
//getNextBlock(), getPrevBlock()
#define getNextBlock(bp) ((char *)(bp) + getSize(((char *)(bp) - WSIZE)))
#define getPrevBlock(bp) ((char *)(bp) - getSize(((char *)(bp) - DSIZE)))

void *heap;

/* 
 * mm_init - initialize the malloc package.
 */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
inline void place(void *bp, size_t asize);

int mm_init(void)
{
	if((heap = mem_sbrk(4*WSIZE)) == (void*)-1) return -1;
	set(heap, 0);
	set(heap + WSIZE, setMask(DSIZE, 1));
	set(heap + WSIZE*2, setMask(DSIZE, 1));
	set(heap + WSIZE*3, setMask(0, 1));
	heap += 2*WSIZE;
	if(extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;
	size = (words % 2) ? (words + 1)*WSIZE : words * WSIZE;
	if((long)(bp = mem_sbrk(size)) == -1) return NULL;
	set(getBlockHeader(bp), setMask(size, 0));
	set(getBlockFooter(bp), setMask(size, 0));
	set(getBlockHeader(getNextBlock(bp)), setMask(0, 1));
	return coalesce(bp);
}
static void *coalesce(void *bp)
{
#ifdef __DEBUG__
	printf("Coalescing...\n");
#endif
	size_t p = isAllocated(getBlockHeader(getPrevBlock(bp)));
	size_t n = isAllocated(getBlockHeader(getNextBlock(bp)));
	size_t size = getSize(getBlockHeader(bp));
	if(p && n) {
		return bp;
	}
	else if(!p && n)
	{
		size += getSize(getBlockHeader(getPrevBlock(bp)));
		set(getBlockFooter(bp), setMask(size, 0));
		set(getBlockHeader(getPrevBlock(bp)), setMask(size, 0));
		return getPrevBlock(bp);
	}
	else if(p && !n)
	{
		size += getSize(getBlockHeader(getNextBlock(bp)));
		set(getBlockHeader(bp), setMask(size, 0));
		set(getBlockFooter(bp), setMask(size, 0));
		return bp;
	}
	else
	{
		size += getSize(getBlockHeader(getNextBlock(bp))) + getSize(getBlockHeader(getPrevBlock(bp)));
		set(getBlockHeader(getPrevBlock(bp)), setMask(size, 0));
		set(getBlockFooter(getNextBlock(bp)), setMask(size, 0));
		return getPrevBlock(bp);
	}
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
#ifdef __DEBUG__
	printf("Trying to allocate...\n");
#endif
	size_t adjSize;
	size_t extSize;
	void* bp;
	if(size == 0) return NULL;
	if(size<=DSIZE) adjSize = 2*DSIZE;
	else adjSize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
	if((bp = find_fit(adjSize)) != NULL) {
#ifdef __DEBUG__
		fprintf(stderr, "alloc ty 1 : %u\n",getBlockHeader(bp));
#endif
		place(bp, adjSize);
		return bp;
	}
	extSize = MAX(adjSize, CHUNKSIZE);
	if((bp = extend_heap(extSize/WSIZE)) == NULL) return NULL;
#ifdef __DEBUG__
	fprintf(stderr, "alloc ty 2 : %u\n",getBlockHeader(bp));
#endif
	place(bp, adjSize);
	return bp;
}

static void* find_fit(size_t size)
{
#ifdef __DEBUG__
	fprintf(stderr, "find fitting place - size : %d\n", size);
#endif
	void* bp;
	for(bp = heap; getSize(getBlockHeader(bp)) > 0; bp = getNextBlock(bp))
	{
#ifdef __DEBUG__
		fprintf(stderr, "searching : %u / allocated? : %u / size? : %u\n", getBlockHeader(bp), isAllocated(getBlockHeader(bp)), getSize(getBlockHeader(bp)));
#endif
		if(!isAllocated(getBlockHeader(bp)) && size <= getSize(getBlockHeader(bp)))
		{
			return bp;
		}
	}
	return NULL;
}
inline void place(void *bp, size_t aSize)
{
#ifdef __DEBUG__
	printf("placing on %u\n",getBlockHeader(bp));
#endif
	size_t cSize = getSize(getBlockHeader(bp));
	if((cSize - aSize) >= 2*DSIZE)
	{
		set(getBlockHeader(bp), setMask(aSize, 1));
		set(getBlockFooter(bp), setMask(aSize, 1));
		bp = getNextBlock(bp);
		set(getBlockHeader(bp), setMask(cSize - aSize, 0));
		set(getBlockFooter(bp), setMask(cSize - aSize, 0));
	}
	else
	{
		set(getBlockHeader(bp), setMask(cSize, 1));
		set(getBlockFooter(bp), setMask(cSize, 1));
	}
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
#ifdef __DEBUG__
	printf("Trying to free...\n");
#endif
	if(ptr==NULL) return; 
#ifdef __DEBUG__
	fprintf(stderr, "freeing : %u / allocated? : %u / size? : %u\n",getBlockHeader(ptr), isAllocated(getBlockHeader(ptr)), getSize(getBlockHeader(ptr)));
#endif
	size_t size = getSize(getBlockHeader(ptr));
	set(getBlockHeader(ptr), setMask(size, 0));
	set(getBlockFooter(ptr), setMask(size, 0));
	coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
#ifdef __DEBUG_REALLOC__
	printf("Trying to realloc...\n");
#endif
    void *oldptr = ptr;
	void *newptr;
	size_t copySize;
	newptr = mm_malloc(size);
	if(size==0) {
		mm_free(oldptr);
		return 0;
	}
	if(ptr == NULL) {
		return mm_malloc(size);
	}
	copySize = getSize(getBlockHeader(ptr));
	if(size<copySize) copySize = size;
	memcpy(newptr, oldptr, copySize);
	mm_free(oldptr);
	return newptr;
}
