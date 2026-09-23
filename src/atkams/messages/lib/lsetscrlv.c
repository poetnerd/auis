/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

#include <andrewos.h>
#include <class.h>

#include "lsetscrlv.eh"

#include <dataobj.ih>
#include <view.ih>
#include <scroll.ih>
#include <lset.ih>
#include <rect.h>

/* Deferred to here (not InitializeObject, where the dataobject isn't
   set yet), mirroring lsetscrollcontent__SetDataObject's own reasoning
   (lsetscrlc.c). scroll_GetChild, not a direct self->child read -- the
   published accessor, not an assumption about inherited-field layout. */
void lsetscrollview__SetDataObject(struct lsetscrollview *self, struct dataobject *newDataObject)
{
    super_SetDataObject(self, newDataObject);
    if (!lsetscrollview_GetChild(self) && newDataObject) {
        struct view *content = (struct view *) class_NewObject("lsetscrollcontent");
        if (content) {
            view_SetDataObject(content, newDataObject);
            lsetscrollview_SetView(self, content);
            /* scroll_TOP, not scroll_BOTTOM (2026-09-22, found
               live-testing): wdc's real complaint -- for a table
               taller than the window, the bar sat below all that
               content, so reaching it meant first scrolling the whole
               OUTER message view down past the table's entire height.
               scroll_TOP puts the bar at the box's top edge, reachable
               the moment the table itself scrolls into view. See the
               matching FullUpdate override below, which re-derives
               this same location from real geometry every time. */
            lsetscrollview_SetLocation(self, scroll_TOP);
        }
    }
}

/* Safe, hardcoded estimate of this object's own scrollbar-chrome
   height (one bottom bar + surrounding padding) -- CORRECTION
   (2026-09-22, found live-testing): originally computed from this
   object's own barWidth/viewPadding/windowPadding fields (scroll.ch),
   but those are only ever set lazily, inside scroll__FullUpdate's
   InitPrefs() call (scroll.c:1289-1293, guarded by !self->prefsready)
   -- the FIRST paint. DesiredSize is queried during layout, strictly
   before any paint, so at that point those fields hold whatever was
   left on this object's heap slot by a PREVIOUS, already-freed object
   (confirmed live: a reused address carried over a stale value large
   enough to make the computed height go *negative*, collapsing the
   whole table to nothing -- exactly the bookrack blank-render bug).
   COLORBARWIDTH(20)+viewPadding(windowPadding+1)+windowPadding*2, at
   scroll.c's own COLORWINDOWPADDING(1)/COLORBARWIDTH(20) defaults, is
   24; rounded up slightly for headroom since mono-mode/profile
   overrides could differ slightly and a little extra blank space
   under the table is harmless, unlike an under-report. */
#define LSETSCROLLVIEW_BAR_HEIGHT 20

/* CORRECTION (2026-09-22, found live-testing): compute_inner
   (scroll.c:789-834) shrinks the interior by border padding
   UNCONDITIONALLY, whenever self->drawborder is set (its own
   ScrollDrawBorders profile default, nothing to do with whether a
   scrollbar is actually shown/self->desired.location) -- so a table
   that needs no scrollbar was still having its box's real interior
   shrunk by the border on all four sides, chopping content that was
   sized to fill the reported DesiredSize exactly (the original bug
   report: social-media icons chopped at bottom and right even with
   the scrollbar itself gone). Reserving a small constant on BOTH axes
   unconditionally (not just height, and not just when scrolling)
   fixes this -- deliberately generous (real default border padding
   from scroll.c's own COLORWINDOWPADDING(1)/viewPadding(2) is only a
   few px) since a little extra blank space is harmless, unlike a
   chopped edge. Can't query the real values for the same reason as
   the bar-height constant below -- see its own comment. */
#define LSETSCROLLVIEW_BORDER_PAD 8

/* Forwards lsetscrollcontent's genuine content height (lsetscrlc.c's
   own DesiredSize override), plus bar height and border padding,
   UNCONDITIONALLY -- see this function's own 2026-09-23 correction
   comment inline below for why the earlier "only when this table
   actually needs to scroll" version was reverted (it made *dHeight
   flip between two different answers for the exact same table across
   repeated queries within one layout pass, corrupting textview's own
   line-height bookkeeping). The matching FullUpdate override further
   down still correctly hides the border/bar visually for a table that
   fits, from stable final geometry -- this only means a little unused
   blank space (<=48px) is reserved and never drawn under such a
   table, not that unwanted chrome reappears.

   The bar-height constant itself (as opposed to border padding) is a
   safe, hardcoded estimate (one bottom bar) rather than a query of
   this object's own barWidth/viewPadding/windowPadding fields
   (scroll.ch), because those are only ever set lazily, inside
   scroll__FullUpdate's InitPrefs() call (scroll.c:1289-1293, guarded
   by !self->prefsready) -- the FIRST paint. DesiredSize is queried
   during layout, strictly before any paint, so at that point those
   fields hold whatever was left on this object's heap slot by a
   PREVIOUS, already-freed object (confirmed live: a reused address
   carried over a stale value large enough to make the computed height
   go *negative*, collapsing the whole table to nothing -- the
   bookrack blank-render bug). COLORBARWIDTH(20) is the real default;
   rounded up slightly for headroom. */
