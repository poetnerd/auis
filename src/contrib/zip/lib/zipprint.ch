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
zipprint.H

  03/31/88	Created for ATK (TCP)
   08/20/90	Add Ensure_Line_Attributes method (SCG)
*/

#define  zipprint_VERSION		    1

struct zipprint_options
  {
  unsigned int				    invert		: 1;
  };

struct zipprint_states
  {
  unsigned int				    xxxx		: 1;
  };


class zipprint
  {
overrides:

methods:

  Set_Debug( boolean state );
  Print_Figure( zip_type_figure figure, zip_type_pane pane )					    returns long;
  Print_Image( zip_type_image image, zip_type_pane pane )					    returns long;
  Print_Stream( zip_type_stream stream, zip_type_pane pane )					    returns long;
  Print_Pane( zip_type_pane pane )						    returns long;
  Set_Print_Resolution( long resolution )				    returns long;
  Set_Print_Dimensions( float width, float height )				    returns long;
  Set_Print_Coordinates( zip_type_percent x_origin, zip_type_percent y_origin, zip_type_percent width, zip_type_percent height )	    returns long;
  Set_Print_Orientation( long orientation )				    returns long;
  Set_Print_Language( char *language )				    returns long;
  Set_Print_Processor( char *processor )				    returns long;
  Set_Print_Level( long level )					    returns long;
  Set_Print_File( FILE *file )					    returns long;

  Set_Data_Object( struct zip *data_object );
  Set_View_Object( struct zipview *view_object );
  Set_Line_Width( long line_width )					    returns long;
  Ensure_Line_Attributes( zip_type_figure figure )				    returns long;
  Set_Shade( long line_width )					    returns long;
  Move_To( int x, int y )						    returns long;
  Draw_To( long x, long y )						    returns long;
  Draw_Multi_Line( int count, int x, int y, zip_type_point_pairs points )			    returns long;
  Draw_String( int x, int y, char *string, long mode )				    returns long;
  Draw_Line( int x1, int y1, int x2, int y2 )					    returns long;
  Draw_Rectangle( long x1, long y1, long x2, long y2 )				    returns long;
  Draw_Round_Rectangle( long x1, long y1, long x2, long y2, long xr, long yr )		    returns long;
  Draw_Circle( int x, int y, int radius )					    returns long;
  Draw_Ellipse( int x, int y, int x_radius, int y_radius )			    returns long;
  Draw_Arc( long x, long y, long x_radius, long y_radius, long x_start, long y_start, long x_end, long y_end ) returns long;
  Arc_To( int x_center, int y_center, int x_radius, int y_radius, int x_start, int y_start, int x_end, int y_end ) returns long;
  Fill_Trapezoid( int x1, int y1, int x2, int y2, int l1, int l2, char pattern )		    returns long;

  Close_Path()							    returns long;
  Change_Font( struct fontdesc *font_index )					    returns long;
  Restore_Font()						    returns long;
  Try_Printing_Exception_Handler( zip_type_printing printing )			    returns long;

macromethods:

classprocedures:

  InitializeObject( struct zipprint *self )			    returns boolean;
  FinalizeObject( struct zipprint *self );


data:

  struct zip			*data_object;
  struct zipview		*view_object;
  struct zipprint_options	 options;
  struct zipprint_states	 states;
  };
