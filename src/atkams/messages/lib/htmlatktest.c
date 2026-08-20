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
	htmlatktest.c -- standalone driver for htmlatk.c (Stage 3 of the
		     HTML-mail-rendering project). Not part of any library;
		     built only via the .test suffix rule (see this
		     directory's Imakefile). Unlike htmlparttest.c/
		     htmltexttest.c (Stage 1/2's drivers, ATK-independent),
		     this one links ATK's class system and must initialize
		     it headlessly -- no X11 display is opened, matching
		     src/atk/raster/convert/convrast.c's precedent (a real,
		     working non-GUI ClassProgramTarget in this tree) and
		     confirmed empirically during this task's Gate-1
		     research: struct text/table/style/image objects can
		     be created, edited, and read via class_Init() +
		     class_NewObject() alone, with no XOpenDisplay ever
		     called, as long as no *view* is ever instantiated.

	Usage:
	    htmlatktest.test dump <fixturefile>
		Parses <fixturefile> via htmlpart_Parse(), renders it via
		htmlatk_Render() into a freshly created "text" dataobject
		(no resolver -- images always placeholder, matching every
		other standalone test in this suite, see htmlatk.h's own
		note on why), then dumps a line-oriented, greppable
		representation of the resulting content: every character
		run's text (newlines shown as literal "\n" so the dump
		stays line-oriented) tagged with which styles cover it,
		and one line per embedded view (table/image) giving its
		class name and, for a table, its row/column count and a
		per-cell summary. Exits nonzero if htmlatk_Render() itself
		reports failure (the "ok" return) or if anything in the
		process crashes/aborts (letting the test harness's process
		exit code catch a hard crash the way html-parse-tests'/
		html-totext-tests' harnesses already do for Stage 1/2).

	    htmlatktest.test resolve <fixturefile> <cidmap>
		Same as "dump", but wires a resolver that reads <cidmap> --
		a small text file of "cid:VALUE PATH MIMETYPE" lines, one
		per resolvable image -- so a synthetic fixture can exercise
		the real image-embedding path (Gate 3/4) without needing an
		actual MIME envelope, since the real fixture corpus was
		harvested as bare HTML bodies with no attached image bytes
		to resolve against (see revival/tests/html-fixtures/
		README.md and htmlatk.h's resolver note).

	    htmlatktest.test linkat <fixturefile> <pos>
		Renders <fixturefile> the same way, then calls
		htmlatk_LinkAt() at character position <pos> and prints
		either "LINK: <url>" or "NOLINK".

	ANSI C (C89 prototypes) throughout, no scanf/sscanf/fscanf
	anywhere -- same policy as every other module in this project.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <andrewos.h>
#include <class.h>
#include <text.ih>
#include <envrment.ih>
#include <style.ih>
#include <table.ih>
#include <lset.ih>
#include <dataobj.ih>
#include <viewref.ih>
#include <observe.ih>
#include <proctbl.ih>
#include <filetype.ih>

#include <htmlpart.h>
#include <mimepart.h>
#include "htmlatk.h"

extern char *AndrewDir(char *str);

static void InitATK(void)
{
    class_Init(AndrewDir("/dlib/atk"));
    observable_StaticEntry;
    proctable_StaticEntry;
    dataobject_StaticEntry;
}

/* Raw byte read, no interpretation of the content at all -- the
   correct helper for arbitrary binary data (a fixture's referenced
   image bytes via TestResolver() below). readfile() (next) layers
   HTML-fixture-specific UTF-8-to-Latin1 handling on top of this for
   the one caller that actually wants it; using readfile() itself for
   binary image bytes was a real, silent bug here (found 2026-08-19,
   sniffing-fallback verification work) -- mimepart_Utf8ToLatin1()
   treats its input as UTF-8 text and replaces byte sequences that
   aren't valid UTF-8 with '?', which arbitrary binary data (a real
   JPEG's bytes almost certainly contain such sequences) triggers
   constantly, silently corrupting the image before it ever reached a
   decoder -- any "resolve" mode test against a real binary image was
   exercising corrupted bytes, not the real ones, until this split. */
static unsigned char *readfile_raw(const char *path, long *lenp)
{
    FILE *fp;
    unsigned char *buf = NULL;
    long alloced = 0, used = 0;
    size_t got;

    fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "htmlatktest.test: cannot open %s\n", path); exit(2); }
    for (;;) {
        if (used + 65536 > alloced) {
            long ncap = alloced ? alloced * 2 : 65536;
            unsigned char *nb;
            if (ncap < used + 65536) ncap = used + 65536;
            nb = (unsigned char *) realloc(buf, ncap);
            if (!nb) { fprintf(stderr, "htmlatktest.test: out of memory\n"); exit(2); }
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

static unsigned char *readfile(const char *path, long *lenp)
{
    unsigned char *buf = readfile_raw(path, lenp);
    long used = *lenp;

    /* Mirror RenderHtmlPart's own UTF-8-to-Latin1 conversion (text822.c)
       so this driver exercises the same real bug/fix that live-messages
       reproduction found: every real fixture here is genuinely UTF-8
       (harvested from real modern mail), and htmlpart_Parse()/
       htmlatk_Render() do not do this conversion themselves (by design
       -- see RenderHtmlPart's own comment on why that responsibility
       belongs at the integration layer, not inside Stage 1/3). Applied
       unconditionally rather than gated on a real charset= header,
       since every fixture file this driver reads is HTML-body-only
       (no MIME envelope) and was harvested from UTF-8 mail -- matches
       real-world usage, not a synthetic worst case. */
    {
        long convlen;
        unsigned char *conv = mimepart_Utf8ToLatin1(buf, used, &convlen);
        if (conv) { free(buf); buf = conv; used = convlen; }
    }

    *lenp = used;
    return buf;
}

/* ---- cid: resolver backed by a flat text map file, Gate 3/4 test-only ---- */

struct cidentry { char *cid; char *path; char *mimetype; };
static struct cidentry *CidMap = NULL;
static int CidMapCount = 0;

static void LoadCidMap(const char *path)
{
    FILE *fp = fopen(path, "r");
    char line[2000];
    if (!fp) { fprintf(stderr, "htmlatktest.test: cannot open cidmap %s\n", path); exit(2); }
    while (fgets(line, sizeof(line), fp)) {
        char *cid, *p, *mtpath, *mt;
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (!line[0] || line[0] == '#') continue;
        cid = line;
        p = strchr(cid, ' ');
        if (!p) continue;
        *p = '\0';
        mtpath = p + 1;
        p = strchr(mtpath, ' ');
        if (!p) continue;
        *p = '\0';
        mt = p + 1;
        CidMap = (struct cidentry *) realloc(CidMap, (CidMapCount + 1) * sizeof(struct cidentry));
        CidMap[CidMapCount].cid = strdup(cid);
        CidMap[CidMapCount].path = strdup(mtpath);
        CidMap[CidMapCount].mimetype = strdup(mt);
        ++CidMapCount;
    }
    fclose(fp);
}

static boolean TestResolver(void *rock, const char *src, unsigned char **bytesOut, long *lenOut, char **mimetypeOut)
{
    int i;
    (void) rock;
    for (i = 0; i < CidMapCount; ++i) {
        if (strcmp(CidMap[i].cid, src) == 0) {
            long len;
            unsigned char *bytes = readfile_raw(CidMap[i].path, &len);
            *bytesOut = bytes;
            *lenOut = len;
            *mimetypeOut = strdup(CidMap[i].mimetype);
            return TRUE;
        }
    }
    return FALSE;
}

/* ---- dump: walk dest's content + environment tree and print it ---- */

static void PrintEscaped(const char *s, long len)
{
    long i;
    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char) s[i];
        if (c == '\n') printf("\\n");
        else if (c == '\t') printf("\\t");
        else if (c < 32 || c == 127) printf("\\x%02x", c);
        else putchar(c);
    }
}

/* Dumps every environment_Style span (name + [start,start+length)) in
   the order they were created (nestedmark's own child list, walked
   via GetInnerMost at each successive position would be the "correct"
   traversal but is O(n^2)-ish for a printout; since this is a test
   dump, not production code, a simpler approach is used: for each
   character position where a NEW style becomes active, print a
   marker. Implemented by sampling environment_GetInnerMost at every
   position and printing a "STYLE-CHANGE" line whenever the innermost
   style's identity changes -- gives a compact, still-verifiable
   picture of run boundaries without needing nestedmark's internal
   layout. */
static void DumpStyledText(struct text *t)
{
    long len = text_GetLength(t);
    long i;
    struct environment *lastEnv = NULL;
    long runStart = 0;

    printf("TEXTLEN: %ld\n", len);
    printf("BEGIN-CONTENT\n");
    for (i = 0; i <= len; ++i) {
        struct environment *env = (i < len) ? environment_GetInnerMost(t->rootEnvironment, i) : NULL;
        int changed = (i == len) || (env != lastEnv);
        if (i > 0 && changed) {
            char c = text_GetChar(t, runStart);
            printf("RUN[%ld,%ld) style=", runStart, i);
            if (lastEnv && lastEnv->type == environment_Style && lastEnv->data.style && lastEnv->data.style->name) {
                printf("%s", lastEnv->data.style->name);
                {
                    char *href = style_GetAttribute(lastEnv->data.style, "href");
                    if (href) printf(" href=%s", href);
                    {
                        char *color = style_GetAttribute(lastEnv->data.style, "color");
                        if (color) printf(" color=%s", color);
                    }
                }
            } else if (lastEnv && lastEnv->type == environment_View) {
                printf("VIEW");
            } else {
                printf("(none)");
            }
            printf(" text=\"");
            {
                long k;
                for (k = runStart; k < i; ++k) {
                    char ch = text_GetChar(t, k);
                    PrintEscaped(&ch, 1);
                }
            }
            (void) c;
            printf("\"\n");
            runStart = i;
        }
        lastEnv = env;
    }
    printf("END-CONTENT\n");
}

/* Forward declaration: DumpTableCell (below) and DumpCellTextContent
   (further below) are mutually recursive -- a table_ImbeddedObject
   cell whose class is "text" (the general case htmlatk.c's
   BuildTableGrid() now uses for every non-empty cell, see htmlatk.h's
   judgment-call log) needs its content dumped the same way the
   top-level document's content is, including any further nested views
   (a nested <table>, which recurses back into DumpTableCell; an image,
   which is just noted). Without this, a cell mixing text and an <img>
   would print only "OBJECT class=text" with nothing to show the
   mixed content actually landed -- not an adequate proof this fix
   works. */
static void DumpCellTextContent(struct text *t, int indent);
static void DumpTableCell(struct table *T, int r, int c);

/* Dumps an lset tree (the new lset-table-reflow path's own root/leaf
   objects, htmlatk.c's BuildLsetGrid/RenderTableAsLset) -- mutually
   recursive with DumpTableCell/DumpCellTextContent above/below, since
   any of the three object shapes (table cell, cell's "text" content,
   lset leaf) can nest inside any other: a table-path cell's text can
   contain a nested <table> that itself has no colspan/rowspan and so
   routes to the lset path (an "lset" dataobject embedded inside a
   table cell's own "text" object), and symmetrically an lset leaf's
   "text" content can contain a nested <table> that DOES have spans and
   so routes back to the old table/spread path. Printed type/pct/
   viewname fields are exactly lset's own public struct fields (see
   src/atk/adew/lset.ch's data: section) -- this dump does not
   reimplement any lset/lpair layout math itself, just reads back what
   htmlatk.c's construction wrote, the same "prove the real object
   shape, not a parallel model of it" spirit DumpTableCell already
   uses for table cells. */
static void DumpLsetTree(struct lset *ls, int indent)
{
    if (!ls) { printf("%*s(null)\n", indent, ""); return; }
    printf("%*sLSET type=%d pct=%d nobar=%d", indent, "", ls->type, ls->pct, ls->nobar);
    if (ls->left || ls->right) {
        printf(" (split)\n");
        printf("%*sLEFT:\n", indent + 2, "");
        DumpLsetTree((struct lset *) ls->left, indent + 4);
        printf("%*sRIGHT:\n", indent + 2, "");
        DumpLsetTree((struct lset *) ls->right, indent + 4);
        return;
    }
    if (!ls->dobj) { printf(" LEAF (empty)\n"); return; }
    printf(" LEAF class=%s viewname=%s\n", class_GetTypeName(ls->dobj), ls->viewname);
    if (strcmp(class_GetTypeName(ls->dobj), "text") == 0) {
        struct text *ct = (struct text *) ls->dobj;
        printf("%*sCELL-TEXT-CONTENT len=%ld\n", indent + 2, "", text_GetLength(ct));
        DumpCellTextContent(ct, indent + 4);
    } else if (strcmp(class_GetTypeName(ls->dobj), "table") == 0) {
        struct table *T = (struct table *) ls->dobj;
        int r, c;
        printf("%*sNESTED-TABLE %dx%d\n", indent + 2, "", table_NumberOfRows(T), table_NumberOfColumns(T));
        for (r = 0; r < table_NumberOfRows(T); ++r)
            for (c = 0; c < table_NumberOfColumns(T); ++c)
                DumpTableCell(T, r, c);
    } else if (strcmp(class_GetTypeName(ls->dobj), "lset") == 0) {
        printf("%*sNESTED-LSET\n", indent + 2, "");
        DumpLsetTree((struct lset *) ls->dobj, indent + 4);
    }
}

static void DumpTableCell(struct table *T, int r, int c)
{
    struct cell *cell = table_GetCell(T, r, c);
    printf("  CELL[%d,%d] ", r, c);
    switch (cell->celltype) {
        case table_EmptyCell:
            printf("EMPTY joined=%d\n", (int) table_IsJoinedToAnother(T, r, c));
            break;
        case table_TextCell:
            /* htmlatk.c's BuildTableGrid() no longer produces this
               cell type itself (see htmlatk.h's judgment-call log),
               but table.c's own format still supports it, so this
               dump still handles it for any hand-authored/legacy
               table content that reaches this test driver. */
            printf("TEXT \"%s\"\n", cell->interior.TextCell.textstring ? cell->interior.TextCell.textstring : "");
            break;
        case table_ValCell:
            printf("VAL\n");
            break;
        case table_ImbeddedObject:
            printf("OBJECT class=%s\n",
                   cell->interior.ImbeddedObject.data ? class_GetTypeName(cell->interior.ImbeddedObject.data) : "(null)");
            if (cell->interior.ImbeddedObject.data
                && strcmp(class_GetTypeName(cell->interior.ImbeddedObject.data), "table") == 0) {
                struct table *inner = (struct table *) cell->interior.ImbeddedObject.data;
                int ir, ic;
                printf("    NESTED-TABLE %dx%d\n", table_NumberOfRows(inner), table_NumberOfColumns(inner));
                for (ir = 0; ir < table_NumberOfRows(inner); ++ir)
                    for (ic = 0; ic < table_NumberOfColumns(inner); ++ic)
                        DumpTableCell(inner, ir, ic);
            } else if (cell->interior.ImbeddedObject.data
                && strcmp(class_GetTypeName(cell->interior.ImbeddedObject.data), "text") == 0) {
                struct text *ct = (struct text *) cell->interior.ImbeddedObject.data;
                printf("    CELL-TEXT-CONTENT len=%ld\n", text_GetLength(ct));
                DumpCellTextContent(ct, 4);
            } else if (cell->interior.ImbeddedObject.data
                && strcmp(class_GetTypeName(cell->interior.ImbeddedObject.data), "lset") == 0) {
                /* A nested <table> inside this (table-path) cell had no
                   colspan/rowspan of its own, so htmlatk.c's routing
                   put it on the new lset path -- see DumpLsetTree's own
                   comment on why both nesting directions are real. */
                printf("    NESTED-LSET\n");
                DumpLsetTree((struct lset *) cell->interior.ImbeddedObject.data, 4);
            }
            break;
    }
}

/* Dumps a cell's own "text" dataobject content -- same run/style-span
   walk DumpStyledText does at the top level, plus a scan for further
   embedded views (an inline <img>'s dataobject class is just named;
   a nested <table> recurses back into DumpTableCell). indent spaces
   are printed before every line so nested-cell-inside-nested-table
   output stays visually distinguishable from the top-level dump. */
static void DumpCellTextContent(struct text *t, int indent)
{
    long len = text_GetLength(t);
    long i;
    struct environment *lastEnv = NULL;
    long runStart = 0;

    for (i = 0; i <= len; ++i) {
        struct environment *env = (i < len) ? environment_GetInnerMost(t->rootEnvironment, i) : NULL;
        int changed = (i == len) || (env != lastEnv);
        if (i > 0 && changed) {
            printf("%*sRUN[%ld,%ld) style=", indent, "", runStart, i);
            if (lastEnv && lastEnv->type == environment_Style && lastEnv->data.style && lastEnv->data.style->name) {
                printf("%s", lastEnv->data.style->name);
            } else if (lastEnv && lastEnv->type == environment_View) {
                printf("VIEW");
            } else {
                printf("(none)");
            }
            printf(" text=\"");
            {
                long k;
                for (k = runStart; k < i; ++k) {
                    char ch = text_GetChar(t, k);
                    PrintEscaped(&ch, 1);
                }
            }
            printf("\"\n");
            runStart = i;
        }
        lastEnv = env;
    }

    for (i = 0; i < len; ++i) {
        struct environment *env = environment_GetInnerMost(t->rootEnvironment, i);
        if (env && env->type == environment_View && env->data.viewref) {
            struct dataobject *dob = env->data.viewref->dataObject;
            printf("%*sVIEW-AT[%ld] class=%s\n", indent, "", i, dob ? class_GetTypeName(dob) : "(null)");
            if (dob && strcmp(class_GetTypeName(dob), "table") == 0) {
                struct table *T = (struct table *) dob;
                int r, c;
                printf("%*sNESTED-TABLE %dx%d\n", indent + 2, "", table_NumberOfRows(T), table_NumberOfColumns(T));
                for (r = 0; r < table_NumberOfRows(T); ++r)
                    for (c = 0; c < table_NumberOfColumns(T); ++c)
                        DumpTableCell(T, r, c);
            } else if (dob && strcmp(class_GetTypeName(dob), "lset") == 0) {
                printf("%*sNESTED-LSET\n", indent + 2, "");
                DumpLsetTree((struct lset *) dob, indent + 4);
            }
        }
    }
}

/* Walks the whole document once more, this time just to find and
   print embedded views (tables/images) by scanning for
   environment_View spans the same STYLE-CHANGE-detection way
   DumpStyledText does for styles. */
static void DumpViews(struct text *t)
{
    long len = text_GetLength(t);
    long i;
    for (i = 0; i < len; ++i) {
        struct environment *env = environment_GetInnerMost(t->rootEnvironment, i);
        if (env && env->type == environment_View && env->data.viewref) {
            struct dataobject *dob = (struct dataobject *) env->data.viewref;
            /* struct viewref wraps a dataobject; ATK's own convention
               (see envrment.ch's union) -- print what we can safely
               introspect without a live view. */
            printf("VIEW-AT[%ld] (viewref present)\n", i);
            (void) dob;
        }
    }
}

static int do_dump(const char *fixture, htmlatk_ImageResolver resolver)
{
    long len;
    unsigned char *data = readfile(fixture, &len);
    struct htmlnode *root;
    struct text *t;
    boolean ok;
    long inserted = 0;

    InitATK();

    root = htmlpart_Parse(data, len);
    free(data);

    t = (struct text *) class_NewObject("text");
    if (!t) { fprintf(stderr, "htmlatktest.test: could not create text object\n"); return 2; }

    ok = htmlatk_Render(t, 0, root, resolver, NULL, &inserted);
    htmlpart_Free(root);

    printf("RENDER-OK: %d\n", (int) ok);
    printf("INSERTED: %ld\n", inserted);
    DumpStyledText(t);
    DumpViews(t);

    /* Also walk for table views specifically, with a full cell dump --
       DumpViews above only proves a view slot exists; this finds
       actual table objects for structural verification. */
    {
        long i;
        long tlen = text_GetLength(t);
        for (i = 0; i < tlen; ++i) {
            struct environment *env = environment_GetInnerMost(t->rootEnvironment, i);
            if (env && env->type == environment_View && env->data.viewref) {
                struct dataobject *dob = env->data.viewref->dataObject;
                if (dob && strcmp(class_GetTypeName(dob), "table") == 0) {
                    struct table *T = (struct table *) dob;
                    int r, c;
                    printf("TABLE-AT[%ld] %dx%d\n", i, table_NumberOfRows(T), table_NumberOfColumns(T));
                    for (r = 0; r < table_NumberOfRows(T); ++r)
                        for (c = 0; c < table_NumberOfColumns(T); ++c)
                            DumpTableCell(T, r, c);
                } else if (dob && strcmp(class_GetTypeName(dob), "lset") == 0) {
                    printf("LSET-AT[%ld]\n", i);
                    DumpLsetTree((struct lset *) dob, 2);
                }
            }
        }
    }

    return ok ? 0 : 1;
}

/* htmlatktest.test writeds <fixturefile> <outfile> -- renders
   <fixturefile> exactly like "dump" does (same InitATK/htmlpart_Parse/
   htmlatk_Render sequence, no resolver), but instead of the internal
   greppable dump format, calls the real text_Write() to emit an actual
   ATK datastream to <outfile> -- openable directly in ez, independent
   of messages/text822/captions.do entirely. Ad-hoc debugging aid (not
   part of the normal test suite) added to isolate a live-messages-only
   rendering-corruption report: if ez shows the same corruption on this
   file, the bug is in ATK's own table/text view code (general, not
   messages-specific); if ez renders cleanly, the bug is specific to
   messages' own display context. writeID is a fixed nonzero constant
   (1L) -- correct per text__Write's own contract (it just needs to
   differ from whatever this fresh object's zero-initialized writeID
   field already holds so the write actually proceeds), not fetched
   from im_GetWriteID() since that would need an extra header this
   standalone driver doesn't otherwise include. */
/* resolver may be NULL (original writeds behavior, no image fetch) or
   TestResolver (writedsr mode below) -- lets a synthetic cidmap-backed
   fixture round-trip real local image bytes into the emitted
   datastream, so `ez` can open it and show whatever htmlatk_Render's
   own image-embedding path actually produced, independent of
   messages/network entirely. */
static int do_writeds(const char *fixture, const char *outpath,
    boolean (*resolver)(void *, const char *, unsigned char **, long *, char **))
{
    long len;
    unsigned char *data = readfile(fixture, &len);
    struct htmlnode *root;
    struct text *t;
    boolean ok;
    long inserted = 0;
    FILE *out;

    InitATK();

    root = htmlpart_Parse(data, len);
    free(data);

    t = (struct text *) class_NewObject("text");
    if (!t) { fprintf(stderr, "htmlatktest.test: could not create text object\n"); return 2; }

    ok = htmlatk_Render(t, 0, root, resolver, NULL, &inserted);
    htmlpart_Free(root);

    printf("RENDER-OK: %d\n", (int) ok);
    printf("INSERTED: %ld\n", inserted);

    out = fopen(outpath, "w");
    if (!out) { fprintf(stderr, "htmlatktest.test: could not open %s for writing\n", outpath); return 2; }
    text_Write(t, out, 1L, 0);
    fclose(out);
    printf("WROTE: %s\n", outpath);

    return ok ? 0 : 1;
}

/* htmlatktest.test roundtrip <dsfile> -- the write-side counterpart to
   "writeds": loads a real ATK datastream file back into live objects
   via the same generic filetype_Lookup()+class_NewObject()+
   dataobject_Read() sequence real ATK apps use to open a file (see
   e.g. src/atk/adew/cel.c's cel__ReadFile()), then dumps the resulting
   structure with the exact same DumpStyledText()/DumpViews()/table-
   walk logic "dump" uses. Ad-hoc debugging aid (not part of the normal
   test suite): isolates whether a live-messages/ez rendering-
   corruption report is a Read()-time bug (this dump itself would show
   wrong dimensions/garbled cell content even though nothing here ever
   touches X11 or a view) versus a draw-time-only bug (this dump comes
   back clean, meaning the corruption must be introduced later, when a
   view actually gets established/drawn on screen). */
static int do_roundtrip(const char *dsfile)
{
    FILE *f;
    char *objectName;
    long objectID;
    struct dataobject *obj;
    long result;

    InitATK();

    f = fopen(dsfile, "r");
    if (!f) { fprintf(stderr, "htmlatktest.test: could not open %s\n", dsfile); return 2; }

    objectName = filetype_Lookup(f, NULL, &objectID, NULL);
    if (!objectName) { fprintf(stderr, "htmlatktest.test: filetype_Lookup could not identify %s\n", dsfile); fclose(f); return 2; }
    printf("CLASS: %s\n", objectName);
    printf("OBJECTID: %ld\n", objectID);

    obj = (struct dataobject *) class_NewObject(objectName);
    if (!obj) { fprintf(stderr, "htmlatktest.test: could not create a %s object\n", objectName); fclose(f); return 2; }

    result = dataobject_Read(obj, f, objectID);
    fclose(f);
    printf("READ-RESULT: %ld\n", result);

    if (strcmp(objectName, "text") == 0) {
        struct text *t = (struct text *) obj;
        long i;
        long tlen;
        DumpStyledText(t);
        DumpViews(t);
        tlen = text_GetLength(t);
        for (i = 0; i < tlen; ++i) {
            struct environment *env = environment_GetInnerMost(t->rootEnvironment, i);
            if (env && env->type == environment_View && env->data.viewref) {
                struct dataobject *dob = env->data.viewref->dataObject;
                if (dob && strcmp(class_GetTypeName(dob), "table") == 0) {
                    struct table *T = (struct table *) dob;
                    int r, c;
                    printf("TABLE-AT[%ld] %dx%d\n", i, table_NumberOfRows(T), table_NumberOfColumns(T));
                    for (r = 0; r < table_NumberOfRows(T); ++r)
                        for (c = 0; c < table_NumberOfColumns(T); ++c)
                            DumpTableCell(T, r, c);
                } else if (dob && strcmp(class_GetTypeName(dob), "lset") == 0) {
                    printf("LSET-AT[%ld]\n", i);
                    DumpLsetTree((struct lset *) dob, 2);
                }
            }
        }
    } else if (strcmp(objectName, "lset") == 0) {
        /* A datastream whose top-level object is itself an lset tree
           (e.g. a file extracted/hand-crafted the way Gate 1's probe
           wrote one directly, rather than one produced by this
           driver's own "writeds", which always wraps content in a
           "text" root -- see the branch above) -- dump it the same
           way a nested one gets dumped. */
        DumpLsetTree((struct lset *) obj, 0);
    }

    return 0;
}

static int do_linkat(const char *fixture, long pos)
{
    long len;
    unsigned char *data = readfile(fixture, &len);
    struct htmlnode *root;
    struct text *t;
    boolean ok;
    char *url;

    InitATK();
    root = htmlpart_Parse(data, len);
    free(data);
    t = (struct text *) class_NewObject("text");
    if (!t) { fprintf(stderr, "htmlatktest.test: could not create text object\n"); return 2; }
    ok = htmlatk_Render(t, 0, root, NULL, NULL, NULL);
    htmlpart_Free(root);
    if (!ok) { printf("RENDER-FAILED\n"); return 1; }

    url = htmlatk_LinkAt(t, pos);
    if (url) { printf("LINK: %s\n", url); free(url); }
    else printf("NOLINK\n");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "dump") == 0) {
        return do_dump(argv[2], NULL);
    }
    if (argc == 4 && strcmp(argv[1], "resolve") == 0) {
        LoadCidMap(argv[3]);
        return do_dump(argv[2], TestResolver);
    }
    if (argc == 4 && strcmp(argv[1], "linkat") == 0) {
        return do_linkat(argv[2], strtol(argv[3], NULL, 10));
    }
    if (argc == 4 && strcmp(argv[1], "writeds") == 0) {
        return do_writeds(argv[2], argv[3], NULL);
    }
    if (argc == 5 && strcmp(argv[1], "writedsr") == 0) {
        LoadCidMap(argv[3]);
        return do_writeds(argv[2], argv[4], TestResolver);
    }
    if (argc == 3 && strcmp(argv[1], "roundtrip") == 0) {
        return do_roundtrip(argv[2]);
    }
    fprintf(stderr, "usage: htmlatktest.test dump <fixturefile>\n");
    fprintf(stderr, "       htmlatktest.test resolve <fixturefile> <cidmap>\n");
    fprintf(stderr, "       htmlatktest.test linkat <fixturefile> <pos>\n");
    fprintf(stderr, "       htmlatktest.test writeds <fixturefile> <outfile>\n");
    fprintf(stderr, "       htmlatktest.test writedsr <fixturefile> <cidmap> <outfile>\n");
    fprintf(stderr, "       htmlatktest.test roundtrip <dsfile>\n");
    return 2;
}
