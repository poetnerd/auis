/*          Copyright IBM Corporation 1988,1991 - All Rights Reserved
	Copyright Carnegie Mellon University 1992 - All Rights Reserved
	For full copyright information see:'andrew/config/COPYRITE'
*/

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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/atk/text/RCS/textv.c,v 3.9 1994/01/12 21:38:34 rr2b Exp $";
#endif

#include <andrewos.h>
#include <stdlib.h>
#include <stdarg.h>
#include <class.h>
#include <ctype.h>
#include <dict.ih>
#include <environ.ih>
#include <envrment.ih>
#include <fontdesc.ih>
#include <graphic.ih>
#include <keystate.ih>
#include <mark.ih>
#include <matte.ih>
#include <menulist.ih>
#include <message.ih>
#include <rectlist.ih>
#include <scroll.ih>
#include <style.ih>
#include <stylesht.ih>
#include <text.ih>
#include <txttroff.ih>
#include <view.ih>
#include <viewref.ih>
#include <im.ih>

#include <tabs.ih>
#include <txtvinfo.h>
#include <textv.eh>

struct textview_classinfo;
static long BackSpace(struct textview *self, long pos, long units, enum textview_MovementUnits type, long *distMoved, long *linesAdded);
static long CalculateBltToTop(struct textview *self, long pos, long *distMoved, long *linesAdded);
static int CalculateLineHeight(struct textview *self);
static struct environment * CheckHidden(struct text *self, long pos);
static void CopyLineInfo(struct textview *self, struct linedesc *newline, struct linedesc *line, int movement);
static void CreateMatte(struct textview *self, struct viewref *vr);
static void DoCopySelection(struct textview *self, FILE *cutFile, long pos, long len);
static void DoUpdate();
static void EnsureSize();
static void FreeTextData(struct textview *self);
static void HandleSelection(struct textview *self, long len);
static long ReverseNewline(struct text *self, long pos);
static void UpdateCursor(struct textview *self, boolean oldCursor);
static void XorCursor(struct textview *self);
static long position(struct textview *self, long pos, struct linedesc *theline, long coord);
static long CalculateCharsPerLine(struct textview *self);
static void RecordScrollWeight(struct textview *self, long pos, long height, boolean containsView);
static long AccumulatedWeightThrough(struct textview *self, long pos);
static long DecodeWeight(struct textview *self, long encoded);
static void FreeScrollWeights(struct textview *self);
/* Forward reference -- real declaration + rationale comment is down by
   GetScrollWeights(). Needed here too: LinkTree/FinalizeObject (below)
   must recognize and skip this dictionary entry (see their own comments). */
static char ScrollWeightsKey;
static int stringmatch(struct text *d, long pos, char *c);

/* Storage for the elevator-drag position correction -- see position()'s
   own 2026-09-24 comment (below, near getinfo/setframe) for the full
   rationale. Kept out of `struct textview` itself (textv.ch) and out of
   this blob's own struct too (deliberately a `char *`-opaque blob
   reached only via dictionary_LookUp/Insert, textv.c's existing
   per-object auxiliary-storage mechanism -- see LinkTree/FinalizeObject
   for its other, older use here) specifically so that adding this
   feature never requires shifting any field offset either subclasses of
   textview (there are 25+ across the tree, `messages` among them, built
   as its own separately-linked messages.do) would need recompiling to
   stay in sync with. */
struct scrollweightlist {
    struct scrollweight *entries;
    int nEntries;
    int aEntries;
};

static struct graphic *pat;

#define TEXT_VIEWREFCHAR '\377'  /* place holder character for viewrefs */
#define textview_MOVEVIEW 99999999

#define NLINES 200    /* This set to keep redisplay from going into an infinite loop if something goes drastically wrong */
#define BX 20
#define BY 5
#define EBX 2
#define EBY 2
#define BADCURPOS -1
#define MAXPARA 200
#define REMOVEDCURSOR 32765
#define TABBASE ((int) 'n')
/* CORRECTION (2026-09-23, found live-testing messages' new HTML-table
   scrollable-tableau feature): FINESCROLL/FINEMASK bounded the fine
   (sub-line) component of the scrollbar elevator's position encoding
   to 7 bits, i.e. FINEMASK(127)*FINEGRID(12) = 1524px of resolution
   *within* a single line/view -- fine for ordinary text lines and
   images, but this codebase now has a real case of one embedded view
   several thousand pixels tall (a whole multi-row HTML table, kept as
   one view so its horizontal scroll position stays in sync across
   rows -- see html-scroll-plan.md). Positions beyond 1524px into such
   a view all saturated to the same encoded value (position()'s own
   "off > FINEMASK" clamp below), so the elevator's drag/click math --
   which decodes this encoding to figure out where you dragged to --
   could resolve two genuinely different scroll positions to the same
   value, or dead-reckon the wrong one, producing exactly the
   "dragging near the bottom jumps way back up" symptom reported live.
   14 bits (16383) here raises that ceiling to 16383*12 =~ 196,000px,
   comfortably past any real message content. Purely an internal
   encoding width: grepped the whole tree, nothing outside this file
   reads FINESCROLL/FINEMASK/FINEGRID -- the scroll-interface callers
   (scroll.c) only ever see the resulting opaque `long` position
   values, never these constants, so this is safe to widen without
   touching anything else. Kept in textv.c despite this project's own
   "lpair.c/textv.c/drawtxtv.c untouched" scoping (html-scroll-plan.md)
   -- explicitly re-scoped in: this is the actual root cause, it's a
   narrow encoding-width change, not the deeper pixel-accurate-paging
   rewrite that scoping was written to avoid (see roadmap.md's own
   open item on that separate, still-unfixed imprecision).

   CORRECTION (2026-09-25): that "nothing outside this file reads
   FINESCROLL" claim was wrong -- textv.ch's own EncodePosition/
   DecodePosition macromethods hardcoded the same shift as a literal
   7, independently of this #define, and every textview subclass
   (messages' `captions` in particular) inherits and calls them to
   decode getinfo()'s FINESCROLL-shifted positions back to raw
   character offsets. That mismatch (decode by 7 what was encoded by
   14) is what broke the captions scrollbar. FINESCROLL is now defined
   once, in textv.ch (textview_FINESCROLL), and both this #define and
   textv.ch's macromethods read that single definition. */
#define FINESCROLL textview_FINESCROLL
#define FINEMASK 16383 /* 2 ^ FINESCROLL - 1 */
#define FINEGRID 12

static struct fontdesc *iconFont = NULL;

static struct keymap *textviewEmacsKeymap;
static struct keymap *textviewViInputModeKeymap;
static struct keymap *textviewViCommandModeKeymap;
static struct menulist *viewMenus;
static boolean initialExposeStyles;
static boolean alwaysDisplayStyleMenus;
static boolean highlightToBorders;

extern void textview__LookCmd(struct textview *self, int look); /* Needed for menulist functions. */
extern void InitializeMod();	/* defined in txtvcsty.c */
extern int charType(char c);		/* defined in txtvcmds.c */

/* Scroll stuff. */
static void getinfo(struct textview *self, struct range *total, struct range *seen, struct range *dot), setframe(struct textview *self, long position, long numerator, long denominator), endzone(struct textview *self, int end, enum view_MouseAction action);
static long whatisat(struct textview *self, long numerator, long denominator);
static struct scrollfns scrollInterface = {getinfo, setframe, endzone, whatisat};

#define Text(self) ((struct text *) ((self)->header.view.dataobject))

boolean textview__InitializeObject(struct classheader *classID, struct textview *self)
{
    long fontSize = 12;
    char bodyFont[100];
    char *font;
    long fontStyle = fontdesc_Plain;
    struct style *defaultStyle;
    boolean justify;
    char *editorPtr;

    self->displayLength = 0;
    self->hasInputFocus = FALSE;
    self->dot = NULL;
    self->top = NULL;
    self->frameDot = NULL;
    self->force = FALSE;
    self->nLines = 0;
    self->aLines = 0;
    self->lines = NULL;
    self->bx = BX;
    self->by = BY;
    self->ebx = EBX;
    self->eby = EBY;
    self->hasApplicationLayer = FALSE;
    self->editor = EMACS;
    self->keystate = self->emacsKeystate = keystate_Create(self, textviewEmacsKeymap);
    self->viCommandModeKeystate = keystate_Create(self, textviewViCommandModeKeymap);
    self->viInputModeKeystate = keystate_Create(self, textviewViInputModeKeymap);
    /* Look first at preference, then at shell variables to determine editor */
    /* if VI user, set initial keystate to VI command mode */
    if ( (editorPtr = environ_GetProfile("editor")) != NULL )	
    {
	if ( strlen(editorPtr) >= 2 && !strcmp(editorPtr + strlen(editorPtr) - 2, "vi" ) )
	{
	    self->editor = VI;
	    self->viMode = COMMAND;
	    self->keystate = self->viCommandModeKeystate;
	}
    }
    self->styleMenus = NULL;
    self->menus = menulist_DuplicateML(viewMenus, self);
    menulist_SetMask(self->menus, textview_NoMenus);
    self->clearFrom = 99999999;
    self->csxPos = BADCURPOS;
    self->csyPos = BADCURPOS;
    self->cshPos = BADCURPOS;
    self->csbPos = BADCURPOS;
    self->cexPos = BADCURPOS;
    self->ceyPos = BADCURPOS;
    self->cehPos = BADCURPOS;
    self->cebPos = BADCURPOS;
    self->scroll = textview_NoScroll;
    self->scrollLine = 0;
    self->scrollDist = -1;
    self->needUpdate = FALSE;
    self->lastStyleMenuVersion = -1;
    self->atMarker = NULL;
    self->displayEnvironment = NULL;
    self->displayEnvironmentPosition = 0;
    self->exposeStyles = initialExposeStyles;
    self->showColorStyles = -1;		/* Update when view tree is linked. */
    self->tabWidth = 1;
    self->predrawn = self->prepredrawn = NULL;
    self->predrawnY = self->predrawnX = -1;
    
    defaultStyle = style_New();
    style_SetName(defaultStyle, "default");

    self->ScreenScaleMul = environ_GetProfileInt("TabScalingMultiplier", 14);
    self->ScreenScaleDiv  = environ_GetProfileInt("TabScalingDivisor", 12);
    self->LineThruFormatNotes =
      environ_GetProfileSwitch("LineThruFormatNotes", FALSE);
    if ((font = environ_GetProfile("bodyfont")) == NULL || ! fontdesc_ExplodeFontName(font, bodyFont, sizeof(bodyFont), &fontStyle, &fontSize)) {
	strcpy(bodyFont, "Andy");
    }
    justify = environ_GetProfileSwitch("justified", TRUE);
    style_SetFontFamily(defaultStyle, bodyFont);
    style_SetFontSize(defaultStyle, style_ConstantFontSize, fontSize);
    style_AddNewFontFace(defaultStyle, fontStyle);
    if (! justify)
        style_SetJustification(defaultStyle, style_LeftJustified);
    textview_SetDefaultStyle(self,defaultStyle);

    self->insertStack = NULL;
    self->insertEnvMark = NULL;

    self->pixelsShownOffTop = 0;
    self->pixelsReadyToBeOffTop = 0;
    self->pixelsComingOffTop = 0;

    return TRUE;
}

void textview__LinkTree(struct textview *self, struct view *parent)
{

    int cnt;
    super_LinkTree(self, parent);
    if((cnt = dictionary_CountRefs(self)) > 0){
        char **list;
	struct viewref **vr;
	struct view *view;
	if((list = (char **)malloc(cnt * sizeof(char *))) != NULL){
	    dictionary_ListRefs(self,list,cnt);
	    for(vr = (struct viewref **)list; cnt--; vr++){
		/* dictionary_ListRefs returns every key ever Inserted for
		   `self`, not just viewrefs -- GetScrollWeights() shares
		   this same per-textview dictionary, keyed on
		   &ScrollWeightsKey, to stash its scroll-weight blob.
		   Without this check, a reused textview that still has a
		   scroll-weight entry from a prior body gets that blob
		   handed to view_LinkTree() as if it were a view, jumping
		   through garbage (confirmed live via crash log,
		   2026-09-25: SIGBUS in a nested lsetview/lpair LinkTree
		   chain, faulting PC inherited from the blob's first
		   bytes). */
		if ((char *) *vr == &ScrollWeightsKey) continue;
		view = (struct view *) dictionary_LookUp(self,(char *)*vr);
		if(view){
		    view_LinkTree(view, self);
		}
	    }
            free(list);
        }
    }
}

static void FreeTextData(struct textview *self)
{
    struct text *t = Text(self);

    if(t!=NULL){
	if (self->dot != NULL) {
	    text_RemoveMark(t, self->dot);
	    mark_Destroy(self->dot);
	    self->dot = NULL;
	}
	if (self->top != NULL) {
	    text_RemoveMark(t, self->top);
	    mark_Destroy(self->top);
	    self->top = NULL;
	}
	if (self->frameDot != NULL) {
	    text_RemoveMark(t, self->frameDot);
	    mark_Destroy(self->frameDot);
	    self->frameDot = NULL;
    }
	if (self->predrawn != NULL) {
	    text_RemoveMark(t, self->predrawn);
	    mark_Destroy(self->predrawn);
	    self->predrawn = NULL;
	}
	if (self->prepredrawn != NULL) {
	    text_RemoveMark(t, self->prepredrawn);
	    mark_Destroy(self->prepredrawn);
	    self->prepredrawn = NULL;
	}
	if (self->insertEnvMark != NULL) {
	    text_RemoveMark(t, self->insertEnvMark);
	    mark_Destroy(self->insertEnvMark);
	    self->insertEnvMark = NULL;
	}
	if (self->atMarker != NULL) {
	    text_RemoveMark(t, self->atMarker);
	    mark_Destroy(self->atMarker);
	    self->atMarker = NULL;
	}
    }

    if (self->lines != NULL)  {
	int i;

	for(i = 0; i < self->aLines; i++)  {
	    if (t != NULL)
		text_RemoveMark(t, self->lines[i].data);
	    mark_Destroy(self->lines[i].data);
	}
	free(self->lines);
	self->lines = NULL;
	self->nLines = 0;
	self->aLines = 0;
    }
    FreeScrollWeights(self);
}


void textview__FinalizeObject(struct classheader *classID, struct textview *self)
{
    int cnt;
    if((cnt = dictionary_CountRefs(self)) > 0){
	char **list;
	struct viewref **vr;
	struct view *view;
	if((list = (char **)malloc(cnt * sizeof(char *))) != NULL){
	    dictionary_ListRefs(self,list,cnt);
	    for(vr = (struct viewref **)list; cnt--; vr++){
		/* Same non-viewref dictionary entry as
		   textview__LinkTree's own scroll-weight guard above --
		   see its comment. FreeTextData (called below) is what
		   actually frees/deletes the scroll-weight blob; skip it
		   here rather than dispatching view_Destroy/
		   viewref_RemoveObserver on it. */
		if ((char *) *vr == &ScrollWeightsKey) continue;
		view = (struct view *) dictionary_LookUp(self,(char *)*vr);
		if(view) view_Destroy(view);
		dictionary_Delete(self,(char *)*vr);
		viewref_RemoveObserver(*vr,self);
	    }
	    free(list);
	}
    }


    FreeTextData(self);

    style_Destroy(self->defaultStyle);
    keystate_Destroy(self->emacsKeystate);
    keystate_Destroy(self->viInputModeKeystate);
    keystate_Destroy(self->viCommandModeKeystate);
    if (self->menus != NULL)  {
	menulist_Destroy(self->menus);
    }
    if (self->styleMenus != NULL)  {
	menulist_Destroy(self->styleMenus);
    }
    if (self->insertStack != NULL) {
	textview_ClearInsertStack(self);
    }
}

