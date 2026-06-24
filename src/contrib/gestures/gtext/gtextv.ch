/******************************************************************************
 *
 * gtextv - Gesture Text View
 * Medical Informatics 
 * Washington University, St. Louis
 * July 29, 1991
 *
 * Scott Hassan
 * Steve Cousins
 * Mark Frisse
 *
 *****************************************************************************/

/*
	$Disclaimer: 
*Permission to use, copy, modify, and distribute this software and its 
*documentation for any purpose is hereby granted without fee, 
*provided that the above copyright notice appear in all copies and that 
*both that copyright notice, this permission notice, and the following 
*disclaimer appear in supporting documentation, and that the names of 
*IBM, Carnegie Mellon University, and other copyright holders, not be 
*used in advertising or publicity pertaining to distribution of the software 
*without specific, written prior permission.
*
*IBM, CARNEGIE MELLON UNIVERSITY, AND THE OTHER COPYRIGHT HOLDERS 
*DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING 
*ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT 
*SHALL IBM, CARNEGIE MELLON UNIVERSITY, OR ANY OTHER COPYRIGHT HOLDER 
*BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY 
*DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
*WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS 
*ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
*OF THIS SOFTWARE.
* $
*/


/*****************************************************************************
 * 
 * gtextv.c -- The Gesture Text View Module
 *
 *****************************************************************************/

class gtextv : textview [textv] {

 overrides:
  Hit(enum view_MouseAction action, long x, long y, long numberOfClicks)
    returns struct view *;
 methods:
 classprocedures:
  InitializeClass() returns boolean;
  InitializeObject(struct gtextv *self) returns boolean;
  FinalizeObject(struct gtextv *self);

 data:
  int *xp, *yp;          /* Mouse Buffering Arrays */
  int index;             /* The current length of the buffers */
  int limit;             /* The current limit of the buffers */
  long parstart, parend; /* The starting and ending positions for */
                         /* the selected region. */
}; 


