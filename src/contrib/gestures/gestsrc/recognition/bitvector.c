
/***********************************************************************

bitvector.c - some routines for dealing with bitvectors

Copyright (C) 1991 Dean Rubine

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License. See ../COPYING for
the full agreement.

 **********************************************************************/

/*
	$Disclaimer: 
 * Permission to use, copy, modify, and distribute this software and its 
 * documentation for any purpose is hereby granted without fee, 
 * provided that the above copyright notice appear in all copies and that 
 * both that copyright notice, this permission notice, and the following 
 * disclaimer appear in supporting documentation, and that the names of 
 * IBM, Carnegie Mellon University, and other copyright holders, not be 
 * used in advertising or publicity pertaining to distribution of the software 
 * without specific, written prior permission.
 * 
 * IBM, CARNEGIE MELLON UNIVERSITY, AND THE OTHER COPYRIGHT HOLDERS 
 * DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING 
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT 
 * SHALL IBM, CARNEGIE MELLON UNIVERSITY, OR ANY OTHER COPYRIGHT HOLDER 
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY 
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS 
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 *  $
*/

#ifndef NORCSID
#define NORCSID
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/contrib/gestures/gestsrc/recognition/RCS/bitvector.c,v 1.3 1992/12/15 21:49:28 rr2b R6tape $";
#endif


/*LINTLIBRARY*/

#include "util.h"
#undef	BITS_PER_VECTOR
#define BITS_PER_VECTOR	128
#include "bitvector.h"

int
bitcount(max, bv)
int max;
BitVector bv;
{
	register i, count;

	for(count = i = 0; i < max; i++)
		if(IS_SET(i, bv))
			count++;
	return count;
}

char *
BitVectorToString(max, bv)
BitVector bv;
{
	char *string = tempstring();
	register i;

	for(i = 0; i < max; i++)
		string[i] = IS_SET(i, bv) ? (i % 10) + '0' : '-' ;
	string[i] = '\0';
	return string;
}


void
StringToBitVector(string, max, bv)
char *string;
int max;
BitVector bv;
{
	register i;

	if(strlen(string) != max)
		error("StringToBitVector: strlen(%s)=%d != %d",
			string, strlen(string), max);

	for(i = 0; i < max; i++)
		if(string[i] != '-')
			BIT_SET(i, bv);
		else
			BIT_CLEAR(i, bv);		
}


SetBitVector(v)
register BitVector v;
{
	register int nints = INTS_PER_VECTOR;

	while(--nints >= 0)
		*v++ = -1;
}


ClearBitVector(nints, v)
register int nints;
register BitVector v;
{

	while(--nints >= 0)
		*v++ = 0;
}


AssignBitVector(nints, v1, v2)
register int nints;
register BitVector v1, v2;
{

	while(--nints >= 0)
		*v1++ = *v2++;
}

int
BitVectorDeQ(max, v)
register int max;
register BitVector v;
{
	register int i;
	for(i = 0; i < max; i++)
		if(IS_SET(i, v)) {
			BIT_CLEAR(i, v);
			return i;
		}
	return -1;

}

int *
BitVectorOr(v, v1, v2, ipv)
int *v;
register int *v1, *v2;
register int ipv;
{
	int *vv = v;
	do
		*vv++ = *v1++ | *v2++;
	while(--ipv > 0);
	return v;
}

int *
BitVectorAnd(v, v1, v2, ipv)
int *v;
register int *v1, *v2;
register int ipv;
{
	int *vv = v;
	do
		*vv++ = *v1++ & *v2++;
	while(--ipv > 0);
	return v;
}

int
BitVectorNoBitsSet(v, ipv)
register int *v;
register int ipv;
{
	do
		if(*v++) return 0;
	while(--ipv > 0);
	return 1;
}
