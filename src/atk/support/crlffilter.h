/* crlffilter.h
 *
 * The .ez datastream reader (lset.c/cel.c and their many siblings' Read()
 * routines) parses \begindata{...}...\enddata{...} framing byte-by-byte
 * assuming Unix LF-only line endings: the "eat the newline after a closing
 * brace" checks look for a bare '\n', and several classes locate field
 * boundaries with strchr(...,'\n'). A file saved with CRLF (e.g. touched
 * by a PC/Windows tool somewhere in its history -- see
 * Andrew-Girl-Scout-Sale.ez, August 2026) desyncs all of that: stray '\r'
 * bytes get vacuumed up as ordinary data, shifting field extraction and
 * corrupting things like lset refnames.
 *
 * Rather than patch every one of those copy-pasted parsing loops, this
 * filters CRLF down to LF at the two places a named datastream file is
 * actually opened for reading (bufferlist.c and buffer.c), via a
 * funopen() wrapper. A lone '\r' not followed by '\n' is left untouched,
 * since it isn't a line ending.
 *
 * Included (not compiled separately) into each of the two callers so
 * there's no cross-.do symbol dependency between the two dynamically
 * loaded modules -- see revival notes on .do silent underlinking.
 */
#ifndef crlffilter_DEFINED
#define crlffilter_DEFINED 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct crlffilter_cookie {
    FILE *real;
    char *filename;
    long strippedCount;
};

static int crlffilter_read(void *cookie, char *buf, int nbytes)
{
    struct crlffilter_cookie *ck = (struct crlffilter_cookie *) cookie;
    int n = 0;
    int c;

    while (n < nbytes) {
        c = getc(ck->real);
        if (c == EOF)
            break;
        if (c == '\r') {
            int c2 = getc(ck->real);
            if (c2 == '\n') {
                ck->strippedCount++;
                c = '\n';
            } else {
                if (c2 != EOF)
                    ungetc(c2, ck->real);
            }
        }
        buf[n++] = (char) c;
    }
    return n;
}

static int crlffilter_close(void *cookie)
{
    struct crlffilter_cookie *ck = (struct crlffilter_cookie *) cookie;

    if (ck->strippedCount > 0) {
        fprintf(stderr,
            "%s: normalized %ld CRLF line ending%s while reading (file has \\r\\n, ATK datastreams expect \\n)\n",
            ck->filename ? ck->filename : "(datastream)",
            ck->strippedCount, ck->strippedCount == 1 ? "" : "s");
    }
    fclose(ck->real);
    if (ck->filename)
        free(ck->filename);
    free(ck);
    return 0;
}

/* Opens filename for datastream reading, transparently normalizing CRLF
 * to LF so the LF-only \begindata/\enddata parsers don't desync on a
 * PC-saved file. Returns NULL (errno set by fopen) on failure, exactly
 * like a plain fopen(filename, "r").
 *
 * Known tradeoff: fileno() on the returned stream does not identify a
 * real descriptor (funopen streams aren't fd-backed), so simpletext__Read's
 * id==0 fast path (raw read()/lseek() straight off the fd, used only when
 * opening a plain, non-datastream text file directly) falls back to its
 * character-at-a-time path instead. Still correct, just slower for very
 * large plain-text opens; datastream-framed reads (the actual CRLF hazard)
 * are unaffected since they never used that fast path to begin with.
 */
static FILE *crlffilter_fopen(char *filename)
{
    FILE *real;
    struct crlffilter_cookie *ck;

    real = fopen(filename, "r");
    if (real == NULL)
        return NULL;

    ck = (struct crlffilter_cookie *) malloc(sizeof(struct crlffilter_cookie));
    if (ck == NULL) {
        fclose(real);
        return NULL;
    }
    ck->real = real;
    ck->filename = strdup(filename);
    ck->strippedCount = 0;

    return funopen(ck, crlffilter_read, NULL, NULL, crlffilter_close);
}

#endif /* crlffilter_DEFINED */
