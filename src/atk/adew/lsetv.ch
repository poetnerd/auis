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


 

#define lsetview_UnInitialized 0
#define lsetview_MakeHorz 1
#define lsetview_MakeVert 2
#define lsetview_INSERTVIEW 3
#define lsetview_HasView 4
#define lsetview_Initialized 5
#define lsetview_IsSplit 6
#define lsetview_UpdateView 7
#define lsetview_FirstUpdate 8
#define lsetview_NoUpdate 9
/* Values 11/12, not 10, deliberately -- lsetv.c's own file-local
   lsetview_NeedLink is 10; these two live in this public header
   (unlike NeedLink) because ls->type is a real dataobject field a
   builder like htmlatk.c writes directly, so it needs to be visible
   outside this file. Like MakeHorz/MakeVert, a split's ls->pct field
   holds the split's size -- but here as an absolute pixel bsize (fed
   straight to lpair_HTFixed/VTFixed, i.e. lpair_TOPFIXED: the LEFT/
   TOP child gets exactly that many pixels, min'd against whatever
   space is actually available, and the RIGHT/BOTTOM child gets the
   remainder), not a 0-100 percentage. Only ever set by a builder
   (e.g. htmlatk.c's BuildLsetChain) constructing a fresh lset tree in
   code -- there is no interactive/authoring-mode UI path that creates
   one of these (lsetview__Hit's click-to-split logic, ~line 194-230,
   is unchanged and still only ever creates MakeHorz/MakeVert
   percentage splits), so self->mode never takes these values, only
   ls->type does; safe to add without touching the self->mode state
   machine above. See initkids() and the two ls->pct/objsize sync
   sites in lsetview__Update/lsetview__ObservedChanged (lsetv.c) for
   the three places that actually need to know about these. */
#define lsetview_MakeHorzFixed 11
#define lsetview_MakeVertFixed 12

class lsetview[lsetv]:lpair {
overrides:
    DesiredSize(long width, long height, enum view_DSpass pass, long *dWidth, long *dHeight) returns enum view_DSattributes;
    FullUpdate(enum view_UpdateType type, long left, long top, long width, long right);
    Update();
    Hit (enum view_MouseAction action, long x, long y, long numberOfClicks) returns struct view *;
    ReceiveInputFocus();
    LoseInputFocus();
    SetDataObject(struct dataobject *ls);
    ObservedChanged (struct observable *changed, long value);
    LinkTree(struct view *parent);
    WantNewSize(struct view *requestor);
    Print(FILE *file, char *processor, char *finalFormat, boolean topLevel);
    InitChildren();
    CanView(char *TypeName) returns boolean;
methods:
    ReadFile(FILE *thisFile,char *iname);
    Unsplit(struct lsetview *who) returns boolean;
classprocedures:
    InitializeClass()returns boolean;
    Create(int level,struct lset *ls,struct view *parent) returns struct lsetview *;
    InitializeObject(struct lsetview *self) returns boolean;    
    FinalizeObject(struct lsetview *self);
data:
	int HasFocus;
	struct keystate *keystate;
	struct menulist *menulist;
	struct cursor *cursor;
	int mode;
	int level;
    struct view *child,*app;
    int promptforparameters;
    struct text *pdoc;
    int revision;
    int sizepending; /* debounce for WantNewSize escalation -- see lsetview__WantNewSize/DesiredSize */
};

