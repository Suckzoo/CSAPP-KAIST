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
#define getBlockHeader(bp) ((char *)(bp) - 3*WSIZE) 
#define getBlockFooter(bp) ((char *)(bp) + getSize(getBlockHeader(bp)) - 2*DSIZE)
//getNextBlock(), getPrevBlock()
#define getNextBlock(bp) ((char *)(bp) + getSize(((char *)(bp) - 3*WSIZE)))
#define getPrevBlock(bp) ((char *)(bp) - getSize(((char *)(bp) - 2*DSIZE)))
//getPrevNode(), getNextNode()
#define getPrevNode(bp) (get((char *)(bp) - DSIZE))
#define getNextNode(bp) (get((char *)(bp) - WSIZE))
#define setPrevNode(node, bp) (set((char *)(node) - DSIZE, bp))
#define setNextNode(node, bp) (set((char *)(node) - WSIZE, bp))
//isHead(), isTail()
#define isHead(bp) ((bp) == (getPrevNode(bp)))
#define isTail(bp) ((bp) == (getNextNode(bp)))
//#define __DEBUG__ // debug flab
typedef void* vp;
void *heap;
vp seg_8=NULL, seg_16=NULL, seg_24=NULL, seg_32=NULL, seg_40=NULL, seg_48=NULL, seg_56=NULL, seg_60=NULL, seg_64=NULL, seg_96=NULL, seg_128=NULL, seg_inf=NULL; //segregated lists
vp tail_8=NULL, tail_16=NULL, tail_24=NULL, tail_32=NULL, tail_40=NULL, tail_48=NULL, tail_56=NULL, tail_60=NULL, tail_64=NULL, tail_96=NULL, tail_128=NULL, tail_inf=NULL; //tails
/* 
 * mm_init - initialize the malloc package.
 */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
inline void *place(void *bp, size_t asize);

inline void attach_node(void** seg, void** tail, void *node) //attach node to linked list
{
#ifdef __DEBUG__
	printf("Attaching node...\n");
#endif
	if(*seg==NULL || *tail==NULL) //when list is empty
	{
		*seg = node;
		*tail = node;
		setNextNode(node, node);
		setPrevNode(node, node);
		return;
	}
	setNextNode(*tail, node); //link node
	setPrevNode(node, *tail);
	setNextNode(node, node); //tail invariant : next==tail
	*tail = node;
}

