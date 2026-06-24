/***********************************************************************

util.h - memory allocation, error reporting, and other mundane stuff

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


/*
 * General utility functionss
 *
 * Mostly for dealing with mundane issues such as:
 *	Memory allocation
 *	Error handling
 */

/*
 * General allocation macro
 *
 * Example:
 *	struct list *s; s = allocate(4, struct list)
 * returns space for an array of 4 list structures.
 * Allocate will die if there is no more memory
 */

#define	allocate(n, type)	\
	((type *) myalloc(n, sizeof(type), "type"))

/*
 * Functions
 */

#define	STREQ(a,b)	( ! strcmp(a,b) )

char	*myalloc();	/* Do not call this function directly */
char	*scopy();	/* allocates memory for a string */
void	debug();	/* printf on stderr -
			   setting DebugFlag = 0 turns off debugging */
void	error();	/* printf on stderr, then dies */
int	ucstrcmp();	/* strcmp, upper case = lower case */
char	*tempstring();	/* returns a pointer to space that will reused soon */

/*
   this is the wrong place for all of this, but got chosen since
   every file includes this one
 */

#ifdef _IBMR2
/* IBM doesn't define unix? */
#define unix
#endif

#ifdef unix
#	define GRAPHICS		/* only GDEV on unix machines */
#endif

#ifndef unix

/* various BSD to lattice C name changes */

#define	bcopy	movmem
#define index	strchr
#define	rindex	strrchr

#endif
