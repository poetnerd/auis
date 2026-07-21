/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	mimeparttest.c -- Gate 2 standalone driver for mimepart.c.  Not
		     part of libmsshr.a; built only via the .test suffix
		     rule (TestingOnlyTestingRule in this directory's
		     Imakefile):

			make mimeparttest.test

		Usage:
		    mimeparttest.test dump       <fixturefile>
		    mimeparttest.test select     <fixturefile> <outfile>
		    mimeparttest.test decodepart <fixturefile> <partindex> <outfile>
		    mimeparttest.test htmlstrip  <fixturefile> <outfile>
		    mimeparttest.test charset    <fixturefile> <outfile>

		Every subcommand prints "KEY: value" lines to stdout for
		revival/tests/mime-display-tests to parse; where a
		subcommand's output could contain arbitrary bytes
		(decoded part bodies, stripped HTML, converted charset
		text) it is written to <outfile> instead and the caller
		compares that file's contents directly, exactly as
		imaptest.test's "fetch" subcommand streams a message body
		to a file rather than printing it.

		<fixturefile> is read with mimepart_ParseMessageFile(),
		which only looks at Content-Type/Content-Transfer-
		Encoding/Content-Disposition -- it does not understand
		To/From/Subject etc., so fixtures need only those three
		headers (plus whatever nested part headers the body
		itself contains).

		ANSI C (C89 prototypes) throughout, no scanf/sscanf/fscanf
		anywhere -- same policy as mimepart.c itself.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mimepart.h>

static const char *encname(int enc)
{
    switch (enc) {
    case MIMEPART_ENC_QP:     return "QP";
    case MIMEPART_ENC_BASE64: return "BASE64";
    default:                  return "NONE";
    }
}

static int dumpcount = 0;

static void dump_one(const struct mimepart *p, int parent)
{
    int myidx = dumpcount++;
    struct mimeparam *pm;
    const struct mimepart *c;

    printf("PART %d PARENT: %d\n", myidx, parent);
    printf("PART %d TYPE: %s\n", myidx, p->type);
    printf("PART %d ENCODING: %s\n", myidx, encname(p->encoding));
    printf("PART %d BODYLEN: %ld\n", myidx, p->bodylen);
    for (pm = p->params; pm; pm = pm->next) {
        printf("PART %d PARAM %s: %s\n", myidx, pm->name, pm->value);
    }
    printf("PART %d DISPOSITION: %s\n", myidx, p->disposition ? p->disposition : "(none)");
    for (pm = p->dispparams; pm; pm = pm->next) {
        printf("PART %d DISPPARAM %s: %s\n", myidx, pm->name, pm->value);
    }
    for (c = p->children; c; c = c->next) {
        dump_one(c, myidx);
    }
}

static FILE *openfixture(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "mimeparttest.test: cannot open %s\n", path);
        exit(2);
    }
    return fp;
}

static void writefile(const char *path, const unsigned char *data, long len)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "mimeparttest.test: cannot create %s\n", path);
        exit(2);
    }
    if (len > 0) fwrite(data, 1, (size_t) len, fp);
    fclose(fp);
}

static int do_dump(const char *fixture)
{
    FILE *fp = openfixture(fixture);
    struct mimepart *root = mimepart_ParseMessageFile(fp);
    fclose(fp);
    if (!root) { printf("PARSE-RC: 1\n"); return 1; }
    printf("PARSE-RC: 0\n");
    dumpcount = 0;
    dump_one(root, -1);
    printf("PARTCOUNT: %d\n", dumpcount);
    mimepart_Free(root);
    return 0;
}

