/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/* See htmlimg.ch. */

#include <andrewos.h>
#include <class.h>
#include <htmlimg.eh>

boolean htmlimage__InitializeObject(struct classheader *classID, struct htmlimage *self)
{
    self->pct = 100;
    self->maxwidth = 0;
    return TRUE;
}

char *htmlimage__ViewName(struct htmlimage *self)
{
    return "htmlimgview";
}
