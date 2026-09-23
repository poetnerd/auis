/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/* The scrollee half of "tableau" HTML table row rendering (see
   lsetscrlv.ch for the other half, the scroll subclass that actually
   gets embedded by htmlatk.c's RenderTableAsLset and constructs one of
   these as its own child) -- a "tableau" row is one whose composed
   minwidth (lset.ch) is nonzero, meaning it (or something nested inside
   it) declared a real pixel design width, as opposed to an ordinary
   percentage-reflow row.

   Matches Thunderbird's real-world behavior for a fixed-width email
   table: stays at its true pixel width instead of being crushed to fit
   the window, reachable by horizontal scroll when wider than the
   window. See html-scroll-plan.md for the full design history,
   including two rejected approaches (teaching core textview/drawtxtv.c
   to let an embedded view's line exceed the offered width, and having
   scroll.c widen any scrollee based on its own DesiredSize) -- both
   would have touched widely-shared code (drawtxtv.c/textv.c, or
   scroll.c for every scrollee in the tree, including imagev/raster's
   own already-working scrolling); this class instead behaves as an
   ordinary, size-bounded scrollee from the OUTSIDE, and does all the
   real work internally: it owns a genuine inner lsetview (the SAME
   dataobject, a second, separate view onto it -- ATK's ordinary
   multi-view model) sized to its own full natural width via
   FullUpdate's own explicit view_InsertView call, shifted left by
   ->panx. The oversized inner view is simply clipped by this class's
   own (narrow, bounded) window, ordinary nested-window clipping, the
   same mechanism drawtxtv.c's own embedded-view width clamp already
   relies on elsewhere in this renderer -- lpair.c's internal
   child-placement math (which the inner lsetview still goes through,
   completely unmodified) never sees anything unusual, since it only
   ever computes rectangles relative to whatever width THIS class
   explicitly hands the inner view.

   CORRECTION (2026-09-22, found live-testing in messages): DesiredSize
   is NOT dead here after all. scroll.c's own compute_inner never
   queries it (true, and still the reasoning for staying width-passive
   below), but that's only half the picture -- lsetscrollVIEW (the
   outer scroll subclass embedded directly in message text, lsetscrlv.c)
   inherits ITS OWN DesiredSize from the same passive view__DesiredSize
   default this class originally copied, and THAT one very much is
   queried, by drawtxtv.c's embedded-view-in-a-text-line layout
   (drawtxtv.c:330, offered height 16384, pass view_NoSet). The default
   caps any height>2048 probe at view_STARTHEIGHT (150px, view.c:51) --
   so without an override, the entire scrollable table was being told
   "you get 150px of line height" regardless of real content, silently
   clipping everything below that to nothing. This override supplies
   the genuine number: query ->inner's real height at this object's own
   natural width (not the possibly-narrower offered width -- the inner
   view actually gets laid out at natural width regardless, per
   FullUpdate above), stay passive on width exactly as before. See
   lsetscrlv.ch's matching correction for the other half (lsetscrollview
   must forward this instead of also relying on the same broken
   default).

   GetInterface answers "scroll,horizontal" (not "scroll,vertical" --
   row height still comes from ordinary reflow, no vertical scroll
   needed): lsetscrollview's own scroll_SetView call makes this object
   scroll's ->scrollee, and scroll.c's own generic machinery
   (scroll.c:580-581) queries *this* object's GetInterface, not
   lsetscrollview's. */
class lsetscrollcontent[lsetscrlc] : view {
classprocedures:
    InitializeObject(struct lsetscrollcontent *self) returns boolean;
    FinalizeObject(struct lsetscrollcontent *self);
overrides:
    SetDataObject(struct dataobject *newDataObject);
    LinkTree(struct view *parent);
    FullUpdate(enum view_UpdateType type, long left, long top, long width, long height);
    Update();
    Hit(enum view_MouseAction action, long x, long y, long numberOfClicks) returns struct view *;
    GetInterface(char *interfaceName) returns char *;
    DesiredSize(long width, long height, enum view_DSpass pass, long *dWidth, long *dHeight) returns enum view_DSattributes;
data:
    struct lsetview *inner;
    long panx;
};
