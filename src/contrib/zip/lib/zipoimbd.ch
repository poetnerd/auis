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
zipoimbed.H

  03/31/88	Create for ATK (TCP)
  08/07/89	Add Object_Modified override (TCP)
*/

#define  zipoimbed_VERSION    1

#define  Data				(self->header.zipobject.data_object)
#define  Env				(Data->env)
#define  View				(self->header.zipobject.view_object)
#define  Edit				(self->header.zipobject.edit_object)
#define  Print				(self->header.zipobject.print_object)


class zipoimbed[zipoimbd] : ziporect
  {

overrides:

  Object_Icon()						returns char;
  Object_Icon_Cursor()					returns char;
  Object_Datastream_Code()				returns char;

  Object_Hit( zip_type_figure object, enum view_MouseAction action, long x, long y, long clicks )		returns struct view *;
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

classprocedures:
  InitializeObject( struct zipoimbed *self )		returns boolean;

data:
  boolean						no_outline;

  };

