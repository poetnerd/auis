/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	htmlparttest.c -- standalone driver for htmlpart.c.  Not part of
		     libmsshr.a; built only via the .test suffix rule
		     (TestingOnlyTestingRule in this directory's
		     Imakefile):

			make htmlparttest.test

		Usage:
		    htmlparttest.test dump      <fixturefile>
		    htmlparttest.test stats     <fixturefile>
		    htmlparttest.test counttag  <fixturefile> <tagname>
		    htmlparttest.test firstattr <fixturefile> <tagname> <attrname>
		    htmlparttest.test styleprop <fixturefile> <tagname> <propname>

		Every subcommand prints "KEY: value" lines to stdout for
		revival/tests/html-parse-tests to parse, same convention
		as mimeparttest.c's driver. <fixturefile> is read whole
		and handed to htmlpart_Parse() as raw bytes -- this
		driver does not know about MIME or RFC822 headers at
		all; fixtures here are raw (or already MIME/QP/base64-
		decoded, as revival/tests/html-fixtures already are)
		HTML bytes, nothing more.

		All subcommands other than "dump" walk the parsed tree
		via a single shared, explicitly-stacked (non-recursive)
		pre-order traversal (build_walk() below) -- deliberately
		mirroring htmlpart.c's own no-C-recursion discipline, so
		that even the test driver doesn't defeat the point of
		the several-thousand-level-deep regression fixture by
		blowing its own C stack while walking the result.

		ANSI C (C89 prototypes) throughout, no scanf/sscanf/
		fscanf anywhere -- same policy as htmlpart.c itself.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <htmlpart.h>

struct walkentry {
    struct htmlnode *node;
    long parent;
    long depth;
};

static struct walkentry *g_walk = NULL;
static long g_walkcount = 0, g_walkcap = 0;

static void walk_record(struct htmlnode *node, long parent, long depth)
{
    if (g_walkcount >= g_walkcap) {
        long ncap = g_walkcap ? g_walkcap * 2 : 256;
        struct walkentry *ni = (struct walkentry *) realloc(g_walk, ncap * sizeof(struct walkentry));
        if (!ni) { fprintf(stderr, "htmlparttest.test: out of memory\n"); exit(2); }
        g_walk = ni;
        g_walkcap = ncap;
    }
    g_walk[g_walkcount].node = node;
    g_walk[g_walkcount].parent = parent;
    g_walk[g_walkcount].depth = depth;
    ++g_walkcount;
}

struct pendingentry {
    struct htmlnode *node;
    long parent;
    long depth;
};

/* Pushes a sibling chain onto *stack (growable array used as a LIFO)
   in reverse order, so popping the stack visits them in original
   (document) order -- the standard iterative-preorder trick, used
   both for the top-level sibling list and for each element's children
   list. Never recurses. */
static void push_siblings_reversed(struct pendingentry **stack, long *count, long *cap,
                                    struct htmlnode *first, long parent, long depth)
{
    struct htmlnode **tmp = NULL;
    long tc = 0, tcap = 0, i;
    struct htmlnode *s;

    for (s = first; s; s = s->next) {
        if (tc >= tcap) {
            long ncap = tcap ? tcap * 2 : 64;
            struct htmlnode **ni = (struct htmlnode **) realloc(tmp, ncap * sizeof(struct htmlnode *));
            if (!ni) { fprintf(stderr, "htmlparttest.test: out of memory\n"); exit(2); }
            tmp = ni;
            tcap = ncap;
        }
        tmp[tc++] = s;
    }
    for (i = tc - 1; i >= 0; --i) {
        if (*count >= *cap) {
            long ncap = *cap ? *cap * 2 : 64;
            struct pendingentry *ni = (struct pendingentry *) realloc(*stack, ncap * sizeof(struct pendingentry));
            if (!ni) { fprintf(stderr, "htmlparttest.test: out of memory\n"); exit(2); }
            *stack = ni;
            *cap = ncap;
        }
        (*stack)[*count].node = tmp[i];
        (*stack)[*count].parent = parent;
        (*stack)[*count].depth = depth;
        ++(*count);
    }
    free(tmp);
}