static struct environment * CheckHidden(struct text *self, long pos)
{
    struct environment *env=environment_GetInnerMost(self->rootEnvironment, pos);
    while(env!=NULL && env->type==environment_Style) {
	struct style *s=env->data.style;
	if(s && style_IsHiddenAdded(s)) break;
	env=(struct environment *)environment_GetParent(env);
    }
    if(env && env->type==environment_Style) return env;
    return NULL;
}


static long ReverseNewline(struct text *self, long pos)
{
    long tp;
    for (tp = pos-1; tp >= 0; tp--) {
	unsigned char c = text_GetChar(self, tp);

	if (c  == '\n' || c == '\r') {
	    struct environment *env=CheckHidden(self, tp);
	    if(env) {
		tp=environment_Eval(env);
		continue;
	    } else break;
	}
    }
    return tp;
}

boolean textview_PrevCharIsNewline(struct text *self, long pos)
{
    struct environment *env;
    unsigned char ch;
    while(pos>0) {
	env=CheckHidden(self, pos-1);
	if(env) {
	    pos=environment_Eval(env);
	    continue;
	}
	ch=text_GetChar(self, pos-1);
	if(ch=='\n') return TRUE;
	else return FALSE;
    }
    return FALSE;			
}
    
static void HandleSelection(struct textview *self, long len)
{
    struct im *im=textview_GetIM(self);
    if(im==NULL) return;

    if(mark_GetLength(self->dot) && len==0) {
	im_GiveUpSelectionOwnership(im, self);
    } else if(len) {
	mark_SetLength(self->dot, len);
	im_RequestSelectionOwnership(im, self);
	return;
    }
    mark_SetLength(self->dot, len);
}

void textview__ObservedChanged(struct textview *self, struct observable *changed, long value)
{
    struct text *tself=Text(self);
    struct view *vself = (struct view *) self;
    if (changed == (struct observable *) vself->dataobject)  {
	if (value == observable_OBJECTDESTROYED) {
	    if (self->displayEnvironment != NULL) {
		textview_ReleaseStyleInformation(self, self->displayEnvironment);
		self->displayEnvironment = NULL;
	    }
	    vself->dataobject = NULL;
	}
	else {
	    if(text_GetLength(tself)==0 && im_GetIM(self)) textview_ClearColors(self);
	    if(self->dot && mark_GetModified(self->dot)) {
		mark_SetModified(self->dot, FALSE);
		if(mark_GetLength(self->dot)==0 && textview_GetIM(self)) im_GiveUpSelectionOwnership(textview_GetIM(self), self);
	    }
	    view_WantUpdate(vself, vself);
	}
    }
    else if (value == observable_OBJECTDESTROYED &&
           (class_IsTypeByName(class_GetTypeName(changed), "viewref"))) {

               struct view *vw;
               struct viewref *vr = (struct viewref *)changed;
               if((vw = (struct view *)dictionary_LookUp(self,(char *)vr)) != NULL){
		   view_UnlinkTree(vw);
		   view_Destroy(vw); 
                   dictionary_Delete(self,(char *)vr);
               }
           }
}

void textview__WantUpdate(struct textview *self, struct view *requestor)
{
    if ((struct view *)self == requestor && self->needUpdate)
        return;

    if ((struct view *)self == requestor)
        self->needUpdate = TRUE;
   super_WantUpdate(self, requestor);
}

void textview__SetDataObject(struct textview *self, struct dataobject *dataObject)
{
    if (!class_IsTypeByName(class_GetTypeName(dataObject), "text"))  {
	fprintf(stderr, "Incompatible dataobject associated with textview\n");
	return;
    }

    if (self->displayEnvironment != NULL) {
	textview_ReleaseStyleInformation(self, self->displayEnvironment);
	self->displayEnvironment = NULL;
    }

    FreeTextData(self);
    self->force = TRUE;

    super_SetDataObject(self, dataObject);

    self->dot = text_CreateMark((struct text *) dataObject, 0, 0);
    self->top = text_CreateMark((struct text *) dataObject, 0, 0);
    self->frameDot = text_CreateMark((struct text *) dataObject, (long)-1, 0);
    /* The frameDot must start at -1 because that is later interpreted
          (near the end of DoUpdate) as meaning that it has never been set */
    self->atMarker = text_CreateMark((struct text *) dataObject, 0, 0);

    self->predrawn = text_CreateMark((struct text *) dataObject, 0, 0);
    self->prepredrawn = text_CreateMark((struct text *) dataObject, 0, 0);
    mark_SetStyle(self->predrawn, TRUE, FALSE);
    mark_SetStyle(self->prepredrawn, TRUE, FALSE);

    mark_SetStyle(self->dot, FALSE, FALSE);
    mark_SetStyle(self->top, FALSE, FALSE);
    mark_SetStyle(self->frameDot, FALSE, FALSE);
    mark_SetStyle(self->atMarker, FALSE, FALSE);
    self->lastStyleMenuVersion = -1;
    menulist_SetMask(self->menus, textview_NoMenus);
    self->movePosition = -1;

    self->insertEnvMark = text_CreateMark((struct text *) dataObject, 0, 0);
    mark_SetStyle(self->insertEnvMark, FALSE, FALSE);

    textview_WantUpdate(self, self);
}

struct view * textview__GetApplicationLayer(struct textview *self)
{
    long scrollpos=scroll_LEFT;
    char *pos=environ_GetProfile("ScrollbarPosition");
    
    self->hasApplicationLayer = TRUE;

    if(pos) {
	if(!strcmp(pos,"right")) scrollpos=scroll_RIGHT;
    } else if(environ_GetProfileSwitch("MotifScrollbars", FALSE)) scrollpos=scroll_RIGHT;
    
     return (struct view *) scroll_Create(self, scrollpos);
}

void textview__DeleteApplicationLayer(struct textview *self, struct view *scrollbar)
{
    self->hasApplicationLayer = FALSE;
    scroll_Destroy((struct scroll *) scrollbar);
}

static void EnsureSize(struct textview *self, long lines)
{
    register int i, j;
    register long nSize;

    if (lines < self->aLines) return;
    i = (lines > 10) ? (lines<<1) : lines + 2;
    nSize = i * (sizeof(struct linedesc));
    self->lines = (struct linedesc *) ((self->lines != NULL) ? realloc(self->lines, nSize) : malloc(nSize));
    for(j=self->aLines; j<i; j++)  {
	self->lines[j].y = 0;
	self->lines[j].height = 0;
	self->lines[j].containsView = FALSE;
	self->lines[j].data = text_CreateMark(Text(self),0,0);
    }
    self->aLines = i;
}

/* Updates the cursor.  If oldcursor is true then it adds the elements to the old rectangle list.  If it is false then it adds them to the new rectangle list and then at the end does an invert rectangle.
*/

static void UpdateCursor(struct textview *self, boolean oldCursor)
{
    int csx, csy, csh, csb, cex, cey, ceh, ceb, by;

    csx = self->csxPos; csy = self->csyPos;
    csh = self->cshPos; csb = self->csbPos;
    cex = self->cexPos; cey = self->ceyPos;
    ceh = self->cehPos; ceb = self->cebPos;

    if (csx == BADCURPOS && cex == BADCURPOS && ((mark_GetPos(self->top) < mark_GetPos(self->dot) || mark_GetPos(self->top) >= mark_GetPos(self->dot) + mark_GetLength(self->dot)) || ! self->hasInputFocus)) {
	if (oldCursor)
	    rectlist_ResetList();
	else
	    rectlist_InvertRectangles(self);
	return;
    }

    /* Handle start and end positions that lie offscreen. */

    if (csx == BADCURPOS)  {
	csx = 0;
	if (cey-ceh > 0)  {
	    /* ending is not on first line, fake a big first line. */

	    csy = cey-ceh;
	    csh = csy;
	    csb = 0;
	}
	else  {
	    /* ending is on first line, put beginning there too */

	    csy = cey;
	    csh = ceh;
	    csb = ceb;
	}
    }

    if (cex == BADCURPOS)  { 
	cex = textview_GetLogicalWidth(self) - 1;
	if (csy == textview_GetLogicalHeight(self))  {
	    /* beginning is at bottom, put ending there too-> */

	    cey = csy;
	    ceh = csh;
	    ceb = csb;
	}
	else  {
	    /* beginning not at bottom, fake big second line. */

	    cey = textview_GetLogicalHeight(self) - 1;
	    ceh = cey-csy;
	    ceb = 0;
	}
    }

    textview_SetFont(self,iconFont);
    textview_SetTransferMode(self,graphic_XOR);

    by = (self->hasApplicationLayer) ? self->by : self->eby;

    if (oldCursor)
	rectlist_ResetList();

    if (csx != cex || csy != cey)  {
	/* Fix up sometimes bogus x values.  We move them in an extra pixel if necessary to show an empty rectangle. */

	if (csx < 0) csx = 0;
	if (csx > textview_GetLogicalWidth(self) - 1) csx = textview_GetLogicalWidth(self)-1;
	if (cex < 0) cex = 0;
	if (cex > textview_GetLogicalWidth(self) - 1) cex = textview_GetLogicalWidth(self) - 1;
	if (cex == 0 && cey != csy)  {
	    ceh = ((cey = (cey - ceh)) == csy) ? csh : cey - csy;
	    cex = textview_GetLogicalWidth(self) - 1;
	}
	if (cey <= by)  {
	    self->cexPos = self->csxPos = BADCURPOS;
	    if (oldCursor)
		rectlist_ResetList();
	    else
		rectlist_InvertRectangles(self);
	    return;
	}

	/* Now figure out how to draw it. */

	if (csy == cey)  {
	    /* start and end are on same line */

	    if (oldCursor)
		rectlist_AddOldRectangle(csy, csy - csh, csx, cex);
	    else
		rectlist_AddNewRectangle(csy, csy - csh, csx, cex, 0);
	}
	else  { 
	    /* Box first line */

	    if (oldCursor)
		rectlist_AddOldRectangle(csy, csy - csh, csx, textview_GetLogicalWidth(self) - 1);
	    else
		rectlist_AddNewRectangle(csy, csy - csh, csx, textview_GetLogicalWidth(self) - 1, 0);

	    if (cey-ceh!=csy)  {
		/* Box middle lines */

		if (oldCursor)
		    rectlist_AddOldRectangle(cey - ceh, csy, 0,textview_GetLogicalWidth(self) - 1);
		else
		    rectlist_AddNewRectangle(cey - ceh, csy, 0,textview_GetLogicalWidth(self) - 1, 0);
	    }

	    /* Box last line */

	    if (oldCursor)
		rectlist_AddOldRectangle(cey, cey - ceh, 0, cex);
	    else
		rectlist_AddNewRectangle(cey, cey - ceh, 0, cex, 0);
	}
    }
    else  {
	textview_MoveTo(self,csx,csy-csb);
	textview_DrawString(self,"|",graphic_NOMOVEMENT);
    }
    if (! oldCursor)  {
	rectlist_InvertRectangles(self);
	self->csxPos = csx;
	self->csyPos = csy;
	self->cshPos = csh;
	self->csbPos = csb;
	self->cexPos = cex;
	self->ceyPos = cey;
	self->cehPos = ceh;
	self->cebPos = ceb;
    }
}

static void XorCursor(struct textview *self)
{
    rectlist_ResetList();
    UpdateCursor(self, FALSE);
}

static void CopyLineInfo(struct textview *self, struct linedesc *newline, struct linedesc *line, int movement)
{
    if (line != newline) {
        mark_SetPos(newline->data,  mark_GetPos(line->data));
        mark_SetLength(newline->data, mark_GetLength(line->data));
        mark_SetStyle(newline->data, mark_IncludeBeginning(line->data), mark_IncludeEnding(line->data));
        newline->nChars = line->nChars;
        mark_SetModified(newline->data, FALSE);
        mark_SetObjectFree(newline->data, mark_ObjectFree(line->data));
        newline->height = line->height;
        newline->xMax = line->xMax;
        newline->containsView = line->containsView;
    }
    newline->y = line->y + movement;
    if (newline->containsView) {
        /* Put code for moving inset here */
        textview_ViewMove(self, newline, movement);
    }
}

/* This routine redraws that which must change.  The key concept is this: marks have a modified flag that can be cleared, and which is set whenever the text within a marker is modified.  A view consists of a set of lines and the marks representing the corresponding text.  When we are supposed to update a view, we redraw the lines which correspond to modified marks. *//* 

 *//* For each line that needs to be redrawn, we call the type-specific redrawing routine.  For now, we will call specific routines in the document handler, but this will change soon. */

