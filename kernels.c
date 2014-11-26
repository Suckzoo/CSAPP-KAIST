/********************************************************
 * Kernels to be optimized for the CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/***************
 * ROTATE KERNEL
 ***************/

/******************************************************
 * Your different versions of the rotate kernel go here
 ******************************************************/

/* 
 * naive_rotate - The naive baseline version of rotate 
 */
char naive_rotate_descr[] = "naive_rotate: Naive baseline implementation";
void naive_rotate(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
}

/* 
 * rotate - Your current working version of rotate
 * IMPORTANT: This is the version you will be graded on
 */
char rotate_descr[] = "rotate: Current working version";
inline void swap_pixel(pixel *p, pixel *q)
{
	p->red^=q->red^=p->red^=q->red;
	p->green^=q->green^=p->green^=q->green;
	p->blue^=q->blue^=p->blue^=q->blue;
}
void rotate(int dim, pixel *src, pixel *dst) 
{
	int i, j, i1, j1, im, jm;
	if(dim==1024)
	{
		for(i=0;i<dim;i+=64)
			for(j=0;j<dim;j+=64) {
				im = i+64;
				for(i1=i;i1<im;i1+=4) {
					jm = j+64;
					for(j1=j;j1<jm;j1+=4) {
						dst[RIDX(i1,j1,dim)]= src[RIDX(j1,dim-i1-1,dim)];
						dst[RIDX(i1,j1+1,dim)]= src[RIDX(j1+1,dim-i1-1,dim)];
						dst[RIDX(i1,j1+2,dim)]= src[RIDX(j1+2,dim-i1-1,dim)];
						dst[RIDX(i1,j1+3,dim)]= src[RIDX(j1+3,dim-i1-1,dim)];
						dst[RIDX(i1+1,j1,dim)]= src[RIDX(j1,dim-i1-2,dim)];
						dst[RIDX(i1+1,j1+1,dim)]= src[RIDX(j1+1,dim-i1-2,dim)];
						dst[RIDX(i1+1,j1+2,dim)]= src[RIDX(j1+2,dim-i1-2,dim)];
						dst[RIDX(i1+1,j1+3,dim)]= src[RIDX(j1+3,dim-i1-2,dim)];
						dst[RIDX(i1+2,j1,dim)]= src[RIDX(j1,dim-i1-3,dim)];
						dst[RIDX(i1+2,j1+1,dim)]= src[RIDX(j1+1,dim-i1-3,dim)];
						dst[RIDX(i1+2,j1+2,dim)]= src[RIDX(j1+2,dim-i1-3,dim)];
						dst[RIDX(i1+2,j1+3,dim)]= src[RIDX(j1+3,dim-i1-3,dim)];
						dst[RIDX(i1+3,j1,dim)]= src[RIDX(j1,dim-i1-4,dim)];
						dst[RIDX(i1+3,j1+1,dim)]= src[RIDX(j1+1,dim-i1-4,dim)];
						dst[RIDX(i1+3,j1+2,dim)]= src[RIDX(j1+2,dim-i1-4,dim)];
						dst[RIDX(i1+3,j1+3,dim)]= src[RIDX(j1+3,dim-i1-4,dim)];
					}
				}
			}
	}
	else {
		for(i=0;i<dim;i+=32)
			for(j=0;j<dim;j+=32) {
				im = i+32;
				for(i1=i;i1<im;i1++) {
					jm = j+32;
					for(j1=j;j1<jm;j1++) {
						dst[RIDX(i1,j1,dim)]= src[RIDX(j1,dim-i1-1,dim)];
					}
				}
			}	
	}
}

/*********************************************************************
 * register_rotate_functions - Register all of your different versions
 *     of the rotate kernel with the driver by calling the
 *     add_rotate_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_rotate_functions() 
{
    add_rotate_function(&naive_rotate, naive_rotate_descr);   
    add_rotate_function(&rotate, rotate_descr);   
    /* ... Register additional test functions here */
}


/***************
 * SMOOTH KERNEL
 **************/

/***************************************************************
 * Various typedefs and helper functions for the smooth function
 * You may modify these any way you like.
 **************************************************************/

/* A struct used to compute averaged pixel value */
typedef struct {
    int red;
    int green;
    int blue;
    int num;
} pixel_sum;

/* Compute min and max of two integers, respectively */
static int min(int a, int b) { return (a < b ? a : b); }
static int max(int a, int b) { return (a > b ? a : b); }

/* 
 * initialize_pixel_sum - Initializes all fields of sum to 0 
 */
static void initialize_pixel_sum(pixel_sum *sum) 
{
    sum->red = sum->green = sum->blue = 0;
    sum->num = 0;
    return;
}

/* 
 * accumulate_sum - Accumulates field values of p in corresponding 
 * fields of sum 
 */
static void accumulate_sum(pixel_sum *sum, pixel p) 
{
    sum->red += (int) p.red;
    sum->green += (int) p.green;
    sum->blue += (int) p.blue;
    sum->num++;
    return;
}

/* 
 * assign_sum_to_pixel - Computes averaged pixel value in current_pixel 
 */
static void assign_sum_to_pixel(pixel *current_pixel, pixel_sum sum) 
{
    current_pixel->red = (unsigned short) (sum.red/sum.num);
    current_pixel->green = (unsigned short) (sum.green/sum.num);
    current_pixel->blue = (unsigned short) (sum.blue/sum.num);
    return;
}

