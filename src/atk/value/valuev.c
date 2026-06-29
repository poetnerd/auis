/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1989 - All Rights Reserved      *
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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/atk/value/RCS/valuev.c,v 2.12 1992/12/15 21:47:14 rr2b R6tape $";
#endif


 

#include <class.h>
#include <value.h>
#include <atom.ih>
#include <atomlist.ih>
#include <graphic.ih>
#include <rm.ih>
#include <view.ih>
#include <valuev.eh>
#define False 0
#define True  1
static struct atomlist *  AL_background;
static struct atomlist *  AL_border;
static struct atomlist *  AL_border_width;
static struct atom *  A_graphic;
static struct atom *  A_long;
#define InternAtoms ( \
   AL_background = atomlist_StringToAtomlist("background") ,\
   AL_border = atomlist_StringToAtomlist("border") ,\
   AL_border_width = atomlist_StringToAtomlist("border-width") ,\
   A_graphic = atom_Intern("graphic") ,\
   A_long = atom_Intern("long") )

/**************** private functions ****************/

static void LookupParameters(struct valueview *self)
{
  struct resourceList parameters[4];
  parameters[0].name = AL_background;
  parameters[0].type = A_graphic;
  parameters[1].name = AL_border;
  parameters[1].type = A_graphic;
  parameters[2].name = AL_border_width;
  parameters[2].type = A_long;
  parameters[3].name = NULL;
  parameters[3].type = NULL;

  valueview_GetManyParameters(self,parameters,NULL,NULL);
  if (parameters[0].found)
    self->back = (struct graphic *)parameters[0].data;
  else
    self->back = valueview_WhitePattern(self);

  if (parameters[1].found)
    self->border = (struct graphic *)parameters[1].data;
  else
    self->border = valueview_BlackPattern(self);

  if (parameters[2].found)
    self->borderPixels = parameters[2].data;
  else
    self->borderPixels = 1;

  if (self->deactivationMask == NULL)
    self->deactivationMask = valueview_GrayPattern(self,1,10);

  valueview_LookupParameters(self);
}


static void DrawFromScratch(struct valueview *self)
{
  long x,y,width,height;

  x = valueview_GetLogicalLeft(self);
  y = valueview_GetLogicalTop(self);
  width = valueview_GetLogicalWidth(self);
  height = valueview_GetLogicalHeight(self);

/*  valueview_RestoreGraphicsState( self ); */
  valueview_ClearClippingRect( self );

  valueview_SetTransferMode( self, graphic_COPY );

  if (width < (self->borderPixels << 1) || height < (self->borderPixels << 1))
    {
      self->x = x;
      self->y = y;
      self->width = width;
      self->height = height;
    }
  else
    {
      if (self->borderPixels != 0)
	valueview_FillRectSize(self, x, y, width, height, self->border);
      self->x = x + self->borderPixels;
      self->y = y + self->borderPixels;
      self->width = width - (self->borderPixels << 1);
      self->height = height - (self->borderPixels << 1);
    }
  self->viewx = x;
  self->viewy = y;
  self->viewheight = height;
  self->viewwidth = width;

  valueview_FillRectSize(self, self->x, self->y, self->width,
			  self->height, self->back);
  valueview_DrawFromScratch(self, self->x, self->y, self->width,
			  self->height);

  if (!self->active)
    {
      valueview_SetTransferMode( self, self->deactivationTransferMode );
      valueview_FillRectSize(self, self->x, self->y,
			      self->width, self->height,
			      self->deactivationMask);
    }
  else if (self->mouseIsOnTarget)
    valueview_DrawHighlight(self);

}





/**************** class methods ****************/

boolean valueview__InitializeClass(struct classheader *classID)
{
  InternAtoms;
  return TRUE;
}


/**************** instance methods ****************/

boolean valueview__InitializeObject(struct classheader *classID, struct valueview *self)
{
  self->borderPixels = 1;
  self->back =  NULL;
  self->border =  NULL;
  self->deactivationMask = NULL;
  self->deactivationTransferMode = graphic_AND;
  self->active = True;
  self->mouseIsOnTarget = False;
  self->HasInputFocus = FALSE;
  self->updateq = updateq_New();
  updateq_SetView(self->updateq, self);
  return TRUE;
}


enum view_DSattributes valueview__DesiredSize(self, width, height, pass, desiredwidth, desiredheight)
struct valueview *self;
long width, height;
enum view_DSpass pass;
long *desiredwidth, *desiredheight;

{
    *desiredwidth = 75;
    *desiredheight = 75;
    return( view_WidthFlexible | view_HeightFlexible) ;
}

