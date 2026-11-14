/* jpeg.c - routines for reading and writing jpeg images */

/* Copyright Carnegie Mellon University, 1993 1994 - All rights reserved */

/*
 * This file is derived from a file with the following copyrights:

 ****************************************************************
 * Portions Copyright 1989, 1990, 1991, 1992 by John Bradley and
 *                                The University of Pennsylvania
 *
 * Permission to use, copy, and distribute for non-commercial purposes,
 * is hereby granted without fee, providing that the above copyright
 * notice appear in all copies and that both the copyright notice and this
 * permission notice appear in supporting documentation. 
 *
 * The software may be modified for your own purposes, but modified versions
 * may not be distributed.
 *
 * This software is provided "as is" without any expressed or implied warranty.
 *
 * The author may be contacted via:
 *    US Mail:   John Bradley
 *               GRASP Lab, Room 301C
 *               3401 Walnut St.  
 *               Philadelphia, PA  19104
 *
 *    Phone:     (215) 898-8813
 *    EMail:     bradley@cis.upenn.edu       
 *********************************************************

 * Permission has been granted by the above copyright holder to distribute 
 * this derived file under the following conditions:

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

#ifdef NORCSID
#define NORCSID
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/atk/basics/common/RCS/jpeg.c,v 1.16 1994/03/17 20:23:23 wjh Exp $";
#endif


#include <andrewos.h>
#include <setjmp.h>
#include <image.ih>
#include <jpeg.eh>
/* class.h's boolean is an int, the same as libjpeg's own. */
#define HAVE_BOOLEAN
#include <jpeglib.h>

/* The codec is the system libjpeg (libjpeg-turbo, see
   config/darwin/system.mcr), not the 1992 IJG v4 library that was
   vendored in overhead/image/jpeg, which couldn't decode progressive
   JPEGs. */

int LoadJFIF(struct jpeg *jpeg, char *fname, FILE *f);
static int writeJFIF(FILE *fp, struct jpeg *jpeg, int quality);

/* libjpeg's default error_exit calls exit(); jump back to the caller
   instead. */
struct jpegerr {
    struct jpeg_error_mgr pub;
    jmp_buf jmp;
};

static void JpegErrorExit(j_common_ptr cinfo)
{
    struct jpegerr *err = (struct jpegerr *) cinfo->err;
    char msg[JMSG_LENGTH_MAX];

    (*cinfo->err->format_message)(cinfo, msg);
    fprintf(stderr, "jpeg: %s\n", msg);
    longjmp(err->jmp, 1);
}

/* Corrupt-data warnings stay quiet, as they did with the old library. */
static void JpegOutputMessage(j_common_ptr cinfo)
{
}

static void JpegErrorInit(struct jpegerr *err)
{
    jpeg_std_error(&err->pub);
    err->pub.error_exit = JpegErrorExit;
    err->pub.output_message = JpegOutputMessage;
}

long jpeg__WriteNative(struct jpeg *jpeg, FILE *file, char *filename)
{
    FILE *fp;
    int rv;

    if(!(fp = file) &&
       !(fp = fopen(filename, "w"))) {
	printf("jpeg: couldn't open file %s\n", filename);
	return(-1);
    }
    /* libjpeg takes greyscale or 24-bit rows */
    switch(jpeg_Type(jpeg)) {
	case IBITMAP:
	    jpeg_Bit2Grey(jpeg);
	    break;
	case IRGB:
	    jpeg_Expand(jpeg);
	    break;
    }
    rv = writeJFIF(fp, jpeg, jpeg->saveQuality);
    if(!file)
	fclose(fp);
    return(rv);
}

/*******************************************/
int jpeg__Load(struct jpeg *jpeg, char *fullname, FILE *fp)
{
    return(LoadJFIF(jpeg, fullname, fp));
}

/* Reads a JPEG from f, or from fname if f is NULL.  Closes only a file
   it opened itself: the old version also closed the caller's f, which
   every caller then closed again. */