static void DoUpdate(struct textview *self, boolean reformat)
{
    register struct mark *lob, *tob;
    register struct linedesc *tl;
    long lobChars;
    register int line;
    int textLength = text_GetLength(Text(self));
    int curx, cury, height, force, cStart, cEnd, mStart, mLength, csx;
    int csy, csh, csb, cex, cey, ceh, ceb, ysleft, xs, ys, t,stopline,redrawline;
    boolean cursorVisible = 0;
    boolean changed;
    int cont;		/* force continuation of redraw */
    /* set to v->scroll if this is a scrolling update */
    enum textview_ScrollDirection scrolling = textview_NoScroll;
    boolean zapRest;
    struct rectangle tempSrcRect;
    struct point tempDstOrigin;	/* Temps for graphics operations */
    struct formattinginfo info;
    long textheight;
    long bx, by;
    
    if(self->force) {
	if(self->predrawn) {
	    mark_SetPos(self->predrawn, -1);
	    mark_SetLength(self->predrawn, 0);
	}
	if(self->prepredrawn) {
	    mark_SetPos(self->prepredrawn, -1);
	    mark_SetLength(self->prepredrawn, 0);
	}
    }
    if (self->lastStyleMenuVersion != Text(self)->styleSheet->version)  {
	/* Have to recompute the size of the tab character */
	struct menulist *styleMenus;

	/* InitStateVector will fill in the tabs field
	 with a new tabs object, up to here the state vector is random garbage. */
	text_InitStateVector(&info.sv);

	text_ApplyEnvironment(&info.sv, self->defaultStyle, Text(self)->rootEnvironment);
	info.sv.CurCachedFont = fontdesc_Create(info.sv.CurFontFamily,
						info.sv.CurFontAttributes, info.sv.CurFontSize);
	info.myWidths = fontdesc_WidthTable(info.sv.CurCachedFont, 	    textview_GetDrawable(self));
	self->tabWidth = info.myWidths[TABBASE];

	/* Now reset the menus for the new styles */

	if (self->styleMenus != NULL)
	    menulist_Destroy(self->styleMenus);
	styleMenus = stylesheet_GetMenuList(((struct text *) Text(self))->styleSheet, (procedure) textview__LookCmd, &textview_classinfo);
	self->styleMenus = menulist_DuplicateML(styleMenus, self);
	self->lastStyleMenuVersion = ((struct text *) Text(self))->styleSheet->version;
	menulist_SetMask(self->menus, textview_NoMenus);   
	/* we're done with this state vector, all the other
calls in this function are being RETURNED state vector information in info.sv, but the tabs field will NOT be valid after this next line */
	text_FinalizeStateVector(&info.sv);
    }
    self->needUpdate = FALSE;
    if (iconFont == NULL)
	iconFont = fontdesc_Create("Icon",fontdesc_Plain,12);

    /* copy dimensions out of the view */

    by = cury = (self->hasApplicationLayer) ? self->by : self->eby;
    bx = curx = (self->hasApplicationLayer) ? self->bx : self->ebx;
    xs = textview_GetLogicalWidth(self) - 2 * bx;
    ys = textview_GetLogicalHeight(self) - 2 * by;

    cury -= self->pixelsReadyToBeOffTop;

    lob = self->top;	/* the first char on the screen */
    lobChars = 0;
    mark_SetLength(lob,0);	/* ignore the length part, which can grow at random */
    force = self->force;

    if (self->scroll == textview_ScrollForward || self->scroll == textview_ScrollBackward)  {
	for (line = 0; line < self->nLines && ! mark_GetModified(self->lines[line].data); line++);
	if (line == self->nLines && line >= 1
	    && !(self->scroll == textview_ScrollForward && self->nLines == 1
		 && self->lines[0].height > self->lines[0].textheight))  { /* was > */
	    /* At this point we know that only scrolling has occurred and we can just do the bit blt and continue formatting */

	    int sy, dy, h, newLine, movement, lasty, yoff, extendedTop = 0;

	    scrolling = self->scroll;

	    if (self->scroll == textview_ScrollForward)  {
		/* Scrolling toward the end of the document */

		/* Determine area that has to be moved */

		if (self->lines[self->nLines - 1].height > self->lines[self->nLines - 1].textheight) {
		    /* last line contains a view that will probably need a full redraw */
		    stopline = self->nLines - 2;
		    redrawline = stopline + 1;
		    mark_SetModified(self->lines[redrawline].data, TRUE); 
		}
		else {
		    stopline = self->nLines - 1;
		    redrawline = -1;
		}

		sy = self->lines[self->scrollLine].y;
		dy = cury;
		/* CORRECTION (2026-09-25, elevator-drag-into-endzone-warps-
		   to-top investigation): stopline goes to -1 above when
		   nLines==1 and that single line is itself an oversized
		   view (e.g. deep inside a many-thousand-pixel embedded
		   table -- exactly the state once scrolled near a
		   document's end, live-traced: nLines=1 sy=5 h=-5). Indexing
		   self->lines[stopline] then read self->lines[-1], one
		   linedesc before the array -- out-of-bounds, garbage in,
		   corrupting this blit's source/dest rectangle math and
		   leaving stale pixels on screen (matches wdc's live report:
		   drag lands at the correct document position internally,
		   but the screen shows the document's top until the next
		   full redraw, e.g. on leaving the endzone). With
		   stopline==-1 there's nothing preceding scrollLine to
		   blit, so the correct range-to-move is empty:
		   lasty==sy, giving h==0 below, which the existing
		   tempSrcRect.height<=0 check already treats as a no-op
		   blit -- exactly right, since redrawline's own
		   mark_SetModified() (above) already queues this line for
		   an ordinary full redraw. */
		lasty = (stopline >= 0)
		    ? self->lines[stopline].y + self->lines[stopline].height
		    : sy;
		if (self->ceyPos >= lasty && self->ceyPos == self->csyPos && self->cexPos == self->csxPos)  {
		    /* We are viewing the end of the file with the carat at the end. */

		    lasty = cury + ys;
		}
		h = lasty - sy;
		movement = sy - dy;

		/* Remove Cursor if it is on the scroll-1 line */

		if (cursorVisible && mark_GetLength(self->dot) == 0 && self->scrollLine > 0 && 
		    mark_GetPos(self->dot) >= mark_GetPos(self->lines[self->scrollLine-1].data) && mark_GetPos(self->dot) < mark_GetPos(self->lines[self->scrollLine].data)) {
		    textview_SetTransferMode(self,graphic_XOR);
		    XorCursor(self);
		}

		textview_SetTransferMode(self, graphic_COPY);
		yoff = (dy < 0) ? dy : 0;/* clip */
		rectangle_SetRectSize(&tempSrcRect, 0, sy - yoff, textview_GetLogicalWidth(self), h - yoff );
		point_SetPt(&tempDstOrigin, 0, dy  - yoff );
		if(tempDstOrigin.y<0) {
		    tempSrcRect.height+=tempDstOrigin.y;
		    tempSrcRect.top-=tempDstOrigin.y;
		    tempDstOrigin.y=0;
		}
		if(tempSrcRect.top<0) {
		    tempSrcRect.height+=tempSrcRect.top;
		    tempDstOrigin.y-=tempSrcRect.top;
		    tempSrcRect.top=0;
		}
		if(tempSrcRect.top + tempSrcRect.height>textview_GetLogicalHeight(self)) {
		    tempSrcRect.height = textview_GetLogicalHeight(self) - tempSrcRect.top;
		}
		if(tempDstOrigin.y + tempSrcRect.height > textview_GetLogicalHeight(self)) {
		    tempSrcRect.height = textview_GetLogicalHeight(self) - tempDstOrigin.y;
		}
		if (tempSrcRect.height <= 0) {
		    /* zero or negative height */
		}
		else {
		    textview_BitBlt(self, &tempSrcRect, self, &tempDstOrigin, (struct rectangle *) NULL);
		}

		rectangle_SetRectSize(&tempSrcRect, 0, dy+h, textview_GetLogicalWidth(self), textview_GetLogicalHeight(self) - (dy+h));
		pat = textview_WhitePattern(self);
		textview_FillRect(self, &tempSrcRect, pat);

		/* if there was something going off the top, 
		    and now there isn't, clear that little bit of spoog */
		if (self->pixelsReadyToBeOffTop == 0 && self->pixelsShownOffTop != 0) {
		    rectangle_SetRectSize(&tempSrcRect, 0, 0, textview_GetLogicalWidth(self), by);
		    pat = textview_WhitePattern(self);
		    textview_FillRect(self, &tempSrcRect, pat);
		}

		/* remove scrolled off views */
		for (line = 0; line < self->scrollLine; line++) {
		    if (self->lines[line].containsView) {
			textview_ViewMove(self,&self->lines[line], textview_REMOVEVIEW);
			/*                      mark_SetModified(self->lines[line].data, TRUE); */
		    }
		}

		for (newLine = 0, line = self->scrollLine; line <= stopline; line++, newLine++)  {
		    CopyLineInfo(self, &self->lines[newLine], &self->lines[line], -movement);
		}

		self->nLines -= self->scrollLine;

		if (self->csxPos != BADCURPOS)  {
		    extendedTop = (self->csyPos - self->cshPos) < by;
		    if (self->ceyPos > lasty)  {
			self->cehPos -= self->ceyPos - lasty;
			self->ceyPos = lasty;

			rectangle_SetRectSize(&tempSrcRect, 0, lasty, textview_GetLogicalWidth(self), textview_GetVisualHeight(self) - lasty);
			pat = textview_WhitePattern(self);
			textview_FillRect(self,&tempSrcRect, pat);
		    }
		    self->csyPos -= movement;
		    self->ceyPos -= movement;
		    if (self->ceyPos - self->cehPos < dy)
			self->cehPos = self->ceyPos - dy;
		    if (self->ceyPos <= by)  {
			if (extendedTop)  {
			    /* clear out the top part of the selection box */

			    rectangle_SetRectSize(&tempSrcRect, 0, 0, textview_GetLogicalWidth(self), by);
			    pat = textview_WhitePattern(self);
			    textview_FillRect(self, &tempSrcRect, pat);
			}
			self->csyPos = BADCURPOS;
			self->csxPos = BADCURPOS;
			self->ceyPos = BADCURPOS;
			self->cexPos = BADCURPOS;
		    }
		    else if ((self->csyPos - self->cshPos) < by)  {
			if (! extendedTop)  {
			    /* Just moved into the border, so color it black */
			    rectangle_SetRectSize(&tempSrcRect, 0, 0,
						  textview_GetLogicalWidth(self), by);
			    pat = textview_BlackPattern(self);
			    textview_FillRect(self, &tempSrcRect, pat);
			}
			self->csyPos = by;
			self->csxPos = 0;
			self->cshPos = by;
		    }
		}
		if (cursorVisible && redrawline >=0 && mark_GetPos(self->dot) >= mark_GetPos(self->lines[redrawline].data)){
		    /* if cursor is on the view line that need updated */
		    rectlist_ResetList();
		    self->csyPos = BADCURPOS;
		    self->csxPos = BADCURPOS;
		    self->ceyPos = BADCURPOS;
		    self->cexPos = BADCURPOS;
		}
		cursorVisible = self->csxPos != BADCURPOS || self->cexPos != BADCURPOS;
	    }
	    else  {
		/* Scrolling toward the beginning of the document */

		int lastLine;
		sy = self->lines[0].y;
		movement = self->scrollDist;
		dy = sy + self->scrollDist;
		lasty = textview_GetLogicalHeight(self) - by;
		h = 0;

		/* determine the height of the area to raster op and the number of lines that will be moved. */

		for (lastLine = 0; lastLine < self->nLines; lastLine++)  {
		    if (dy + h + self->lines[lastLine].height > lasty) break;
		    h += self->lines[lastLine].height;
		}

		textview_SetTransferMode(self, graphic_COPY);
		yoff = (sy < 0) ? sy : 0; /* clip */
		rectangle_SetRectSize(&tempSrcRect, 0, sy - yoff, textview_GetLogicalWidth(self), h - yoff);
		point_SetPt(&tempDstOrigin, 0, dy - yoff);
		if(tempDstOrigin.y<0) {
		    tempSrcRect.height+=tempDstOrigin.y;
		    tempSrcRect.top-=tempDstOrigin.y;
		    tempDstOrigin.y=0;
		}
		if(tempSrcRect.top<0) {
		    tempSrcRect.height+=tempSrcRect.top;
		    tempDstOrigin.y-=tempSrcRect.top;
		    tempSrcRect.top=0;
		}
		if(tempSrcRect.top + tempSrcRect.height>textview_GetLogicalHeight(self)) {
		    tempSrcRect.height = textview_GetLogicalHeight(self) - tempSrcRect.top;
		}
		if(tempDstOrigin.y + tempSrcRect.height > textview_GetLogicalHeight(self)) {
		    tempSrcRect.height = textview_GetLogicalHeight(self) - tempDstOrigin.y;
		}

		textview_BitBlt(self, &tempSrcRect, self, &tempDstOrigin, (struct rectangle *) NULL);

		rectangle_SetRectSize(&tempSrcRect, 0, 0, textview_GetLogicalWidth(self), (sy < 0) ? self->scrollDist : dy);
		pat = textview_WhitePattern(self);
		textview_FillRect(self, &tempSrcRect, pat);

		rectangle_SetRectSize(&tempSrcRect, 0, dy+h, textview_GetLogicalWidth(self), textview_GetLogicalHeight(self) - (dy + h));
		pat = textview_WhitePattern(self);
		textview_FillRect(self,&tempSrcRect,pat);

		newLine = (self->aLines > self->nLines)? self->nLines + 1 :self->nLines ;
		self->nLines = self->scrollLine + lastLine;
		EnsureSize(self, self->nLines);

		/* remove scrolled off views */
		for(line = lastLine + 1; line < newLine ; line++)
		    if(self->lines[line].containsView){
			textview_ViewMove(self, &self->lines[line], textview_REMOVEVIEW);
		    }

		for (newLine = self->nLines - 1, line = lastLine-1; line >= 0; line--, newLine--)  {
		    CopyLineInfo(self, &self->lines[newLine], &self->lines[line], movement);
		}

		if (self->csxPos != BADCURPOS)  {
		    if (self->csyPos - self->cshPos < by)
			self->cshPos = self->csyPos - by;
		    self->csyPos += movement;
		    self->ceyPos += movement;
		    if (self->csyPos > (dy + h))  {
			if (self->csyPos - self->cshPos >= dy + h)  {
			    self->csxPos = BADCURPOS;
			    self->csyPos = BADCURPOS;
			    self->cexPos = BADCURPOS;
			    self->ceyPos = BADCURPOS;
			}
			else  {
			    self->cshPos -= self->csyPos - (dy + h);
			    self->csyPos = dy + h;
			}
		    }
		    if (self->ceyPos > dy + h)  {
			if (self->csxPos != BADCURPOS)  {
			    self->cexPos = textview_GetLogicalWidth(self) - 1;
			    self->ceyPos = dy + h;
			    self->cehPos = (self->ceyPos == self->csyPos) ? self->cshPos : (self->ceyPos - self->csyPos);
			}
		    }
		}
	    }
	}
	else  {
	    /* Both scrolling and some text has changed.  Have no choice but to redraw the entire view.

	       Also reached (2026-09-25, elevator-drag-into-endzone-warps-
	       to-top investigation) when nLines==1 and that single line
	       is itself an oversized view -- scrollLine and redrawline
	       are then the SAME line (0), a case the blit-shortcut above
	       was never designed for (it assumes redrawline is always
	       excluded from, not identical to, the range being shifted).
	       Traced repeatedly: the shortcut's own reposition of the
	       view (view_InsertView, drawtxtv.c) computed a provably
	       correct rectangle every time, yet the screen kept showing
	       the document's original top-of-view content instead of the
	       correct near-bottom sliver, self-correcting only once a
	       real force=1 pass ran (e.g. on leaving the endzone) -- so
	       rather than chase the exact defect further into decades-old
	       X11/backing-store code, this reuses the one path already
	       proven live to repaint correctly in every case. */
	    self->force = 1;
	}
    }
    self->scroll = textview_NoScroll;
    self->scrollDist = -1;

    cursorVisible = self->csxPos != BADCURPOS || self->cexPos != BADCURPOS;

    if (reformat && self->force)  {
	/* 	the reason for doing a full-screen clear is that redrawing the
	    screen with one clear is about twice as fast as doing it line by line->  Sigh. */

	self->force = 0;

	/* Zap cursor since it may spill into other insets. */

	if (cursorVisible)  { 
	    XorCursor(self);
	    cursorVisible = FALSE;
	}
	line = 0;
	while (line < self->nLines)  {
	    if(self->lines[line].containsView) {
		textview_ViewMove(self, &self->lines[line], textview_REMOVEVIEW);
	    }
	    mark_SetModified(self->lines[line++].data, TRUE);
	} 

	textview_SetTransferMode(self, graphic_COPY);
	textview_GetVisualBounds(self, &tempSrcRect);
	pat = textview_WhitePattern(self);
	textview_FillRect(self, &tempSrcRect, pat);
    }

    cStart = mark_GetPos(self->dot);
    cEnd = cStart + mark_GetLength(self->dot);
    textview_SetTransferMode(self, graphic_COPY);
    csx = BADCURPOS;
    csy = BADCURPOS;
    cex = BADCURPOS;
    cey = BADCURPOS;
    csh = 0; csb = 0; ceh = 0; ceb = 0;
    ysleft = ys + self->pixelsReadyToBeOffTop;
    zapRest = TRUE;
    line = 0;
    cont = 0;

    while (line < NLINES) {	/* for each potential line on the screen */
	EnsureSize(self, line);
	tl = &(self->lines[line]);
	tob = tl->data;

	/* move next mark to follow last redisplayed line, if necessary
	    this sets the modified flag on the mark iff a change was performed
	    inline expansion of doc_makefollow(lob, tob) */

	if (reformat && mark_GetPos(tob) != mark_GetPos(lob) + lobChars)  {
	    /*            if(tl->containsView)
		textview_ViewMove(self,tl,textview_REMOVEVIEW); */
	    mark_SetPos(tob, mark_GetPos(lob) + lobChars);
	    mark_SetLength(tob, 0);
	    mark_SetModified(tob, 1);
	}

	/* next check to see if the line must be redrawn */

	if (reformat &&
	    ((cont && scrolling != textview_ScrollBackward)
	     || line >= self->nLines
	     || mark_GetModified(tob)
	     || cury != tl->y
	     || (scrolling == textview_ScrollBackward
		 && (line < self->scrollLine
		     || (line == self->scrollLine
			 && self->pixelsReadyToBeOffTop
			 < self->pixelsShownOffTop))))) {

	    /* zap cursor if need to */
	    if (cursorVisible && self->ceyPos >= cury &&
		(! scrolling || (self->csxPos == self->cexPos &&
				 self->csyPos == self->ceyPos &&
				 self->ceyPos == self->lines[self->nLines - 1].y +
				 self->lines[self->nLines - 1].height))) {
		XorCursor(self);
		cursorVisible = FALSE;
	    }

	    height = textview_LineRedraw(self, textview_FullLineRedraw, tob, curx, cury, xs, ysleft, force, &cont, &textheight, &info);
	    tl->containsView = (info.foundView != NULL);
	    RecordScrollWeight(self, mark_GetPos(tob), height, tl->containsView);
	    tl->height = height;	/* set the new length */
	    tl->textheight = textheight;
	    tl->nChars = info.lineLength;
	    tl->y = cury;

	    ysleft -= height;
	    mark_SetModified(tob,0);	/* clear the mod flag */
	}
	else  {
	    ysleft -= tl->height;
	    cont = 0;
	}
	mStart = mark_GetPos(tob);
	mLength = tl->nChars;

	if (reformat && cury + tl->textheight > ys + by)  {
	    cury += tl->height;
	    zapRest = FALSE;
	    break;
	} 
	cury += tl->height;

	/* Now do the cursor computations.  Note that we try to
	      avoid recomputing the other end of the cursor if it is the same
		  point on the screen. */

	if (cStart >= mStart 
	    && (cStart < mStart+mLength 
		|| mStart + mLength == textLength) 
	    && self->hasInputFocus)  {
	    if (csx < 0)  {
		csx = textview_LineRedraw(self, textview_GetCoordinate, tob, 
					  curx, cury, xs, ys, cStart, NULL,NULL, &info);

		if (csx<0 && cEnd!=cStart) 
		    csx= textview_GetLogicalWidth(self); /*RSK92add*/

		/* If we seem to be at the beginning of the line, and we aren't */
		/* going to draw a caret, adjust back to the inset boundary. */
		/* I hate this. wjh */
		/* oh, yeah? Well, I hate it not being there. Now it's a preference. */
		if (highlightToBorders && cStart == mStart && cEnd != cStart)
		    csx = 0;

		csy = cury - (info.below - info.lineBelow);
		csb = info.lineBelow;
		csh = tl->height - (info.below - info.lineBelow);
	    }
	}
	if (cEnd != cStart && self->hasInputFocus)  {
	    if (cEnd >= mStart 
		&& (cEnd < mStart+mLength 
		    || mStart + mLength == textLength))  {
		if (cex < 0)  {
		    cex = textview_LineRedraw(self, textview_GetCoordinate, tob,
					      curx, cury, xs, ys, cEnd,NULL,NULL, &info);

		    if (cex<0) cex= textview_GetLogicalWidth(self); /*RSK92add*/

		    /* If we seem to be at the beginning of the line, and we aren't */
		    /* going to draw a caret, adjust back to the inset boundary. */
		    /* I hate this. wjh */
		    /* oh, yeah? Well, I hate it not being there. Now it's a preference. */
		    if (highlightToBorders && cEnd == mStart && cEnd != cStart)
			cex = 0;

		    cey = cury - (info.below - info.lineBelow);
		    ceb = info.lineBelow;
		    ceh = tl->height - (info.below - info.lineBelow);
		}
	    }
	}
	else  {
	    cex=csx;
	    cey=csy;
	}

	/* markatend is checked here, since one null line should be redrawn in all
	 redisplays, since this null line could contain the cursor, should it be at the end of the buffer. */

	line++;
	if (mark_GetPos(tob) >= textLength) {
	    break;
	}
	lob = tob;		/* last mark object */
	lobChars = self->lines[line-1].nChars;
    }
    if (reformat && (self->zapRest = zapRest))  {
	/* consider zapping the rest of the screen */

	if (cury < self->clearFrom)  {
	    textview_SetTransferMode(self, graphic_COPY);
	    rectangle_SetRectSize(&tempSrcRect, 0, cury, textview_GetLogicalWidth(self), textview_GetLogicalHeight(self) - cury);
	    pat = textview_WhitePattern(self);
	    textview_FillRect(self,&tempSrcRect,pat);
	}
    }
    self->clearFrom = cury;

    self->nLines = line;

    /* now draw the cursor */

    changed =  ! (self->csxPos == csx && self->csyPos == csy
		   && self->cshPos == csh && self->csbPos == csb
		   && self->cexPos == cex && self->ceyPos == cey
		   && self->cehPos == ceh && self->cebPos == ceb);

    if (changed || ! cursorVisible)  {
	if (cursorVisible)  {
	    UpdateCursor(self, TRUE);
	}
	else
	    rectlist_ResetList();
	self->csxPos = csx;
	self->csyPos = csy;
	self->cshPos = csh;
	self->csbPos = csb;
	self->cexPos = cex;
	self->ceyPos = cey;
	self->cehPos = ceh;
	self->cebPos = ceb;
	UpdateCursor(self, FALSE);
    }

    self->displayLength = textLength;

    /* now check if we're supposed to force the cursor to be visible
		     contains expansion of doc_setmarkpos */

    if ((line = mark_GetPos(self->frameDot)) != -1) {
	/* line is now the position to start back from */

	if (! textview_Visible(self, line)) {
	    mark_SetPos(self->frameDot, -1);		/* prevent recursive loops */
#if 1

	    if(self->pixelsReadyToBeOffTop) {
		textview_SetTopPosition(self, line);
	    }
	    t = textview_MoveBack(self, line, textview_GetLogicalHeight(self)/3,
				  textview_MoveByPixels, 0, 0);			/* our new top */
	    textview_SetTopOffTop(self, t, self->pixelsComingOffTop);			/* adjust the frame */
#else
	    textview_SetTopPosition(self, (line>=10)?line-10: line);
#endif	    
	}
	else {
	    mark_SetPos(self->frameDot, -1);
	}
    }

    /* Not the greatest place for this but... */                                             
    if (self->hasInputFocus) {

	long mask;
	boolean readonly = (textview_GetDotPosition(self) < text_GetFence(Text(self))) || text_GetReadOnly(Text(self));

	mask = ((mark_GetLength(self->dot) > 0) ? textview_SelectionMenus : textview_NoSelectionMenus) |
	  (readonly ? 0 : textview_NotReadOnlyMenus);

	if (menulist_SetMask(self->menus, mask)) {
	    if (readonly || !(alwaysDisplayStyleMenus || (mask & textview_SelectionMenus)))
		menulist_UnchainML(self->menus, (void *)textview_StyleMenusKey);
	    else
		menulist_ChainBeforeML(self->menus, self->styleMenus, (void *)textview_StyleMenusKey);
	    textview_PostMenus(self, self->menus);
	}
    }
    self->pixelsShownOffTop = self->pixelsReadyToBeOffTop;
}

