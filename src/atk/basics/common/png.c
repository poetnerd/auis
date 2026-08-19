/* png.c - routines for reading PNG images */

/* Copyright Carnegie Mellon University, 1988, 1991 - All rights reserved */

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

#ifdef NORCSID
#define NORCSID
static char rcsid[]="$Header$";
#endif

#include <andrewos.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <image.ih>
#include <png.eh>

/* Decode-only PNG reader. Deliberate scope cuts, all sharing the same
   root cause (image.ch's data model has no alpha plane and no wider-
   than-8-bit-per-channel storage):
     - Alpha channel (color types 4 and 6, and tRNS-based simple
       transparency) is decoded and then discarded; images are always
       flattened to opaque RGB/grey/palette. Revisit if/when the image/
       imagev view layer ever grows real alpha compositing.
     - 16-bit-per-channel samples are truncated to 8 bits (high byte
       of each big-endian sample) - matches the existing jpeg.c/gif.c
       convention of working in 8-bit-per-channel throughout.
     - Adam7 interlacing is not implemented; interlaced PNGs fail to
       load with dataobject_BADFORMAT rather than silently mis-decoding.
   None of this is a fundamental limit of the approach, just unstarted
   work - ordinary non-interlaced 8-bit truecolor/palette/grey PNGs
   (the overwhelming majority of real web/mail images) decode fully. */

#define PNG_CT_IHDR 0x49484452UL /* "IHDR" */
#define PNG_CT_PLTE 0x504c5445UL /* "PLTE" */
#define PNG_CT_IDAT 0x49444154UL /* "IDAT" */
#define PNG_CT_IEND 0x49454e44UL /* "IEND" */

static unsigned char PNGSignature[8] = {0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a};

static unsigned long ReadBE32(unsigned char *p)
{
    return (((unsigned long)p[0]) << 24) | (((unsigned long)p[1]) << 16) |
	   (((unsigned long)p[2]) << 8)  |  ((unsigned long)p[3]);
}

/* A simple growable byte buffer, used both for the concatenated IDAT
   payload (compressed) and for the inflated scanline data (raw). */
struct bytebuf {
    unsigned char *data;
    unsigned long len;
    unsigned long cap;
};