/* 
 * avg - Returns averaged pixel value at (i,j) 
 */
static pixel avg(int dim, int i, int j, pixel *src) 
{
    int ii, jj;
    pixel_sum sum;
    pixel current_pixel;

    initialize_pixel_sum(&sum);
    for(ii = max(i-1, 0); ii <= min(i+1, dim-1); ii++) 
	for(jj = max(j-1, 0); jj <= min(j+1, dim-1); jj++) 
	    accumulate_sum(&sum, src[RIDX(ii, jj, dim)]);

    assign_sum_to_pixel(&current_pixel, sum);
    return current_pixel;
}

/******************************************************
 * Your different versions of the smooth kernel go here
 ******************************************************/

/*
 * naive_smooth - The naive baseline version of smooth 
 */
char naive_smooth_descr[] = "naive_smooth: Naive baseline implementation";
void naive_smooth(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(i, j, dim)] = avg(dim, i, j, src);
}

/*
 * smooth - Your current working version of smooth. 
 * IMPORTANT: This is the version you will be graded on
 */
char smooth_descr[] = "smooth: Current working version";
void twosum(pixel_sum *d, pixel *a1, pixel *a2)
{
	d->red = (int)a1->red + (int)a2->red;
	d->green = (int)a1->green + (int)a2->green;
	d->blue = (int)a1->blue + (int)a2->blue;
	d->num = 2;
}
void threesum(pixel_sum* d, pixel *a1, pixel *a2, pixel *a3)
{
	d->red = (int)a1->red + (int)a2->red + (int)a3->red;
	d->green = (int)a1->green + (int)a2->green + (int)a3->green;
	d->blue = (int)a1->blue + (int)a2->blue + (int)a3->blue;
	d->num = 3;
}
void rowcache(int dim, pixel_sum *dst, pixel *src)
{
	pixel* p;
	pixel_sum* cache;
	p=src;
	cache=dst;
	twosum(cache++,p,p+1);
	p++;
	int i;
	int dm = dim-1;
	for(i=1;i<dm;i++,p++)
	{
		threesum(cache++, p-1, p, p+1);
	}
	twosum(cache,p-1,p);
}
void twoline(int dim, pixel_sum *a1, pixel_sum *a2, pixel* dst)
{
	int i;
	pixel* d;
	pixel_sum* s1;
	pixel_sum* s2;
	s1 = a1;
	s2 = a2;
	d = dst;
	int sum;
	for(i=0;i<dim;i++)
	{
		sum = s1->num + s2->num;
		d->red = (unsigned short)((s1->red + s2->red) / sum);
		d->green = (unsigned short) ((s1->green + s2->green) / sum);
		d->blue = (unsigned short) ((s1->blue + s2->blue) / sum);
		s1++;
		s2++;
		d++;
	}
}
void threeline(int dim, pixel_sum *a1, pixel_sum *a2, pixel_sum *a3, pixel *dst)
{
	int i;
	pixel* d;
	pixel_sum *s1, *s2, *s3;
	s1 = a1;
	s2 = a2;
	s3 = a3;
	d = dst;
	int sum;
	for(i=0;i<dim;i++)
	{
		sum = s1->num + s2->num + s3->num;
		d->red = (unsigned short)((s1->red + s2->red + s3->red) / sum);
		d->green = (unsigned short) ((s1->green + s2->green + s3->green) / sum);
		d->blue = (unsigned short) ((s1->blue + s2->blue + s3->blue) / sum);
		s1++;
		s2++;
		s3++;
		d++;
	}
}
void smooth(int dim, pixel *src, pixel *dst) 
{
    //naive_smooth(dim, src, dst);
	pixel_sum tmp[3*dim];
	pixel_sum *s1, *s2, *s3;
	s1=tmp;
	s2=tmp+dim;
	s3=tmp+(dim<<1);
	pixel *row = src;
	pixel *d = dst;
	pixel *s = src;
	rowcache(dim, s1, s);
	s+=dim;
	rowcache(dim, s2, s);
	s+=dim;
	twoline(dim, s1, s2, d);
	d+=dim;
	int counter=3;
	int i;
	int dm = dim-1;
	for(i=1;i<dm;i++)
	{
		if(counter%3==0)
		{
			rowcache(dim, s3, s);
		}
		else if(counter%3==1)
		{
			rowcache(dim, s1, s);
		}
		else
		{
			rowcache(dim, s2, s);
		}
		threeline(dim, s1, s2, s3, d);
		s+=dim;
		d+=dim;
		counter++;
	}
	if(counter%3==0)
	{
		twoline(dim, s1, s2, d);
	}
	else if(counter%3==1)
	{
		twoline(dim, s2, s3, d);
	}
	else
	{
		twoline(dim, s3, s1, d);
	}
}	


/********************************************************************* 
 * register_smooth_functions - Register all of your different versions
 *     of the smooth kernel with the driver by calling the
 *     add_smooth_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_smooth_functions() {
    add_smooth_function(&smooth, smooth_descr);
    add_smooth_function(&naive_smooth, naive_smooth_descr);
    /* ... Register additional test functions here */
}

