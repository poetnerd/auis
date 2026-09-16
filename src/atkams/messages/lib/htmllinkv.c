/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

#include <andrewos.h>
#include <class.h>
#include <text.ih>
#include <htmlatk.h>
#include <htmllinkv.eh>

struct view *htmllinkview__Hit(struct htmllinkview *self, enum view_MouseAction action,
                                long x, long y, long numberOfClicks)
{
    struct view *retv = super_Hit(self, action, x, y, numberOfClicks);
    htmlatk_HandleLinkHit((struct textview *) self, retv, action, x, y);
    return retv;
}