inline void delete_node(void** seg, void** tail, void *bp)
{
#ifdef __DEBUG__
	printf("Delete node / *seg : %u, tail : %u, bp : %u\n", *seg, *tail, bp);
#endif
	if(isHead(bp) && isTail(bp)) //make list empty
	{
		*seg = NULL;
		*tail = NULL;
		return;
	}
	else if(isHead(bp)) //move head
	{
		*seg = getNextNode(*seg);
		setPrevNode(*seg, *seg);
		return;
	}
	else if(isTail(bp)) //move tail
	{
		*tail = getPrevNode(bp);
		setNextNode(*tail, *tail);
		return;
	}

	void *prev_bp, *next_bp; //link change
	prev_bp = getPrevNode(bp);
	next_bp = getNextNode(bp);
	setNextNode(prev_bp, next_bp);
	setPrevNode(next_bp, prev_bp);
}
inline void push_back(size_t size, void* node) //attach_node wrapper : much list, wow
{
#ifdef __DEBUG__
	printf("Push back / size : %u, node : %u\n",size, node);
#endif
	if(size<=8) attach_node(&seg_8, &tail_8, node); //call with block size
	else if(size<=16) attach_node(&seg_16, &tail_16, node);
	else if(size<=24) attach_node(&seg_24, &tail_24, node);
	else if(size<=32) attach_node(&seg_32, &tail_32, node);
	else if(size<=40) attach_node(&seg_40, &tail_40, node);
	else if(size<=48) attach_node(&seg_48, &tail_48, node);
	else if(size<=56) attach_node(&seg_56, &tail_56, node);
	else if(size<=60) attach_node(&seg_60, &tail_60, node);
	else if(size<=64) attach_node(&seg_64, &tail_64, node);
	else if(size<=96) attach_node(&seg_96, &tail_96, node);
	else if(size<=128) attach_node(&seg_128, &tail_128, node);
	else attach_node(&seg_inf, &tail_inf, node);
}
inline void erase(size_t size, void* node) //delete_node wrapper : much list, wow
{
#ifdef __DEBUG__
	printf("Erase / size : %u, node : %u\n",size, node);
#endif
	if(size<=8) delete_node(&seg_8, &tail_8, node); //call with block size
	else if(size<=16) delete_node(&seg_16, &tail_16, node);
	else if(size<=24) delete_node(&seg_24, &tail_24, node);
	else if(size<=32) delete_node(&seg_32, &tail_32, node);
	else if(size<=40) delete_node(&seg_40, &tail_40, node);
	else if(size<=48) delete_node(&seg_48, &tail_48, node);
	else if(size<=56) delete_node(&seg_56, &tail_56, node);
	else if(size<=60) delete_node(&seg_60, &tail_60, node);
	else if(size<=64) delete_node(&seg_64, &tail_64, node);
	else if(size<=96) delete_node(&seg_96, &tail_96, node);
	else if(size<=128) delete_node(&seg_128, &tail_128, node);
	else delete_node(&seg_inf, &tail_inf, node);
}
inline void* getSegBySize(size_t size) //seg_list wrapper, returns seg_list by size
{
	if(size<=8) return seg_8;
	else if(size<=16) return seg_16;
	else if(size<=24) return seg_24;
	else if(size<=32) return seg_32;
	else if(size<=40) return seg_40;
	else if(size<=48) return seg_48;
	else if(size<=56) return seg_56;
	else if(size<=60) return seg_60;
	else if(size<=64) return seg_64;
	else if(size<=96) return seg_96;
	else if(size<=128) return seg_128;
	return seg_inf;
}
int mm_init(void) //initialize
{
	if((heap = mem_sbrk(8*WSIZE)) == (void*)-1) return -1;
	set(heap, 0);
	set(heap + WSIZE, setMask(2*DSIZE, 1)); //size, header
	set(heap + WSIZE*2, heap + WSIZE*4); //prev
	set(heap + WSIZE*3, heap + WSIZE*4); //next
	set(heap + WSIZE*4, setMask(2*DSIZE, 1));//footer
	set(heap + WSIZE*5, setMask(0, 1)); //next block
	set(heap + WSIZE*6, heap + WSIZE*8); //prev
	set(heap + WSIZE*7, heap + WSIZE*8); //next
	heap += 4*WSIZE;
	if(extend_heap((1<<6)/WSIZE) == NULL) return -1; //initialize heap
    return 0;
}