void textview__FullUpdate(struct textview *self, enum view_UpdateType type, long left, long top, long width, long height)
{
    long dotpos;
    long line = 0;
    if (self->showColorStyles == -1) {
	/* We defer this until now because we want the default to
	 * depend on the display class, which is not known until
	 * this point.
	 */
	self->showColorStyles = environ_GetProfileSwitch("showcolorstyles",
				!(textview_DisplayClass(self) & graphic_Monochrome));
    }
    switch(type){
	case view_Remove:
	    while (line < self->nLines)  {
		if(self->lines[line].containsView)
		    textview_ViewMove(self,&self->lines[line],textview_REMOVEVIEW); 
		line++;
	    } 
	    break;
	case view_MoveNoRedraw:
	    while (line < self->nLines)  {
		if(self->lines[line].containsView)
		    textview_ViewMove(self,&self->lines[line],textview_MOVEVIEW); 
		line++;
	    }
	    break;
	default:
	    self->force = TRUE;
	    if (textview_Visible(self, (dotpos = textview_GetDotPosition(self))))  {
		textview_FrameDot(self, dotpos); 
	    }
	    self->csxPos = BADCURPOS;		/* Indication that cursor is not visible */
	    self->cexPos = BADCURPOS;
	    DoUpdate(self, TRUE);
    }
}

void textview__WantNewSize(struct textview *self, struct view *requestor)
{
    self->force = TRUE;
    self->csxPos = BADCURPOS;		/* Indication that cursor is not visible */
    self->cexPos = BADCURPOS;
    if((struct view *)self!=requestor) { /* if a child wants a new size, just redraw */
	textview_WantUpdate(self, self);
    } else super_WantNewSize(self, requestor); /* if self wants a new size pass up the request */
}

void textview__Update(struct textview *self)
{
    DoUpdate(self, TRUE);
}

struct view * textview__Hit(struct textview *self, enum view_MouseAction action, long x, long y, long numberOfClicks)
{
    int newPos, oldPos, oldLen, oldMid;
    struct view *vptr;
    static int leftStartPos;
    static int rightStartPos;
    int redrawCursor = 0;
    long newLeftPos ;
    long newRightPos;

    newPos = textview_Locate(self,x,y, &vptr);
    oldPos = mark_GetPos(self->dot);
    oldLen = mark_GetLength(self->dot);
    oldMid = oldPos + (oldLen>>1);

    if (action == view_LeftDown || action == view_RightDown)  {
	if (vptr != NULL)
	    return view_Hit(vptr, action, view_EnclosedXToLocalX(vptr, x), view_EnclosedYToLocalY(vptr, y), numberOfClicks);

 	if (! self->hasInputFocus)
	    textview_WantInputFocus(self, self);
	
	textview_GetClickPosition(self, newPos, numberOfClicks, action, oldPos, oldPos + oldLen, &newLeftPos, &newRightPos);
	
	if (action == view_LeftDown)  {
	    mark_SetPos(self->dot, newLeftPos);
	    HandleSelection(self,newRightPos - newLeftPos);
	  /*  mark_SetLength(self->dot, newRightPos - newLeftPos); */
	    leftStartPos = newLeftPos;
	    rightStartPos = newRightPos;
	}
	else  {
	    int lPos = oldPos;
	    int rPos = oldPos + oldLen;
	    
	    if (numberOfClicks == 1)  {
		if (newPos < oldMid)  {
		    leftStartPos = rightStartPos = rPos;
		    lPos = newLeftPos;
		}
		else  {
		    leftStartPos = rightStartPos = lPos;
		    rPos = newRightPos;
		}
	    }
	    else  {
		if (newPos < leftStartPos)
		    lPos = newLeftPos;
		else if (newPos >= leftStartPos)    /* with multiple right clicks left and right start pos are the same */
		    rPos = newRightPos;
	    }
		
	    mark_SetPos(self->dot,lPos);
	    HandleSelection(self, rPos - lPos);
	    /* mark_SetLength(self->dot, rPos - lPos); */
	}
    }
    else {
	if (action == view_LeftMovement || action == view_RightMovement || (numberOfClicks == 1 && (action == view_LeftUp || action == view_RightUp)))  {
	    int lPos = leftStartPos;
	    int rPos = rightStartPos;

	    textview_GetClickPosition(self, newPos, numberOfClicks, action, leftStartPos, rightStartPos, &newLeftPos, &newRightPos);
	    redrawCursor = FALSE;

	    if (newPos <= leftStartPos)
		lPos = newLeftPos;
	    else if (newPos >= rightStartPos)  
		rPos = newRightPos;

	    if(rPos<lPos) {
		int t=rPos;
		rPos=lPos;
		lPos=t;
	    }
	    
	    if(action==view_LeftUp || action==view_RightUp) HandleSelection(self, rPos - lPos);
	    
	    if (lPos != oldPos || rPos != oldPos + oldLen)  {
		mark_SetPos(self->dot, lPos);
		mark_SetLength(self->dot, rPos - lPos);
		redrawCursor = TRUE;
	    }
	}
    }

    if (action == view_LeftMovement || action == view_RightMovement)  {
	if (redrawCursor)
	    DoUpdate(self, FALSE);
    }
    else
	textview_WantUpdate(self, self);

    return (struct view *) self;
}

void textview__ReceiveInputFocus(struct textview *self)
{
    self->hasInputFocus = TRUE;
    self->keystate->next = NULL;
    menulist_SetMask(self->menus, textview_NoMenus);
    textview_PostKeyState(self, self->keystate);
    if ( self->editor == VI )
	if ( self->viMode == COMMAND )
	    message_DisplayString(self, 0, "Command Mode");
        else
	    message_DisplayString(self, 0, "Input Mode");
    textview_WantUpdate(self, self);
}

void textview__LoseInputFocus(struct textview *self)
{
    self->hasInputFocus = FALSE;
    textview_WantUpdate(self, self);
}

void textview__SetDotPosition(struct textview *self, long newPosition)
{
    long len;

    if (newPosition < 0)
	newPosition = 0;
    else  {
	if (newPosition > (len = text_GetLength(Text(self))))
	    newPosition = len;
    }
    mark_SetPos(self->dot, newPosition);
    textview_WantUpdate(self, self);
	
}

void textview__SetDotLength(struct textview *self, long newLength)
{
    struct im *im=textview_GetIM(self);

    if (newLength < 0)
	newLength = 0;
    HandleSelection(self, newLength);
   /* mark_SetLength(self->dot, newLength); */
    textview_WantUpdate(self, self);

}

long textview__GetDotPosition(struct textview *self)
{
    return mark_GetPos(self->dot);
}

long textview__GetDotLength(struct textview *self)
{
    return mark_GetLength(self->dot);
}

long textview__GetTopPosition(struct textview *self)
{
    return mark_GetPos(self->top);
}

