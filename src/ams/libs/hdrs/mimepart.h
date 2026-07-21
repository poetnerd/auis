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
	mimepart.h -- MIME body structure parser and decoder.  ANSI C
		     (C89 prototypes) throughout, no scanf/sscanf/fscanf
		     anywhere (numbers parsed with strtol, keywords with
		     strncmp/case-folding loops) -- same policy as
		     overhead/mail/lib/imap_prot.c, the tree's other
		     born-ANSI module.

	Scope: parses the MIME structure of an already-fetched message
	body (multipart nesting, Content-Type parameters, Content-
	Transfer-Encoding) and decodes leaf parts (quoted-printable,
	base64). Does not know about ATK, class.h, or the "text"/
	"environment" display machinery -- lives in libmsshr.a
	specifically so it links into both atkams/messages/lib
	(amsn.do/amss.do) and ams/msclients/cui (cuin/cuis) without
	pulling in ATK. Callers decide what to do with a parsed part
	tree (insert inline, wrap as a clickable alien object, list as
	an attachment, etc.).

	This module intentionally does not duplicate ordinary top-level
	RFC822 header parsing (To/From/Subject/Date) -- callers already
	have that (text822.c's own ReadMessage, or cuilib.c). The one
	exception is mimepart_ParseMessageFile(), a convenience entry
	point for callers (and this module's own standalone test
	driver, mimeparttest.c) that only care about Content-Type/
	Content-Transfer-Encoding/Content-Disposition and want to hand
	in a whole RFC822 file without parsing those three headers out
	themselves first.
*/

#ifndef MIMEPART_H
#define MIMEPART_H

#include <stdio.h>	/* FILE * */

/* Content-Transfer-Encoding actually applied to a part's stored body.
   mimepart_Parse() decodes QP/base64 eagerly -- body/bodylen below are
   always already-decoded bytes; this field just records what the
   encoding *was*, for callers that want to know (e.g. deciding
   whether "8bit" bytes are safe to treat as text). */
#define MIMEPART_ENC_NONE	0	/* 7bit/8bit/binary/unspecified: verbatim */
#define MIMEPART_ENC_QP		1	/* was quoted-printable */
#define MIMEPART_ENC_BASE64	2	/* was base64 */

struct mimeparam {
    char *name;			/* lowercased parameter name */
    char *value;		/* value, quotes/backslash-escapes removed */
    struct mimeparam *next;
};

struct mimepart {
    char *type;			/* lowercased "type/subtype", e.g.
				   "text/plain"; never NULL -- defaults
				   to "text/plain" per RFC 2045 when no
				   Content-Type header was present */
    struct mimeparam *params;	/* Content-Type parameters (boundary,
				   charset, name, ...); see
				   mimepart_GetParam() */
    char *disposition;		/* lowercased Content-Disposition value
				   ("attachment", "inline"), or NULL if
				   the header was absent */
    struct mimeparam *dispparams; /* Content-Disposition parameters
				   (filename, ...); see
				   mimepart_GetDispParam() */
    int encoding;		/* MIMEPART_ENC_* -- what was decoded */
    unsigned char *body;	/* decoded bytes; NULL for a multipart
				   container (only children matter then) */
    long bodylen;		/* byte length of body; the raw
				   (pre-decode) byte length for a
				   multipart container's own body is
				   never stored -- children carry it */
    struct mimepart *children;	/* first child part, multipart only;
				   NULL for a leaf part */
    struct mimepart *next;	/* next sibling at this nesting level;
				   NULL after the last one */
};

/* Parses a body whose Content-Type/Content-Transfer-Encoding are
   already known (the normal case: text822.c has already parsed the
   RFC822 headers and is positioned at the body). data/len is the
   *entire* raw body -- for a multipart entity this includes the
   preamble, all "--boundary" delimiter lines, and the epilogue,
   exactly as it appears on the wire/in the file, undecoded.
   content_type and content_transfer_encoding are the raw header
   values (leading/trailing whitespace tolerated; may be NULL/empty,
   meaning "not present" -- defaults per RFC 2045 apply). Returns a
   malloc'd tree (free with mimepart_Free), or NULL only on
   allocation failure. Malformed structure (missing boundary,
   unterminated part, bad base64/QP) degrades gracefully -- never
   returns NULL for that alone -- matching this tree's existing
   decoder leniency (see getcqp/getc64 in text822.c). */
struct mimepart *mimepart_Parse(const unsigned char *data, long len,
                                 const char *content_type,
                                 const char *content_transfer_encoding);

/* Same as mimepart_Parse, but reads the body from fp (current
   position to EOF) instead of an in-memory buffer. */
struct mimepart *mimepart_ParseFile(FILE *fp, const char *content_type,
                                     const char *content_transfer_encoding);

/* Convenience wrapper: reads an RFC822 header block from fp (up to
   the first blank line) looking only for Content-Type,
   Content-Transfer-Encoding, and Content-Disposition, then parses the
   remainder of fp as the body via mimepart_ParseFile(). Intended for
   callers (and mimeparttest.c) that have a whole message file and no
   header parser of their own already in hand; text822.c, which has
   its own richer header parse, calls mimepart_Parse()/
   mimepart_ParseFile() directly instead. */
struct mimepart *mimepart_ParseMessageFile(FILE *fp);

/* Frees a part tree (children and siblings included). NULL-safe. */
void mimepart_Free(struct mimepart *p);

/* Case-insensitive Content-Type / Content-Disposition parameter
   lookup. Returns NULL if absent. The returned pointer is owned by
   the part and valid until mimepart_Free(). */
const char *mimepart_GetParam(const struct mimepart *p, const char *name);
const char *mimepart_GetDispParam(const struct mimepart *p, const char *name);

#define mimepart_IsMultipart(p) ((p)->children != NULL)

/* Given a parsed multipart/alternative part (mimepart_IsMultipart(alt)
   must be true; the caller is responsible for having checked
   alt->type), returns the child to display: the first exact
   "text/plain" child if any; else the first "text/html" child; else
   the first child; else NULL if alt has no children at all. Does not
   modify the tree or transfer ownership -- the returned pointer is
   still owned by alt. */
const struct mimepart *mimepart_SelectAlternative(const struct mimepart *alt);

/* ---- standalone decode/convert primitives ----
   Each of these is usable independently of parsing a part tree
   (mimepart_Parse calls the first two internally for leaf parts). */

/* Decodes quoted-printable. Handles soft line breaks ("=" at end of
   line, either "=\n" or "=\r\n") and "=XX" hex escapes; an
   unrecognized "=XX" (bad hex digits) is passed through as a literal
   'X' byte rather than aborting, matching this tree's existing QP
   leniency. *outp is malloc'd (caller frees); returns the decoded
   length, always <= inlen. Returns -1 only on allocation failure. */
long mimepart_DecodeQP(const unsigned char *in, long inlen,
                        unsigned char **outp);

/* Decodes base64. Whitespace/newlines anywhere in the input are
   skipped; any other non-alphabet byte (other than trailing "="
   padding) is likewise skipped rather than treated as an error, so a
   truncated or slightly-malformed part still decodes as much as it
   can. *outp is malloc'd (caller frees); returns the decoded length.
   Returns -1 only on allocation failure. */
long mimepart_DecodeBase64(const unsigned char *in, long inlen,
                            unsigned char **outp);

/* Deliberately dumb text/html -> plain-text shim (see
   revival/doc/mime-display-prompt.md -- scheduled for replacement by
   the HTML-rendering objective's htmlview work): drops "<...>" tags,
   drops the entire contents of <style>/<script>/<head> elements,
   decodes &amp; &lt; &gt; &quot; &#39; &nbsp; and numeric entities
   (&#NN; and &#xHH;), and collapses runs of 3+ newlines down to 2.
   Input need not be NUL-terminated (len is authoritative; embedded
   NULs are passed through as literal bytes). Returns a malloc'd,
   NUL-terminated string (caller frees); never NULL except on
   allocation failure. */
char *mimepart_HtmlToText(const char *html, long len);

/* Converts UTF-8 to ATK's Latin-1-era 8-bit text: a byte < 0x80
   passes through unchanged; a valid UTF-8 sequence decoding to a
   codepoint <= 0xFF becomes that single Latin-1 byte (this is exact
   -- Unicode's first 256 codepoints are Latin-1 by definition); a
   valid sequence decoding above 0xFF (most emoji, CJK, smart
   punctuation, ...) becomes a single '?'; a malformed/truncated UTF-8
   byte sequence consumes one byte and emits one '?', then resumes
   scanning at the next byte. *outlenp receives the output length
   (never larger than inlen). Returns a malloc'd, NUL-terminated
   buffer (caller frees); NULL only on allocation failure. */
unsigned char *mimepart_Utf8ToLatin1(const unsigned char *in, long inlen,
                                      long *outlenp);

#endif /* MIMEPART_H */
