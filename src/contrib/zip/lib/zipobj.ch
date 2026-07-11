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


 

/*
 * P_R_P_Q_# (C) COPYRIGHT IBM CORPORATION 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 * REFER TO COPYRIGHT INSTRUCTIONS FORM NUMBER G120-2083
 */
/*
zipobject.ch

  03/31/88	Create for ATK (TCP)
  07/12/89	Added Object_Visible method (SCG)
  08/07/89	Add Object_Modified method (TCP)
   08/14/90	Add Contains stub. Remove Object_Visible stub (SCG)

*/

#define  zipobject_VERSION    1

#include "zip.ih"
#include "zipv.ih"
#include "zipedit.ih"
#include "zipprint.ih"


#define  CurrentFigure			((pane)->zip_pane_current_figure)
#define  CurrentImage			((pane)->zip_pane_current_image)
#define  CurrentStream			((pane)->zip_pane_current_stream)

class zipobject[zipobj] : view {

methods:

  Set_Debug( char state );
  Set_Data_Object( struct zip *data_object );
  Set_View_Object( struct zipview *view_object );
  Set_Edit_Object( struct zipedit *edit_object );
  Set_Print_Object( struct zipprint *print_object );

  Object_DesiredSize( long width, long height, enum view_DSpass pass, long *dWidth, long *dHeight )
							returns enum view_DSattributes;
  Object_FullUpdate( enum view_UpdateType type, long left, long top, long width, long height );
  Object_Update();
  Object_Hit( zip_type_figure object, enum view_MouseAction action, long x, long y, long clicks )		returns struct view *;

  Object_Icon()						returns char;
  Object_Icon_Font_Name()				returns char *;
  Object_Icon_Cursor()					returns char;
  Object_Icon_Cursor_Font_Name()			returns char *;
  Object_Datastream_Code()				returns char;
  Object_Modified( zip_type_figure object )				returns long;

  Build_Object( zip_type_pane pane, long action, long x, long y, long clicks, zip_type_point X, zip_type_point Y )	returns long;
  Destroy_Object( zip_type_figure object );
  Show_Object_Properties( zip_type_pane pane, zip_type_figure figure )		returns long;
  Read_Object( zip_type_figure object )					returns long;
  Read_Object_Stream( zip_type_figure object, FILE *file, long id )		returns long;
  Write_Object( zip_type_figure object )				returns long;
  Draw_Object( zip_type_figure object, zip_type_pane pane )				returns long;
  Clear_Object( zip_type_figure object, zip_type_pane pane )				returns long;
  Print_Object( zip_type_figure object, zip_type_pane pane )				returns long;
  Proximate_Object_Points( zip_type_figure object, zip_type_pane pane, zip_type_pixel x, zip_type_pixel y )		returns long;
  Within_Object( zip_type_figure object, zip_type_pane pane, zip_type_pixel x, zip_type_pixel y )			returns long;
  Enclosed_Object( zip_type_figure object, zip_type_pane pane, zip_type_pixel x, zip_type_pixel y, zip_type_pixel w, zip_type_pixel h )		returns boolean;
  Object_Enclosure( zip_type_figure object, zip_type_pane pane, zip_type_pixel *x, zip_type_pixel *y, zip_type_pixel *w, zip_type_pixel *h )	returns long;
  Highlight_Object_Points( zip_type_figure object, zip_type_pane pane )		returns long;
  Normalize_Object_Points( zip_type_figure object, zip_type_pane pane )		returns long;
  Expose_Object_Points( zip_type_figure object, zip_type_pane pane )			returns long;
  Hide_Object_Points( zip_type_figure object, zip_type_pane pane )			returns long;
  Set_Object_Point( zip_type_figure object, long point, zip_type_point x, zip_type_point y )		returns long;
  Object_Point( zip_type_figure object, long point, zip_type_point *x, zip_type_point *y )			returns long;
  Adjust_Object_Point_Suite( zip_type_figure object, zip_type_point x_delta, zip_type_point y_delta )	returns long;
  Set_Object_Shade( zip_type_figure object, long shade )			returns long;
  Set_Object_Font( zip_type_figure object, short font )			returns long;
  Contains( zip_type_figure figure, zip_type_pane pane, zip_type_pixel x, zip_type_pixel y )			returns boolean;

classprocedures:

  InitializeObject( struct zipobject *self )		returns boolean;

data:

  struct zip			 *data_object;
  struct zipview		 *view_object;
  struct zipedit		 *edit_object;
  struct zipprint		 *print_object;
  };

