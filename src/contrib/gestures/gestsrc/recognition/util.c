/***********************************************************************

util.c - memory allocation, error reporting, and other mundane stuff

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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/contrib/gestures/gestsrc/recognition/RCS/util.c,v 1.3 1992/12/15 21:49:55 rr2b R6tape $";
#endif

/*
 * Mundane utility routines
 *	see util.h
 */

/*LINTLIBRARY*/

#include "util.h"
#include <stdio.h>
#include <ctype.h>

/*
 * Function used by allocation macro
 */

char *
myalloc(nitems, itemsize, typename)
char *typename;
{
	char	*malloc();
	register unsigned int bytes = nitems * itemsize;
	register char *p = malloc(bytes);
	if(p == NULL)
	     error("Can't get mem for %d %s's (each %d bytes, %d total bytes)",
		nitems, typename, itemsize, bytes);
	return p;
}

/*
 * Return a copy of a string
 */

char *
scopy(s)
char *s;
{
	register char *p = allocate(strlen(s) + 1, char);
	char *strcpy();
	(void) strcpy(p, s);
	return p;
}

/*
 * Print an error message, a newline, and then exit
 */

/*VARARGS1*/
void
error(a, b, c, d, e, f, g)
char *a;
{
	fprintf(stderr, a, b, c, d, e, f, g);
	fprintf(stderr, "\n");
	exit(1);
}

/*
 * print a message if DebugFlag is non-zero
 */

int	DebugFlag = 1;

void
debug(a, b, c, d, e, f, g)
char *a;
{
	if(DebugFlag)
		fprintf(stderr, a, b, c, d, e, f, g);
}

#define	upper(c)	(islower(c) ? toupper(c) : (c))

int
ucstrcmp(s1, s2)
register char *s1, *s2;
{
	register i;

	for(; *s1 && *s2; s1++, s2++)
		if( (i = (upper(*s1) - upper(*s2))) != 0)
			return i;
	return (upper(*s1) - upper(*s2));
}

#define NSTRINGS 3

char *
tempstring()
{
	static char strings[NSTRINGS][100];
	static int index;
	if(index >= NSTRINGS) index = 0;
	return strings[index++];
}
