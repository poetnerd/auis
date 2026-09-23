/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

#include <andrewos.h>
#include <class.h>

#include "lsetscrlc.eh"

#include <dataobj.ih>
#include <view.ih>
#include <lset.ih>
#include <lsetv.ih>
#include <scroll.ih>
#include <rect.h>

/* ATK LAZY-LINKING INVARIANT (documented here 2026-09-23, found live
   via lldb after a real crash -- see lsetscrollcontent__DesiredSize's
   own guard below for the fix, and project memory
   project_lazy_linking_invariant.md for the durable cross-session
   note):

   A freshly class_NewObject'd + SetDataObject'd view (self->inner,
   here) is fully constructed but has no window/graphic context --
   self->imPtr (view_GetIM(view)) is NULL -- until something actually
   links it in. Two DIFFERENT things can do that linking, and they are
   NOT equivalent for every purpose:
     1. view_LinkTree(view, parent) -- the "real"/intended mechanism
        (this class's own LinkTree override calls it on self->inner).
     2. view_InsertView(view, parent, rect) -- primarily a GEOMETRY
        placement call, but view__InsertView (view.c) has the side
        effect of also setting view->imPtr = parent->imPtr, as a
        byproduct, regardless of whether LinkTree ever ran.

   Querying an UNLINKED view's DesiredSize is NOT inherently safe: for
   an ordinary leaf view this is usually fine (the base view__DesiredSize
   default doesn't touch imPtr at all), but for anything whose
   DesiredSize recurses into REAL content -- as self->inner does here,
   walking down into cells whose own content may itself be a textview
   showing a nested embedded table -- that recursion can reach code
   (textview__LineRedraw, called internally from textview__DesiredSize
   in a "measure" pass) that still executes real color-allocation calls
   assuming a valid drawable/colormap context. Confirmed live: this
   crashed with EXC_BAD_ACCESS (x0=NULL) inside xcolormap__AllocColor
   <- SetFGColor <- xgraphic__SetForegroundColor <- textview__LineRedraw,
   three real HTML-table nesting levels deep, during page-forward (when
   content is measured for the very first time, before anything about
   it has been linked yet).

   FullUpdate/Update below do NOT need the same guard, and deliberately
   don't have one: place_inner() (shared by both) calls view_InsertView
   on self->inner BEFORE either function recurses any further -- which
   already sets self->inner->imPtr as described above, in the same
   function, before the risky recursive call. DesiredSize is the ONE
   path with no such side effect: a pure query, called by drawtxtv.c's
   line-layout code potentially long before this content is ever
   linked or painted at all. If a future change adds another place
   that recurses into self->inner's (or any similarly lazily-linked
   child's) DesiredSize WITHOUT a prior InsertView/LinkTree in the same
   call, it needs this same guard. */

/* Small safety margin added to lset->minwidth everywhere this class
   reads it for scroll-range/placement math (below) -- CORRECTION
   (2026-09-22, found live-testing): dragging fully to the right still
   left the last few pixels of real content chopped off. The scroll
   math itself is internally consistent (place_inner/x_getinfo/
   x_setframe all agree: at full pan you see up to exactly
   naturalWidth) -- the shortfall is naturalWidth itself, Step 1's
   composed minwidth (htmlatk.c), coming in a few px under the true
   rendered width for some cells (this codebase already has a
   documented precedent for the same kind of shortfall -- see
   CellFixedPixelWidth's own ICONCELL_WIDTH_PAD comment in htmlatk.c).
   Rather than chase which specific cell/border source is responsible
   in every fixture, pad the number actually used for scrolling a
   little: a few extra px of scrollable blank space past the true edge
   is harmless, a chopped edge is not. */
#define MINWIDTH_SAFETY_PAD 12
static long EffectiveNaturalWidth(struct lsetscrollcontent *self)
{
    long mw = ((struct lset *) lsetscrollcontent_GetDataObject(self))->minwidth;
    return (mw > 0) ? (mw + MINWIDTH_SAFETY_PAD) : mw;
}

boolean lsetscrollcontent__InitializeObject(struct classheader *classID, struct lsetscrollcontent *self)
{
    self->inner = NULL;
    self->panx = 0;
    return TRUE;
}

void lsetscrollcontent__FinalizeObject(struct classheader *classID, struct lsetscrollcontent *self)
{
    if (self->inner) {
        view_UnlinkTree((struct view *) self->inner);
        view_Destroy((struct view *) self->inner);
        self->inner = NULL;
    }
}

