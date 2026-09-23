/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/* The view class htmlatk.c's RenderTableAsLset routes a "tableau" HTML
   table row to (a row whose composed minwidth, lset.ch, is nonzero --
   see lsetscrlc.ch for the full story). A thin scroll subclass with
   exactly one job: construct its own scrollee (lsetscrollcontent,
   lsetscrlc.ch/.c -- where all the real width/panning work lives) from
   the same dataobject once it becomes available, and request a
   horizontal-only scrollbar. Everything else -- chrome drawing,
   button/elevator handling, the generic scroll,horizontal interface
   dispatch to the scrollee -- is plain, completely unmodified scroll
   (src/atk/supportviews/scroll.ch/.c) behavior.

   Needed as a SEPARATE class from lsetscrollcontent (rather than one
   class doing both jobs) because text_AlwaysAddView's own view-
   construction path (RenderTableAsLset's embedding call) never
   consults GetApplicationLayer -- the usual place a view gets wrapped
   in scroll bars -- so nothing would ever wrap the content view in a
   real scroll otherwise. Being a scroll subclass directly, constructed
   by the ordinary class_NewObject(viewname)+SetDataObject sequence
   every embedded view goes through, sidesteps that gap without any
   change to text/drawtxtv.c's embedding machinery.

   CORRECTION (2026-09-22, found live-testing in messages): this class
   IS the one drawtxtv.c's embedded-view-in-a-text-line layout queries
   directly (drawtxtv.c:330) -- being plain, unmodified scroll (which
   has no DesiredSize override of its own either) meant it fell all the
   way to view__DesiredSize's passive default, which caps any
   height-probe over 2048 at a hardcoded 150px (view_STARTHEIGHT,
   view.c:51). scroll's own default is fine for its traditional
   whole-window usage (the window system hands it a real HeightSet
   budget, never queries "what height do you actually want"), but wrong
   here, where the embedding text line genuinely needs to know this
   table's real height. Overridden below to forward the real number
   from lsetscrollcontent (lsetscrlc.ch's matching override) plus this
   object's own scrollbar-chrome height -- but only when the content
   doesn't already fit (FullUpdate below toggles the actual scrollbar
   the same way, from real placed geometry, so a small fixed-width
   table -- e.g. a row of social-media icons -- doesn't carry permanent
   unwanted scrollbar chrome it never needs). */
class lsetscrollview[lsetscrlv] : scroll {
overrides:
    SetDataObject(struct dataobject *newDataObject);
    DesiredSize(long width, long height, enum view_DSpass pass, long *dWidth, long *dHeight) returns enum view_DSattributes;
    FullUpdate(enum view_UpdateType type, long left, long top, long width, long height);
};