enum view_DSattributes lsetscrollview__DesiredSize(struct lsetscrollview *self, long width, long height, enum view_DSpass pass, long *dWidth, long *dHeight)
{
    struct view *content = lsetscrollview_GetChild(self);
    long dw, dh, barheight, borderpad;
    if (!content) {
        return super_DesiredSize(self, width, height, pass, dWidth, dHeight);
    }
    view_DesiredSize(content, width, height, pass, &dw, &dh);
    /* CORRECTION (2026-09-23, found live-testing): this used to key
       barheight/borderpad off (lset->minwidth > width) -- but `width`
       here is this CALL's own offered-width parameter, and repeated
       DesiredSize queries for the exact same object within a single
       layout pass have been observed (trace log) offering wildly
       different widths call to call (528/585/645/693 for the same
       object) as drawtxtv.c/textview negotiate line width. That made
       *dHeight itself flip between two different values from one call
       to the next for the SAME table -- exactly the kind of unstable
       answer that confuses textview's own line-height bookkeeping
       (GenerateLineItems/drawtxtv.c cache/accumulate these across
       calls), and is what produced the large stray whitespace gaps
       and misplaced content reported live right after this was added.
       Unconditional again -- stable, width-independent, matches the
       last known-good round. FullUpdate's own needsScroll (below) is
       NOT the same risk: it runs once against final placed geometry,
       not a fluctuating query parameter, so the border/bar still
       correctly disappear for a table that fits -- this only gives up
       reserving a LITTLE unused blank space (<=48px) under a
       non-scrolling table, not visible chrome, when the two occasionally
       disagree. */
    barheight = LSETSCROLLVIEW_BAR_HEIGHT;
    borderpad = LSETSCROLLVIEW_BORDER_PAD;
    *dWidth = dw + borderpad;
    *dHeight = dh + borderpad + barheight;
    return view_Fixed;
}

/* Companion to DesiredSize above: actually turns the horizontal
   scrollbar chrome on/off, from real placed geometry (scroll_GetLogicalBounds
   inside compute_inner is what really decides how much room the child
   gets, so this has to run before super_FullUpdate, which is what
   calls compute_inner). Same condition as DesiredSize (natural width
   vs available width) so the two stay consistent: DesiredSize already
   reserved chrome height whenever this decides to turn the bar on.

   CORRECTION (2026-09-23, found live-testing -- the "two frames,
   only one goes away on <space>b" bug): this function also runs on
   view_Remove (type 4) teardown passes, not just real layout passes
   -- confirmed via trace log, every view_Remove call reports
   GetLogicalBounds as a (0,0,-2,-2) sentinel, since the view has no
   real placement at that point, it's being torn down. naturalWidth
   (always a real positive number) is always > -2, so needsScroll
   unconditionally came out TRUE on every single teardown, forcing
   SetDrawBorder(TRUE)/SetLocation(scroll_TOP) back on regardless of
   the table's real fit -- and since each relayout does a Remove pass
   THEN a FullRedraw pass, whichever one happened to run last (a race,
   not a decision) is what stuck. Skip the decision entirely except on
   a genuine geometry pass (FullRedraw/PartialRedraw, with sane
   placed bounds); Remove and MoveNoRedraw leave whatever state is
   already set untouched, since there's no real geometry to decide
   from and super_FullUpdate below still needs to run either way. */
void lsetscrollview__FullUpdate(struct lsetscrollview *self, enum view_UpdateType type, long left, long top, long width, long height)
{
    struct dataobject *dob = lsetscrollview_GetDataObject(self);
    if (dob && type != view_Remove && type != view_MoveNoRedraw) {
        struct rectangle ownBounds;
        /* CORRECTION (2026-09-23, found live-testing -- "no change in
           behavior" after the view_Remove fix above, then a worse visible
           glitch from a since-reverted double-super_FullUpdate attempt):
           scroll__FullUpdate's own InitPrefs (called inside super_FullUpdate,
           below) used to unconditionally reset self->drawborder from the
           ScrollDrawBorders profile default the first time it ever ran on
           a linked instance, silently overwriting the SetDrawBorder call
           below regardless of call order. Fixed at the root in scroll.c
           itself: SetDrawBorder now marks the override as permanent
           (self->drawborderoverridden), and InitPrefs checks that flag
           before touching drawborder -- so simply calling SetDrawBorder
           here, before the single super_FullUpdate call below, is now
           sufficient; no throwaway pass needed. */
        lsetscrollview_GetLogicalBounds(self, &ownBounds);
        if (ownBounds.width > 0) {
            long naturalWidth = ((struct lset *) dob)->minwidth;
            boolean needsScroll = naturalWidth > ownBounds.width;
            lsetscrollview_SetLocation(self, needsScroll ? scroll_TOP : 0);
            /* scroll.ch's new SetDrawBorder (2026-09-22): border chrome
               off entirely when this table already fits -- it behaves as
               a plain, invisible passthrough, exactly like an ordinary
               unwrapped lsetview would, matching wdc's own framing ("make
               it conditional, make it go away"). Only a genuinely
               scrollable table keeps the border, as a visual cue that
               there's more to see. */
            lsetscrollview_SetDrawBorder(self, needsScroll);
        }
    }
    super_FullUpdate(self, type, left, top, width, height);
}
