/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

#include <andrewos.h>
#include <class.h>
#include <text.ih>
#include <cursor.ih>
#include <htmlatk.h>
#include <htmllinkv.eh>

struct view *htmllinkview__Hit(struct htmllinkview *self, enum view_MouseAction action,
                                long x, long y, long numberOfClicks)
{
    struct view *retv = super_Hit(self, action, x, y, numberOfClicks);
    htmlatk_HandleLinkHit((struct textview *) self, retv, action, x, y);
    return retv;
}

boolean htmllinkview__InitializeObject(struct classheader *classID, struct htmllinkview *self)
{
    self->cursor = cursor_Create(self);
    if (self->cursor) cursor_SetStandard(self->cursor, Cursor_Gunsight);
    return TRUE;
}

/* Same idiom imagev.c and pshbttnv.c use for their own view-hover
   cursor: re-post over our own bounds on (re)draw/move, retract when
   removed from the screen. No base-class hook does this for us --
   see the .ch data: comment. */
void htmllinkview__FullUpdate(struct htmllinkview *self, enum view_UpdateType type,
                               long left, long top, long width, long height)
{
    struct rectangle Rect;

    super_FullUpdate(self, type, left, top, width, height);

    if (!self->cursor) return;

    switch (type) {
        case view_FullRedraw:
        case view_LastPartialRedraw:
        case view_MoveNoRedraw:
            htmllinkview_RetractCursor(self, self->cursor);
            /* Coarse, view-level gate -- see htmlatk_TextHasLink()'s
               comment in htmlatk.h. Without this every table cell got
               the link cursor, link or not (reported live 2026-09-15). */
            if (htmlatk_TextHasLink((struct text *) htmllinkview_GetDataObject(self))) {
                htmllinkview_GetLogicalBounds(self, &Rect);
                htmllinkview_PostCursor(self, &Rect, self->cursor);
            }
            break;
        case view_Remove:
            htmllinkview_RetractCursor(self, self->cursor);
            break;
        default:
            break;
    }
}
