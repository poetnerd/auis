/* ********************************************************************** *\
 *         Copyright IBM Corporation 1991 - All Rights Reserved           *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/contrib/time/RCS/timeodayv.c,v 1.7 1992/12/15 21:57:11 rr2b R6tape $";
#endif

#include <class.h>
#include <timeodayv.eh>
#include <timeoday.ih>
#include <andrewos.h>
#include <graphic.ih>
#include <message.ih>
#include <observe.ih>
#include <view.ih>
#include <fontdesc.ih>
#include <proctbl.ih>
#include <util.h>
static void MenuSetFormat();
static void Redraw();

/* Defined constants and macros */
#define FUDGEFACTOR 1.1

/* External Declarations */

/* Forward Declarations */

/* Global Variables */
static struct menulist *timeodayview_menulist = NULL;

static char *formats[] = {
  "Default~1", NULL,
  "H:MM AM/PM~10", "%u:%M %P",
  "Month DD YYYY~11", "%o %A, %Y",
  NULL, NULL};


static void MenuSetFormat(struct timeodayview *self, char *format)
{
  struct timeoday *b = (struct timeoday *) timeodayview_GetDataObject(self);

  timeoday_SetFormat(b, format);
  timeoday_SetModified(b);
}


boolean timeodayview__InitializeClass(struct classheader *c)
{
/* 
  Initialize all the class data.
*/
  int i;
  char *menuname;
  char temp[250];
  struct proctable_Entry *proc = NULL;
  char menutitlefmt[200];
  char procname[200];

  timeodayview_menulist = menulist_New();
  
  sprintf(procname, "%s-set-format", "timeoday");
  proc = proctable_DefineProc(procname, MenuSetFormat, &timeodayview_classinfo, NULL, "Set the timeoday inset's format.");
  
  sprintf(menutitlefmt, "%s,%%s", "Time O'Day");
  
  for (i=0; formats[i]; i+=2) {
    sprintf(temp, menutitlefmt, formats[i]);
    menuname = NewString(temp);
    menulist_AddToML(timeodayview_menulist, menuname, proc, formats[i+1], 0);
  }
  
  return(TRUE);
}


boolean timeodayview__InitializeObject(struct classheader *c, struct timeodayview *self)
{
/*
  Set up the data for each instance of the object.
*/
  self->ml = menulist_DuplicateML(timeodayview_menulist, self);

  if (!(self->cursor = cursor_Create(self))) return(FALSE);
  cursor_SetStandard(self->cursor, Cursor_Gunsight);

  return(TRUE);
}


void timeodayview__FinalizeObject(struct classheader *c, struct timeodayview *self)
{
  if (self->cursor) cursor_Destroy(self->cursor);
  self->cursor = NULL;
  if (self->ml) menulist_Destroy(self->ml);
  self->ml = NULL;
  return;
}



static void Redraw(struct timeodayview *self)
{
/*
  Redisplay this object.
*/
  struct timeoday *b = (struct timeoday *) timeodayview_GetDataObject(self);
  struct rectangle rect;
  char *tod;
  struct FontSummary *my_FontSummary;
  struct fontdesc *my_fontdesc;
  struct graphic *my_graphic;
  long new_width, new_height;
  
  timeodayview_EraseVisualRect(self);
  timeodayview_GetLogicalBounds(self, &rect);
  tod = timeoday_GetTod(b);
  my_graphic = (struct graphic *)timeodayview_GetDrawable(self);
  my_fontdesc= timeoday_GetFont(b);
  if (my_fontdesc) {
    fontdesc_StringSize(my_fontdesc, my_graphic, timeoday_GetTod(b), &new_width, &new_height);
    my_FontSummary =  fontdesc_FontSummary(my_fontdesc, my_graphic);
    if (my_FontSummary) new_height = my_FontSummary->maxHeight;
    if ((new_width > self->last_width) || (new_height > self->last_height)
	|| (new_width < self->last_width/FUDGEFACTOR) || (new_height < self->last_height/FUDGEFACTOR)) {
      timeodayview_WantNewSize(self,self);
    }
  }
  if (my_FontSummary) timeodayview_MoveTo(self, rect.left + rect.width/2, rect.top + my_FontSummary->maxHeight - my_FontSummary->maxBelow);

  if (my_fontdesc) timeodayview_SetFont(self, my_fontdesc);
  timeodayview_DrawString(self, tod,  graphic_BETWEENLEFTANDRIGHT | graphic_ATBASELINE);
}


