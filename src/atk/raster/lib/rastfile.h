/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
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


 

/* *********************************************** *\
*                                                   *
* Definition of the layout of a raster image file.  *
*                                                   *
\* *********************************************** */
 
#include <stdint.h>

/* On-disk fields are 4 bytes each (14-byte header total); this must stay
   fixed-width regardless of the size of "long" on the host, since the
   read/write code in oldrf.c fread/fwrites a hardcoded 14 bytes and
   htonl/htons-converts each field as 32/16 bit. */
struct RasterHeader {
        int32_t Magic;           /* should be RasterMagic */
        int32_t width;           /* Width in pixels */
        int32_t height;          /* Height in pixels */
        short depth;            /* number of bits per pixel */
};                      
     /* This heading structure is followed by the bits of the image,
        one row after another.  Bits are packed into 8-bit bytes, and
        each row is padded to a multiple of 8 bits. Bits in each row
        are left to right high-order bit to low-order bit */
 
     /* In the packed bit array, 0 is a black bit; 1 is a white bit */
 
#define RasterMagic 0xF10040BB