/* Builds g_walk[] as a full pre-order traversal of the tree rooted at
   root (a top-level sibling list), entirely iteratively via an
   explicit heap stack -- see push_siblings_reversed above. */
static void build_walk(struct htmlnode *root)
{
    struct pendingentry *stack = NULL;
    long count = 0, cap = 0;

    g_walkcount = 0;
    push_siblings_reversed(&stack, &count, &cap, root, -1, 0);
    while (count > 0) {
        struct pendingentry pe = stack[--count];
        long myidx = g_walkcount;
        walk_record(pe.node, pe.parent, pe.depth);
        if (pe.node->type == HTMLPART_ELEMENT && pe.node->children) {
            push_siblings_reversed(&stack, &count, &cap, pe.node->children, myidx, pe.depth + 1);
        }
    }
    free(stack);
}

static unsigned char *readfile(const char *path, long *lenp)
{
    FILE *fp;
    unsigned char *buf = NULL;
    long alloced = 0, used = 0;
    size_t got;

    fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "htmlparttest.test: cannot open %s\n", path);
        exit(2);
    }
    for (;;) {
        if (used + 65536 > alloced) {
            long ncap = alloced ? alloced * 2 : 65536;
            unsigned char *nb;
            if (ncap < used + 65536) ncap = used + 65536;
            nb = (unsigned char *) realloc(buf, ncap);
            if (!nb) { fprintf(stderr, "htmlparttest.test: out of memory\n"); exit(2); }
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

/* Escapes for KV-line stdout: backslash/\n/\r/NUL get short escapes;
   anything outside printable 7-bit ASCII (0x80-0xFF -- the whole
   point of htmlpart's Latin-1 output, e.g. an entity-decoded
   accented letter) gets \xHH, so driver stdout is always plain ASCII
   -- the Python wrapper decodes it with the default UTF-8 text mode,
   which a raw Latin-1 byte would otherwise break. */
static void print_escaped(const char *s, long len)
{
    long i;
    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char) s[i];
        if (c == '\\') printf("\\\\");
        else if (c == '\n') printf("\\n");
        else if (c == '\r') printf("\\r");
        else if (c == '\0') printf("\\0");
        else if (c < 0x20 || c >= 0x7f) printf("\\x%02x", c);
        else putchar(c);
    }
}

static struct htmlnode *parse_fixture(const char *path)
{
    long len;
    unsigned char *data = readfile(path, &len);
    struct htmlnode *root = htmlpart_Parse(data, len);
    free(data);
    return root;
}

static int do_dump(const char *fixture)
{
    struct htmlnode *root = parse_fixture(fixture);
    long i;

    printf("PARSE-RC: 0\n");
    build_walk(root);
    printf("NODECOUNT: %ld\n", g_walkcount);
    for (i = 0; i < g_walkcount; ++i) {
        struct htmlnode *n = g_walk[i].node;
        printf("NODE %ld PARENT: %ld\n", i, g_walk[i].parent);
        printf("NODE %ld DEPTH: %ld\n", i, g_walk[i].depth);
        if (n->type == HTMLPART_ELEMENT) {
            struct htmlattr *a;
            printf("NODE %ld TYPE: ELEMENT\n", i);
            printf("NODE %ld TAG: %s\n", i, n->tag);
            for (a = n->attrs; a; a = a->next) {
                printf("NODE %ld ATTR %s: ", i, a->name);
                print_escaped(a->value, (long) strlen(a->value));
                printf("\n");
            }
        } else {
            printf("NODE %ld TYPE: TEXT\n", i);
            printf("NODE %ld TEXT: ", i);
            print_escaped(n->text, n->textlen);
            printf("\n");
        }
    }
    htmlpart_Free(root);
    return 0;
}