void textview__SetTopOffTop(struct textview *self, long newTopPosition, long pixelsOffTop)
{
    long len;
    long curTop;
    long curPixel;

    if (newTopPosition < 0) {
        newTopPosition = 0;
    }
    else  {
	if (newTopPosition > (len = text_GetLength(Text(self)))) {
            newTopPosition = len;
        }
    }
    
    if ((curTop = mark_GetPos(self->top)) != newTopPosition || pixelsOffTop != self->pixelsReadyToBeOffTop)  {
	long line;

        if (curTop != newTopPosition) {
            mark_SetPos(self->top, newTopPosition);
        }
        curPixel = self->pixelsReadyToBeOffTop;
        self->pixelsReadyToBeOffTop = pixelsOffTop;

	if (curTop < newTopPosition || ((curTop == newTopPosition) && (curPixel < pixelsOffTop)))  {
	    if (self->scroll != textview_ScrollBackward)   {
		line = textview_FindLineNumber(self, newTopPosition);
		if (line != -1)  {
		    self->scroll = textview_ScrollForward;
		    self->force = FALSE;
                    self->scrollLine = line;
		}
		else  {
		    self->scroll = textview_NoScroll;
                    self->force = TRUE;
		}
	    }
	    else {
                self->scroll = textview_MultipleScroll;
            }
	}
	else if (self->scroll != textview_ScrollForward)  {
	    if (self->scrollDist != -1)  {
		self->scroll = textview_ScrollBackward;
		self->force = FALSE;
	    }
	    else {
		self->force = TRUE;
		self->scroll = textview_NoScroll;
	    }
	}
	else {
            self->scroll = textview_MultipleScroll;
        }
    }
    if (self->scroll == textview_MultipleScroll)  {
	self->force = TRUE;
    }
    textview_WantUpdate(self, self);
}

void textview__SetTopPosition(struct textview *self, long newTopPosition)
{
    textview_SetTopOffTop(self, newTopPosition, 0);
}

void textview__SetBorder(struct textview *self, long xBorder, long yBorder)
{
    self->bx = xBorder;
    self->by = yBorder;
    
    /* Have to do some update here for resetting the border. */
}

void textview__SetEmbeddedBorder(struct textview *self, long xBorder, long yBorder)
{
    self->ebx = xBorder;
    self->eby = yBorder;
    
    /* Have to do some update here for resetting the border. */
}

long textview__CollapseDot(struct textview *self)
{
    long pos;
    struct im *im=textview_GetIM(self);
    
    pos = mark_GetPos(self->dot) + mark_GetLength(self->dot);
    mark_SetPos(self->dot, pos);
    HandleSelection(self, 0);
   /* mark_SetLength(self->dot, 0); */
    textview_WantUpdate(self, self);
    return pos;
}

void textview__GetClickPosition(struct textview *self, long position, long numberOfClicks, enum view_MouseAction action, long startLeft, long startRight, long *leftPos, long *rightPos)
{
    register int pos;
    register int testType;
    struct text *text = Text(self);
    register int textLength = text_GetLength(text);
    int extEnd;

    switch (numberOfClicks % 3)  {
	case 1:
	    /* Single Click */

	    *leftPos = *rightPos = position;
	    break;
	    
	case 2:
	    /* Double Click - word select */

	    if (position < textLength && (testType = charType(text_GetChar(text,position))) != WHITESPACE)  {
		/* Inside a word */

		pos = position - 1;
		for (pos = position - 1; pos >= 0 && charType(text_GetChar(text,pos)) == testType; pos--);
		*leftPos = ++pos;
		for (pos = position + 1; pos < textLength && charType(text_GetChar(text, pos)) == testType; pos++);
		*rightPos = pos;
	    }
	    else if (position > 0 && (testType = charType(text_GetChar(text, position - 1))) != WHITESPACE)  {
		/* Right of Word */

		*rightPos = position;
		for (pos = position - 1; pos >= 0 && charType(text_GetChar(text, pos)) == testType; pos--);
		*leftPos = pos + 1;
	    }
	    else  {
		/* No word either side */
		
		if (action == view_LeftDown)  {
		    *rightPos = *leftPos = position;
		}
		else  {
		    if (position <= startLeft)  {
			for (pos = position + 1; pos < textLength && charType(text_GetChar(text, pos)) != WORD; pos++);
			for (pos -= 1; pos >= 0 && isspace(text_GetChar(text, pos)) == 0; pos--);
			pos += 1;
		    }
		    else  {
			for (pos = position - 1; pos >= 0 && charType(text_GetChar(text, pos)) != WORD; pos--);
			for (pos += 1; pos < textLength && isspace(text_GetChar(text, pos)) == 0; pos++);
		    }
		    *rightPos = *leftPos = pos;
		}
	    }
	    break;

	case 0:
	    /* Triple Click - Paragraph select */

	extEnd = (action == view_LeftDown || action == view_LeftMovement);

	*leftPos = text_GetBeginningOfLine(text, position);
	pos = text_GetEndOfLine(text, position);
	*rightPos = (extEnd && pos < textLength) ? pos + 1 : pos;
	break;
    }
}

boolean textview__Visible(struct textview *self, long pos)
{
    register struct mark *lineMark;
    register long len = self->displayLength;
    register long endMark;
    
    if (self->nLines <= 0 || pos < mark_GetPos(self->top)) {
	return FALSE;
    }
    lineMark = self->lines[self->nLines - 1].data;
    if (pos < (endMark = mark_GetPos(lineMark) + self->lines[self->nLines-1].nChars))
	return TRUE;
    if (pos == len)  {
	if (pos == mark_GetPos(lineMark))
	    return (self->nLines != 1 || textview_PrevCharIsNewline(Text(self), pos));
	else if (pos == endMark)
	    /* pos falls exactly at the end of the last laid-out line, but
	       that line's own bottom edge (y+height) may still run past the
	       viewport -- true for a single embedded view taller than the
	       screen, only partly scrolled into view. Position containment
	       alone isn't enough; also require the line's bottom actually
	       fit on screen, mirroring getinfo()'s identical check. */
	    return !textview_PrevCharIsNewline(Text(self), pos) &&
		((textview_GetLogicalHeight(self) - (2*BY)) >=
		 self->lines[self->nLines-1].y + self->lines[self->nLines-1].height);
    }
    return FALSE;
}

long textview__Locate(struct textview *self, long x, long y, struct view **foundView)
{
    register long i;
    register long textLength = text_GetLength(Text(self));
    register long end = self->nLines;
    struct formattinginfo info;
    long pos;
    long by, bx;

    if(self->aLines > end && self->lines[end].containsView) end++; 
    if (foundView)
        *foundView = NULL;

    if (y < self->lines[0].y)   /* Tamper with clicks in top margin */
        y = self->lines[0].y;

    by = (self->hasApplicationLayer) ? self->by : self->eby;
    bx = (self->hasApplicationLayer) ? self->bx : self->ebx;

    for (i = 0; i < end ; i++)  { 
 	if (y >= self->lines[i].y && y < self->lines[i].y + self->lines[i].height)  {
	    pos = textview_LineRedraw(self, textview_GetPosition, self->lines[i].data, bx, by, textview_GetLogicalWidth(self) - 2 * bx, textview_GetLogicalHeight(self) - 2 * by, x, NULL, NULL, &info);
	    if (foundView)
	        *foundView = info.foundView;
	    return pos;
	}
	if (mark_GetPos(self->lines[i].data) + self->lines[i].nChars >= textLength)
	    return textLength;
    }

    if (i == 0)
	return textLength;
    else
	return mark_GetPos(self->lines[i - 1].data) + self->lines[i - 1].nChars;
}

void textview__GetTextSize(struct textview *self, long *width, long *height)
{
    long by = (self->hasApplicationLayer) ? self->by : self->eby;
    long bx = (self->hasApplicationLayer) ? self->bx : self->ebx;

    *width = textview_GetLogicalWidth(self) - 2 * bx;
    *height = textview_GetLogicalHeight(self) - 2 * by;
}

static long CalculateBltToTop(struct textview *self, long pos, long *distMoved, long *linesAdded)
{
    struct mark *tm;
    struct text *text = Text(self);
    long textLength = text_GetLength(text);
    long vxs, vys, length;
    long height, tpos;
    register int tp;
    struct formattinginfo info;

    textview_GetTextSize(self, &vxs, &vys);
    tm = mark_New();
    tpos = textview_GetTopPosition(self);

    if (pos > textLength) {
	pos = textLength;
    }

    tp=ReverseNewline(text, pos);

    /* tp is -1 if paragraph starts document */

    tp++;                   /* skip forward to first char in paragraph */

    while (TRUE)  {         /* scan lines in the paragraph */
	mark_SetPos(tm, tp); /* start mark at paragraph start */
	height = textview_LineRedraw(self, textview_GetHeight, tm, 0, 0, vxs, vys, 0, NULL, NULL, &info);
	RecordScrollWeight(self, tp, height, info.foundView != NULL);
	length = info.lineLength;

	/* handle stopping prematurely at end of file, instead of at a newline */

	if (tp+length == textLength && !textview_PrevCharIsNewline(text, textLength)) length++;

	if (pos == tp || (pos >= tp && pos < tp+length))  {
	    /* we've stumbled over pos again */
	    long accumHeight = height;
	    long numLines = 1;
	    long newTop = tp;

	    while (TRUE)  {
		tp += length;
		mark_SetPos(tm,tp);		/* start mark at paragraph start */
		height = textview_LineRedraw(self, textview_GetHeight, tm, 0, 0, vxs, vys, 0, NULL,NULL, &info);
		RecordScrollWeight(self, tp, height, info.foundView != NULL);
		length = info.lineLength;
		if (tp+length == textLength && !textview_PrevCharIsNewline(text, textLength)) length ++;
		if (tpos == tp || (tpos >= tp && tpos < tp+length))  {
		    if (distMoved != NULL)  {
			*distMoved = accumHeight + self->pixelsReadyToBeOffTop;
		    }
		    if (linesAdded != NULL)  {
			*linesAdded = numLines;
		    }
		    mark_Destroy(tm);
		    return newTop;
		}

		accumHeight += height;
		numLines++;

		if (accumHeight >= vys)  {
		    if (distMoved != NULL)  {
			*distMoved = -1;
		    }
		    if (linesAdded != NULL)  {
			*linesAdded = -1;
		    }
		    mark_Destroy(tm);
		    return newTop;
		}
	    }
	}
	tp += length;
    }
}

#define PSEUDOLINEHEIGHT 24
#define VIEWTOOSMALL 36
#define PLines(height) (((height) == 0) ? 0 : (((height) <= VIEWTOOSMALL) ? 1 : \
    ((height) + PSEUDOLINEHEIGHT - 1) / PSEUDOLINEHEIGHT))

/*
  Return the pos that would be at the top of the screen
  if we were to move pos down by units.
*/

