/* Copyright 1992 by the Andrew Toolkit Consortium and Carnegie Mellon University All rights Reserved. */

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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/atk/prefed/RCS/ssliderv.c,v 1.5 1992/12/15 21:39:11 rr2b R6tape $";
#endif



 


#include <andrewos.h>
#include <math.h>
#include <class.h>

#include <scroll.ih>
#include <prefval.ih>
#include "ssliderv.eh"

#include <stdlib.h>
#define zfree(x) do { if(x) { free(x); (x)=NULL;}} while (0)
#define DATA(self) ((struct prefval *)ssliderv_GetDataObject(self))

boolean ssliderv__InitializeObject(struct classheader *classID, struct ssliderv *self)
{
    ssliderv_SetScrollee(self, self);
    return TRUE;
}

void ssliderv__FinalizeObject(struct classheader *classID, struct ssliderv *self)
{
}

static void getinfo(struct ssliderv *self, struct range *total, struct range *seen, struct range *dot)
{
    ssliderv_GetInfo(self, total, seen, dot);
}

void ssliderv__GetInfo(struct ssliderv *self, struct range *total, struct range *seen, struct range *dot)
{    
}

static long whatisat(struct ssliderv *self, long numerator, long denominator)
{
    ssliderv_WhatIsAt(self, numerator, denominator);
}

long ssliderv__WhatIsAt(struct ssliderv *self, long numerator, long denominator)
{
    return 0;
}

static void setframe(struct ssliderv *self, long position, long numerator, long denominator)
{
    ssliderv_SetFrame(self, position, numerator, denominator);
}

void ssliderv__SetFrame(struct ssliderv *self, long pos, long num, long denom)
{
}

static void endzone(struct ssliderv *self, int end, enum view_MouseAction action)
{
    ssliderv_Endzone(self, end, action);
}

void ssliderv__Endzone(struct ssliderv *self, int end, enum view_MouseAction action)
{
}
    
static struct scrollfns scrollInterface = {getinfo, setframe, endzone, whatisat};

char *ssliderv__GetInterface(struct ssliderv *self, char *name)
{
    return (char *)&scrollInterface;
}

void ssliderv__FullUpdate(struct ssliderv *self, enum view_UpdateType type, long left, long top, long width, long height)
{
    ((struct scroll *)self)->desired.location=((struct scroll *)self)->ideal_location;
    super_FullUpdate(self, type, left, top, width, height);
}


enum view_DSattributes ssliderv__DesiredSize(self, width, height, pass, dWidth, dHeight)
struct ssliderv *self;
long width;
long height;
enum view_DSpass pass;
long *dWidth;
long *dHeight;
{
   unsigned int loc=(unsigned int)ssliderv_GetLocation(self);
   int w=0, h=0;
   enum view_DSattributes retval=view_Fixed;
   if(loc&(scroll_LEFT|scroll_RIGHT)) {
       w+=ssliderv_GetWidth(self)+10;
       h+=128;
       retval=(enum view_DSattributes)(((int)retval) || view_HeightFlexible);
   }
   if(loc&(scroll_TOP|scroll_BOTTOM)) {
       h+=ssliderv_GetWidth(self)+10;
       w+=128;
       retval=(enum view_DSattributes)(((int)retval) || view_WidthFlexible);
   }
  *dWidth=w;
  *dHeight=h;
  return retval;
}

void ssliderv__GetOrigin(struct ssliderv *self, long width, long height, long *originX, long *originY)
{
    *originX=0;
    *originY=height;
}
