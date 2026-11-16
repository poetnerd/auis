/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/* See htmlimgv.ch. */

#include <andrewos.h>
#include <class.h>
#include <image.ih>
#include <htmlimg.ih>
#include <htmlimgv.eh>

/* Offers wider than this are layout probes asking for a natural width
   (lsetv.c's NaturalWidth measures at an effectively unlimited width),
   not a real container: answer with the image's own width. */
#define FIT_MAX_OFFER 8192

#define ORIG(self) ((self)->header.imagev.orig)
#define BORDER(self) ((self)->header.imagev.bordersize)

boolean htmlimgview__InitializeObject(struct classheader *classID, struct htmlimgview *self)
{
    self->drawnw = 0;
    return TRUE;
}

/* The width to draw at when offered offer pixels of content width. */
static long FitWidth(struct htmlimgview *self, long offer)
{
    struct image *orig = ORIG(self);
    long natw = image_Width(orig), pct = 100, maxw = 0, w;

    if (class_IsTypeByName(class_GetTypeName(orig), "htmlimage")) {
	pct = htmlimage_GetPercent((struct htmlimage *) orig);
	maxw = htmlimage_GetMaxWidth((struct htmlimage *) orig);
    }
    if (offer <= 0 || offer > FIT_MAX_OFFER) w = natw;
    else w = offer * pct / 100;
    if (maxw > 0 && w > maxw) w = maxw;
    return (w < 1) ? 1 : w;
}

static long FitHeight(struct image *orig, long w)
{
    long natw = image_Width(orig), h;

    h = (image_Height(orig) * w + natw / 2) / natw;
    return (h < 1) ? 1 : h;
}

enum view_DSattributes htmlimgview__DesiredSize(struct htmlimgview *self, long width, long height,
                                                  enum view_DSpass pass, long *dWidth, long *dheight)
{
    struct image *orig = ORIG(self);
    long w;

    if (!orig || !image_Data(orig) || image_Width(orig) <= 0)
	return super_DesiredSize(self, width, height, pass, dWidth, dheight);
    w = FitWidth(self, width - 2 * BORDER(self));
    *dWidth = w + 2 * BORDER(self);
    *dheight = FitHeight(orig, w) + 2 * BORDER(self);
    return view_Fixed;
}

/* Returns a new true-color w x h copy of src, each output pixel the
   average of the source pixels its area covers (nearest pixel when
   enlarging).  Nearest-neighbour shrinking, as image_Zoom does, drops
   every other pixel of a double-resolution image and breaks up its
   lettering. */
static struct image *Resample(struct image *src, long w, long h)
{
    struct image *t = src, *out;
    long sw, sh, X, Y, x, y, x0, x1, y0, y1;
    unsigned long r, g, b, n;
    unsigned char *sp, *dp, *p;

    if (image_Type(src) != ITRUE) {
	t = image_Expand(src);
	if (!t) return NULL;
    }
    sw = image_Width(t);
    sh = image_Height(t);
    out = image_New();
    if (!out) {
	if (t != src) image_Destroy(t);
	return NULL;
    }
    image_newTrueImage(out, w, h);
    sp = image_Data(t);
    dp = image_Data(out);
    for (Y = 0; Y < h; ++Y) {
	y0 = Y * sh / h;
	y1 = (Y + 1) * sh / h;
	if (y1 <= y0) y1 = y0 + 1;
	if (y1 > sh) y1 = sh;
	for (X = 0; X < w; ++X) {
	    x0 = X * sw / w;
	    x1 = (X + 1) * sw / w;
	    if (x1 <= x0) x1 = x0 + 1;
	    if (x1 > sw) x1 = sw;
	    r = g = b = n = 0;
	    for (y = y0; y < y1; ++y)
		for (x = x0, p = sp + (y * sw + x0) * 3; x < x1; ++x, p += 3) {
		    r += p[0]; g += p[1]; b += p[2]; ++n;
		}
	    if (n == 0) n = 1;
	    *dp++ = (unsigned char) (r / n);
	    *dp++ = (unsigned char) (g / n);
	    *dp++ = (unsigned char) (b / n);
	}
    }
    if (t != src) image_Destroy(t);
    return out;
}

void htmlimgview__FullUpdate(struct htmlimgview *self, enum view_UpdateType type,
                             long left, long top, long width, long height)
{
    struct image *orig = ORIG(self);
    long w = htmlimgview_GetLogicalWidth(self) - 2 * BORDER(self);

    /* The width we were given is the one DesiredSize asked for, so it
       is already the fitted width: draw at exactly that. */
    if (orig && image_Data(orig) && image_Width(orig) > 0 && w > 0 && w != self->drawnw) {
	struct image *s = Resample(orig, w, FitHeight(orig, w));

	if (s) {
	    if (self->header.imagev.scaled) image_Destroy(self->header.imagev.scaled);
	    self->header.imagev.scaled = s;
	    self->drawnw = w;
	}
    }
    super_FullUpdate(self, type, left, top, width, height);
}
