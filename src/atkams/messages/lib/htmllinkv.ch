/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/* A plain textview, used verbatim except for Hit() -- htmlatk.c gives
   every table-cell leaf it builds this class as its viewname instead
   of the auto-derived "textview" (see dataobj.c's dataobject_ViewName)
   specifically so a click on link text inside a table cell reaches
   the same click-to-launch/copy handling as the top-level message
   body (t822view, text822v.ch/.c) -- see roadmap.md's HTML mail
   rendering "Open items" #1 and revival/doc/html-mail-rendering-
   design.md's Links section for the full story of why table cells
   needed their own class rather than reusing plain textview (a
   core ATK class untouched by this app-specific feature) or
   t822view (message-specific machinery -- menus/keymaps/captions --
   that a bare table cell has no business carrying). */
class htmllinkview[htmllinkv]: textview[textv] {
    classprocedures:
      InitializeObject(struct htmllinkview *self) returns boolean;
    overrides:
      Hit(enum view_MouseAction action, long x, long y, long numberOfClicks) returns struct view *;
      FullUpdate(enum view_UpdateType type, long left, long top, long width, long height);
    data:
      /* There's no view-level "default cursor" field in the base ATK
         view/im classes -- what looks like one (e.g. the image pane's
         arrow, the header pane's left-pointer) is each view class
         owning a struct cursor and re-posting it over its own bounds
         from FullUpdate, retracting on removal. imagev.c and
         pshbttnv.c (hyplink's pushbutton view) both already do exactly
         this; we follow the same idiom here so a link inset shows a
         distinct cursor while the pointer is over it. */
      struct cursor *cursor;
};