static long BackSpace(struct textview *self, long pos, long units, enum textview_MovementUnits type, long *distMoved, long *linesAdded)
{
    struct mark *tm;
    long lastn [MAXPARA];
    long posn[MAXPARA];
    long pseudo[MAXPARA];
    struct text *text = Text(self);
    long textLength = text_GetLength(text);
    long pp, vxs, vys, length, realLength;
    long totalHeight, height, accumHeight, numLines;
    long pseudoLines;
    long plines;
    register int tp, px;
    struct formattinginfo info;
    long ounits = units;

    /* try to fail safe */
    if (linesAdded) {
	*linesAdded = 0;
    }

    if(distMoved) {
	*distMoved = 0;
    }
    
    if (type == textview_MoveByPseudoLines) {
        units -= PLines(self->pixelsReadyToBeOffTop);
        if (units <= 0) {
            self->pixelsComingOffTop = self->pixelsReadyToBeOffTop - ounits * PSEUDOLINEHEIGHT;
            if (self->pixelsComingOffTop < PSEUDOLINEHEIGHT / 2) {
                self->pixelsComingOffTop = 0;
            }
            if (distMoved) {
                *distMoved = self->pixelsReadyToBeOffTop - self->pixelsComingOffTop;
            }
            return pos;
        }
    }

    if (type == textview_MoveByPixels) {
        /* Fast-path shortcut for a backward move small enough to stay
           within the current top line, just revealing more of what's
           already scrolled off above it (self->pixelsReadyToBeOffTop)
           -- avoids the real paragraph-by-paragraph search below.

           A second, now-removed check used to run first:
             units -= (self->lines[0].y + self->lines[0].height);
             if (units <= 0) return pos;
           self->lines[0].y is negative exactly when the top line is
           itself partially scrolled into (y == -pixelsReadyToBeOffTop,
           roughly), so y+height computed "how much of this line's
           content remains below the current scroll position" -- i.e.
           the FORWARD remaining extent, not anything relevant to a
           BACKWARD move. For ordinary short lines this fires so
           rarely (a page-up request is essentially always bigger than
           one line's height) it was effectively dead code; for a
           single embedded view many screens tall -- routine for this
           renderer's HTML tables, never seen before it -- y+height is
           enormous and swallowed the entire request every time,
           silently no-op'ing every backward page/frame-into-view call
           while scrolled partway into it (confirmed live 2026-08-18:
           `^V` forward worked, Meta-V back did nothing; "go to end"
           landed wrong too, both going through this same function via
           FrameDot's own textview_MoveBack call). The remaining check
           below is the correct, sufficient shortcut on its own --
           pixelsReadyToBeOffTop already IS "how far this line is
           scrolled into," the only distance a within-this-line
           backward move can cover.

           Guarded on pos == textview_GetTopPosition(self): pixelsReadyToBeOffTop
           describes how far the CURRENT top line is scrolled into -- it
           says nothing about an unrelated pos passed in. The scrollbar's
           bottom-endzone jump (setframe(), textview's endzone()) computes
           its target in two MoveBack calls from the same pos
           (text_GetLength()): first a units==0 realign, then this
           MoveByPixels backward offset -- neither call is moving from the
           current top, both move from a freshly computed target line near
           the end of the document, so this shortcut's premise doesn't
           apply to them. A held click re-issues the identical request via
           the endzone auto-repeat timer before mouse-up; by then the first
           click's landing already set pixelsReadyToBeOffTop to
           (approximately) the very distance being requested again, so the
           shortcut fired and returned pos == text_GetLength() completely
           unmoved -- landing the scroll top exactly at end-of-document, a
           position with no line of its own, which (as with the
           MoveForward bug above) renders as a blank white screen.
           Confirmed live 2026-08-18: holding the scrollbar's bottom
           endzone button past one repeat tick (~100ms, the default
           ButtonRepeatTime) reproduced this every time. */
        if (pos == textview_GetTopPosition(self)) {
            units -= self->pixelsReadyToBeOffTop;
            if (units <= 0) {
                self->pixelsComingOffTop = self->pixelsReadyToBeOffTop - ounits;
                if (distMoved) {
                    *distMoved = self->pixelsReadyToBeOffTop - self->pixelsComingOffTop;
                }
                return pos;
            }
        }
    }

    self->pixelsComingOffTop = 0;
    accumHeight = -self->pixelsReadyToBeOffTop;

    textview_GetTextSize(self, &vxs, &vys);
    tm=mark_New();
    pp = pos;
    numLines = 0;

    for (;;) {

	/* first move tp back to start of paragraph */
	if (pp > 0) {
	    tp=ReverseNewline(text, pp);
	}
        else tp = -1;   /* tp is -1 if paragraph starts document */

	tp++;		/* skip forward to first char in paragraph */
	pp = tp;	/* remember paragraph start */
	px = 0;
	totalHeight = 0;
        pseudoLines = 0;
	while (px < MAXPARA) {		/* scan lines in the paragraph */
	    lastn[px] = tp;
            posn[px] = totalHeight;
            pseudo[px] = pseudoLines;
	    mark_SetPos(tm, tp);	/* start mark at paragraph start */
	    height = textview_LineRedraw(self, textview_GetHeight, tm, 0, 0, vxs, vys, 0, NULL, NULL, &info);
	    RecordScrollWeight(self, tp, height, info.foundView != NULL);

	    length = info.lineLength;
            realLength = length;
            plines = PLines(height);

	    /* handle stopping prematurely at end of file, instead of at a newline */

	    if (tp+length == textLength && !textview_PrevCharIsNewline(text, textLength)) length++;
	    if (pos == tp || (pos >= tp && pos < tp+length)) {
		/* we've stumbled over pos again */

		if (type == textview_MoveByLines) {
		    if (units <= px)  {
			long target = lastn[px-units];
			/* pos sitting exactly at tp+realLength (one past
			   this line's REAL content) only matched this line
			   at all because of the length++ padding just
			   above -- it isn't genuinely inside the line, it's
			   at true end-of-file. That padding is needed so
			   pos==textLength always matches *some* line (or
			   this whole scan can fail to terminate cleanly),
			   but blindly snapping such a pos back to tp (this
			   line's start) is wrong for a units==0 "just
			   realign, don't actually move" caller -- textv.c's
			   NextScreenCmd/PrevScreenCmd (txtvcmv.c) use
			   exactly that units==0 call to keep the top
			   position line-aligned before each page. Normally
			   harmless (real lines are short), but when the
			   document's very last line is a single embedded
			   view taller than one screen -- routine for our
			   lset-based HTML table rows, rare for hand-typed
			   text -- this silently undid legitimate forward
			   scroll progress: MoveForward correctly finishes
			   climbing across the tall view and reaches
			   pos==textLength, then the very next keystroke's
			   align step snapped straight back to the view's
			   start, discarding the accumulated
			   pixelsReadyToBeOffTop and restarting the climb --
			   an infinite loop through the same content.
			   Confirmed live via instrumented tracing,
			   2026-08-16 (project memory has the trace). Fix:
			   only for this exact boundary case, treat pos as
			   already aligned (return it unchanged) instead of
			   snapping to tp. */
			if (units == 0 && pos == tp + realLength) {
			    target = pos;
			}
			mark_Destroy(tm);
			if (distMoved) {
			    *distMoved = accumHeight + totalHeight - posn[px-units];
			}
			if (linesAdded) {
			    *linesAdded = numLines - (px - units);
                        }
			return target;
		    }
		    else break;
                }
                else if (type == textview_MoveByPseudoLines) {
                    if (units <= pseudoLines) {
                        int i;

			mark_Destroy(tm);

                        for (i = 0; (i <= px) && (pseudoLines - pseudo[i] >= units); i++) {
                        }

                        self->pixelsComingOffTop = (pseudoLines - pseudo[i-1] - units) * PSEUDOLINEHEIGHT;

			if (distMoved) {
			    *distMoved = accumHeight + totalHeight - posn[i-1] - self->pixelsComingOffTop;
			}
			if (linesAdded) {
                            *linesAdded = numLines - (i-1);
                        }
			return lastn[i-1];
                    }
                    else break;
                }
		else { /* textview_MoveByPixels */
		    if (units <= totalHeight) {
                        int i;

			mark_Destroy(tm);
                        for (i = 0; (i <= px) && (totalHeight - posn[i] >= units); i++) {
                        }
                        self->pixelsComingOffTop = totalHeight - posn[i-1] - units;
                        if (self->pixelsComingOffTop < PSEUDOLINEHEIGHT) {
                            self->pixelsComingOffTop = 0;
                        }
			if (distMoved) {
                            *distMoved = accumHeight + totalHeight - posn[i-1] - self->pixelsComingOffTop;
                        }
			if (linesAdded) {
                            *linesAdded = numLines - (i-1);
                        }
			return lastn[i-1];
		    }
		    else if (pos == tp + realLength && units <= totalHeight + height) {
			/* Not enough height in the lines strictly BEFORE this
			   one (totalHeight), but pos sits at this line's own
			   end and this line's own height covers the rest of
			   the units budget. Land inside this line itself,
			   offsetting down from its top by (height - units
			   remaining). Without this, a units budget smaller
			   than a single oversized line (an embedded view
			   taller than the screen, routine for this renderer's
			   HTML tables) falls through the loop entirely; for
			   the first line of the first paragraph -- nothing to
			   fall back to -- that clamps the result to absolute
			   position 0 with zero pixel offset, discarding the
			   whole backward-move budget instead of landing
			   partway into the line as requested. Confirmed live
			   2026-08-18 via instrumented tracing: `Escape >`
			   (go to end) landed at the very top of the document
			   instead of leaving the target position framed near
			   the bottom. */
			mark_Destroy(tm);
			self->pixelsComingOffTop = height - (units - totalHeight);
			if (self->pixelsComingOffTop < PSEUDOLINEHEIGHT) {
			    self->pixelsComingOffTop = 0;
			}
			if (distMoved) {
			    *distMoved = accumHeight + units;
			}
			if (linesAdded) {
			    *linesAdded = numLines;
			}
			return tp;
		    }
		    else break;
		}
            }

	    if (tp >= textLength) break;
	    tp += length;
	    totalHeight += height;
	    numLines++;
            px++;
            pseudoLines += plines;
        }

	/*
          Here we've moved back px lines, but still haven't made it.
	  Move back one more paragraph and try again.
        */

	accumHeight += totalHeight;
	if (pp <= 0)  {
	    mark_Destroy(tm);
	    if (distMoved) {
                *distMoved = accumHeight;
            }
	    if (linesAdded) {
                *linesAdded = numLines;
            }
	    return 0;	/* can't go any farther */
	}

	if (type == textview_MoveByLines) {
            units -= px;
        }
        else if (type == textview_MoveByPseudoLines) {
            units -= pseudoLines;
        }
	else {
            units -= totalHeight;
        }
	pos = pp;
	pp--;	/* looking at LF that terminated preceding para */
    }
}

long textview__MoveBack(struct textview *self, long pos, long units, enum textview_MovementUnits type, long *distMoved, long *linesAdded)
{
    long accumHeight = 0;
    long numLines = 0;
    long newPos;

    if (units < 0) return pos;
    
    if (pos < textview_GetTopPosition(self)) {
	/* Get the distance that we need to blt in order to move
	 pos to the top of the view.  This only needs to be 
	 calculated when we are above the top position. */

	pos = CalculateBltToTop(self, pos, &accumHeight, &numLines);
    }

    /* Now determine how much space we need to add in if we want to move
	pos down by units */

    newPos = BackSpace(self, pos, units, type, distMoved, linesAdded);

    if (distMoved) {
	if (accumHeight >= 0) {
	    *distMoved += accumHeight;
	}
	else {
	    *distMoved = -1;
	}
    }
    if (linesAdded) {
	if (accumHeight >= 0) {
	    *linesAdded += numLines;
	}
	else {
	    *linesAdded = -1;
	}
    }

    return newPos;
}

long textview__MoveForward(struct textview *self, long pos, long units, enum textview_MovementUnits type, long *distMoved, long *linesAdded)
{
    struct mark *tm;
    long vxs, vys;
    register int i;
    struct formattinginfo info;
    long viewHeight;
    long textlen = text_GetLength(Text(self));

    if (units < 0) return pos;
    tm = mark_New();
    textview_GetTextSize(self, &vxs, &vys);
    self->pixelsComingOffTop = self->pixelsReadyToBeOffTop;
    if (type == textview_MoveByPixels) {
        i = -self->pixelsReadyToBeOffTop;
    }
    else {
        i = 0;
    }

    while (i < units)  {
        mark_SetPos(tm, pos);
        viewHeight = textview_LineRedraw(self, textview_GetHeight, tm, 0, 0, vxs, vys, 0, NULL, NULL, &info);
        RecordScrollWeight(self, pos, viewHeight, info.foundView != NULL);
        if (type == textview_MoveByPseudoLines && viewHeight > VIEWTOOSMALL) {
            long pixelsLeft = viewHeight - self->pixelsComingOffTop;
            long pseudoLines = (pixelsLeft + PSEUDOLINEHEIGHT - 1) / PSEUDOLINEHEIGHT;

            if (pos + info.lineLength == textlen && pixelsLeft <= VIEWTOOSMALL) {
                break; /* there's nothing left; give up */
            }

            if (pseudoLines <= units - i) {
                /* gobble up the whole line, and keep going */
                pos += info.lineLength;
                i += pseudoLines;
                self->pixelsComingOffTop = 0;
            }
            else {
                /* take what we need and stop */
                self->pixelsComingOffTop += (pixelsLeft / pseudoLines) * (units - i);
                break;
            }
        }
        else if (type == textview_MoveByPixels) {
            if (pos + info.lineLength == textlen) {
                /* This is the document's last line, and gobbling it
                   whole (the ordinary branch below) would drive pos to
                   textlen -- a position with no line of its own.
                   DoUpdate/GenerateLineItems has no fallback for that:
                   it formats one empty line there and whites out the
                   rest of the view, i.e. a blank screen, discarding
                   whatever of this line was still showing. Instead,
                   stay on this line and reveal further into it (same
                   direction/units math as the plain "else" branch
                   below), capped so its bottom never scrolls past the
                   viewport's bottom edge -- once that cap is reached
                   there's genuinely nothing left to page into, so
                   further calls become a no-op instead of blanking.
                   Confirmed live 2026-08-18: ^v right after `Escape >`
                   landed at the true end-of-document produced a
                   totally white window with the caret in the upper
                   left. */
                long viewportHeight = textview_GetLogicalHeight(self) - self->by;
                long cap = viewHeight - viewportHeight;
                long newOffTop;

                if (cap < 0) {
                    cap = 0;
                }
                /* (units - i) alone is already the correct distance from
                   the true top of this line to the new scroll position --
                   i was seeded to -self->pixelsComingOffTop above
                   specifically so it would net out that already-scrolled
                   amount; walking whole lines before reaching this branch
                   also resets self->pixelsComingOffTop to 0 along the way,
                   which is why adding it back in here happened to be
                   harmless in the ordinary multi-line case. But when the
                   very first line is already the last one -- this
                   renderer's whole-message-as-one-giant-embedded-view
                   shape, routine for HTML mail -- this branch is reached
                   on the loop's first iteration, before any such reset,
                   with self->pixelsComingOffTop still holding the same
                   value i was seeded from. Adding it a second time here
                   doubled the effective advance on every repeated forward
                   page (confirmed live via lldb 2026-09-13: pixelsComingOffTop
                   climbed 0->367->1101->2569->5505 for four successive
                   367px page requests, matching 2*prev+367 exactly, capping
                   after 5 presses instead of the ~18 a real per-press
                   linear advance would need) -- the forward-paging-stops-
                   short bug (porting-assessment.md item r.). */
                newOffTop = units - i;
                if (newOffTop > cap) {
                    newOffTop = cap;
                }
                if (newOffTop < self->pixelsComingOffTop) {
                    newOffTop = self->pixelsComingOffTop;
                }
                self->pixelsComingOffTop = newOffTop;
                break;
            }
            else if (viewHeight < VIEWTOOSMALL || viewHeight <= units - i) {
                pos += info.lineLength;
                i += viewHeight;
                self->pixelsComingOffTop = 0;
            }
            else {
                self->pixelsComingOffTop += units - i;
                break;
            }
        }
        else {
            pos += info.lineLength;
            i++;
            self->pixelsComingOffTop = 0;
            if (pos == textlen) break;
        }
    }
    mark_Destroy(tm);
    return pos;
}

#define DEFAULTHEIGHT 20
static int CalculateLineHeight(struct textview *self)
{

    struct style *defaultStyle;
    struct fontdesc *defaultFont;
    struct FontSummary *fontSummary;
    char fontFamily[256];
    long refBasis, refOperand, refUnit, fontSize;
    long by = (self->hasApplicationLayer) ? self->by : self->eby;

    if ((defaultStyle = textview_GetDefaultStyle(self)) == NULL)
        return DEFAULTHEIGHT;
    style_GetFontFamily(defaultStyle, fontFamily, sizeof (fontFamily));
    style_GetFontSize(defaultStyle, (enum style_FontSize * ) &refBasis, &fontSize);
    style_GetFontScript(defaultStyle, (enum style_ScriptMovement *) &refBasis, &refOperand, (enum style_Unit *) &refUnit);
    defaultFont = fontdesc_Create(fontFamily, refOperand, fontSize);
    if ((fontSummary = fontdesc_FontSummary(defaultFont, textview_GetDrawable(self))) == NULL)
        return DEFAULTHEIGHT;
    return fontSummary->maxHeight + by + by;
}

#define MAXWIDTH 1024
#define MINWIDTH 125
#define UNLIMITED 3000000
enum view_DSattributes textview__DesiredSize(struct textview *self, long width, long height, enum view_DSpass pass, long *desiredwidth, long *desiredheight)
{
    struct mark *tm;
    long  len, txheight,totheight,curx,cury,xs,ys = 0,maxlines,newwidth,sw;
    struct formattinginfo info;
    long pos = 0;
    long bx, by;

    if(Text(self) == NULL || ((len = text_GetLength(Text(self)))== 0)) {
	*desiredwidth = width;
	*desiredheight = CalculateLineHeight(self);
	return(view_HeightFlexible | view_WidthFlexible);
    }
    sw = width;
    totheight = 0;
    by = (self->hasApplicationLayer) ? self->by : self->eby;
    bx = (self->hasApplicationLayer) ? self->bx : self->ebx;

    cury = by;
    curx = bx;

    tm = mark_New();
    switch(pass){
	case view_HeightSet:
	    ys = height - 2 * by;
	    xs = UNLIMITED;
	    txheight = textview_LineRedraw(self, textview_GetHeight, tm, curx,cury,xs,ys, 0, NULL,NULL, &info);
	    if(txheight){
		maxlines = ys / txheight ;

	    }
	    else maxlines = 0;
	    if(maxlines) newwidth = info.totalWidth / maxlines;
	    else newwidth = (width > MINWIDTH && width < MAXWIDTH) ? width :MAXWIDTH;
	    if(newwidth	< MINWIDTH) 
		/* this really should be the width of the longest word in the text */
		newwidth = MINWIDTH;
	    width = newwidth;
	    break;
	case view_NoSet:
	    if(width <= 0 || width > 1024) width = 256;
	case view_WidthSet:
	    ys = UNLIMITED;
	    break;
    }
    xs = width - 2 * bx;
    while (pos < len )  {
	mark_SetPos(tm,pos);
	txheight = textview_LineRedraw(self, textview_GetHeight, tm, curx,cury,xs,ys, 0, NULL,NULL, &info);
	RecordScrollWeight(self, pos, txheight, info.foundView != NULL);
	 if(info.lineLength == 0){
	     *desiredwidth = sw;
	     *desiredheight = CalculateLineHeight(self);
	     return(view_HeightFlexible | view_WidthFlexible);
	 }
	 if(pass == view_HeightSet && totheight + txheight > ys ){
	     if(pos == 0 || width >= MAXWIDTH) break;
#if 0
	     newwidth = (width * len)/ pos ;
	     if(newwidth == width ) width += 10;
	     else width = newwidth;
#endif /* 0 */
	     width += 10;
	     if(width > MAXWIDTH) width = MAXWIDTH;
	     pos = 0;	
	     xs = width - 2 * bx;
	     totheight = 0;
	     continue;
	 }
	pos += info.lineLength;
	totheight += txheight;
    }
    mark_Destroy(tm);
    *desiredwidth = width;
    *desiredheight = totheight + by + by;
    return(view_HeightFlexible | view_WidthFlexible);
}