void valueview__RequestUpdateFunction(struct valueview *self, void (*fp)())
{
  updateq_EnqueueUpdateFunction( self->updateq, fp );
  valueview_WantUpdate( self, self );
}


void valueview__RequestFullUpdate(struct valueview *self)
{
  updateq_ClearUpdateQueue( self->updateq );
  valueview_RequestUpdateFunction( self, DrawFromScratch );
}


void valueview__FullUpdate(struct valueview *self, enum view_UpdateType type, long x, long y, long width, long height)
{
  if (/* type == view_NewParameters  ||  */ self->back == NULL) 
    LookupParameters(self);
  switch (type) {
      case view_FullRedraw:
      case view_LastPartialRedraw:
	  DrawFromScratch(self);
	  break;
      case view_PartialRedraw:
      case view_MoveNoRedraw:
      case view_Remove:
      default:
	  break;
  }
}
#define MAXPAR 36
static struct resourceList par[MAXPAR];
int setpar = 0;
void valueview__GetManyParameters(struct celview *self, struct resourceList *resources, struct atomlist *name, struct atomlist *class)
{
    super_GetManyParameters( self, resources, name, class );
    if(setpar){
	register int i;
	for(i = 0; i < MAXPAR; i++){
	    par[i] = *resources++;
	    if(par[i].name == NULL) break;
	}
    }
}
struct resourceList *valueview__GetDesiredParameters(struct valueview *self)
{
    setpar = TRUE;
    valueview_LookupParameters(self);
    setpar = FALSE;
    return par;
}
    
void valueview__LookupParameters(struct valueview *self)
{
  /* subclass responsability */
}

void valueview__ObservedChanged(struct valueview *self, struct observable *observed, long status)
{
  switch (status)
    {
    case observable_OBJECTDESTROYED:
      valueview_Destroyed(self);
      break;
    case value_NEWVALUE:
      valueview_NewValue(self);
      break;
    case value_NOCHANGE:
      valueview_NoChange(self);
      break;
    case value_BADVALUE:
      valueview_BadValue(self);
      break;
    case value_NOTACTIVE:
      valueview_Deactivate(self);
      break;
    case value_ACTIVE:
      valueview_Activate(self);
      break;
    default:
      valueview_Changed(self, status);
      break;
    }
}

static DA(struct valueview *self)
{
    valueview_DrawActivation(self);
}

static DDA(struct valueview *self)
{
    valueview_DrawDeactivation(self);
}
static DNV(struct valueview *self)
{
    valueview_DrawNewValue(self);
}

static DBV(struct valueview *self)
{
    valueview_DrawBadValue(self);
}
static DH(struct valueview *self)
{
    valueview_DrawHighlight(self);
}

static DDH(struct valueview *self)
{
    valueview_DrawDehighlight(self);
}

void valueview__Activate(struct valueview *self)
{
  if (!self->active)
    {
      self->active = True;
      valueview_RequestUpdateFunction(self, DA);
    }
}

void valueview__Deactivate(struct valueview *self)
{
  if (self->active)
    {
      self->active = False;
      valueview_RequestUpdateFunction( self,DDA);
    }
}

void valueview__Update(struct valueview *self)
{
/*  valueview_RestoreGraphicsState( self );*/
  updateq_ExecuteUpdateQueue( self->updateq );
}


void valueview__DrawFromScratch(struct valueview *self)
{
  /* Subclass responsibility */
}


void valueview__DrawActivation(struct valueview *self)
{
  DrawFromScratch( self );
}



void valueview__DrawDeactivation(struct valueview *self)
{
  valueview_ClearClippingRect( self );
  valueview_SetTransferMode( self, self->deactivationTransferMode );
  valueview_FillRectSize(self, self->x, self->y,
			  self->width, self->height,
			  self->deactivationMask);
}


void valueview__DrawNewValue(struct valueview *self)
{
  /* subclass responsibility */
}


void valueview__DrawBadValue(struct valueview *self)
{
  struct graphic * black;
  valueview_ClearClippingRect( self );
  black = valueview_BlackPattern(self);
  valueview_SetTransferMode(self, graphic_XOR);
  valueview_FillRectSize(self, self->viewx, self->viewy,
			  self->viewwidth, self->viewheight, black);
  valueview_FillRectSize(self, self->viewx, self->viewy,
			  self->viewwidth, self->viewheight, black);
}


void valueview__DrawNoChange(struct valueview *self)
{
  /* subclass responsibility */
}


void valueview__DrawDestroyed(struct valueview *self)
{
  /* subclass responsibility */
}