void timeodayview__FullUpdate(struct timeodayview *self, enum view_UpdateType type, long left, long top, long width, long height)
{
/*
  Do an update.
*/
  if ((type == view_FullRedraw) || (type == view_LastPartialRedraw)) {
    self->need_full_update = TRUE;
    Redraw(self);
  }
}


void timeodayview__Update(struct timeodayview *self)
{
  Redraw(self);
}


struct view * timeodayview__Hit(struct timeodayview *self, enum view_MouseAction action, long x, long y, long numclicks)
{
/*
  Handle the button event.  Currently, semantics are:
*/
  timeodayview_WantInputFocus(self, self);
  return((struct view *)self);
}


void timeodayview__Print(struct timeodayview *self, FILE *file, char *processor, char *finalFormat, boolean topLevel)
{
    char *time_str;
    struct timeoday *time_do =  (struct timeoday *) timeodayview_GetDataObject(self);

    time_str = timeoday_GetTod(time_do);

    if (time_str == NULL) {
	fprintf(stderr, "?timeodayview_Print:  there doesn't appear to be any formatted time string.\n");
	fprintf(file, "\n");
    } else {
	fprintf(file, "%s\n", time_str);
    }
}


enum view_DSattributes timeodayview__DesiredSize(struct timeodayview *self, long width, long height, enum view_DSpass pass, long *desired_width, long *desired_height)
{
/* 
  Tell parent that this object  wants to be as big as the box around its
  text string.  For some reason IM allows resizing of this object. (BUG)
*/

  struct fontdesc *my_fontdesc;
  struct graphic *my_graphic;
  struct FontSummary *my_FontSummary;
  struct timeoday *b = (struct timeoday *) timeodayview_GetDataObject(self);

  my_graphic = (struct graphic *)timeodayview_GetDrawable(self);
  my_fontdesc= timeoday_GetFont(b);
  if (my_fontdesc) {
    fontdesc_StringSize(my_fontdesc, my_graphic, timeoday_GetTod(b), desired_width, desired_height);
    *desired_width *= FUDGEFACTOR;
    self->last_width = *desired_width;
    my_FontSummary =  fontdesc_FontSummary(my_fontdesc, my_graphic);
  }
  if (my_FontSummary)
    *desired_height = my_FontSummary->maxHeight;
  self->last_height = *desired_height;

  return(view_Fixed); /* (BUG) should disable user sizing, but this doesn't */
}


void timeodayview__GetOrigin(struct timeodayview *self, long width, long height, long *originX, long *originY)
{
/*
  We want this object to sit in-line with text, not below the baseline.
  Simply, we could negate the height as the originX, but then our
  text would be too high.  So, instead, we use the height of
  our font above the baseline
*/

  struct FontSummary *my_FontSummary;
  struct fontdesc *my_fontdesc;
  struct graphic *my_graphic;
  struct timeoday *b = (struct timeoday *) timeodayview_GetDataObject(self);

  my_graphic = (struct graphic *)timeodayview_GetDrawable(self);
  my_fontdesc= timeoday_GetFont(b);
  if (my_fontdesc) {
    my_FontSummary =  fontdesc_FontSummary(my_fontdesc, my_graphic);
  }

  *originX = 0;
  if (my_FontSummary)
    *originY = (my_FontSummary->maxHeight) - (my_FontSummary->maxBelow) + 1;
  return;
}


void timeodayview__PostMenus(struct timeodayview *self, struct menulist *ml)
{
/*
  Enable the menus for this object.
*/

  menulist_ClearChain(self->ml);
  if (ml) menulist_ChainAfterML(self->ml, ml, ml);
  super_PostMenus(self, self->ml);
}