void textview__GetOrigin(struct textview *self, long width, long height, long *originX, long *originY)
{
  /* NB...GetOrigin assumes BX and BY are 0 and is used to insert inline text
children. */

    struct formattinginfo info;
    long lineHeight;

    lineHeight = textview_LineRedraw(self, textview_GetHeight, self->top, 0, 0, width,
                        height, 0, NULL, NULL, &info);
    RecordScrollWeight(self, mark_GetPos(self->top), lineHeight, info.foundView != NULL);
    *originY = info.lineAbove;
    *originX = 0;
}

void textview__FrameDot(struct textview *self, long pos)
{
    mark_SetPos(self->frameDot, pos);
}

long textview__FindLineNumber(struct textview *self, long pos)
{
    register struct linedesc *tl = self->lines;    
    register int i;
    register long len = text_GetLength(Text(self));
    register long endMark;
    
    for (i = 0; i < self->nLines; i++, tl++)  {
	if (pos >= mark_GetPos(tl->data))  {
	    if (pos < (endMark = mark_GetPos(tl->data) + tl->nChars))
		return i;
	    if (pos == len)  {
		if (pos == mark_GetPos(tl->data))  {
		    if (i != 0 || textview_PrevCharIsNewline(Text(self), pos))
			return i;
		}
		else if (pos == endMark && !textview_PrevCharIsNewline(Text(self), pos))
		    return i;
	    }
	}
    }
    return -1;
}

/* Elevator-drag scrollbar-position correction (2026-09-24, see
   html-scroll-plan.md's Step 3). `position()`'s coarse `pos<<FINESCROLL`
   component treats every character as occupying equal vertical space --
   true enough for ordinary text, badly wrong for a line whose one
   "character" is actually an embedded view many thousand pixels tall
   (an HTML table kept as one view so its own horizontal scroll stays in
   sync across rows, or a large raster in a plain .ez file -- confirmed
   both hit this). `scroll.c`'s elevator math only ever sees the total/
   seen/position values these functions report; it does correct, plain
   linear interpolation over whatever range it's given, so the fix
   belongs here, in what that range actually means -- not in scroll.c.

   No new field is needed on any dataobject to know a line's real pixel
   height: `textview_LineRedraw` already computes and holds it,
   generically, for any embedded view, the moment it lays that line out
   -- both the real paint path (DoUpdate) and the measurement-only walks
   (MoveByPixels/MoveByLines/MoveForward/MoveBack, which already drive
   paging, endzone jumps, and this file's own drag-decode walk in
   setframe below) go through the same formattinginfo/height computation.
   RecordScrollWeight is called from all of those sites (search this
   file for its name) the moment such a line is discovered -- covering
   plain .ez rasters too, not just HTML tables. What's missing without
   this is only somewhere for that number to survive the line scrolling
   back out of `self->lines[]`'s moving window; the per-textview
   scroll-weight blob (`struct scrollweightlist`, reached via
   `dictionary_LookUp`/`GetScrollWeights` above, not a `struct textview`
   field -- see that struct's own comment for why) is that index: a
   small, sorted list of {position, extraWeight}, recording where an
   already-computed height was seen, not a second computation of the
   number itself.

   `extraWeight` estimates, in the same units `position()` returns, how
   many *additional* coarse position-ticks a line's real pixel height
   deserves beyond the single tick it already gets for being "one
   character": scaled against CalculateLineHeight()'s real font metric
   and an assumed typical character density (SCROLLWEIGHT_CHARS_PER_LINE
   below), so a line behaves, position-space-wise, roughly as if it were
   as many ordinary characters tall as its real pixel height implies.
   This only needs to be roughly right, not exact -- `setframe`'s own
   pixel-accurate MoveByPixels walk (unchanged) does the final correction
   once handed a starting guess that's in the right neighborhood; today,
   without any of this, that starting guess can be wrong by the entire
   span of an oversized view, which is the actual bug. Known limitation,
   not yet exercised live: a single very large drag/page/jump that first
   discovers several oversized lines' worth of weight in one step will
   show up as a one-time elevator size/position correction at that
   moment, since discovery only happens as navigation actually reaches
   a line -- see html-scroll-plan.md's Step 3 for the full reasoning and
   why this is judged an acceptable, self-limiting edge case rather than
   something worth a heavier (e.g. eager whole-document) fix. */
/* Fallback only, used when real font/width metrics aren't available
   (see CalculateCharsPerLine below, added 2026-09-24 after a live test
   at a wide window showed the fix still undershooting: a fixed
   chars-per-line assumption doesn't scale with the view's actual
   width, so a wide window -- which packs more real characters into
   each ordinary line -- was making an oversized embedded view's real
   proportional share of the document look smaller than it truly is,
   reintroducing a smaller version of the same density-disproportion
   bug this feature exists to fix). */
#define SCROLLWEIGHT_CHARS_PER_LINE_FALLBACK 60

/* Dynamic estimate of how many ordinary characters occupy one typical
   text line AT THIS VIEW'S CURRENT WIDTH -- mirrors CalculateLineHeight
   just above/below in spirit (same font lookup pattern) but for width
   instead of height. Only needs to be roughly right, same as
   extraWeight itself (see RecordScrollWeight's own comment). */
static long CalculateCharsPerLine(struct textview *self)
{
    struct style *defaultStyle;
    struct fontdesc *defaultFont;
    struct FontSummary *fontSummary;
    char fontFamily[256];
    long refBasis, refOperand, refUnit, fontSize;
    long width;

    if ((defaultStyle = textview_GetDefaultStyle(self)) == NULL)
        return SCROLLWEIGHT_CHARS_PER_LINE_FALLBACK;
    style_GetFontFamily(defaultStyle, fontFamily, sizeof(fontFamily));
    style_GetFontSize(defaultStyle, (enum style_FontSize *) &refBasis, &fontSize);
    style_GetFontScript(defaultStyle, (enum style_ScriptMovement *) &refBasis, &refOperand, (enum style_Unit *) &refUnit);
    defaultFont = fontdesc_Create(fontFamily, refOperand, fontSize);
    if ((fontSummary = fontdesc_FontSummary(defaultFont, textview_GetDrawable(self))) == NULL
        || fontSummary->maxSpacing <= 0)
        return SCROLLWEIGHT_CHARS_PER_LINE_FALLBACK;
    width = textview_GetLogicalWidth(self);
    if (width <= 0) return SCROLLWEIGHT_CHARS_PER_LINE_FALLBACK;
    return width / fontSummary->maxSpacing;
}

/* dictionary_Insert/LookUp/Delete (dict.c) compare `id` by pointer
   identity (`dd->id == id`), not string content -- confirmed by reading
   dict.c after this feature's first live test recorded weights
   (RecordScrollWeight's own tracing showed ACCEPT) that getinfo() then
   never saw (AccumulatedWeightThrough kept returning 0). A string
   literal id doesn't work for that: with -fwritable-strings (this
   build's own flag), each textual occurrence of "scrollWeights" gets
   its own separate address, so Insert's literal never equals LookUp's
   from a different call site. The existing dictionary_* usage
   elsewhere in this file (LinkTree/FinalizeObject) never hits this,
   since it always keys on a real `struct viewref *`, never a string --
   this key needs the same treatment: one static sentinel, same address
   everywhere, every time. */
static char ScrollWeightsKey;

/* Fetch (or, if `create`, lazily allocate and register) this textview's
   scroll-weight blob via the dictionary_* mechanism -- see this file's
   forward-declaration-block comment on `struct scrollweightlist` for
   why it isn't just a struct field. */
static struct scrollweightlist *GetScrollWeights(struct textview *self, boolean create)
{
    struct scrollweightlist *swl =
        (struct scrollweightlist *) dictionary_LookUp(self, &ScrollWeightsKey);
    if (swl == NULL && create) {
        swl = (struct scrollweightlist *) malloc(sizeof(struct scrollweightlist));
        if (swl == NULL) return NULL;
        swl->entries = NULL;
        swl->nEntries = 0;
        swl->aEntries = 0;
        dictionary_Insert(self, &ScrollWeightsKey, (char *) swl);
    }
    return swl;
}

static void FreeScrollWeights(struct textview *self)
{
    struct scrollweightlist *swl = GetScrollWeights(self, FALSE);
    if (swl != NULL) {
        if (swl->entries != NULL) free(swl->entries);
        free(swl);
        dictionary_Delete(self, &ScrollWeightsKey);
    }
}

static void RecordScrollWeight(struct textview *self, long pos, long height, boolean containsView)
{
    long typicalHeight, charsPerLine, weightedUnits, extraWeight;
    struct scrollweightlist *swl;
    int i, insertAt;

    if (!containsView) return;
    typicalHeight = CalculateLineHeight(self);
    if (typicalHeight <= 0 || height <= typicalHeight * 3) {
        return;
    }
    charsPerLine = CalculateCharsPerLine(self);
    if (charsPerLine <= 0) charsPerLine = SCROLLWEIGHT_CHARS_PER_LINE_FALLBACK;

    weightedUnits = (long) (((double) height / (double) typicalHeight)
                             * (double) charsPerLine * (double) (1L << FINESCROLL));
    extraWeight = weightedUnits - (1L << FINESCROLL);
    if (extraWeight <= 0) return;

    swl = GetScrollWeights(self, TRUE);
    if (swl == NULL) return;

    for (i = 0; i < swl->nEntries; i++) {
        if (swl->entries[i].position == pos) {
            swl->entries[i].extraWeight = extraWeight;
            swl->entries[i].height = height;
            return;
        }
        if (swl->entries[i].position > pos) break;
    }
    insertAt = i;
    if (swl->nEntries >= swl->aEntries) {
        int newAlloc = swl->aEntries ? swl->aEntries * 2 : 8;
        struct scrollweight *newArray =
            (struct scrollweight *) malloc(newAlloc * sizeof(struct scrollweight));
        if (newArray == NULL) return;
        if (swl->entries != NULL) {
            memcpy(newArray, swl->entries, swl->nEntries * sizeof(struct scrollweight));
            free(swl->entries);
        }
        swl->entries = newArray;
        swl->aEntries = newAlloc;
    }
    memmove(&swl->entries[insertAt + 1], &swl->entries[insertAt],
            (swl->nEntries - insertAt) * sizeof(struct scrollweight));
    swl->entries[insertAt].position = pos;
    swl->entries[insertAt].extraWeight = extraWeight;
    swl->entries[insertAt].height = height;
    swl->nEntries++;
}

/* Encode side: total extraWeight for every recorded line at or before
   `pos`. Entries are sorted ascending, so this is a short linear scan. */
static long AccumulatedWeightThrough(struct textview *self, long pos)
{
    struct scrollweightlist *swl = GetScrollWeights(self, FALSE);
    long accum = 0;
    int i;

    if (swl == NULL) return 0;
    for (i = 0; i < swl->nEntries; i++) {
        if (swl->entries[i].position > pos) break;
        accum += swl->entries[i].extraWeight;
    }
    return accum;
}

/* Decode side: given an already-encoded scrollbar value, how much
   accumulated weight to undo before the FINESCROLL shift.

   CORRECTION (2026-09-24, found live-testing: "returns to the original
   offset... can never view any rendered content below what you see as
   the bottom"): the first version of this jumped straight from 0 to
   the entry's FULL extraWeight the instant `encoded` crossed the
   entry's own position -- but position()'s own `off` term for that one
   line only ever produces a NARROW real range (0..height/FINEGRID,
   typically a few hundred at most -- nowhere near extraWeight, which
   can be in the tens or hundreds of millions). That leaves a huge
   "dead zone" between the two: any encoded value scroll.c's linear
   `from_bar_to_range` interpolates into that zone (the vast majority
   of the track share this line's extraWeight earns it) doesn't
   correspond to anything position() would ever actually produce, and
   the old all-or-nothing subtraction collapsed every one of them back
   down to nearly the same raw value -- the drag equivalent of the
   click-side bug this whole feature was chasing, just self-inflicted
   this time. Fixed by treating the entry's full span
   (extraWeight + realOffMax + 1) as belonging to it, and proportionally
   rescaling wherever `encoded` falls in that span back down to the
   narrow real 0..realOffMax range position() actually produces --
   instead of an instant step. */
static long DecodeWeight(struct textview *self, long encoded)
{
    struct scrollweightlist *swl = GetScrollWeights(self, FALSE);
    long accumBefore = 0;
    int i;

    if (swl == NULL) return 0;
    for (i = 0; i < swl->nEntries; i++) {
        long realOffMax = swl->entries[i].height / FINEGRID;
        long entryStart, entrySpan, withinLine, desiredOff;
        if (realOffMax > FINEMASK) realOffMax = FINEMASK;
        entryStart = (swl->entries[i].position << FINESCROLL) + accumBefore;
        entrySpan = swl->entries[i].extraWeight + realOffMax + 1;
        if (encoded < entryStart) break;
        if (encoded < entryStart + entrySpan) {
            withinLine = encoded - entryStart;
            desiredOff = (entrySpan > 1) ? (withinLine * realOffMax) / (entrySpan - 1) : 0;
            return accumBefore + (withinLine - desiredOff);
        }
        accumBefore += swl->entries[i].extraWeight;
    }
    return accumBefore;
}

static long position(struct textview *self, long pos, struct linedesc *theline, long coord)
{
    long off;

    if (theline == NULL || !theline->containsView) {
        off = 0;
    }
    else {
	off = coord - theline->y;
	if(off<0) off= -(theline->y+theline->height);
    }
    /* with FINEGRID at 12, this will break with
       views taller than FINEMASK*FINEGRID pixels (see FINESCROLL's
       own 2026-09-23 comment, top of file) */
    off = (off + FINEGRID/2) / FINEGRID;

    /* off has to fit in FINESCROLL bits */
    if (off > FINEMASK) {
        off = FINEMASK;
    }
    /* pos - 1, not pos: this line's OWN weight (if any) must NOT be
       folded into its own encoding (see DecodeWeight's 2026-09-24
       comment -- its `entryStart` assumes exactly this), only into
       whatever comes after it. AccumulatedWeightThrough's other
       callers (getinfo's total/dot, not going through a specific
       line's `off`) are unaffected and still want the full "at or
       before" sum, unchanged. */
    return (pos << FINESCROLL) + off + AccumulatedWeightThrough(self, pos - 1);
}

