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


 

#define STRING 1
#define LONG 2
#define FLOAT 3

/* Values for lset's valign field (see its comment below). */
#define lset_VALIGN_NONE 0
#define lset_VALIGN_TOP 1
#define lset_VALIGN_MIDDLE 2
#define lset_VALIGN_BOTTOM 3

/* Values for lset's halign field. */
#define lset_HALIGN_NONE 0
#define lset_HALIGN_LEFT 1
#define lset_HALIGN_CENTER 2
#define lset_HALIGN_RIGHT 3

class lset : dataobject[dataobj] {
overrides:
    Read (FILE *file, long id) returns long;
    Write (FILE *file, long writeid, int level) returns long;
    GetModified() returns long;
classprocedures:
    InitializeClass() returns boolean;
    InitializeObject(struct lset *self) returns boolean;
methods:
    InsertObject (char *name,char *viewname);
    registername(char *rf) returns char *;
data:
	int type;
	int pct;
	/* When set, the view built for this split should not draw its
	   lpair divider bar (see lpair.ch's barvisible/SetBarVisible).
	   Added 2026-08-19 for htmlatk.c's HTML table rendering: a real
	   browser never shows a resize bar between table cells, so every
	   BuildLsetChain-generated split (htmlatk.c) sets this. Persisted
	   as of the on-disk \V 2 format (lset__Read/Write); defaults to 0
	   (bar shown, the original/pre-2026-08-19 behavior) for \V 1 files
	   and every other lset caller in the tree. */
	int nobar;
	/* When set, the side-by-side view built for this split centers
	   its shorter child vertically within the shared row height
	   (lpair_VCENTER, lpair.ch) instead of pinning it to the top --
	   real browsers' default table-cell valign. Added 2026-08-20
	   alongside nobar, same htmlatk.c motivation and same \V 2->3
	   persisted-format bump (lset__Read/Write); defaults to 0
	   (top-pinned, the original behavior) for \V <3 files and every
	   other lset caller in the tree. */
	int vcenter;
	/* When set, the stacked (top/bottom) view built for this split
	   sizes its top child to its own real desired height instead of
	   an equal/weighted share of the stack (lpair_AUTOHEIGHT,
	   lpair.ch). Added 2026-08-20 for htmlatk.c's BuildLsetLeafFromFloatTable,
	   which stacks a floated table's real HTML rows (title, synopsis,
	   Buy button -- previously all but the first were destroyed
	   outright, confirmed live against the Book Rack fixture) instead
	   of flattening them into one leaf. Persisted as of the on-disk
	   \V 4 format (lset__Read/Write); defaults to 0 (equal/weighted
	   split, the original behavior) for \V <4 files and every other
	   lset caller in the tree. */
	int autoheight;
	/* The pixel width this split's own subtree genuinely needs, bottom-up
	   -- e.g. a real <table width="640"> declared by the HTML author, or
	   the summed natural width of fixed-pixel-width descendants such as
	   a nested <table width="200">. Zero means "no declared floor, keep
	   dividing whatever width is offered by percentage" (every non-HTML-
	   table lset caller, and any table with only width="100%"/no width
	   at all). Added 2026-09-20 for htmlatk.c's fixed-width/scrollable
	   "tableau" table rendering (see html-scroll-plan.md): consulted by
	   lsetview__DesiredSize's split-node case to report a genuine natural
	   width upward instead of always echoing back the offered width, and
	   by the tableau wrapper view to learn how wide to make its inner
	   lsetview regardless of what the surrounding text offered it.
	   Persisted as of the on-disk \V 5 format (lset__Read/Write); defaults
	   to 0 for \V <5 files and every other lset caller in the tree. */
	int minwidth;
	/* An X11-ready color spec ("#rrggbb" or a named color) for this
	   leaf's own cell background -- e.g. a real <td bgcolor="..."> or
	   style="background-color:...". Empty means "no declared
	   background, leave whatever the child view already paints" (every
	   non-HTML-table lset caller, and any table cell with neither).
	   Added 2026-09-25 for htmlatk.c's HTML table rendering (see
	   roadmap.md item 2): consulted by lsetv.c's makeview() to set the
	   leaf's child view's own background color once, at construction
	   time, so every ordinary WhitePattern-based erase that view
	   already does paints this color instead -- no drawtxtv.c/textv.c
	   changes needed (see lsetv.c's makeview() for the full mechanism).
	   Persisted as of the on-disk \V 6 format (lset__Read/Write);
	   defaults to "" for \V <6 files and every other lset caller in
	   the tree. */
	char bgcolor[32];
	/* When set, this split reserves zero pixels between its two
	   children (lpair_NOSEAM, lpair.ch) instead of lpair's always-on
	   1px gap (lpair_ComputeSizesFromTotal's unconditional "totalsize
	   -= 1", present even when nobar is set -- nobar only suppresses
	   the drawn divider line, per its own comment in lpair.ch, not the
	   reserved space). Harmless for lpair's original window-split use
	   (a stable 1px seam regardless of movable state), but a real,
	   visible seam wherever HTML rendering stacks colored cells edge-
	   to-edge expecting them to touch exactly -- found live 2026-09-26
	   (wdc's own screenshot): a 1px line between a card and its own
	   background color that Thunderbird doesn't show. Added alongside
	   nobar wherever htmlatk.c builds an HTML-table split. Persisted
	   as of the on-disk \V 7 format (lset__Read/Write); defaults to 0
	   (1px seam, the original behavior) for \V <7 files and every
	   other lset caller in the tree. */
	int noseam;
	/* A leaf's vertical placement of its content within the height
	   its parent split gives it (lset_VALIGN_*, above) -- a real HTML
	   cell's valign. Added 2026-09-27 for htmlatk.c's table rendering
	   (roadmap.md item 10): side-by-side splits now give every cell
	   the full row height, so the leaf paints its whole rectangle in
	   its bgcolor and places a shorter child at the top, middle or
	   bottom itself (lsetv.c's placechild). NONE and TOP both give the
	   child the full rectangle, the original behavior. Persisted as of
	   the on-disk \V 8 format (lset__Read/Write); defaults to NONE for
	   \V <8 files and every other lset caller in the tree. */
	int valign;
	/* A floor, in pixels, on the height this leaf or split reports
	   upward (lsetview__DesiredSize) -- a real HTML cell's or table's
	   height="N". Zero means none. Added 2026-09-27 with valign, same
	   motivation: bookrack.html's review cards declare height="225"
	   on the cover/synopsis cells, which sets where the Buy row below
	   them starts. Persisted as of \V 8 alongside valign; defaults
	   to 0 for \V <8 files and every other lset caller. */
	int minheight;
	/* A leaf's horizontal placement of a child whose natural width,
	   contentwidth pixels, is narrower than the leaf (lset_HALIGN_*,
	   above). Added 2026-09-27 with valign for htmlatk.c's
	   <table align="center"> of fixed-width cells (bookrack.html's
	   Buy button): the leaf centers the child and paints the sides in
	   its bgcolor. LEFT and RIGHT (2026-09-28) place a shrink-to-fit
	   nested table (no width=) at one side instead. NONE (or
	   contentwidth 0) gives the child the full width, the original
	   behavior. Persisted as of \V 8. */
	int halign;
	int contentwidth;
	/* Pixels of blank space, in the leaf's bgcolor, between the leaf's
	   edges and its child on all four sides -- an HTML table's
	   cellpadding. Added 2026-09-28 with halign; persisted as of \V 8. */
	int padding;
	char dataname[32];
	char viewname[32];
	char refname[64];
    struct dataobject *dobj;
    struct dataobject *left,*right;
    int application;
    struct text *pdoc;
    int revision;
};