void valueview__DrawHighlight(struct valueview *self)
{
  /* subclass responsibility */
}


void valueview__DrawDehighlight(struct valueview *self)
{
  /* subclass responsibility */
}


void valueview__DeactivationMask(struct valueview *self, struct graphic *map)
{
  self->deactivationMask = map;
  if (!self->active)
    valueview_RequestUpdateFunction( self, DrawFromScratch );
}


void valueview__SetDeactivationTransfer(struct valueview *self, short mode)
{
  self->deactivationTransferMode = mode;
  if (!self->active)
    valueview_RequestUpdateFunction( self, DrawFromScratch );
}

void valueview__NewValue(struct valueview *self)
{
  valueview_RequestUpdateFunction( self,DNV);
}

void valueview__BadValue(struct valueview *self)
{
  valueview_RequestUpdateFunction( self,DBV);
}

void valueview__NoChange(struct valueview *self)
{
}

void valueview__Changed(struct valueview *self)
{
  /* subclasses should override this if there observed dataobject
     will change with a status other than NOCHANGE, NEWVALUE, or BADVALUE */
}

void valueview__Destroyed(struct valueview *self)
{
  self->header.view.dataobject = NULL;
  valueview_Deactivate(self);
}


struct valueview * valueview__DoHit(struct valueview *self, enum view_MouseAction type, long x, long y, long numberOfClicks)
{
    /* this is a subclass responsability */
    return self;
}

boolean valueview__OnTarget(struct valueview *self, long x, long y)
{
    return (self->viewx <= x && self->viewx + self->viewwidth >= x &&
	     self->viewy <= y && self->viewy + self->viewheight >= y);
}

/* Highlight the value that's handling events */
/* Dehighlight it and stop sending movement events if the mouse cursor */
/* wanders off.  Dehighlight it on an up transition. */
struct valueview * valueview__Hit(struct valueview *self, enum view_MouseAction type, long x, long y, long numberOfClicks)
{
#if 0
  register short sendEvent;
  if (self->active)
    {
      switch (type)
	{
	case view_NoMouseEvent:
	  sendEvent = False;
	  break;
	case view_LeftDown:
	  self->mouseState = valueview_LeftIsDown;
	  valueview_Highlight(self);
	  sendEvent = True;
	  break;
	case view_RightDown:
	  self->mouseState = valueview_RightIsDown;
	  valueview_Highlight(self);
	  sendEvent = True;
	  break;
	case view_LeftUp:
	case view_RightUp:
	  if (self->mouseIsOnTarget)
	    {
	      valueview_Dehighlight(self);
	      sendEvent = True;
	    }
	  else sendEvent = False;
	  break;
	case view_LeftMovement:
	case view_RightMovement:
	    if(valueview_OnTarget(self,x,y))
	    {
	      valueview_Highlight(self);
	      sendEvent = True;
	    }
	  else
	    {
	      valueview_Dehighlight(self);
	      sendEvent = False;
	    }
	  break;
	}
    }
  else
    sendEvent = False;
  if (sendEvent)
    return valueview_DoHit(self, type, x, y, numberOfClicks);
  else
    return self;
#else /* 0 */
  if(self->HasInputFocus == FALSE)
      valueview_WantInputFocus(self,self);

  if (self->active)
    {
      switch (type)
      {
	  case view_NoMouseEvent:
	      return self;
	      /* break; */
	  case view_LeftDown:
	  case view_RightDown:
	      valueview_Highlight(self);
	      break;
	  case view_LeftUp:
	  case view_RightUp:
	      valueview_Dehighlight(self);
	      break;
      }

    return valueview_DoHit(self, type, x, y, numberOfClicks);
  }
#endif /* 0 */
}


void valueview__Highlight(struct valueview *self)
{
  if (!self->mouseIsOnTarget)
    valueview_RequestUpdateFunction(self,DH);
  self->mouseIsOnTarget = True;
}


void valueview__Dehighlight(struct valueview *self)
{
  if (self->mouseIsOnTarget)
    valueview_RequestUpdateFunction(self,DDH);
  self->mouseIsOnTarget = False;
}



void valueview__GetCenter(struct valueview *self, long *x, long *y)
{
  /* this controls the location of a label. */
  *x = self->x + (self->width >> 1);
  *y = self->y + (self->height >> 1);
}
void valueview__ReceiveInputFocus(struct valueview *self)
{
    valueview_PostMenus(self,NULL);
    valueview_PostKeyState(self,NULL);
    self->HasInputFocus = TRUE;
}
void valueview__LoseInputFocus(struct valueview *self)
{
    self->HasInputFocus = FALSE;
}