static void getinfo(struct textview *self, struct range *total, struct range *seen, struct range *dot)
{
    struct linedesc *last;
    long lastpos;
    long lastcoord;
    long tl = text_GetLength(Text(self));

    total->beg = 0;
    total->end = (tl << FINESCROLL) + AccumulatedWeightThrough(self, tl);

    if (self->nLines > 0) {
        seen->beg = position(self, textview_GetTopPosition(self), &self->lines[0], BY);
        last = &self->lines[self->nLines - 1];
        lastpos = mark_GetPos(last->data);
        if ((textview_GetLogicalHeight(self) - (2*BY)) >= last->y + last->height) {
            lastcoord = last->y;
            lastpos += last->nChars;
            if (lastpos > tl) {
                lastpos = tl;
            }
        }
        else {
            lastcoord = textview_GetLogicalHeight(self) - (2*BY);
        }
        seen->end = position(self, lastpos, last, lastcoord);
    }
    else {
        seen->beg = (textview_GetTopPosition(self) << FINESCROLL)
                    + AccumulatedWeightThrough(self, textview_GetTopPosition(self));
        seen->end = seen->beg;
    }

    dot->beg = (textview_GetDotPosition(self) << FINESCROLL)
               + AccumulatedWeightThrough(self, textview_GetDotPosition(self));
    dot->end = dot->beg + (textview_GetDotLength(self) << FINESCROLL);
}

static long whatisat(struct textview *self, long numerator, long denominator)
{
    long coord;
    long pos;
    long linenum;

    coord = numerator * textview_GetLogicalHeight(self);
    coord /= denominator;

    pos = textview_Locate(self, 0, coord, NULL);
    linenum = textview_FindLineNumber(self, pos);

    return position(self, pos, (linenum >= 0) ? &self->lines[linenum] : NULL, coord);
}

/* move the text at position to be on screen at numerator/denominator */

static void setframe(struct textview *self, long position, long numerator, long denominator)
{
    long dist, lines, coord;
    long newpos;
    long off;
    long weight;
    boolean forceup = FALSE;

    coord = numerator * textview_GetLogicalHeight(self);
    coord /= denominator;

    /* Inverse of position()'s AccumulatedWeightThrough() addition (see
       that function's own 2026-09-24 comment) -- must be undone before
       the FINESCROLL shift below, or a position past an oversized
       embedded view would decode to a wildly wrong raw character
       position. */
    weight = DecodeWeight(self, position);
    position -= weight;
    off = (position & FINEMASK) * FINEGRID;
    position >>= FINESCROLL;

    newpos = textview_MoveBack(self, position, 0, textview_MoveByLines, 0, 0);
    if (newpos != position) {
        forceup = TRUE;
    }
    newpos = textview_MoveBack(self, newpos, coord, textview_MoveByPixels, &dist, &lines);
    /* CORRECTION (2026-09-25, elevator-drag-into-endzone-warps-to-top
       investigation, part 2 -- the first `forceup` fix below wasn't
       enough): a drag landing exactly at the document's end decodes to
       position==tl. The FIRST MoveBack (0 lines) leaves it unchanged
       (tl is already "a line start" as far as it's concerned), so
       forceup stays FALSE here. It's THIS SECOND MoveBack (0 pixels,
       byPixels) that actually snaps tl down to tl-1 -- the real last
       line, the oversized embedded view -- live-traced returning
       11485 from an input of 11486. forceup never saw that snap, so
       the `off` re-validation below still didn't run. Comparing
       against `position` (the byLines-snapped value, before this call)
       catches a change from EITHER MoveBack call. */
    if (newpos != position) {
        forceup = TRUE;
    }
    if (newpos < textview_GetTopPosition(self) && self->scroll != textview_ScrollForward && self->scroll != textview_MultipleScroll)  {
	if (dist == -1)  {
	    self->scrollDist = -1;
	    self->scroll = textview_MultipleScroll;
	}
	if (self->scrollDist == -1)  {
	    self->scrollDist = dist;
	    self->scrollLine = lines;
	}
	else  {
	    self->scrollDist += dist;
	    if (self->scrollDist >= textview_GetLogicalHeight(self))
		self->scrollDist = -1;
	    else
		self->scrollLine += lines;
	}
    }
    if (numerator != 0) {
        off = self->pixelsComingOffTop;
    }
    else {
        /* CORRECTION (2026-09-25, elevator-drag-into-endzone-warps-to-
           top investigation, ROOT CAUSE): this whole block re-validates
           `off` against the line MoveBack actually snapped to (newpos),
           via the `forceup || off > height` clamp below -- but it used
           to run only `if (off != 0)`. A drag landing exactly at the
           document's end decodes to position==tl with off==0 (see
           DecodeWeight/position()); MoveBack then snaps that down to
           tl-1 (the real last line, forceup=TRUE) -- an oversized
           embedded view. off==0 was correct for the ORIGINAL decoded
           (one-past-the-end, nonexistent) line, but says nothing valid
           about the DIFFERENT line just snapped to, and the `off!=0`
           guard skipped the very clamp (`forceup || off > height`) that
           exists to catch exactly this: forceup means the position we
           ended up at isn't the one we decoded, so off must be
           re-derived from the new line, not carried over unchecked.
           Traced live: off stayed 0, landing the view at the very TOP
           of the giant final line instead of its bottom. */
        if (off != 0 || forceup) {
            long line = textview_FindLineNumber(self, newpos);
            long height;

            if (line >= 0) {
                height = self->lines[line].height;
            }
            else {
                struct mark *tm = mark_New();
                long vxs, vys;
                struct formattinginfo info;

                mark_SetPos(tm, newpos);
                textview_GetTextSize(self, &vxs, &vys);
		height = textview_LineRedraw(self, textview_GetHeight, tm, 0, 0, vxs, vys, 0, NULL, NULL, &info);
		RecordScrollWeight(self, newpos, height, info.foundView != NULL);
		mark_Destroy(tm);
            }

            if (height < VIEWTOOSMALL) {
                off = 0;
            }
            else if (forceup || off > height) {
                off = height;
            }
        }
    }
    textview_SetTopOffTop(self, newpos, off);
}

static void endzone(struct textview *self, int end, enum view_MouseAction action)
{
    /* Every setframe() call below constructs its position argument by
       hand (pos<<FINESCROLL) rather than going through position()'s own
       encoding -- setframe now always undoes an accumulated-weight
       correction (see its own 2026-09-24 comment) on whatever it's
       given, so each of these must add that same correction here, or a
       hand-built value that never had it added would get incorrectly
       decoded as if it had. */
    if(action != view_LeftDown && action != view_RightDown) return;

    if (action == view_LeftDown &&
         (end == scroll_TOPENDZONE || end == scroll_BOTTOMENDZONE)) {
	    if (end == scroll_TOPENDZONE)
		setframe(self, AccumulatedWeightThrough(self, 0), 0, textview_GetLogicalHeight(self));
	    else {
		long tl = text_GetLength(Text(self));
		setframe(self, (tl<<FINESCROLL) + AccumulatedWeightThrough(self, tl),
			 textview_GetLogicalHeight(self)>>2, textview_GetLogicalHeight(self));
	    }
    }
    else {
        if (end == scroll_TOPENDZONE || end == scroll_MOTIFTOPENDZONE) {
		long newpos=textview_GetTopPosition(self);
		newpos = textview_MoveBack(self, newpos, 1, textview_MoveByLines, 0, 0);
		setframe(self, (newpos<<FINESCROLL) + AccumulatedWeightThrough(self, newpos),
			 0, textview_GetLogicalHeight(self));
	    } else {
                long newpos;
                if (self->nLines<2) return;
                newpos = mark_GetPos(self->lines[1].data);
		setframe(self, (newpos<<FINESCROLL) + AccumulatedWeightThrough(self, newpos),
			 0, textview_GetLogicalHeight(self));
	    }
    }
}

char * textview__GetInterface(struct textview *self, char *interfaceName)
{

    if (strcmp(interfaceName, "scroll,vertical") == 0)
        return (char *) &scrollInterface;
    return NULL;
}

boolean textview__InitializeClass(struct classheader *classID)
{
    extern struct keymap *textview_InitEmacsKeyMap(struct textview_classinfo *classInfo, struct menulist **normalMenus);
    extern struct keymap *textview_InitViInputModeKeyMap(struct textview_classinfo *classInfo, struct menulist **Menus);
    extern struct keymap *textview_InitViCommandModeKeyMap(struct textview_classinfo *classInfo, struct menulist **Menus);
    extern int drawtxtv_tabscharspaces;
    drawtxtv_tabscharspaces = environ_GetProfileInt("TabsCharSpaces", 8);
    /* these init functions should be called in this specific order */
    textviewEmacsKeymap = textview_InitEmacsKeyMap(&textview_classinfo, &viewMenus);
    textviewViCommandModeKeymap = textview_InitViCommandModeKeyMap(&textview_classinfo, NULL);
    textviewViInputModeKeymap = textview_InitViInputModeKeyMap(&textview_classinfo, NULL);
    initialExposeStyles =
	environ_GetProfileSwitch("ExposeStylesOnStartup", FALSE);
    alwaysDisplayStyleMenus = environ_GetProfileSwitch("AlwaysDisplayStyleMenus", TRUE);
    highlightToBorders = environ_GetProfileSwitch("HighlightToBorders", FALSE);
    InitializeMod();

    return TRUE;
}

void textview__SetDefaultStyle(struct textview *self, struct style *styleptr)
{
    self->defaultStyle = styleptr;
}

struct style * textview__GetDefaultStyle(struct textview *self)
{
    return self->defaultStyle;
}

void textview__Print(struct textview *self, FILE *f, char *process, char *final, boolean toplevel)
{
    /* This is really screwed. This should return an error and the guy above this */
    /* layer should handle it. */
    if (class_Load("texttroff") == NULL) {
	message_DisplayString(self, 0, "Print aborted: could not load class \"texttroff\".");
        return;
    }

    if (strcmp(process, "troff") == 0 && strcmp(final, "PostScript") == 0)
	texttroff_WriteTroff(self, Text(self), f, toplevel); 
}

static void CreateMatte(struct textview *self, struct viewref *vr)
{
    struct view *v =
      (struct view *) dictionary_LookUp(self, (char *) vr);

    if (v == NULL && class_IsTypeByName(vr->viewType, "view")) { 
        v = (struct view *) matte_Create(vr, (struct view *) self);
        if (v != NULL) {
            viewref_AddObserver(vr, self);
            dictionary_Insert(self, (char *) vr, (char *) v);
        }
    }

    if (v != NULL) {
        if (v->parent != (struct view *) self)
            view_LinkTree(v, (struct view *) self);
        view_InitChildren(v);
    }
}

void textview__InitChildren(struct textview *self)
{
    struct text *d = Text(self);
    long pos = 0;

    while (pos < text_GetLength(d)) {
        long gotlen;
        char *s = text_GetBuf(d, pos, 10240, &gotlen);
        while (gotlen--) {
            if (*s == TEXT_VIEWREFCHAR) {
		/* Need to look here - ajp */
                struct environment *env = textview_GetStyleInformation(self, NULL, pos, NULL);

                if (env != NULL && env->type == environment_View)
                    CreateMatte(self, env->data.viewref);

		textview_ReleaseStyleInformation(self, env);
            }
            s++, pos++;
	}
    }
}

boolean textview__CanView(struct textview *self, char *TypeName)
{
    return class_IsTypeByName(TypeName, "text");
}

/* NOTE: sv (if non-NULL) is assumed to be an UN-initialized statevector, either previously initialize and finalized, or never initialized. */
struct environment * textview__GetStyleInformation(struct textview *self, struct text_statevector *sv, long pos, long *length)
{
    struct environment *env;

    env = environment_GetInnerMost(Text(self)->rootEnvironment, pos);
    if (sv != NULL) {
	text_InitStateVector(sv);
	text_ApplyEnvironment(sv, self->defaultStyle, env);
    }
    if (length != NULL) {
	*length = environment_GetNextChange(Text(self)->rootEnvironment, pos);
    }
    return env;
}

struct environment * textview__GetEnclosedStyleInformation(struct textview *self, long pos, long *length)
{
    struct environment *env;

    env = environment_GetEnclosing(Text(self)->rootEnvironment, pos);
    if (length != NULL) {
	*length = environment_GetNextChange(Text(self)->rootEnvironment, pos);
    }
    return env;
}

void textview__ReleaseStyleInformation(struct textview *self, struct environment *env)
{
}

/* functions added to support VI interface - FAS */

void textview__ToggleVIMode(struct textview *self)
{
    /* switch between VI input/command mode */

    self->viMode = self->viMode ^ 1;
    if ( self->viMode == COMMAND )
	self->keystate = self->viCommandModeKeystate;
    else
	self->keystate = self->viInputModeKeystate;
    textview_ReceiveInputFocus(self);
}

void textview__ToggleEditor(struct textview *self)
{
    /* switch between emacs and vi editors */
    self->editor = self->editor ^ 1;
    if ( self->editor == EMACS )
    {
    	self->keystate = self->emacsKeystate;
	message_DisplayString(self, 0, "EMACS interface in effect.");
    }
    else
    {
	self->keystate = self->viCommandModeKeystate;
	self->viMode = COMMAND;
	message_DisplayString(self, 0, "VI interface in effect.");
    }
    textview_ReceiveInputFocus(self);
}

/* Stubs for selection code. */
void textview__LoseSelectionOwnership(struct textview *self)
{
    mark_SetLength(self->dot, 0);
    textview_WantUpdate(self, self);
}

static int stringmatch(struct text *d, long pos, char *c)
{
    /* Tests if the text begins with the given string */
    while(*c != '\0') {
        if(text_GetChar(d,pos) != *c) return FALSE;
        pos++; c++;
    }
    return TRUE;
}

static void DoCopySelection(struct textview *self, FILE *cutFile, long pos, long len)
{
    struct text *d;
    register long nextChange;
    int UseDataStream;

    d = Text(self);
    environment_GetInnerMost(d->rootEnvironment, pos);
    nextChange = environment_GetNextChange(d->rootEnvironment, pos);

    if (UseDataStream = ((nextChange <= len|| stringmatch(d,pos,"\\begindata")) && text_GetExportEnvironments(d)))
	fprintf(cutFile, "\\begindata{%s, %d}\n",
		text_GetCopyAsText(d) ? "text": class_GetTypeName(d),
		/* d->header.dataobject.id */ 999999);
    d->header.dataobject.writeID = im_GetWriteID();
    text_WriteSubString(d, pos, len, cutFile, UseDataStream);
    
    if (UseDataStream)
	fprintf(cutFile, "\\enddata{%s,%d}\n", 
		text_GetCopyAsText(d) ? "text": class_GetTypeName(d), /* d->header.dataobject.id */ 999999);
}

long textview__WriteSelection(struct textview *self, FILE *out)
{
    DoCopySelection(self, out, mark_GetPos(self->dot), mark_GetLength(self->dot));
    /* if this is called on a view which doesn't override it there is an error.*/
    return 0;
}