/* Deferred to here (not InitializeObject, where lsetscrollcontent_GetDataObject(self)/
   the lset dataobject isn't set yet) -- a second, independent view onto the
   SAME lset dataobject lsetscrollcontent itself was constructed for,
   ordinary ATK multi-view usage, not a copy. */
void lsetscrollcontent__SetDataObject(struct lsetscrollcontent *self, struct dataobject *newDataObject)
{
    super_SetDataObject(self, newDataObject);
    if (!self->inner && newDataObject) {
        self->inner = (struct lsetview *) class_NewObject("lsetview");
        if (self->inner) view_SetDataObject((struct view *) self->inner, newDataObject);
    }
}

void lsetscrollcontent__LinkTree(struct lsetscrollcontent *self, struct view *parent)
{
    super_LinkTree(self, parent);
    if (self->inner) view_LinkTree((struct view *) self->inner, self);
}

/* Shared by FullUpdate and Update below -- CORRECTION (2026-09-22,
   found live-testing): x_setframe (scroll drag) only calls WantUpdate,
   which leads to Update(), never FullUpdate() again -- so the actual
   view_InsertView repositioning (this function) has to happen from
   Update() too, not just FullUpdate(), or a scrollbar drag changes
   ->panx in memory but the inner view's on-screen window never
   actually moves. */
static void place_inner(struct lsetscrollcontent *self)
{
    struct rectangle ownBounds, r;
    long naturalWidth, w;
    lsetscrollcontent_GetLogicalBounds(self, &ownBounds);
    naturalWidth = EffectiveNaturalWidth(self);
    w = (naturalWidth > ownBounds.width) ? naturalWidth : ownBounds.width;
    rectangle_SetRectSize(&r, ownBounds.left - self->panx, ownBounds.top, w, ownBounds.height);
    view_InsertView((struct view *) self->inner, self, &r);
}

/* self is always placed by its own parent (scroll's compute_inner,
   unmodified) at its ordinary bounded/narrow rectangle -- exactly
   like any other scrollee. place_inner() inserts the inner lsetview
   at its own full natural width (lsetscrollcontent_GetDataObject(self)->minwidth, computed
   bottom-up at HTML-build time in htmlatk.c -- no runtime DesiredSize
   query needed), shifted left by ->panx, INSIDE that narrow window.
   The excess is simply clipped by self's own window bounds, ordinary
   nested-window clipping. lpair.c's own child-placement math (which
   the inner lsetview's internal split tree still goes through,
   completely unmodified) never sees anything unusual -- it only ever
   computes rectangles relative to whatever width this function hands
   the inner view directly. */
void lsetscrollcontent__FullUpdate(struct lsetscrollcontent *self, enum view_UpdateType type, long left, long top, long width, long height)
{
    if (!self->inner) { super_FullUpdate(self, type, left, top, width, height); return; }
    place_inner(self);
    view_FullUpdate((struct view *) self->inner, type, left, top, width, height);
}

/* See lsetscrlc.ch's 2026-09-22 correction. width stays passive (we
   accept whatever line width we're offered -- that's what makes the
   box narrow and scrollable); height is the genuine number, queried
   from ->inner at this object's own natural width rather than the
   possibly-narrower offered width, since that's the width ->inner
   actually gets placed at (FullUpdate above), regardless of how
   narrow this object's own window ends up being.

   GUARD (2026-09-23, found live via lldb -- see this file's own
   top-of-file note on ATK's lazy-linking invariant for the general
   principle). Confirmed crash: EXC_BAD_ACCESS (x0=NULL) inside
   xcolormap__AllocColor <- SetFGColor <- xgraphic__SetForegroundColor
   <- textview__LineRedraw <- textview__DesiredSize, reached by
   recursing through self->inner's split tree into a cell whose
   content is itself a textview showing a nested embedded table (three
   real bookrack.html nesting levels deep), while self->inner had never
   been linked. Falls back to the same safe default this function
   already uses for !self->inner -- self-corrects on the next real
   layout pass once actually linked, same as any view whose content
   changes after its first DesiredSize query. */