static int do_select(const char *fixture, const char *outfile)
{
    FILE *fp = openfixture(fixture);
    struct mimepart *root = mimepart_ParseMessageFile(fp);
    const struct mimepart *sel;
    fclose(fp);
    if (!root) { printf("PARSE-RC: 1\n"); return 1; }
    printf("PARSE-RC: 0\n");
    if (mimepart_IsMultipart(root) && strcmp(root->type, "multipart/alternative") == 0) {
        sel = mimepart_SelectAlternative(root);
    } else {
        sel = root;
    }
    if (!sel) {
        printf("SELECTED-TYPE: (none)\n");
        writefile(outfile, (const unsigned char *) "", 0);
    } else {
        const char *charset = mimepart_GetParam(sel, "charset");
        printf("SELECTED-TYPE: %s\n", sel->type);
        printf("SELECTED-CHARSET: %s\n", charset ? charset : "(none)");
        printf("SELECTED-BODYLEN: %ld\n", sel->bodylen);
        writefile(outfile, sel->body, sel->bodylen);
    }
    mimepart_Free(root);
    return 0;
}

static const struct mimepart *find_indexed(const struct mimepart *p, int *idx, int target)
{
    const struct mimepart *c, *found;
    int myidx = (*idx)++;
    if (myidx == target) return p;
    for (c = p->children; c; c = c->next) {
        found = find_indexed(c, idx, target);
        if (found) return found;
    }
    return NULL;
}

static int do_decodepart(const char *fixture, int partindex, const char *outfile)
{
    FILE *fp = openfixture(fixture);
    struct mimepart *root = mimepart_ParseMessageFile(fp);
    const struct mimepart *p;
    int idx = 0;
    fclose(fp);
    if (!root) { printf("PARSE-RC: 1\n"); return 1; }
    printf("PARSE-RC: 0\n");
    p = find_indexed(root, &idx, partindex);
    if (!p) {
        printf("FOUND-RC: 1\n");
        mimepart_Free(root);
        return 1;
    }
    printf("FOUND-RC: 0\n");
    printf("DECODED-TYPE: %s\n", p->type);
    printf("DECODED-LEN: %ld\n", p->bodylen);
    writefile(outfile, p->body, p->bodylen);
    mimepart_Free(root);
    return 0;
}

static int do_htmlstrip(const char *fixture, const char *outfile)
{
    FILE *fp = openfixture(fixture);
    struct mimepart *root = mimepart_ParseMessageFile(fp);
    char *text;
    fclose(fp);
    if (!root) { printf("PARSE-RC: 1\n"); return 1; }
    printf("PARSE-RC: 0\n");
    text = mimepart_HtmlToText((const char *) root->body, root->bodylen);
    printf("STRIPPED-LEN: %lu\n", (unsigned long) strlen(text));
    writefile(outfile, (const unsigned char *) text, (long) strlen(text));
    free(text);
    mimepart_Free(root);
    return 0;
}

static int do_charset(const char *fixture, const char *outfile)
{
    FILE *fp = openfixture(fixture);
    struct mimepart *root = mimepart_ParseMessageFile(fp);
    unsigned char *conv;
    long convlen;
    fclose(fp);
    if (!root) { printf("PARSE-RC: 1\n"); return 1; }
    printf("PARSE-RC: 0\n");
    conv = mimepart_Utf8ToLatin1(root->body, root->bodylen, &convlen);
    printf("CONVERTED-LEN: %ld\n", convlen);
    writefile(outfile, conv, convlen);
    free(conv);
    mimepart_Free(root);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: mimeparttest.test dump|select|decodepart|htmlstrip|charset ...\n");
        return 2;
    }
    if (strcmp(argv[1], "dump") == 0) {
        return do_dump(argv[2]);
    } else if (strcmp(argv[1], "select") == 0 && argc == 4) {
        return do_select(argv[2], argv[3]);
    } else if (strcmp(argv[1], "decodepart") == 0 && argc == 5) {
        return do_decodepart(argv[2], atoi(argv[3]), argv[4]);
    } else if (strcmp(argv[1], "htmlstrip") == 0 && argc == 4) {
        return do_htmlstrip(argv[2], argv[3]);
    } else if (strcmp(argv[1], "charset") == 0 && argc == 4) {
        return do_charset(argv[2], argv[3]);
    }
    fprintf(stderr, "usage: mimeparttest.test dump|select|decodepart|htmlstrip|charset ...\n");
    return 2;
}