static int do_stats(const char *fixture)
{
    struct htmlnode *root = parse_fixture(fixture);
    long i, maxdepth = -1;

    printf("PARSE-RC: 0\n");
    build_walk(root);
    for (i = 0; i < g_walkcount; ++i) {
        if (g_walk[i].depth > maxdepth) maxdepth = g_walk[i].depth;
    }
    printf("NODECOUNT: %ld\n", g_walkcount);
    printf("MAXDEPTH: %ld\n", maxdepth);
    htmlpart_Free(root);
    return 0;
}

static int do_counttag(const char *fixture, const char *tagname)
{
    struct htmlnode *root = parse_fixture(fixture);
    long i, n = 0;

    printf("PARSE-RC: 0\n");
    build_walk(root);
    for (i = 0; i < g_walkcount; ++i) {
        struct htmlnode *nd = g_walk[i].node;
        if (nd->type == HTMLPART_ELEMENT && strcmp(nd->tag, tagname) == 0) ++n;
    }
    printf("COUNT: %ld\n", n);
    htmlpart_Free(root);
    return 0;
}

static int do_firstattr(const char *fixture, const char *tagname, const char *attrname)
{
    struct htmlnode *root = parse_fixture(fixture);
    long i;
    int found = 0;

    printf("PARSE-RC: 0\n");
    build_walk(root);
    for (i = 0; i < g_walkcount && !found; ++i) {
        struct htmlnode *nd = g_walk[i].node;
        if (nd->type == HTMLPART_ELEMENT && strcmp(nd->tag, tagname) == 0) {
            const char *v = htmlpart_GetAttr(nd, attrname);
            found = 1;
            printf("FOUND: 1\n");
            if (v) {
                printf("VALUE: ");
                print_escaped(v, (long) strlen(v));
                printf("\n");
            } else {
                printf("VALUE: (absent)\n");
            }
        }
    }
    if (!found) printf("FOUND: 0\n");
    htmlpart_Free(root);
    return 0;
}

static int do_styleprop(const char *fixture, const char *tagname, const char *propname)
{
    struct htmlnode *root = parse_fixture(fixture);
    long i;
    int found = 0;

    printf("PARSE-RC: 0\n");
    build_walk(root);
    for (i = 0; i < g_walkcount && !found; ++i) {
        struct htmlnode *nd = g_walk[i].node;
        if (nd->type == HTMLPART_ELEMENT && strcmp(nd->tag, tagname) == 0) {
            char *v = htmlpart_GetStyleProp(nd, propname);
            found = 1;
            printf("FOUND: 1\n");
            if (v) {
                printf("VALUE: ");
                print_escaped(v, (long) strlen(v));
                printf("\n");
                free(v);
            } else {
                printf("VALUE: (absent)\n");
            }
        }
    }
    if (!found) printf("FOUND: 0\n");
    htmlpart_Free(root);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: htmlparttest.test dump|stats|counttag|firstattr|styleprop ...\n");
        return 2;
    }
    if (strcmp(argv[1], "dump") == 0 && argc == 3) {
        return do_dump(argv[2]);
    } else if (strcmp(argv[1], "stats") == 0 && argc == 3) {
        return do_stats(argv[2]);
    } else if (strcmp(argv[1], "counttag") == 0 && argc == 4) {
        return do_counttag(argv[2], argv[3]);
    } else if (strcmp(argv[1], "firstattr") == 0 && argc == 5) {
        return do_firstattr(argv[2], argv[3], argv[4]);
    } else if (strcmp(argv[1], "styleprop") == 0 && argc == 5) {
        return do_styleprop(argv[2], argv[3], argv[4]);
    }
    fprintf(stderr, "usage: htmlparttest.test dump|stats|counttag|firstattr|styleprop ...\n");
    return 2;
}