static void *extend_heap(size_t words) //extend heap
{
	char *bp;
	size_t size;
	size = (words % 2) ? (words + 1)*WSIZE : words * WSIZE; 
#ifdef __DEBUG__
	fprintf(stderr, "Extending heap by %u...\n",size);
	printf("Heap size now : %u\n",mem_heapsize());
#endif
	if((long)(bp = mem_sbrk(size)) == -1) return NULL; //sbrk : gathering moar spaces
	set(getBlockHeader(bp), setMask(size, 0));
	set(getBlockFooter(bp), setMask(size, 0));
	set(getBlockHeader(getNextBlock(bp)), setMask(0, 1));
	push_back(size, bp); //empty block to the list
	void* result = coalesce(bp); //coalesces!
//	push_back(getSize(getBlockHeader(result)), result);
	return result;
}
static void *coalesce(void *bp)
{
#ifdef __DEBUG__
	fprintf(stderr, "Coalescing...\n");
#endif
	size_t p = isAllocated(getBlockHeader(getPrevBlock(bp))); //allocated "physically" previous block?
	size_t n = isAllocated(getBlockHeader(getNextBlock(bp))); //allocated "physicall" next block?
	size_t size = getSize(getBlockHeader(bp)); //get block size
	if(p && n) { //no coalescing
		return bp;
	}
	else if(!p && n) //previous block is empty!
	{
#ifdef __DEBUG__
		fprintf(stderr, "Merging %u(size : %u) and %u(size : %u)...\n", getPrevBlock(bp), getSize(getBlockHeader(getPrevBlock(bp))), bp, getSize(getBlockHeader(bp)));
#endif
		erase(size, bp); //delete current block
		erase(getSize(getBlockHeader(getPrevBlock(bp))), getPrevBlock(bp)); //delete previous block
		size += getSize(getBlockHeader(getPrevBlock(bp)));
		set(getBlockFooter(bp), setMask(size, 0));
		set(getBlockHeader(getPrevBlock(bp)), setMask(size, 0));
		push_back(size, getPrevBlock(bp)); //add summed block
		return getPrevBlock(bp);
	}
	else if(p && !n) //next block is empty!
	{
#ifdef __DEBUG__
		fprintf(stderr, "Merging %u(size : %u) and %u(size : %u)...\n", bp, getSize(getBlockHeader(bp)), getNextBlock(bp), getSize(getBlockHeader(getNextBlock(bp))));
#endif
		erase(size, bp); //delete current
		erase(getSize(getBlockHeader(getNextBlock(bp))), getNextBlock(bp)); //delete next
		size += getSize(getBlockHeader(getNextBlock(bp)));
		set(getBlockHeader(bp), setMask(size, 0));
		set(getBlockFooter(bp), setMask(size, 0));
		push_back(size, bp); //add summed block
		return bp;
	}
	else //both block are empty!
	{
#ifdef __DEBUG__
		fprintf(stderr, "Merging %u(size : %u), %u(size : %u) and %u(size : %u)...\n", getPrevBlock(bp), getSize(getBlockHeader(getPrevBlock(bp))), bp, getSize(getBlockHeader(bp)), getNextBlock(bp), getSize(getBlockHeader(getNextBlock(bp))));
#endif
		erase(size, bp); //delete every adjacent block
		erase(getSize(getBlockHeader(getPrevBlock(bp))), getPrevBlock(bp));
		erase(getSize(getBlockHeader(getNextBlock(bp))), getNextBlock(bp));
		size += getSize(getBlockHeader(getNextBlock(bp))) + getSize(getBlockHeader(getPrevBlock(bp)));
		set(getBlockHeader(getPrevBlock(bp)), setMask(size, 0));
		set(getBlockFooter(getNextBlock(bp)), setMask(size, 0));
		push_back(size, getPrevBlock(bp)); //sum up
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
	if(size<=DSIZE) adjSize = 3*DSIZE;
	else adjSize = DSIZE * ((size + 2*(DSIZE) + (DSIZE)-1) / DSIZE); //adjust size to align
	if((bp = find_fit(adjSize)) != NULL) { //is there any good place?
#ifdef __DEBUG__
		fprintf(stderr, "alloc ty 1 : %u\n",getBlockHeader(bp));
#endif
		return place(bp, adjSize);
	}
	extSize = MAX(adjSize, CHUNKSIZE); //fucked, extend heap and gather moar spaces
	if((bp = extend_heap(extSize/WSIZE)) == NULL) return NULL;
#ifdef __DEBUG__
	fprintf(stderr, "alloc ty 2 : %u\n",getBlockHeader(bp));
#endif
	return place(bp, adjSize);
}

size_t getNextSize(size_t size) //next seg_list size?
{
	if(size<=8) return 16;
	else if(size<=16) return 24;
	else if(size<=24) return 32;
	else if(size<=32) return 40;
	else if(size<=40) return 48;
	else if(size<=48) return 56;
	else if(size<=56) return 60;
	else if(size<=60) return 64;
	else if(size<=64) return 96;
	else if(size<=96) return 128;
	return 999;
}
static void* find_fit(size_t size) //find best place
{
#ifdef __DEBUG__
	fprintf(stderr, "find fitting place - size : %d\n", size);
#endif
	void* bp;
	void* s;
	size_t sizeIndex = size;
	for(s = getSegBySize(size); ; s = getSegBySize(sizeIndex))  //increase seg size
	{
		sizeIndex = getNextSize(sizeIndex);
		for(bp = s; ; bp = getNextNode(bp)) //iterate list
		{
			if(bp==NULL) break;
#ifdef __DEBUG__
			fprintf(stderr, "searching : %u / allocated? : %u / size? : %u\n", getBlockHeader(bp), isAllocated(getBlockHeader(bp)), getSize(getBlockHeader(bp)));
#endif
			if(!isAllocated(getBlockHeader(bp)) && size <= getSize(getBlockHeader(bp)))
			{
				return bp;
			}
			if(isTail(bp)) break;
		}
		if(s==seg_inf) break;
	}
	return NULL;
}
inline void *place(void *bp, size_t aSize) //alloc!
{
#ifdef __DEBUG__
	printf("placing on %u, size %u\n",getBlockHeader(bp), aSize);
#endif
	erase(getSize(getBlockHeader(bp)), bp); //no empty
	size_t cSize = getSize(getBlockHeader(bp));
	if((cSize - aSize) >= 2*DSIZE)
	{
		if(aSize>=100) { //huge block to right
			set(getBlockHeader(bp), setMask(cSize - aSize, 0));
			set(getBlockFooter(bp), setMask(cSize - aSize, 0));
			bp = getNextBlock(bp);
			set(getBlockHeader(bp), setMask(aSize, 1));
			set(getBlockFooter(bp), setMask(aSize, 1));
			push_back(cSize-aSize, getPrevBlock(bp));
			return bp;
		} else { //tiny block to left
			set(getBlockHeader(bp), setMask(aSize, 1));
			set(getBlockFooter(bp), setMask(aSize, 1));
			bp = getNextBlock(bp);
			set(getBlockHeader(bp), setMask(cSize - aSize, 0));
			set(getBlockFooter(bp), setMask(cSize - aSize, 0));
			push_back(cSize-aSize, bp);
			return getPrevBlock(bp);
		}
	}
	else
	{
		set(getBlockHeader(bp), setMask(cSize, 1));
		set(getBlockFooter(bp), setMask(cSize, 1));
	}
	return bp;
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
#ifdef __DEBUG__
	printf("Trying to free...\n");
#endif
	if(!isAllocated(getBlockHeader(ptr))) return; 
#ifdef __DEBUG__
	fprintf(stderr, "freeing : %u / allocated? : %u / size? : %u\n",getBlockHeader(ptr), isAllocated(getBlockHeader(ptr)), getSize(getBlockHeader(ptr)));
#endif
	size_t size = getSize(getBlockHeader(ptr));
	set(getBlockHeader(ptr), setMask(size, 0));
	set(getBlockFooter(ptr), setMask(size, 0));
	push_back(size, ptr); //new empty block,
	coalesce(ptr); //and coalesce!
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
#ifdef __DEBUG__
	printf("Trying to realloc...\n");
#endif
    void *oldptr = ptr;
	void *newptr;
	size_t copySize;
	newptr = mm_malloc(size); //alloc new mem
	if(size==0) {
		mm_free(oldptr);
		return 0;
	}
	if(ptr == NULL) {
		return mm_malloc(size);
	}
	copySize = getSize(getBlockHeader(ptr));
	if(size<copySize) copySize = size;
	memcpy(newptr, oldptr, copySize); //copy
	mm_free(oldptr); //free old mem
	return newptr;
}
