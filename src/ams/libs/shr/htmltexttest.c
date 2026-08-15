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

/*
	htmltexttest.c -- standalone driver for htmltext.c (Stage 2 of the
		     HTML-mail-rendering project). Not part of libmsshr.a;
		     built only via the .test suffix rule
		     (TestingOnlyTestingRule in this directory's
		     Imakefile):

			make htmltexttest.test

		     Usage:
			 htmltexttest.test totext <fixturefile>

		     Parses <fixturefile> (raw HTML bytes, same convention
		     as htmlparttest.c -- no MIME/RFC822 awareness here
		     either) via htmlpart_Parse(), renders the result via
		     htmltext_ToText(), and prints it to stdout framed by
		     "BEGIN-TEXT"/"END-TEXT" sentinel lines plus a
		     "TEXTLEN: <n>" line giving the exact byte length
		     between them -- revival/tests/html-totext-tests reads
		     the framed slice by byte count rather than by naive
		     line-splitting, since real rendered mail text can
		     itself validly contain any byte sequence (including,
		     in principle, a line equal to one of the sentinels).

		     ANSI C (C89 prototypes) throughout, no scanf/sscanf/
		     fscanf anywhere -- same policy as htmlpart.c and
		     htmltext.c.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <htmlpart.h>
#include <htmltext.h>

static unsigned char *readfile(const char *path, long *lenp)
{
    FILE *fp;
    unsigned char *buf = NULL;
    long alloced = 0, used = 0;
    size_t got;

    fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "htmltexttest.test: cannot open %s\n", path);
        exit(2);
    }
    for (;;) {
        if (used + 65536 > alloced) {
            long ncap = alloced ? alloced * 2 : 65536;
            unsigned char *nb;
            if (ncap < used + 65536) ncap = used + 65536;
            nb = (unsigned char *) realloc(buf, ncap);
            if (!nb) { fprintf(stderr, "htmltexttest.test: out of memory\n"); exit(2); }
            buf = nb;
            alloced = ncap;
        }
        got = fread(buf + used, 1, (size_t) (alloced - used), fp);
        if (got == 0) break;
        used += (long) got;
    }
    fclose(fp);
    *lenp = used;
    return buf;
}

static int do_totext(const char *fixture)
{
    long len;
    unsigned char *data = readfile(fixture, &len);
    struct htmlnode *root = htmlpart_Parse(data, len);
    char *text;
    size_t textlen;

    free(data);
    text = htmltext_ToText(root);
    htmlpart_Free(root);

    if (!text) {
        fprintf(stderr, "htmltexttest.test: htmltext_ToText returned NULL (contract violation)\n");
        return 2;
    }

    textlen = strlen(text);
    printf("PARSE-RC: 0\n");
    printf("TEXTLEN: %lu\n", (unsigned long) textlen);
    printf("BEGIN-TEXT\n");
    fwrite(text, 1, textlen, stdout);
    printf("END-TEXT\n");
    free(text);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "totext") == 0) {
        return do_totext(argv[2]);
    }
    fprintf(stderr, "usage: htmltexttest.test totext <fixturefile>\n");
    return 2;
}
