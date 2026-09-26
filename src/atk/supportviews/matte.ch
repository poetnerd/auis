/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

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


 

class matte : view {
overrides:
    WantNewSize(struct view *requestor);
    PostMenus(struct menulist *menulist);
    FullUpdate(enum view_UpdateType type, long left, long top, long width, long right);
    Update();
    Print(FILE *file, char *processor, char *finalFormat, boolean topLevel);
    Hit (enum view_MouseAction action, long x, long y, long numberOfClicks) returns struct view *;
    DesiredSize(long width, long height, enum view_DSpass pass, long *dWidth, long *dheight) returns enum view_DSattributes;
    GetOrigin(long width, long height, long *originX, long *originY);
    WantInputFocus(struct view *requestor);
    ReceiveInputFocus();
    LoseInputFocus();
    PostMenus(struct menulist *menulist);
    SetDataObject(struct dataobject *dataobject);
    ObservedChanged (struct observable *changed, long value);
    LinkTree(struct view *parent);
    InitChildren();
methods:
    SetResizing(long key);
classprocedures:
    Create(struct viewref *vr,struct view *parent) returns struct matte *;
    InitializeClass() returns boolean;
    InitializeObject(struct matte *self) returns boolean;
    FinalizeObject(struct matte *self);
data:
    int desw,desh;
    struct view *child;
    struct viewref *ref;
    struct cursor *widthcursor, *heightcursor;
    int Moving,resizing,WasMoving, WasResizing;
    struct menulist *menus;
    int drawing, OldMode,sizepending;
    /* Set once, in matte__Create, from the embedded dataobject's own
       class ("lset" -- htmlatk.c's HTML table rendering, see lset.ch).
       matte's normal behavior always reserves a 1px border on every
       side for its own drawn frame/resize-handle outline (matte__
       FullUpdate's matte_DrawRect, matte__DesiredSize's unconditional
       "-2"/"+2") -- appropriate for its original job (a manually
       resizable/grabbable embedded object in the ez document editor),
       but a real browser never draws a frame around a table, and the
       reserved margin showed up live as a small stray band/line around
       HTML-embedded table content (found 2026-09-26, wdc's own
       screenshot: a card's own background color sat inside a 1px
       inset from where the wrapping cell's background was painted,
       leaving a visible seam that doesn't exist in Thunderbird's
       rendering of the same message). No other matte caller sets this
       -- every existing embedded-object use (ez's own inline objects,
       etc.) keeps the original bordered/resizable behavior unchanged. */
    int noborder;
};
