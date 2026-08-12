/* imgtest -- standalone decode-only regression driver for the `image`
   inset's importer classes (imagev/tif/faces/pbm/...). Loads a file
   through the real image_Load()/image_Duplicate()/image_Compress()
   sequence used by imagev.c's Import_Cmd, entirely off-screen (no
   X11), and reports the result as "KEY: value" lines so a test
   script can parse it. This isolates decode correctness from the
   separate X11 rendering path (xgraphic.c's imageToXImage()) -- a
   clean run here does not by itself prove an image will render
   correctly on screen, only that the importer produced sane pixel
   data.

   Usage: imgtest classname filename
     classname is the image subclass to instantiate directly, e.g.
     "imagev" (auto-detects format from file header) or "tif".
*/

#include <stdio.h>
#include <andrewos.h>
#include <class.h>
#include <image.ih>

#define class_StaticEntriesOnly
#include <observe.ih>
#include <proctbl.ih>
#include <dataobj.ih>
#undef class_StaticEntriesOnly

extern char *AndrewDir(char *str);

static const char *TypeName(unsigned int t)
{
	switch (t) {
	case 0: return "IBITMAP";
	case 1: return "IRGB";
	case 2: return "IGREYSCALE";
	case 3: return "ITRUE";
	default: return "UNKNOWN";
	}
}

/* Reports whether a pixel buffer is a single uniform byte value --
   the direct test for the historical "solid black/white" render bug
   at the data level. */
static void ReportBuffer(const char *prefix, unsigned char *data, long n)
{
	long i, minv = 255, maxv = 0;
	int uniform = 1;

	for (i = 0; i < n; i++) {
		if (data[i] != data[0]) uniform = 0;
		if (data[i] < minv) minv = data[i];
		if (data[i] > maxv) maxv = data[i];
	}
	printf("%s-BYTES: %ld\n", prefix, n);
	printf("%s-UNIFORM: %d\n", prefix, uniform);
	printf("%s-MIN: %ld\n", prefix, minv);
	printf("%s-MAX: %ld\n", prefix, maxv);
}

int main(int argc, char **argv)
{
	struct image *img, *dest;
	unsigned char *data;
	long n;

	if (argc != 3) {
		fprintf(stderr, "usage: imgtest classname filename\n");
		return 2;
	}

	class_Init(AndrewDir("/dlib/atk"));
	observable_StaticEntry;
	proctable_StaticEntry;
	dataobject_StaticEntry;

	img = (struct image *) class_NewObject(argv[1]);
	if (!img) {
		printf("NEWOBJECT-RC: 1\n");
		return 1;
	}

	/* Matches imagev.c's Import_Cmd call site exactly: fp is NULL,
	   the loader opens the file itself by name. */
	if (image_Load(img, argv[2], NULL) < 0) {
		printf("LOAD-RC: 1\n");
		return 1;
	}
	printf("LOAD-RC: 0\n");

	printf("TYPE: %s\n", TypeName(image_Type(img)));
	printf("WIDTH: %u\n", image_Width(img));
	printf("HEIGHT: %u\n", image_Height(img));
	printf("DEPTH: %u\n", image_Depth(img));
	printf("PIXLEN: %u\n", image_Pixlen(img));

	data = (unsigned char *) image_Data(img);
	if (!data) {
		printf("DATA-NULL: 1\n");
		return 1;
	}
	n = (long) image_Width(img) * image_Height(img) * (image_Pixlen(img) ? image_Pixlen(img) : 1);
	ReportBuffer("LOAD", data, n);

	/* Mirror Import_Cmd's post-Load sequence (atk/image/imagev.c):
	   image_Reset(image); image_Duplicate(newimage, image);
	   image_Compress(image); -- "image" is a fresh target, "newimage"
	   is what was just loaded. */
	dest = (struct image *) class_NewObject("image");
	if (!dest) {
		printf("NEWOBJECT2-RC: 1\n");
		return 1;
	}
	image_Reset(dest);
	image_Duplicate(img, dest);
	data = (unsigned char *) image_Data(dest);
	if (!data) {
		printf("DUP-DATA-NULL: 1\n");
		return 1;
	}
	n = (long) image_Width(dest) * image_Height(dest) * (image_Pixlen(dest) ? image_Pixlen(dest) : 1);
	printf("DUP-WIDTH: %u\n", image_Width(dest));
	printf("DUP-HEIGHT: %u\n", image_Height(dest));
	ReportBuffer("DUP", data, n);

	image_Compress(dest);
	data = (unsigned char *) image_Data(dest);
	if (!data) {
		printf("COMPRESS-DATA-NULL: 1\n");
		return 1;
	}
	n = (long) image_Width(dest) * image_Height(dest) * (image_Pixlen(dest) ? image_Pixlen(dest) : 1);
	printf("COMPRESS-WIDTH: %u\n", image_Width(dest));
	printf("COMPRESS-HEIGHT: %u\n", image_Height(dest));
	ReportBuffer("COMPRESS", data, n);

	return 0;
}
