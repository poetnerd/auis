/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

#include <andrewos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <class.h>
#include <text.ih>
#include <textv.ih>
#include <view.ih>
#include <scroll.ih>
#include <im.ih>
#include <htmlpart.h>
#include <htmlatk.h>
#include <mimepart.h>
#include <htmlprv.eh>

struct htmlprv_rock {
    char basedir[MAXPATHLEN + 1];
};

/* This tool builds its window as a bare im_SetView(im, scroll) --
   the same minimal, no-menu shape ATK's own ex1 tutorial app uses --
   so there is no Commands/frame menu to carry a Quit item, and the
   window manager's WM_DELETE_WINDOW protocol (the close box) has
   nothing registered to call either (xim.c always offers the
   protocol and always calls im_CallDeleteWindowCallback() on it, but
   that's a no-op with no callback set -- found live 2026-09-27,
   wdc's own report: neither the app-menu Quit item nor the window's
   own close box did anything, though the process itself was
   confirmed alive and idle, not hung, via `ps`/a plain `kill`).
   consolea.c (src/atk/console/cmd) is the same shape of minimal
   single-window app and has the same need; this is its exact
   pattern. */
static void QuitCallback(struct im *im, long rock)
{
    im_KeyboardExit();
}

static boolean HasSuffixCI(const char *s, const char *suffix)
{
    long slen = (long) strlen(s), suflen = (long) strlen(suffix);
    long i;

    if (suflen > slen) return FALSE;
    for (i = 0; i < suflen; ++i) {
        char a = s[slen - suflen + i], b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = a - 'A' + 'a';
        if (b >= 'A' && b <= 'Z') b = b - 'A' + 'a';
        if (a != b) return FALSE;
    }
    return TRUE;
}

/* htmlatk_ImageResolver for this standalone preview tool. Unlike
   text822.c's ResolveImage (cid: against sibling MIME parts, falling
   back to a real http(s):// fetch), a bald HTML file passed to this
   tool has no MIME envelope, and this tool deliberately makes no
   network connections of its own -- any src that looks like a URL
   scheme (contains "://") is declined (placeholder), same as a
   cid: reference would be (no MIME parts to resolve it against).
   Everything else is resolved as a plain filesystem path relative to
   the HTML file's own directory, exactly how a browser resolves a
   relative <img src> against the document's own location -- which is
   what makes revival/tests/bookrack.html's own images/ folder (see
   roadmap.md) just work here with zero network dependency. */
static boolean ResolveLocalImage(void *rockv, const char *src,
    unsigned char **bytesOut, long *lenOut, char **mimetypeOut)
{
    struct htmlprv_rock *rock = (struct htmlprv_rock *) rockv;
    char path[2 * (MAXPATHLEN + 1)];
    const char *mimetype;
    FILE *fp;
    unsigned char *buf;
    long len, got;

    if (!rock || !src || !*src) return FALSE;
    if (strstr(src, "://") != NULL) return FALSE;

    if (HasSuffixCI(src, ".gif")) mimetype = "image/gif";
    else if (HasSuffixCI(src, ".png")) mimetype = "image/png";
    else if (HasSuffixCI(src, ".jpg") || HasSuffixCI(src, ".jpeg")) mimetype = "image/jpeg";
    else return FALSE;

    if (src[0] == '/') {
        strcpy(path, src);
    } else {
        strcpy(path, rock->basedir);
        strcat(path, "/");
        strcat(path, src);
    }

    fp = fopen(path, "rb");
    if (!fp) return FALSE;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return FALSE; }
    len = ftell(fp);
    if (len <= 0) { fclose(fp); return FALSE; }
    rewind(fp);
    buf = (unsigned char *) malloc(len);
    if (!buf) { fclose(fp); return FALSE; }
    got = (long) fread(buf, 1, len, fp);
    fclose(fp);
    if (got != len) { free(buf); return FALSE; }

    *mimetypeOut = (char *) malloc(strlen(mimetype) + 1);
    if (!*mimetypeOut) { free(buf); return FALSE; }
    strcpy(*mimetypeOut, mimetype);
    *bytesOut = buf;
    *lenOut = len;
    return TRUE;
}

boolean htmlprevapp__InitializeObject(struct classheader *classID, struct htmlprevapp *self)
{
    self->filename = NULL;
    return TRUE;
}

void htmlprevapp__FinalizeObject(struct classheader *classID, struct htmlprevapp *self)
{
}

boolean htmlprevapp__ParseArgs(struct htmlprevapp *self, int argc, char **argv)
{
    if (!super_ParseArgs(self, argc, argv))
        return FALSE;

    ++argv;
    if (*argv)
        self->filename = *argv;

    return TRUE;
}

boolean htmlprevapp__Start(struct htmlprevapp *self)
{
    FILE *fp;
    unsigned char *raw, *latin1;
    long rawlen, latin1len;
    struct htmlnode *root;
    struct text *t;
    struct textview *tv;
    struct scroll *sc;
    struct im *im;
    struct htmlprv_rock rock;
    char *slash;

    if (!super_Start(self))
        return FALSE;

    if (!self->filename) {
        fprintf(stderr, "usage: runapp htmlprv <file.html>\n");
        return FALSE;
    }

    fp = fopen(self->filename, "rb");
    if (!fp) {
        fprintf(stderr, "htmlprv: cannot open %s\n", self->filename);
        return FALSE;
    }
    fseek(fp, 0, SEEK_END);
    rawlen = ftell(fp);
    rewind(fp);
    raw = (unsigned char *) malloc(rawlen);
    if (!raw) { fclose(fp); return FALSE; }
    if ((long) fread(raw, 1, rawlen, fp) != rawlen) {
        fclose(fp); free(raw); return FALSE;
    }
    fclose(fp);

    /* Mirror RenderHtmlPart's own UTF-8-to-Latin1 conversion
       (text822.c) -- htmlpart_Parse()/htmlatk_Render() do not do this
       themselves, by design (see mimepart.h's own comment), and every
       real fixture here is genuinely UTF-8. */
    latin1 = mimepart_Utf8ToLatin1(raw, rawlen, &latin1len);
    free(raw);
    if (!latin1) return FALSE;

    /* basedir: the HTML file's own directory, so a relative
       <img src="images/foo.gif"> resolves the same way a browser
       would resolve it against the document's own location. */
    strcpy(rock.basedir, self->filename);
    slash = strrchr(rock.basedir, '/');
    if (slash) *slash = '\0';
    else strcpy(rock.basedir, ".");

    root = htmlpart_Parse(latin1, latin1len);
    free(latin1);

    t = (struct text *) class_NewObject("text");
    if (!t) { htmlpart_Free(root); return FALSE; }
    text_ReadTemplate(t, "default", TRUE);

    htmlatk_Render(t, 0, root, ResolveLocalImage, (void *) &rock, NULL);
    htmlpart_Free(root);

    tv = textview_New();
    if (!tv) return FALSE;
    view_SetDataObject((struct view *) tv, (struct dataobject *) t);

    sc = scroll_Create((struct view *) tv, scroll_LEFT);
    if (!sc) return FALSE;

    im = im_Create(NULL);
    if (!im) return FALSE;
    im_SetDeleteWindowCallback(im, (procedure) QuitCallback, NULL);
    im_SetView(im, (struct view *) sc);

    return TRUE;
}