enum view_DSattributes lsetscrollcontent__DesiredSize(struct lsetscrollcontent *self, long width, long height, enum view_DSpass pass, long *dWidth, long *dHeight)
{
    long naturalWidth, dw, dh;
    if (!self->inner) {
        return super_DesiredSize(self, width, height, pass, dWidth, dHeight);
    }
    if (!view_GetIM((struct view *) self->inner)) {
        return super_DesiredSize(self, width, height, pass, dWidth, dHeight);
    }
    naturalWidth = EffectiveNaturalWidth(self);
    if (naturalWidth <= 0) naturalWidth = width;
    view_DesiredSize((struct view *) self->inner, naturalWidth, height, view_NoSet, &dw, &dh);
    *dWidth = width;
    *dHeight = dh;
    return view_Fixed;
}

/* CORRECTION (2026-09-22, found live-testing): view_Update() was not
   enough to make a scrollbar drag visible. ATK views generally share
   their nearest realized ancestor's actual X window rather than each
   owning one (confirmed via xgraphic.c's HandleInsertion -- InsertView
   only updates logical bookkeeping, self->localWindow just inherits
   the parent's), so repositioning alone never repaints a pixel;
   Update() also isn't guaranteed to force a repaint of geometry it
   already considers unchanged (only ->panx moved, not self->inner's
   own size). view_FullUpdate(child, view_FullRedraw, 0,0,0,0) is this
   codebase's own established idiom for "genuinely repaint yourself
   now" self-triggered refresh (celv.c, imagev.c, colorv.c, etc. all
   do exactly this) -- the 0,0,0,0 args are safe/conventional here
   since this class's own FullUpdate above never reads them, using
   GetLogicalBounds instead. */
void lsetscrollcontent__Update(struct lsetscrollcontent *self)
{
    if (!self->inner) return;
    place_inner(self);
    view_FullUpdate((struct view *) self->inner, view_FullRedraw, 0, 0, 0, 0);
}

struct view *lsetscrollcontent__Hit(struct lsetscrollcontent *self, enum view_MouseAction action, long x, long y, long numberOfClicks)
{
    if (self->inner) return view_Hit((struct view *) self->inner, action, x, y, numberOfClicks);
    return (struct view *) self;
}

/* Scroll interface -- mirrors imagev.c's x_getinfo/x_setframe/
   x_whatisat structure (src/atk/image/imagev.c:1520-1587), the
   existing working horizontal-scroll template, but keyed off panx/
   lsetscrollcontent_GetDataObject(self)->minwidth instead of an image buffer's own pixel
   dimensions. No endzone handler (NULL, same as imagev's own
   horizontal_scroll_interface) -- imagev doesn't implement one for its
   horizontal axis either; scroll.c's own endzone default handling
   covers the common case. */
static void x_getinfo(struct lsetscrollcontent *self, struct range *total, struct range *seen, struct range *dot)
{
    struct rectangle ownBounds;
    long naturalWidth = EffectiveNaturalWidth(self);
    lsetscrollcontent_GetLogicalBounds(self, &ownBounds);
    total->beg = 0;
    total->end = (naturalWidth > ownBounds.width) ? naturalWidth : ownBounds.width;
    seen->beg = self->panx;
    seen->end = self->panx + ownBounds.width;
    if (seen->end > total->end) seen->end = total->end;
    dot->beg = dot->end = -1;
}

static long x_whatisat(struct lsetscrollcontent *self, long coordinate, long outof)
{
    return coordinate + self->panx;
}

static void x_setframe(struct lsetscrollcontent *self, int position, long coordinate, long outof)
{
    long contentPos = coordinate + self->panx;
    long diffpos = contentPos - position;
    if (diffpos) {
        struct rectangle ownBounds;
        long naturalWidth = EffectiveNaturalWidth(self);
        long maxpan;
        self->panx -= diffpos;
        lsetscrollcontent_GetLogicalBounds(self, &ownBounds);
        maxpan = (naturalWidth > ownBounds.width) ? (naturalWidth - ownBounds.width) : 0;
        if (self->panx < 0) self->panx = 0;
        if (self->panx > maxpan) self->panx = maxpan;
        lsetscrollcontent_WantUpdate(self, self);
    }
}

static struct scrollfns horizontal_scroll_interface = {
    x_getinfo,
    x_setframe,
    NULL,
    x_whatisat
};

char *lsetscrollcontent__GetInterface(struct lsetscrollcontent *self, char *interfaceName)
{
    if (strcmp(interfaceName, "scroll,horizontal") == 0)
        return (char *) &horizontal_scroll_interface;
    return NULL;
}
