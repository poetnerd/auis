/***********************************************************************

region.c - kaka for mrecord to work

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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/contrib/gestures/gestsrc/record/RCS/region.c,v 1.7 1993/12/07 02:35:19 rr2b Exp $";
#endif

/*
 region.c - manipulates a GDEV window
 */
#include <andrewos.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <gestures/gdev.h>
#include "region.h"

#ifdef SOLARIS
#define index strchr
#endif

void
REGIONinit()
{

	GDEVinit(NULL);
	GDEVsets("program", "gsink");
}

/* char	*template = "out.%d.rec"; */
char	*template = NULL;
char	name[100];
int	current = 1;

void
setrecordfile(filename)
char *filename;
{
	register char *p, *endp;
	register int i;
	static char fname[100];

	strcpy(fname, filename);

	current = 1;
	for(p = &filename[strlen(filename) - 1]; p >= filename; p--)
		if(isdigit(*p)) {
			for(endp = p ; isdigit(*endp); endp--)
				/* */ ;
			endp++;
			current = atoi(endp);
			for(i = 0; &filename[i] < endp; i++)
				fname[i] = filename[i];
			fname[i++] = '%';
			fname[i++] = 'd';
			for(++p; *p != '\0'; p++, i++)
				fname[i] = *p;
			fname[i] = '\0';
			break;
		}

 	if(index(fname, '%') == NULL)
		printf("Warning: File name \"%s\" does not contain a number or %%d\n",
			filename);

	template = fname;
}

void
REGIONinit2()
{
	char rv[3];

	GDEVstart();

	rv[0] = MOUSE_CHAR; rv[1] = LEFT_DOWN; rv[2] = '\0';
	GDEVmouse(MOUSE_EVENT(LEFT_BUTTON, DOWN_TRANSITION), rv);

	rv[0] = MOUSE_CHAR; rv[1] = LEFT_MOVE; rv[2] = '\0';
	GDEVmouse(MOUSE_EVENT(LEFT_BUTTON, DOWN_MOVEMENT), rv);

	rv[0] = MOUSE_CHAR; rv[1] = LEFT_UP; rv[2] = '\0';
	GDEVmouse(MOUSE_EVENT(LEFT_BUTTON, UP_TRANSITION), rv);

	rv[0] = MOUSE_CHAR; rv[1] = RIGHT_DOWN; rv[2] = '\0';
	GDEVmouse(MOUSE_EVENT(RIGHT_BUTTON, DOWN_TRANSITION), rv);

	rv[0] = MOUSE_CHAR; rv[1] = RIGHT_MOVE; rv[2] = '\0';
	GDEVmouse(MOUSE_EVENT(RIGHT_BUTTON, DOWN_MOVEMENT), rv);

	rv[0] = MOUSE_CHAR; rv[1] = RIGHT_UP; rv[2] = '\0';
	GDEVmouse(MOUSE_EVENT(RIGHT_BUTTON, UP_TRANSITION), rv);

	GDEVflush();

}
