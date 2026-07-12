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
zip.ch

  11/09/87	Support "Absolute" as well as "Relative" sizing (TCP)
  03/31/88	Revise for ATK (TCP)
  11/17/88	Add Line_Width methods (TCP/SCG)
  05/10/89	Have Contextual_Figure_Line_Width return "unsigned char" (SCG)
  08/07/89	Override GetModified to check for changes to Imbedded objects (TCP)
   08/14/90	Added numerous color and line style method declarations (SCG)
*/

#define  zip_VERSION    2

#include "zip.h"

class zip : dataobject[dataobj]
  {

overrides:

  Read( FILE *file, long id )					    returns long;
  Write( FILE *file, long id, long level )			    returns long;
  GetModified()							    returns long;

methods:

  Set_Debug( boolean state );

  Create_Figure( zip_type_figure *figure, char *name, long type, zip_type_image image, zip_type_figure peer )		    returns long;
  Destroy_Figure( zip_type_figure figure )					    returns long;
  Hook_Figure( zip_type_figure figure, zip_type_figure peer_figure )				    returns long;
  Unhook_Figure( zip_type_figure figure )					    returns long;
  Set_Figure_Name( zip_type_figure figure, char *name )				    returns long;
  Set_Figure_Text( zip_type_figure figure, char *text )				    returns long;
  Set_Figure_Pattern( zip_type_figure figure, char pattern )				    returns long;
  Set_Figure_Shade( zip_type_figure figure, long shade )				    returns long;
  Set_Figure_Line_Width( zip_type_figure figure, long width )			    returns long;
  Set_Figure_Line_Dash( zip_type_figure figure, char *pattern, int offset, short type ) returns long;
  Set_Figure_Line_Cap( zip_type_figure figure, short cap )			    returns long;
  Set_Figure_Line_Join( zip_type_figure figure, short join )			    returns long;
  Set_Figure_Line_Color( zip_type_figure figure, double red, double green, double blue )    returns long;
  Set_Figure_FillFG_Color( zip_type_figure figure, double red, double green, double blue )    returns long;
  Set_Figure_FillBG_Color( zip_type_figure figure, double red, double green, double blue )    returns long;
  Set_Figure_Font( zip_type_figure figure, char *font_name )				    returns long;
  Set_Figure_Mode( zip_type_figure figure, long mode )				    returns long;
  Set_Figure_Point( zip_type_figure figure, long point, zip_type_point x, zip_type_point y )			    returns long;
  Adjust_Figure_Point_Suite( zip_type_figure figure, zip_type_point x_delta, zip_type_point y_delta )		    returns long;
  Change_Figure_Point( zip_type_figure figure, long old_x, long old_y, long new_x, long new_y )	    returns long;
  Remove_Figure_Point( zip_type_figure figure, long old_x, long old_y )			    returns long;
  Add_Figure_Point( zip_type_figure figure, long new_x, long new_y )			    returns long;
  Figure( char *name )						    returns struct zip_figure *;
  Image_Figure( struct zip_image *image, char *name )		    returns struct zip_figure *;
  Stream_Figure( struct zip_stream *stream, char *name )	    returns struct zip_figure *;

  Create_Peer_Image( zip_type_image *image, char *name, zip_type_stream stream, zip_type_image peer )		    returns long;
  Create_Inferior_Image( zip_type_image *image, char *name, zip_type_stream stream, zip_type_image superior )	    returns long;
  Destroy_Image( zip_type_image image )					    returns long;
  Hook_Peer_Image( zip_type_image image, zip_type_image peer_image )				    returns long;
  Hook_Inferior_Image( zip_type_image image, zip_type_image superior_image )			    returns long;
  Unhook_Image( zip_type_image image )						    returns long;
  Image_Left_Peer( zip_type_image image )					    returns struct zip_image *;
  Set_Image_Name( zip_type_image image, char *name )					    returns long;
  Set_Image_Text( zip_type_image image, char *text )					    returns long;
  Set_Image_Pattern( zip_type_image image, char pattern )				    returns long;
  Set_Image_Shade( zip_type_image image, long shade )				    returns long;
  Set_Image_Line_Width( zip_type_image image, long width )				    returns long;
  Set_Image_Line_Dash( zip_type_image image, char *pattern, int offset, short type ) returns long;
  Set_Image_Line_Cap( zip_type_image image, short cap )			    returns long;
  Set_Image_Line_Join( zip_type_image image, short join )			    returns long;
  Set_Image_Line_Color( zip_type_image image, double red, double green, double blue )    returns long;
  Set_Image_FillFG_Color( zip_type_image image, double red, double green, double blue )    returns long;
  Set_Image_FillBG_Color( zip_type_image image, double red, double green, double blue )    returns long;
  Set_Image_Font( zip_type_image image, char *font_name )				    returns long;
  Superior_Image_Pattern( zip_type_image image )				    returns char;
  Superior_Image_Shade( zip_type_image image )					    returns char;
  Superior_Image_Line_Width( zip_type_image image )				    returns unsigned char;
  Superior_Image_Line_Dash( zip_type_image image, char **pattern, int *offset, short *type ) returns long;
  Superior_Image_Line_Cap( zip_type_image image )			    returns short;
  Superior_Image_Line_Join( zip_type_image image )			    returns short;
  Superior_Image_Line_Color( zip_type_image image )			    returns struct zip_color *;
  Superior_Image_FillFG_Color( zip_type_image image )			    returns struct zip_color *;
  Superior_Image_FillBG_Color( zip_type_image image )			    returns struct zip_color *;
  Superior_Image_Text( zip_type_image image )					    returns char *;
  Superior_Image_Font( zip_type_image image )					    returns struct fontdesc *;
  Adjust_Image_Point_Suite( zip_type_image image, zip_type_point x_delta, zip_type_point y_delta )		    returns long;
  Image( char *name )						    returns struct zip_image *;
  Stream_Image( zip_type_stream stream, char *image_name )				    returns struct zip_image *;
  Next_Image( zip_type_image image )						    returns struct zip_image *;

  Open_Stream( struct zip_stream **stream, char *name, long mode )  returns long;
  Close_Stream( struct zip_stream *stream )			    returns long;
  Read_Stream( struct zip_stream *stream )			    returns long;
  Write_Stream( struct zip_stream *stream )			    returns long;
  Create_Stream( struct zip_stream **stream, char *name, long mode )	    returns long;
  Set_Stream_Name( zip_type_stream stream, char *name )				    returns long;
  Set_Stream_Pattern( zip_type_stream stream, char pattern )				    returns long;
  Set_Stream_Line_Width( zip_type_stream stream, long width )			    returns long;
  Set_Stream_Line_Dash( zip_type_stream stream, char *pattern, int offset, short type ) returns long;
  Set_Stream_Line_Cap( zip_type_stream stream, short cap )			    returns long;
  Set_Stream_Line_Join( zip_type_stream stream, short join )			    returns long;
  Set_Stream_Line_Color( zip_type_stream stream, double red, double green, double blue )    returns long;
  Set_Stream_FillFG_Color( zip_type_stream stream, double red, double green, double blue )    returns long;
  Set_Stream_FillBG_Color( zip_type_stream stream, double red, double green, double blue )    returns long;
  Set_Stream_Text( zip_type_stream stream, char *text )				    returns long;
  Set_Stream_Font( zip_type_stream stream, char *font_name )				    returns long;
  Set_Stream_Source( struct zip_stream *stream, char *name )	    returns long;
  Stream( char *name )						    returns struct zip_stream *;

  Contextual_Figure_Pattern( zip_type_figure figure )				    returns char;
  Contextual_Figure_Shade( zip_type_figure figure )				    returns char;
  Contextual_Figure_Line_Width( zip_type_figure figure )			    returns unsigned char;
  Contextual_Figure_Line_Dash( zip_type_figure figure, char **pattern, int *offset, short *type );
  Contextual_Figure_Line_Join( zip_type_figure figure ) returns short;
  Contextual_Figure_Line_Cap( zip_type_figure figure ) returns short;
  Contextual_Figure_Line_Color( zip_type_figure figure, double *red, double *green, double *blue ) returns long;
  Contextual_Figure_FillFG_Color( zip_type_figure figure, double *red, double *green, double *blue ) returns long;
  Contextual_Figure_FillBG_Color( zip_type_figure figure, double *red, double *green, double *blue ) returns long;


  /****  Following Facilities For Sub-Class (Internal) Usage Only  ****/

  Allocate_Color_Values()					    returns struct zip_color_values *;
  Allocate_Color()						    returns struct zip_color *;
  Define_Font( char *font_name, short *font_index )				    returns struct fontdesc *;
  Read_Figure( zip_type_figure figure )						    returns long;
  Write_Figure( zip_type_figure figure )					    returns long;
  Parse_Figure_Point( zip_type_figure figure, zip_type_point *x, zip_type_point *y )				    returns long;
  Parse_Figure_Points( zip_type_figure figure )					    returns long;
  Parse_Figure_Attributes( zip_type_figure figure )				    returns long;
  Allocate_Figure_Points_Vector( zip_type_point_pairs *figure )			    returns long;
  Enlarge_Figure_Points_Vector( zip_type_point_pairs *figure )			    returns long;
  Set_Image_Extrema( zip_type_image image, zip_type_point x, zip_type_point y )				    returns long;
  Set_Stream_Extrema( zip_type_stream stream, zip_type_image image )				    returns long;
  Try_general_Exception_Handler()				    returns long;
  Try_Figure_Exception_Handler( zip_type_figure figure )			    returns long;
  Try_Image_Exception_Handler( zip_type_image image )				    returns long;
  Try_Stream_Exception_Handler( zip_type_stream stream )			    returns long;

macromethods:

  Figure_Name( figure )		    ((figure) ? (figure)->zip_figure_name : NULL )
  Figure_Type( figure )		    ((figure) ? (figure)->zip_figure_type : NULL )
  Figure_Text( figure )		    ((figure) ? (figure)->zip_figure_datum.zip_figure_text : NULL )
  Figure_Anchor( figure )	    ((figure) ? (figure)->zip_figure_datum.zip_figure_anchor : NULL )
  Figure_Pattern( figure )	    ((figure) ? (figure)->zip_figure_fill.zip_figure_pattern : NULL )
  Figure_Shade( figure )	    ((figure) ? (figure)->zip_figure_fill.zip_figure_shade : NULL )
  Figure_Image( figure )	    ((figure) ? (figure)->zip_figure_image : NULL )
  Figure_Zoom_Level( figure )	    ((figure) ? (figure)->zip_figure_zoom_level : NULL )
  Figure_Detail_Level( figure )	    ((figure) ? (figure)->zip_figure_detail_level : NULL )
  Next_Figure( figure )		    ((figure) ? (figure)->zip_figure_next : NULL )
  Figure_Root( image )		    ((image) ? (image)->zip_image_figure_anchor : NULL )

  Image_Root( stream )		    ((stream) ? stream->zip_stream_image_anchor : NULL )
  Image_Name( image )		    ((image) ? (image)->zip_image_name : NULL )
  Image_Text( image )		    ((image) ? (image)->zip_image_text : NULL )
  Image_Type( image )		    ((image) ? (image)->zip_image_type : NULL )
  Image_Pattern( image )	    ((image) ? (image)->zip_figure_fill.zip_image_pattern : NULL )
  Image_Shade( image )		    ((image) ? (image)->zip_figure_fill.zip_image_shade : NULL )
  Image_Datum( image )		    ((image) ? (image)->zip_image_client_data : NULL )
  Image_Zoom_Level( image )	    ((image) ? (image)->zip_image_zoom_level : NULL )
  Image_Detail_Level( image )	    ((image) ? (image)->zip_image_detail_level : NULL )
  Image_Superior( image )	    ((image) ? (image)->zip_image_superior : NULL )
  Image_Inferior( image )	    ((image) ? (image)->zip_image_inferior : NULL )
  Image_Right_Peer( image )	    ((image) ? (image)->zip_image_right_peer : NULL )
  Image_Least_X( image )	    ((image) ? (image)->zip_image_least_x : NULL )
  Image_Greatest_X( image )	    ((image) ? (image)->zip_image_greatest_x : NULL )
  Image_Least_Y( image )	    ((image) ? (image)->zip_image_least_y : NULL )
  Image_Greatest_Y( image )	    ((image) ? (image)->zip_image_greatest_y : NULL )

  Stream_Name( stream )		    ((stream) ? (stream)->zip_stream_name : NULL )

  Containing_Figure_Stream( figure ) \
    ((figure) ? (figure->zip_figure_image->zip_image_stream) : NULL)
  Containing_Figure_Image( figure ) \
    ((figure) ? (figure->zip_figure_image) : NULL)

  Containing_Image_Stream( image ) \
    ((image) ? (image->zip_image_stream) : NULL)
  Containing_Image_Image( image ) \
    ((image) ? (image->zip_image_superior) : NULL)

  Set_general_Exception_Handler( handler ) \
    {self->general_exception_handler = handler;}


classprocedures:

  InitializeClass() returns boolean;
  InitializeObject( struct zip *self )				    returns boolean;
  FinalizeObject( struct zip *self );

data:

  zip_type_stream		 stream;
  char				*stream_file_name;
  FILE				*write_stream_file;
  long				 write_stream_id;
  long				 write_stream_level;
  long				 id;
  long				 desired_view_width;
  long				 desired_view_height;
  char				 desired_view_metric;
  long				 object_width;
  long				 object_height;
  char				 object_metric;
  struct zipobject	       *((*objects)[]);
  long				 page_count;
  struct zip_stream_chain	*stream_anchor;
  struct zip_image		*image_anchor;
  struct zip_figure		*figure_anchor;
  struct zip_paths		*paths;
  struct zip_fonts		*fonts;
  long				 status;
  long				 status_addenda;
  char				*facility;
  long				(*general_exception_handler)();
  long				(*stream_exception_handler)();
  long				(*image_exception_handler)();
  long				(*figure_exception_handler)();
  long				(*message_acknowledger)();
  long				(*message_writer)();
  long				(*message_clearer)();
  };