int LoadJFIF(struct jpeg *jpeg, char *fname, FILE *f)
{
    struct jpeg_decompress_struct cinfo;
    struct jpegerr jerr;
    FILE *volatile in = f;
    unsigned char *volatile row = NULL;
    JSAMPROW rowp;
    byte *out;
    long w, h, x;
    int i, ncomp;

    if (in == NULL) {
	if (fname == NULL || (in = fopen(fname, "r")) == NULL)
	    return(-1);
    }
    cinfo.err = &jerr.pub;
    JpegErrorInit(&jerr);
    if (setjmp(jerr.jmp)) {
	jpeg_destroy_decompress(&cinfo);
	if (row) free(row);
	if (in != f) fclose(in);
	return(-1);
    }
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, in);
    jpeg_read_header(&cinfo, TRUE);
    switch (cinfo.jpeg_color_space) {
	case JCS_GRAYSCALE:
	    cinfo.out_color_space = JCS_GRAYSCALE;
	    break;
	case JCS_CMYK:
	case JCS_YCCK:
	    /* libjpeg won't convert these to RGB; done below */
	    cinfo.out_color_space = JCS_CMYK;
	    break;
	default:
	    cinfo.out_color_space = JCS_RGB;
	    break;
    }
    jpeg_start_decompress(&cinfo);

    w = cinfo.output_width;
    h = cinfo.output_height;
    ncomp = cinfo.output_components;
    if (cinfo.out_color_space == JCS_GRAYSCALE)
	jpeg_newGreyImage(jpeg, w, h, 8);
    else
	jpeg_newTrueImage(jpeg, w, h);
    out = jpeg_Data(jpeg);
    row = (unsigned char *) malloc(w * ncomp);
    if (row == NULL)
	longjmp(jerr.jmp, 1);

    while (cinfo.output_scanline < h) {
	rowp = row;
	jpeg_read_scanlines(&cinfo, &rowp, 1);
	if (cinfo.out_color_space == JCS_CMYK) {
	    /* Adobe's CMYK JPEGs (the APP14 marker) store inverted
	       values, so ink is low rather than high. */
	    for (x = 0; x < w; x++) {
		unsigned char *p = row + 4 * x;
		int c = p[0], m = p[1], yel = p[2], k = p[3];

		if (!cinfo.saw_Adobe_marker) {
		    c = 255 - c; m = 255 - m; yel = 255 - yel; k = 255 - k;
		}
		*out++ = c * k / 255;
		*out++ = m * k / 255;
		*out++ = yel * k / 255;
	    }
	}
	else {
	    memcpy(out, row, w * ncomp);
	    out += w * ncomp;
	}
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    free(row);
    if (in != f)
	fclose(in);

    if (jpeg_Type(jpeg) == IGREYSCALE) {
	jpeg_RGBUsed(jpeg) = jpeg_RGBSize(jpeg) = 256;
	for (i = 0; i < 256; i++)
	    jpeg_RedPixel(jpeg, i) =
	      jpeg_GreenPixel(jpeg, i) =
	      jpeg_BluePixel(jpeg, i) = i << 8;
    }
    return(0);
}

/* Writes jpeg, which must be greyscale or 24-bit, to fp. */
static int writeJFIF(FILE *fp, struct jpeg *jpeg, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpegerr jerr;
    JSAMPROW rowp;
    byte *data = jpeg_Data(jpeg);

    cinfo.err = &jerr.pub;
    JpegErrorInit(&jerr);
    if (setjmp(jerr.jmp)) {
	jpeg_destroy_compress(&cinfo);
	return(-1);
    }
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);
    cinfo.image_width = jpeg_Width(jpeg);
    cinfo.image_height = jpeg_Height(jpeg);
    if (jpeg_Type(jpeg) == IGREYSCALE) {
	cinfo.input_components = 1;
	cinfo.in_color_space = JCS_GRAYSCALE;
    }
    else {
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
    }
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
	rowp = data + (long) cinfo.next_scanline * cinfo.image_width * cinfo.input_components;
	jpeg_write_scanlines(&cinfo, &rowp, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return(0);
}

long jpeg__Read(struct jpeg *self, FILE *file, long id)
{
    if(jpeg_Load(self, NULL, file) == 0) {
	jpeg_Compress(self);
	return(dataobject_NOREADERROR);
    }
    else
	return(dataobject_BADFORMAT);
}

long jpeg__Write(struct jpeg *self, FILE *file, long writeID, int level)
{
    return(super_Write(self, file, writeID, level));
}

/* Any JPEG starts FF D8 FF.  This used to also demand a JFIF APP0
   segment, which rejected the Exif files cameras and phones write. */
int jpeg__Ident(struct classheader *classID, char *fullname)
{
    FILE *f;
    boolean ret = FALSE;

    if((f = fopen(fullname, "r")) != NULL) {
	if(getc(f) == 0xFF && getc(f) == 0xD8 && getc(f) == 0xFF)
	    ret = TRUE;
	fclose(f);
    }
    return(ret);
}

boolean jpeg__InitializeObject(struct classheader *classID, struct jpeg *self)
{
    self->saveQuality = 75;
    return(TRUE);
}

void jpeg__FinalizeObject(struct classheader *classID, struct jpeg *self)
{}