static void buf_init(struct bytebuf *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static boolean buf_append(struct bytebuf *b, unsigned char *src, unsigned long n)
{
    if (b->len + n > b->cap) {
	unsigned long newcap = b->cap ? b->cap * 2 : 4096;
	unsigned char *newdata;
	while (newcap < b->len + n)
	    newcap *= 2;
	newdata = (unsigned char *) realloc(b->data, newcap);
	if (!newdata)
	    return(FALSE);
	b->data = newdata;
	b->cap = newcap;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return(TRUE);
}

static void buf_free(struct bytebuf *b)
{
    if (b->data)
	free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

/* Inflate compressed's full contents (a complete zlib stream, per the
   PNG spec's IDAT concatenation rule) into a freshly grown bytebuf.
   Returns FALSE on any zlib error or premature end of stream. */
static boolean InflateAll(unsigned char *compressed, unsigned long compressedLen, struct bytebuf *out)
{
    z_stream strm;
    unsigned char outchunk[8192];
    int zstatus;

    memset(&strm, 0, sizeof(strm));
    if (inflateInit(&strm) != Z_OK)
	return(FALSE);

    strm.next_in = compressed;
    strm.avail_in = compressedLen;

    do {
	strm.next_out = outchunk;
	strm.avail_out = sizeof(outchunk);
	zstatus = inflate(&strm, Z_NO_FLUSH);
	if (zstatus != Z_OK && zstatus != Z_STREAM_END) {
	    inflateEnd(&strm);
	    return(FALSE);
	}
	if (!buf_append(out, outchunk, sizeof(outchunk) - strm.avail_out)) {
	    inflateEnd(&strm);
	    return(FALSE);
	}
    } while (zstatus != Z_STREAM_END && strm.avail_in > 0);

    inflateEnd(&strm);
    return(zstatus == Z_STREAM_END);
}

static int PaethPredictor(int a, int b, int c)
{
    int p = a + b - c;
    int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if (pa <= pb && pa <= pc)
	return(a);
    if (pb <= pc)
	return(b);
    return(c);
}

/* Undo the per-row PNG filters in place. raw holds height rows of
   (1 + rowbytes) bytes each (filter-type byte followed by the row's
   filtered bytes); on return each row's filter-type byte is left at 0
   (None) and the rowbytes that follow hold real unfiltered pixel
   data. bpp is the filter's "bytes per complete pixel" (minimum 1). */
static boolean Unfilter(unsigned char *raw, unsigned long height, unsigned long rowbytes, int bpp)
{
    unsigned long y;
    unsigned char *prior = NULL;

    for (y = 0; y < height; y++) {
	unsigned char *row = raw + y * (rowbytes + 1);
	int filtertype = row[0];
	unsigned char *cur = row + 1;
	unsigned long i;

	switch (filtertype) {
	case 0: /* None */
	    break;
	case 1: /* Sub */
	    for (i = 0; i < rowbytes; i++) {
		int left = (i >= (unsigned long) bpp) ? cur[i - bpp] : 0;
		cur[i] = (unsigned char)(cur[i] + left);
	    }
	    break;
	case 2: /* Up */
	    for (i = 0; i < rowbytes; i++) {
		int up = prior ? prior[i] : 0;
		cur[i] = (unsigned char)(cur[i] + up);
	    }
	    break;
	case 3: /* Average */
	    for (i = 0; i < rowbytes; i++) {
		int left = (i >= (unsigned long) bpp) ? cur[i - bpp] : 0;
		int up = prior ? prior[i] : 0;
		cur[i] = (unsigned char)(cur[i] + ((left + up) / 2));
	    }
	    break;
	case 4: /* Paeth */
	    for (i = 0; i < rowbytes; i++) {
		int left = (i >= (unsigned long) bpp) ? cur[i - bpp] : 0;
		int up = prior ? prior[i] : 0;
		int upleft = (prior && i >= (unsigned long) bpp) ? prior[i - bpp] : 0;
		cur[i] = (unsigned char)(cur[i] + PaethPredictor(left, up, upleft));
	    }
	    break;
	default:
	    return(FALSE); /* unknown filter type: corrupt or unsupported */
	}
	row[0] = 0;
	prior = cur;
    }
    return(TRUE);
}

/* Extract the n'th depth-bit sample (0-based, MSB-first packing) from
   a row of packed bytes. Used for bit depths 1, 2 and 4. */
static unsigned int GetPackedSample(unsigned char *rowdata, unsigned long n, int depth)
{
    unsigned long bitoff = n * (unsigned long) depth;
    unsigned char byte = rowdata[bitoff / 8];
    int shift = 8 - depth - (int)(bitoff % 8);
    return((byte >> shift) & ((1 << depth) - 1));
}

int png__Load(struct png *self, char *fullname, FILE *fp)
{
    FILE *f = fp;
    unsigned char sig[8];
    unsigned long width = 0, height = 0;
    int bitdepth = 0, colortype = -1, interlace = 0;
    boolean haveIHDR = FALSE;
    struct bytebuf idat, raw;
    unsigned char palette[256][3];
    unsigned int palettesize = 0;
    int channels, bpp, samplesPerPixel;
    unsigned long rowbytes;
    unsigned long y, x;
    int rc = -1;

    buf_init(&idat);
    buf_init(&raw);

    if (!f) {
	if (!(f = fopen(fullname, "r")))
	    return(-1);
    }

    if (fread(sig, 1, 8, f) != 8 || memcmp(sig, PNGSignature, 8) != 0)
	goto done;

    for (;;) {
	unsigned char lenbuf[4], typebuf[4];
	unsigned long length, type;
	unsigned char *chunkdata = NULL;

	if (fread(lenbuf, 1, 4, f) != 4 || fread(typebuf, 1, 4, f) != 4)
	    goto done;
	length = ReadBE32(lenbuf);
	type = ReadBE32(typebuf);

	if (length > 0) {
	    chunkdata = (unsigned char *) malloc(length);
	    if (!chunkdata || fread(chunkdata, 1, length, f) != length) {
		if (chunkdata) free(chunkdata);
		goto done;
	    }
	}

	if (type == PNG_CT_IHDR) {
	    if (length < 13) { if (chunkdata) free(chunkdata); goto done; }
	    width = ReadBE32(chunkdata);
	    height = ReadBE32(chunkdata + 4);
	    bitdepth = chunkdata[8];
	    colortype = chunkdata[9];
	    /* chunkdata[10] compression method, chunkdata[11] filter
	       method: both always 0 in every published PNG spec version,
	       nothing else to branch on. */
	    interlace = chunkdata[12];
	    haveIHDR = TRUE;
	} else if (type == PNG_CT_PLTE) {
	    unsigned int i;
	    palettesize = (unsigned int)(length / 3);
	    if (palettesize > 256) palettesize = 256;
	    for (i = 0; i < palettesize; i++) {
		palette[i][0] = chunkdata[i*3];
		palette[i][1] = chunkdata[i*3+1];
		palette[i][2] = chunkdata[i*3+2];
	    }
	} else if (type == PNG_CT_IDAT) {
	    if (length > 0 && !buf_append(&idat, chunkdata, length)) {
		if (chunkdata) free(chunkdata);
		goto done;
	    }
	} else if (type == PNG_CT_IEND) {
	    if (chunkdata) free(chunkdata);
	    fseek(f, 4, SEEK_CUR); /* CRC: unchecked, see design note in png.ch */
	    break;
	}
	/* All other chunk types (gAMA, cHRM, sRGB, tRNS, tEXt, ...) are
	   ancillary to a basic decode and are silently skipped. */

	if (chunkdata) free(chunkdata);
	fseek(f, 4, SEEK_CUR); /* skip this chunk's CRC, unchecked */
    }

    if (!haveIHDR || width == 0 || height == 0 || idat.len == 0)
	goto done;
    if (interlace != 0)
	goto done; /* Adam7 not implemented, see design note above */
    switch (bitdepth) {
    case 1: case 2: case 4: case 8: case 16: break;
    default: goto done;
    }

    switch (colortype) {
    case 0: channels = 1; break; /* grey */
    case 2: channels = 3; break; /* RGB */
    case 3: channels = 1; break; /* palette */
    case 4: channels = 2; break; /* grey+alpha */
    case 6: channels = 4; break; /* RGBA */
    default: goto done;
    }
    if (colortype == 3 && bitdepth == 16)
	goto done; /* not a legal PNG combination */

    samplesPerPixel = channels;
    rowbytes = (width * (unsigned long) channels * (unsigned long) bitdepth + 7) / 8;
    bpp = (channels * bitdepth + 7) / 8;
    if (bpp < 1) bpp = 1;

    if (!InflateAll(idat.data, idat.len, &raw))
	goto done;
    buf_free(&idat);

    if (raw.len < height * (rowbytes + 1))
	goto done;
    if (!Unfilter(raw.data, height, rowbytes, bpp))
	goto done;

    if (colortype == 3) {
	unsigned int i, maxpal;
	png_newRGBImage(self, (unsigned int) width, (unsigned int) height, bitdepth);
	/* newRGBImage's RGBMap only has depthToColors(bitdepth) == 1<<bitdepth
	   slots; a malformed PLTE claiming more entries than the bit depth
	   allows must not be allowed to write past it. */
	maxpal = 1u << bitdepth;
	if (palettesize > maxpal)
	    palettesize = maxpal;
	for (i = 0; i < palettesize; i++) {
	    png_RedPixel(self, i)   = ((unsigned long) palette[i][0]) << 8;
	    png_GreenPixel(self, i) = ((unsigned long) palette[i][1]) << 8;
	    png_BluePixel(self, i)  = ((unsigned long) palette[i][2]) << 8;
	}
	png_RGBUsed(self) = palettesize;
	for (y = 0; y < height; y++) {
	    unsigned char *rowdata = raw.data + y * (rowbytes + 1) + 1;
	    unsigned char *outrow = png_Data(self) + y * width;
	    for (x = 0; x < width; x++) {
		unsigned int idx = (bitdepth == 8) ? rowdata[x] : GetPackedSample(rowdata, x, bitdepth);
		outrow[x] = (unsigned char) idx;
	    }
	}
    } else if (colortype == 0 || colortype == 4) {
	/* grey, or grey+alpha with alpha dropped */
	int outdepth = (bitdepth == 16) ? 8 : bitdepth;
	unsigned int i, greys;
	png_newGreyImage(self, (unsigned int) width, (unsigned int) height, outdepth);
	greys = 1 << outdepth;
	for (i = 0; i < greys; i++) {
	    unsigned long v = ((unsigned long) i) << (16 - outdepth);
	    png_RedPixel(self, i) = png_GreenPixel(self, i) = png_BluePixel(self, i) = v;
	}
	png_RGBUsed(self) = greys;
	for (y = 0; y < height; y++) {
	    unsigned char *rowdata = raw.data + y * (rowbytes + 1) + 1;
	    unsigned char *outrow = png_Data(self) + y * width;
	    for (x = 0; x < width; x++) {
		unsigned int v;
		if (bitdepth == 16) {
		    unsigned long sampleoff = x * (unsigned long) channels * 2;
		    v = rowdata[sampleoff]; /* high byte only: 16->8 truncation */
		} else if (bitdepth == 8) {
		    v = rowdata[x * channels];
		} else {
		    v = GetPackedSample(rowdata, x, bitdepth);
		}
		outrow[x] = (unsigned char) v;
	    }
	}
    } else {
	/* colortype 2 (RGB) or 6 (RGBA, alpha dropped) */
	png_newTrueImage(self, (unsigned int) width, (unsigned int) height);
	for (y = 0; y < height; y++) {
	    unsigned char *rowdata = raw.data + y * (rowbytes + 1) + 1;
	    unsigned char *outrow = png_Data(self) + y * width * 3;
	    for (x = 0; x < width; x++) {
		unsigned long sampleoff = x * (unsigned long) channels * (bitdepth == 16 ? 2 : 1);
		int stride = (bitdepth == 16) ? 2 : 1;
		outrow[x*3]   = rowdata[sampleoff];
		outrow[x*3+1] = rowdata[sampleoff + stride];
		outrow[x*3+2] = rowdata[sampleoff + stride*2];
	    }
	}
    }

    rc = 0;

done:
    buf_free(&idat);
    buf_free(&raw);
    if (!fp && f)
	fclose(f);
    return(rc);
}

long png__Read(struct png *self, FILE *file, long id)
{
    if (png_Load(self, NULL, file) == 0) {
	png_Compress(self);
	return(dataobject_NOREADERROR);
    }
    return(dataobject_BADFORMAT);
}

long png__Write(struct png *self, FILE *file, long writeID, int level)
{
    return(super_Write(self, file, writeID, level));
}

int png__Ident(struct classheader *classID, char *fullname)
{
    FILE *f;
    unsigned char sig[8];
    boolean ret = FALSE;

    if (f = fopen(fullname, "r")) {
	if (fread(sig, 1, 8, f) == 8 && memcmp(sig, PNGSignature, 8) == 0)
	    ret = TRUE;
	fclose(f);
    }
    return(ret);
}
