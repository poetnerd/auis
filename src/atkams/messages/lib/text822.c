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

#ifndef NORCSID
#define NORCSID
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/atkams/messages/lib/RCS/text822.c,v 1.55 1994/05/10 18:11:39 rr2b Exp $";
#endif

#include <ctype.h>
#include <andrewos.h>
#include <class.h>
#include <text822.eh>
#include <text.ih>
#include <style.ih>
#include <fontdesc.ih>
#include <envrment.ih>
#include <readscr.ih>
#include <amsutil.ih>
#include <dataobj.ih>
#include <stylesht.ih>
#include <environ.ih>
#include <fnote.ih>
#include <mailobj.ih>
#include <ams.ih>
#include <message.ih>
#include <ams.h>
#include <mimepart.h>
#include <fdphack.h>

static char *EmptyMsgString = "<empty message>";
static struct style *FixedStyle, *BoldStyle, *FormatStyle, *TinyStyle, *GlobalStyle;
static char *myfontname = NULL;
static int myfontsize, UsingFootNote, PrintMinorHeaders;
static char *fgetsdecoding(), *UnquoteString();
static boolean ReadMessage();
static RotateThirteen();
static FindParam();
static InsertProperObject();
static ParseEncoding();
static getcdecoding();
static ungetcdecoding();
static getc64();
static ungetc64();
static getcqp();
static ungetcqp();
static hexchar();
static char64();
static PlainAsciiText();
static int ForceMetamail();
static int InsertDecodedText();
static void InsertAttachmentLine();

boolean text822__InitializeObject(c, self)
struct classheader *c;
struct text822 *self;
{
    if (text822_ReadTemplate(self, "messages", FALSE)) {
	fprintf(stderr, "Could not read messages template!\n");
    }
    self->InstructionsStyle = style_New();
    style_SetFontSize(self->InstructionsStyle, style_ConstantFontSize, 16);
    style_SetJustification(self->InstructionsStyle, style_LeftJustified);
    style_SetFontFamily(self->InstructionsStyle, myfontname);
    self->BigBoldStyle = style_New();
    style_SetFontSize(self->BigBoldStyle, style_PreviousFontSize, 2);
    style_AddNewFontFace(self->BigBoldStyle, (long) fontdesc_Bold);
    text822_SetCopyAsText(self, TRUE);
    return(TRUE);
}

boolean text822__InitializeClass(c)
struct classheader *c;
{
    UsingFootNote = environ_GetProfileSwitch("usefootnote", TRUE);
    PrintMinorHeaders = environ_GetProfileSwitch("printminorheaders", TRUE);
    myfontname = amsutil_GetDefaultFontName();
    myfontsize = environ_GetProfileInt("fontsize", 12);
    FixedStyle = style_New();
    style_SetName(FixedStyle, "typewriter");
    style_SetFontFamily(FixedStyle, "andytype");
    style_AddNewFontFace(FixedStyle, (long) fontdesc_Fixed);
    /* The following is necessary if you don't want fixed-width printing to wrap almost every single line! */
    if (myfontsize >= 10) style_SetFontSize(FixedStyle, style_ConstantFontSize, 10);
    BoldStyle = style_New();
    style_SetName(BoldStyle, "bold");
    style_AddNewFontFace(BoldStyle, (long) fontdesc_Bold);
    style_SetFontFamily(BoldStyle, "andy");
    FormatStyle = style_New();
    style_SetName(FormatStyle, "formatnote");
    style_AddPassThru(FormatStyle);
    TinyStyle = style_New();
    style_SetFontSize(TinyStyle, style_ConstantFontSize, 8);
    style_SetName(TinyStyle, "smaller");
    GlobalStyle = style_New();
    style_SetJustification(GlobalStyle, style_LeftJustified);
    style_SetFontSize(GlobalStyle, style_ConstantFontSize, myfontsize);
    style_SetFontFamily(GlobalStyle, myfontname);
    return(TRUE);
}

long text822__Read(self, fp, id)
struct text822 *self;
FILE *fp;
long id;
{
    int len, bs, ig;

    text822_Clear(self);
    text822_SetID(self, text822_UniqueID(self));
    if (text822_ReadIntoText(self, fp, MODE822_NORMAL, NULL, &len, FALSE, &bs, &ig, NULL)) {
	return(dataobject_NOREADERROR);
    }
    return(dataobject_BADFORMAT);
}

long text822__ReadSubString(self, pos, fp, quoteCharacters)
struct text822 *self;
long pos;
FILE *fp;
boolean quoteCharacters;
{
#ifdef OLDCODE
    int len, bs, ig;

    text822_ReadIntoText(self, fp, MODE822_NORMAL, NULL, &len, FALSE, &bs, &ig, NULL);
    return(len);
#else
    return super_ReadSubString(self, pos, fp, quoteCharacters);
#endif
}

char *StripWhiteSpace( t )
char *t;
{
    boolean inquotes = FALSE;
    char *s = t;
    while(s && *s && isspace(*s)) s++;
    t = s;
    while (s && *s) {
	if (!inquotes && isspace(*s)) {
	    char *b = s, *e = NULL;
	    while (s && *s && isspace (*s)) s++;
	    e = s;
	    strcpy(b, e);
	    s = b;
	}
	else if (*s == '\\')
	    s += 2;
	else {
	    if(*s == '"')
		inquotes = !inquotes;
	    s++;
	}
    }
    return t;
}

static char *GetHeader(LineBuf, lim, fp)
char *LineBuf;
int lim;
FILE *fp;
{
    char *s, *lb;
    int c;
    int len;

    lb = LineBuf;
    while (TRUE) {
	s = fgets(lb, lim, fp);
	if (!s) return(s);
	if (lb[0] == '\n' || (lb[0] == '\r' && lb[1] == '\n')) return(s); /* end of headers (CRLF or LF), no peeking ahead! */
	c = getc(fp);
	if (c == EOF) return(s);
	ungetc(c, fp);
	if (c == ' ' || c == '\t') {
	    len = strlen(lb);
	    lb += len;
	    lim -= len;
	    if (lim <= 1) return(NULL);	/* leave room for \n */
	} else {
	    return(s); /* Not quite right, but we only check for non-null */
	}
    }
}

boolean text822__ReadIntoText(ch, d, fp, Mode, ContentTypeOverride, len, IsReallyTextObject, BodyStart, IgnorePosition, AuxHeadText)
struct classheader *ch;
struct text822 *d;
FILE *fp;
int Mode;
char *ContentTypeOverride;
int *len;
boolean IsReallyTextObject;
int *BodyStart, *IgnorePosition;
struct text *AuxHeadText;
{
    if (text822_ReadTemplate(d, "messages", FALSE)) {
	fprintf(stderr, "Could not read messages template!\n");
    }
    text822_SetGlobalStyle(d, GlobalStyle);
    return(ReadMessage(d, fp, Mode, ContentTypeOverride, len, IsReallyTextObject, BodyStart, IgnorePosition, AuxHeadText, 0, 0, 0, 0));
}

/* Auxilliary routines for text/richtext */
static ignoretoken(t)
char *t;
{
    if (*t == '/') ++t;
    if (!strcmp(t, "us-ascii")) return(1);
    if (!strcmp(t, "no-op")) return(1);
    return(0);
}

static char *translate(t)
char *t;
{
    if (!strcmp(t, "fixed")) return("typewriter");
    if (!strcmp(t, "excerpt")) return("quotation");
    /* Really ought to handle ISO-10646 and ISO-8859-X somehow */
    return(t);
}

static boolean ReadMessage(d, fp, Mode, ContentTypeOverride, len, IsReallyTextObject, BodyStart, IgnorePosition, AuxHeadText, InsideRecursion, AlternativeNumber, JunkAtEnd, DisplayAllHeaders)
struct text822 *d;
FILE *fp;
int Mode;
char *ContentTypeOverride;
int *len;
boolean IsReallyTextObject;
int *BodyStart, *IgnorePosition;
struct text *AuxHeadText;
int InsideRecursion, AlternativeNumber, JunkAtEnd, DisplayAllHeaders;
{
    struct environment *et;
    char LineBuf[10000], ScribeFormatVersion[100], *ColonLocation, c, ContentType[400], ContentEncoding[50], ContentDescription[200], Subject[200];
    int HighlightThisOne, ShowPos, linelen, ColonOffset;
    boolean SawEndData;
    int SVers, pos, fnlen, didbig, didsmall, ForceMet;
    char fmttype[25], fmtvers[25], fmtresources[200], *sfmttype, *sfmtvers, *s, charsetbuf[100], *currentcharset;
    char **MyHeadsArray;
    static char *SendHeadsArray[] = {
	"if-type-unsupported", "content-type",
	"x-andrew-scribeformat", "x-andrew-message-size",
	"x-andrew-text822mode", NULL
    };
    boolean showheads, usefn, printminors;
    boolean showallbutkeys = amsutil_GetOptBit(EXP_SHOWALLBUTKEYS);
    struct fnote *fn;

    currentcharset = (char *) getenv("MM_CHARSET");
    if (!currentcharset) {
#ifdef ISO80_FONTS_ENV
	currentcharset = "iso-8859-1";
#else
	currentcharset = "us-ascii";
#endif
    } else {
	for (s=currentcharset; *s; ++s) if (isupper(*s)) *s = tolower(*s);
    }

    if (AuxHeadText) {
	MyHeadsArray = SendHeadsArray;
	showheads = TRUE;
    } else {
	MyHeadsArray = amsutil_GetKeyHeadsArray();
	showheads = !amsutil_GetOptBit(EXP_SHOWNOHEADS);
    }
    if (IsReallyTextObject || InsideRecursion) {
	usefn = FALSE;
	printminors = TRUE;
    } else {
	printminors = PrintMinorHeaders;
	usefn = PrintMinorHeaders ? UsingFootNote : FALSE;
    }
    if (AuxHeadText) 
	usefn = FALSE;
restart:
    didbig = 0;
    didsmall = 0;
    *IgnorePosition = text822_GetLength(d);
    ShowPos = *IgnorePosition;
    if (usefn) {
	fn = fnote_New();
	if (!fn) {
	    fprintf(stderr, "Could not create footnote object!\n");
	    usefn = FALSE;
	} else {
	    fnlen = 0;
	    text822_AlwaysInsertCharacters(d, *IgnorePosition, "\n\n", 2);
	    text822_AlwaysAddView(d, *IgnorePosition, "fnotev", fn);
	    fnote_addenv(fn,d,*IgnorePosition);
	    fnote_Close(fn, d);
	    ShowPos = *IgnorePosition;
	}
    }
    if (!usefn) { /* Not just an else clause if fnote_New fails */
	if (!AuxHeadText && (!InsideRecursion || DisplayAllHeaders)) {
	    text822_AlwaysInsertCharacters(d, *IgnorePosition, "\n\n", 2);
	    ShowPos = *IgnorePosition + 1;
	}
    }
    SawEndData = FALSE;
    HighlightThisOne = FALSE;
    ScribeFormatVersion[0] = '\0';
    ContentType[0] = '\0';
    ContentEncoding[0] = (char)0;
    ContentDescription[0] = (char)0;
    Subject[0] = (char)0;
    fmtvers[0] = '\0';
    fmttype[0] = '\0';
    fmtresources[0] = '\0';
    while (GetHeader(LineBuf, sizeof(LineBuf), fp)) {
	linelen = strlen(LineBuf);
	c = LineBuf[0];
	if (c == '\n' || (c == '\r' && LineBuf[1] == '\n')) break; /* done with headers (CRLF or LF) */
	if (c == ' ' || c == '\t') {
	    ColonOffset = 0;
	} else {
	    /* Decide where to show it */
	    ColonLocation = strchr(LineBuf, ':');
	    if (ColonLocation) {
		*ColonLocation = '\0';
		ColonOffset = ColonLocation - LineBuf;
	    } else {
		ColonOffset = 0;
	    }
	    if (!amsutil_lc2strncmp("content-type", LineBuf, sizeof(LineBuf))) {
		if (!ColonLocation)
		    strcpy(ContentType, "");
		else {
		    char *headerContent = (char *) malloc(strlen(ColonLocation+1) + 1);
		    char *StrippedHeader;
		    strcpy(headerContent, ColonLocation+1);
		    StrippedHeader = StripWhiteSpace(headerContent);
		    strncpy(ContentType, StrippedHeader, sizeof(ContentType));
		    free(headerContent);
		}
	    } else if (!amsutil_lc2strncmp("content-transfer-encoding", LineBuf, sizeof(LineBuf))) {

		strncpy(ContentEncoding, ColonLocation ? ColonLocation+1 : "", sizeof(ContentEncoding));
	    } else if (!amsutil_lc2strncmp("content-description", LineBuf, sizeof(LineBuf))) {

		strncpy(ContentDescription, ColonLocation ? ColonLocation+1 : "", sizeof(ContentDescription));
	    } else if (!amsutil_lc2strncmp("subject", LineBuf, sizeof(LineBuf))) {

		strncpy(Subject, ColonLocation ? ColonLocation+1 : "", sizeof(Subject));
	    } else if (!amsutil_lc2strncmp("x-andrew-scribeformat", LineBuf, sizeof(LineBuf))) {

		strncpy(ScribeFormatVersion, ColonLocation ? ColonLocation+1 : "", sizeof(ScribeFormatVersion));
	    } else if ((Mode == MODE822_NORMAL) && !amsutil_lc2strncmp("x-andrew-text822mode", LineBuf, sizeof(LineBuf))) {
		int newmode;
		newmode = ColonLocation ? (atoi(ColonLocation+1)) : MODE822_NORMAL;
		Mode = 0;
		if (newmode & AMS_PRINT_FIXED) Mode |= MODE822_FIXEDWIDTH;
		if (newmode & AMS_PRINT_ROT13) Mode |= MODE822_ROT13;
	    }
	    if (showheads && MyHeadsArray) {
		int i;

		if (AuxHeadText || showallbutkeys) {
		    HighlightThisOne = TRUE;
		} else {
		    HighlightThisOne = FALSE;
		}
		for (i=0; MyHeadsArray[i]; ++i) {
		    if (!amsutil_lc2strncmp(MyHeadsArray[i], LineBuf, sizeof(LineBuf))) {
			HighlightThisOne = !HighlightThisOne;
			break;
		    }
		}
	    }
	    if (ColonLocation) *ColonLocation = ':';
	}
	if (AuxHeadText) {
	    /* We're really being called by sendmessage, with two text objects */
	    int hlen = text_GetLength(AuxHeadText);
	    if (HighlightThisOne) {

		text_AlwaysInsertCharacters(AuxHeadText, hlen, LineBuf, linelen);
		hlen += linelen;
	    }
	} else if (!InsideRecursion || DisplayAllHeaders) {
	    /* Normal case -- put important ones in front, others in back */
	    if (HighlightThisOne || DisplayAllHeaders) {
		++didbig;
		text822_AlwaysInsertCharacters(d, ShowPos, LineBuf, linelen);
		if (DisplayAllHeaders) {
		    et = environment_InsertStyle(((struct text *)d)->rootEnvironment, ShowPos-1, TinyStyle, 1);
		    environment_SetLength(et, linelen);
		    environment_SetStyle(et, FALSE, FALSE);
		} else if (ColonOffset) {
		    et = environment_InsertStyle(((struct text *)d)->rootEnvironment, ShowPos, BoldStyle, 1);
		    environment_SetLength(et, ColonOffset);
		}
		ShowPos += linelen;
	    } else {
		if (printminors) {
		    ++didsmall;
		    if (usefn) {
			fnote_AlwaysInsertCharacters(fn, fnlen, LineBuf, linelen);
			if (ColonOffset) {
			    et = environment_InsertStyle(((struct text *)fn)->rootEnvironment, fnlen, BoldStyle, 1);
			    environment_SetLength(et, ColonOffset);
			}
			fnlen += linelen;
			/* The next line prevents a troff bug with enormous footnotes */
			if (((didbig*14) + (didsmall*10)) > 600) usefn = FALSE;
		    } else {
			text822_AlwaysInsertCharacters(d, *IgnorePosition, LineBuf, linelen);
			et = environment_InsertStyle(((struct text *)d)->rootEnvironment, *IgnorePosition, TinyStyle, 1);
			environment_SetLength(et, linelen);
			environment_SetStyle(et, FALSE, FALSE);
			if (ColonOffset) {
			    et = environment_InsertStyle(et, 0, BoldStyle, 1);
			    environment_SetLength(et, ColonOffset);
			}
			*IgnorePosition += linelen;
			ShowPos += linelen;
		    }
		}
	    }
	}
    } /* All done with headers, ready for body */
    if (AuxHeadText) {
	/* We don't want the very last newline */
	int hlen = text_GetLength(AuxHeadText) -1;
	char c = text_GetChar(AuxHeadText, hlen);
	if (c == '\n') {
	    text_AlwaysDeleteCharacters(AuxHeadText, hlen, 1);
	}
    }
    if (usefn) {
	/* Ditto */
	char c = text822_GetChar(d, ShowPos - 1);
	if (c == '\n') {
	    text822_AlwaysDeleteCharacters(d, ShowPos-1, 1);
	}
    }
    ShowPos = *BodyStart = text822_GetLength(d) - JunkAtEnd;
    if (Mode & MODE822_FIXEDWIDTH) {
	et = environment_InsertStyle(((struct text *)d)->rootEnvironment, *BodyStart, FixedStyle, 1);
	environment_SetLength(et, text822_GetLength(d) - *BodyStart);
	environment_SetStyle(et, TRUE, TRUE);
    }
    if (!ContentTypeOverride) {
	ContentTypeOverride = ContentType;
    }
    while (*ContentTypeOverride && isspace(*ContentTypeOverride)) ++ContentTypeOverride;
    if (ContentTypeOverride[0]) {
	amsutil_BreakDownContentTypeField(ContentTypeOverride, fmttype, sizeof(fmttype), fmtvers, sizeof(fmtvers), fmtresources, sizeof(fmtresources));
    } else if (ScribeFormatVersion[0]) {
	linelen = strlen(ScribeFormatVersion);
	ScribeFormatVersion[linelen-1] = '\0';
	strcpy(fmttype, "x-be2");
	strcpy(fmtvers, ScribeFormatVersion);
	fmtresources[0] = '\0';
    }
    for (sfmttype = fmttype; *sfmttype && isspace(*sfmttype); ++sfmttype) {
	;
    }
    for (sfmtvers = fmtvers; *sfmtvers && isspace(*sfmtvers); ++sfmtvers) {
	;
    }
    for (s=sfmttype; *s; ++s) {
	if (isupper(*s)) *s = tolower(*s);
    }
    for (s=ContentEncoding; s && *s; ++s) {
	if (isupper(*s)) *s = tolower(*s);
    }
    ForceMet = ForceMetamail(sfmttype);
    if (!ForceMet && !AlternativeNumber && sfmttype && sfmttype[0]
	 && (!amsutil_lc2strncmp("x-be2", sfmttype, strlen(sfmttype))
	     || !amsutil_lc2strncmp("application/andrew-inset", sfmttype, strlen(sfmttype)))) {
	SVers = atoi(fmtvers);
	if (!amsutil_lc2strncmp("application/andrew-inset", sfmttype, strlen(sfmttype))) {
	    SVers = 12;
	}
	if (SVers < 10) {	
	    while(fgets(LineBuf, sizeof(LineBuf), fp)) {
		if (!strncmp(LineBuf, "\\enddata{text822", 16)) {
		    SawEndData = TRUE;
		    break;
		}
		linelen = strlen(LineBuf);
		text822_AlwaysInsertCharacters(d, ShowPos, LineBuf, linelen);
		ShowPos += linelen;
	    }
	    if (readscr_Begin(d, *BodyStart, text822_GetLength(d) - *BodyStart, TRUE, fmtvers, TRUE)) {
		/* Succeeded in handling as scribe file */
	    }
	} else {
	    /* New BE2 datastream, call InsertFile routine */
	    *len = text822_AlwaysInsertFile(d, fp, NULL, ShowPos);
	    ShowPos++;
	}
    } else if (!ForceMet && !AlternativeNumber
          && !amsutil_lc2strncmp("text/richtext", ContentTypeOverride, 13)
		&& (FindParam(ContentTypeOverride, "charset", charsetbuf)
		    || !amsutil_lc2strncmp("us-ascii", charsetbuf, 8)
		    || !amsutil_lc2strncmp(currentcharset, charsetbuf, 10))) {
	char TmpFileName[1000], Buf[1000], token[100];
	int c, i, JustDidNewline = 0, dum;
	FILE *tmpfp;

	int EncodingCode = ParseEncoding(ContentEncoding);
	ams_CUI_GenLocalTmpFileName(ams_GetAMS(), TmpFileName);
	tmpfp = (FILE *) fopen (TmpFileName, "w");
	if (!tmpfp) return(FALSE);
	fputs("Content-type: application/andrew-inset\n\n\\begindata{text, 42}\n\\template{messages}\n", tmpfp);
	while((c = getcdecoding(fp, EncodingCode)) != EOF) {
	    if (c == '<') { 
		for (i = 0; ((c = getcdecoding(fp, EncodingCode)) != '>') && (c != EOF) && i < (sizeof(token)-1); ++i) {
		    token[i] = isupper(c) ? tolower(c) : c;
		}
		token[i] = (char)0;
		if (!strcmp(token, "lt")) {
		    fputc('<', tmpfp);
		    JustDidNewline = 0;
		} else if (!strcmp(token, "nl")) {
		    fputs(JustDidNewline ? "\n" : "\n\n", tmpfp);
		    ++JustDidNewline;
		} else if (!strcmp(token, "paragraph")
			   || !strcmp(token, "/paragraph")) {
		    while (JustDidNewline < 3) {
			fputs("\n", tmpfp);
			++JustDidNewline;
		    }
		} else if (!strcmp(token, "comment")) {
		    int commct = 1;
		    while (commct > 0) {
			while ((c = getcdecoding(fp, EncodingCode)) != '<' &&
			       (c != EOF));
			if (c != EOF) {
			    for (i = 0; (c = getcdecoding(fp, EncodingCode)) != '>' &&
			      (c != EOF) && i < (sizeof(token)-1); ++i) {
				token[i] = isupper(c) ? tolower(c) : c;
			    }
			    token[i] = (char)0;
			    if (!strcmp(token, "/comment")) --commct;
			    if (!strcmp(token, "comment")) ++commct;
			}
			else break;
		    }
		} else if (!ignoretoken(token)) {
		    if (token[0] == '/') {
			fputc('}', tmpfp);
		    } else {
			fprintf(tmpfp, "\\%s{", translate(token));
		    }
		}
	    } else if (c == '\n') {
		int edct, i, c;
		char EDBuf[25];
		boolean sawEOF = FALSE;

		if(!JustDidNewline) putc(' ', tmpfp);
		/* Need to check for enddata */
		EDBuf[0] = (char)0;
		edct = 0;
		while (edct < 16) {
		    if((c = getcdecoding(fp, EncodingCode)) != EOF)
			EDBuf[edct++] = (char) c;
		    else {
			sawEOF = TRUE;
			break;
		    }
		    if (strncmp(EDBuf, "\\enddata{text822", edct)) break;
		}
		if (edct < 16) {
		    /* Not an enddata */
		    if(sawEOF)
			ungetcdecoding(EOF, fp, EncodingCode);
		    for (i = edct-1; i >= 0; --i) 
			ungetcdecoding((int) EDBuf[i], fp, EncodingCode);
		} else {
		    /* Really is an enddata! */
		    while ((c = getcdecoding(fp, EncodingCode)) != EOF && c != '\n');
		    SawEndData = TRUE;
		    break;
		}
	    } else if (c == '\\'|| c == '}' || c == '{') {
		putc('\\', tmpfp);
		putc(c, tmpfp);
		JustDidNewline = 0;
	    } else {
		putc(c, tmpfp);
		JustDidNewline = 0;
	    }
	}
	fputs("\\enddata{text, 42}\n", tmpfp);
	fclose(tmpfp);
	tmpfp = (FILE *) fopen (TmpFileName, "r");
	if (!tmpfp) return(FALSE);
	ReadMessage(d, tmpfp, Mode, NULL, len, IsReallyTextObject, &dum, &dum, NULL, 1, 0, JunkAtEnd, 0);
	fclose(tmpfp);
	unlink(TmpFileName); 
    } else if (!AlternativeNumber
	       && !amsutil_lc2strncmp("text/enriched", ContentTypeOverride, 13)
	       && (FindParam(ContentTypeOverride, "charset", charsetbuf)
		   || !amsutil_lc2strncmp("us-ascii", charsetbuf, 8)
		   || !amsutil_lc2strncmp(currentcharset, charsetbuf, 10))) {
	char TmpFileName[1000], Buf[1000], token[100];
	int c, i, JustSawNewline = 0, dum;
	int paramcnt = 0, nofill = 0, quotecnt = 0;
	FILE *tmpfp;

	int EncodingCode = ParseEncoding(ContentEncoding);
	ams_CUI_GenLocalTmpFileName(ams_GetAMS(), TmpFileName);
	tmpfp = (FILE *) fopen (TmpFileName, "w");
	if (!tmpfp) return(FALSE);
	fputs("Content-type: application/andrew-inset\n\n\\begindata{text, 42}\n\\template{messages}\n", tmpfp);

#define NEWLINE_TEST				\
if (nofill <= 0 && JustSawNewline > 0) {		\
	while(JustSawNewline > 0) {		\
	    fputc('\n', tmpfp);			\
	    JustSawNewline--;			\
	}					\
        JustSawNewline = 0;			\
}

	while((c = getcdecoding(fp, EncodingCode)) != EOF) {
	    switch (c) {
		case '<': 
		    NEWLINE_TEST
		      if((c = getcdecoding(fp, EncodingCode)) != EOF &&
			 (c == '<')) {
			  fputc('<', tmpfp);
			  break;
		      }
		      else
			  ungetcdecoding(c, fp, EncodingCode);
		    for (i = 0; ((c = getcdecoding(fp, EncodingCode)) != '>') && (c != EOF) && i < (sizeof(token)-1); ++i) {
			token[i] = isupper(c) ? tolower(c) : c;
		    }
		    token[i] = (char)0;
		    if (!strcmp(token, "param")) {
			paramcnt++;
		    } else if (!strcmp(token, "/param")) {
			paramcnt--;
		    } else if (!strcmp(token, "nofill")) {
			nofill++;
		    } else if (!strcmp(token, "/nofill")) {
			nofill--;
		    } else if (!ignoretoken(token)) {
			if (token[0] == '/') {
			    if (strcmp("/excerpt", token) == 0)
				quotecnt--;
			    fputc('}', tmpfp);
			    if((c = getcdecoding(fp, EncodingCode)) != EOF &&
			       (c != '\n')) { /* If next char isn't newline, stick one in */
				fputc('\n', tmpfp);
			    }
			    ungetcdecoding(c, fp, EncodingCode);
			} else {
			    if (strcmp("excerpt", token) == 0)
				quotecnt++;
#if 0
			    if (LastChar != '\n')
				fputc('\n', tmpfp);
#endif
			    fprintf(tmpfp, "\\%s{", translate(token));
			}
		    }
		    break;
		case '\n':
		    if (nofill > 0)
			fputc('\n', tmpfp);
		    else
			JustSawNewline++;
		    break;
		case '\\':
		case '}':
		case '{':
		    NEWLINE_TEST
		      if (paramcnt <= 0) {
			  putc('\\', tmpfp);
			  putc(c, tmpfp);
		      }
		    break;
		default:
		    NEWLINE_TEST
		      if (paramcnt <= 0)
			  putc(c, tmpfp);
		    break;
	    }
	}
	fputs("\\enddata{text, 42}\n", tmpfp);
	fclose(tmpfp);
	tmpfp = (FILE *) fopen (TmpFileName, "r");
	if (!tmpfp) return(FALSE);
	ReadMessage(d, tmpfp, Mode, NULL, len, IsReallyTextObject, &dum, &dum, NULL, 1, 0, JunkAtEnd, 0);
	fclose(tmpfp);
	unlink(TmpFileName);
    } else if (!ForceMet && !AlternativeNumber && !amsutil_lc2strncmp("message/rfc822", ContentTypeOverride, 14)) {
	int dum=0;
	ReadMessage(d, fp, Mode, NULL, len, IsReallyTextObject, &dum, &dum, NULL, 1, 0, JunkAtEnd, 1);
    } else if (!ForceMet && !AlternativeNumber && !amsutil_lc2strncmp("multipart/", ContentTypeOverride, 10)) { 
	char boundary[100];
	char TmpFileName[1000], Buf[1000], holdBuf[1000];
	FILE *tmpfp;
	boolean foundBoundary;
	int Done = 0;
	int dum = 0;
	int IsAlternative = 0, IsFinalPart = 0, IsDigest = 0;
	/* multipart/alternative only: each part is parsed (and its
	   Content-Transfer-Encoding decoded) via mimepart instead of
	   being rendered immediately, so all of them can be seen
	   before picking a winner below -- prefer text/plain, else
	   text/html, else the first part. 32 alternatives is far more
	   than any real message has; extras beyond that are silently
	   dropped from consideration rather than overflowing anything. */
	struct mimepart *AltParts[32];
	int AltCount = 0;
	/* multipart/mixed (and any other multipart that isn't
	   alternative or digest) similarly: parts are parsed via
	   mimepart rather than rendered immediately, so the first
	   displayable text part can be found (it need not be the
	   first part on the wire) and rendered, with every other part
	   listed as a one-line "[attachment: ...]" placeholder
	   afterward instead of today's button-per-part. Digest is left
	   on its original immediate-render path below (its parts are
	   nested messages, not attachments -- folding it into this
	   scheme would just list every digest message as an
	   "attachment" and render none of them). */
	struct mimepart *MixedParts[64];
	int MixedCount = 0;

	if (!amsutil_lc2strncmp("multipart/alternative", ContentTypeOverride, 21)) {
	    IsAlternative = 1;
	    text822_AlwaysInsertCharacters(d, ShowPos++, "\n", 1);
	}
	else if (!amsutil_lc2strncmp("multipart/digest", ContentTypeOverride, 16)) {
	    IsDigest = 1;
	    text822_AlwaysInsertCharacters(d, ShowPos++, "\n", 1);
	}
	FindParam(ContentTypeOverride, "boundary", boundary);
	ams_CUI_GenLocalTmpFileName(ams_GetAMS(), TmpFileName);
	/* Get to first boundary */
	while (fgets(Buf, sizeof(Buf), fp)
	       && ((Buf[0] != '-')
		   || (Buf[1] != '-')
		   || strncmp(&Buf[2], boundary, strlen(boundary)))) {
	    continue;
	}
	do { /* Handle next part */
	    boolean InHeader = TRUE, FoundContentType = FALSE;
	    int newlinecnt = 0;
	    foundBoundary = FALSE;
	    tmpfp = (FILE *) fopen (TmpFileName, "w");
	    if (!tmpfp) return(FALSE);
	    fprintf(tmpfp, "X-Bogus: bogus\n"); /* ensure there's at least one hdr */
	    *holdBuf = (char)0;
	    while (fgets(Buf, sizeof(Buf), fp) != NULL) {
		if(InHeader == TRUE && *Buf == '\n') {
		    newlinecnt++;
		    if(newlinecnt == 1) { /* see BodyFormat.ez: sect. 7.2.1 _Multipart:_The_common_syntax_ */
			/* Each part starts with CRLF, possibly zero headers, CRLF */
			InHeader = FALSE;
			newlinecnt = 0;
			if(IsDigest && !FoundContentType)
			    fprintf(tmpfp, "Content-Type: message/rfc822\n");
		    }
		}
		else if(!amsutil_lc2strncmp("content-type", Buf, 12))
		    FoundContentType = TRUE;
		else if((Buf[0] == '-') && (Buf[1] == '-')
		    && !amsutil_lc2strncmp(Buf+2, boundary, strlen(boundary))) {
		    foundBoundary = TRUE;
		    if (*holdBuf != (char)0)
			fputs(holdBuf, tmpfp);	/* found boundary, spit out holdBuf if it has contents */
		    break;
		}
		if(*holdBuf != (char)0)
		    fputs(holdBuf, tmpfp);
		strcpy(holdBuf, Buf);
	    }
	    if(foundBoundary == FALSE && *holdBuf != (char)0) { /* got to EOF without seeing boundary; spit out holdBuf */
		fputs(holdBuf, tmpfp);
	    }
	    fclose(tmpfp);
	    tmpfp = (FILE *) fopen(TmpFileName, "r");
	    if (!tmpfp) return(FALSE);
	    if ((Buf[2+strlen(boundary)] == '-') && (Buf[3+strlen(boundary)] == '-')) {
		IsFinalPart = 1;
	    }
	    if (IsAlternative) {
		if (AltCount < (int) (sizeof(AltParts) / sizeof(AltParts[0]))) {
		    rewind(tmpfp);
		    AltParts[AltCount++] = mimepart_ParseMessageFile(tmpfp);
		}
	    } else if (IsDigest) {
		ReadMessage(d, tmpfp, Mode, NULL, len, IsReallyTextObject, &dum, &dum, NULL, 1, 0, JunkAtEnd, 0);
	    } else {
		if (MixedCount < (int) (sizeof(MixedParts) / sizeof(MixedParts[0]))) {
		    rewind(tmpfp);
		    MixedParts[MixedCount++] = mimepart_ParseMessageFile(tmpfp);
		}
	    }
	    fclose(tmpfp);
	    if (IsAlternative) IsAlternative++;
	} while (!feof(fp) && !IsFinalPart);
	if (IsAlternative) {
	    /* pick the winner: first exact text/plain sibling; else
	       the first text/html; else just the first part -- mirrors
	       mimepart_SelectAlternative(), applied here across parts
	       that were parsed independently rather than as one
	       mimepart tree (the boundary-scan above already isolates
	       each part into its own temp file, so there was no single
	       buffer to hand mimepart_Parse as one multipart entity) */
	    struct mimepart *winner = NULL;
	    int wi;
	    for (wi = 0; wi < AltCount; ++wi) {
		if (AltParts[wi] && !strcmp(AltParts[wi]->type, "text/plain")) { winner = AltParts[wi]; break; }
	    }
	    if (!winner) {
		for (wi = 0; wi < AltCount; ++wi) {
		    if (AltParts[wi] && !strcmp(AltParts[wi]->type, "text/html")) { winner = AltParts[wi]; break; }
		}
	    }
	    if (!winner && AltCount > 0) winner = AltParts[0];

	    if (winner && winner->body && !strcmp(winner->type, "text/plain")) {
		InsertDecodedText(d, &ShowPos, winner->body, winner->bodylen, mimepart_GetParam(winner, "charset"));
	    } else if (winner && winner->body && !strcmp(winner->type, "text/html")) {
		/* No text/plain sibling, but there's html: the
		   deliberately dumb strip shim (see
		   revival/doc/mime-display-prompt.md), not inline HTML
		   rendering -- that's the separate HTML-rendering
		   objective's htmlview work. */
		char *stripped = mimepart_HtmlToText((char *) winner->body, winner->bodylen);
		InsertDecodedText(d, &ShowPos, (unsigned char *) stripped, (long) strlen(stripped), mimepart_GetParam(winner, "charset"));
		free(stripped);
	    } else if (winner && winner->body) {
		/* Neither text/plain nor text/html was on offer (e.g.
		   an alternative set of just an image and an audio
		   clip): fold the chosen, already CTE-decoded
		   alternative into one clickable object instead of
		   today's one-metamail-button-per-alternative pile. */
		char AltTmpName[1000];
		FILE *btmpfp;
		struct mailobj *mo = mailobj_New();
		struct environment *env;
		char Label[200];
		if (mo) {
		    ams_CUI_GenLocalTmpFileName(ams_GetAMS(), AltTmpName);
		    btmpfp = (FILE *) fopen(AltTmpName, "w");
		    if (btmpfp) {
			fwrite(winner->body, sizeof(char), winner->bodylen, btmpfp);
			fclose(btmpfp);
			btmpfp = (FILE *) fopen(AltTmpName, "r");
			if (btmpfp) {
			    text822_AlwaysInsertCharacters(d, ShowPos, "\n", 1);
			    ++ShowPos;
			    env = text822_AlwaysAddView(d, ShowPos, "mailobjv", mo);
			    ++ShowPos;
			    text822_AlwaysInsertCharacters(d, ShowPos, "\n", 1);
			    ++ShowPos;
			    mailobj_SetTextInsertion(mo, d, env);
			    mailobj_ReadAlienMail(mo, winner->type, NULL, btmpfp, FALSE);
			    fclose(btmpfp);
			    if (ContentDescription[0]) {
				sprintf(Label, "%s ('%s' format)", ContentDescription, winner->type);
			    } else if (Subject[0]) {
				sprintf(Label, "%s ('%s' format)", Subject, winner->type);
			    } else {
				strcpy(Label, "Object of type '");
				strncat(Label, winner->type, sizeof(Label) - 25);
				strcat(Label, "'");
			    }
			    mailobj_SetLabel(mo, 0, Label);
			}
		    }
		    unlink(AltTmpName);
		}
	    }
	    for (wi = 0; wi < AltCount; ++wi) {
		mimepart_Free(AltParts[wi]);
	    }
	}
	if (!IsAlternative && !IsDigest) {
	    /* find the first displayable text part: text/plain or
	       text/html directly, or -- for something like an HTML
	       mail with an attachment, which MUAs send as
	       multipart/mixed[multipart/alternative[...], attachment]
	       -- whichever mimepart_SelectAlternative() would pick out
	       of a nested multipart/alternative sibling */
	    int mi, textidx = -1, textIsHtml = 0;
	    const struct mimepart *textpart = NULL;

	    for (mi = 0; mi < MixedCount; ++mi) {
		struct mimepart *p = MixedParts[mi];
		if (!p) continue;
		if (!strcmp(p->type, "text/plain")) {
		    textidx = mi; textpart = p; textIsHtml = 0; break;
		}
		if (!strcmp(p->type, "text/html")) {
		    textidx = mi; textpart = p; textIsHtml = 1; break;
		}
		if (mimepart_IsMultipart(p) && !strcmp(p->type, "multipart/alternative")) {
		    const struct mimepart *sel = mimepart_SelectAlternative(p);
		    if (sel && sel->body && !strcmp(sel->type, "text/plain")) {
			textidx = mi; textpart = sel; textIsHtml = 0; break;
		    } else if (sel && sel->body && !strcmp(sel->type, "text/html")) {
			textidx = mi; textpart = sel; textIsHtml = 1; break;
		    }
		}
	    }
	    if (textpart && textpart->body) {
		if (textIsHtml) {
		    char *stripped = mimepart_HtmlToText((char *) textpart->body, textpart->bodylen);
		    InsertDecodedText(d, &ShowPos, (unsigned char *) stripped, (long) strlen(stripped), mimepart_GetParam(textpart, "charset"));
		    free(stripped);
		} else {
		    InsertDecodedText(d, &ShowPos, textpart->body, textpart->bodylen, mimepart_GetParam(textpart, "charset"));
		}
	    }
	    for (mi = 0; mi < MixedCount; ++mi) {
		struct mimepart *p = MixedParts[mi];
		char *fname;
		if (!p || mi == textidx) continue;
		fname = (char *) mimepart_GetDispParam(p, "filename");
		if (!fname) fname = (char *) mimepart_GetParam(p, "name");
		InsertAttachmentLine(d, &ShowPos, fname, p->type, p->bodylen);
	    }
	    for (mi = 0; mi < MixedCount; ++mi) {
		mimepart_Free(MixedParts[mi]);
	    }
	}
	unlink(TmpFileName);
    } else if (!ForceMet && !AlternativeNumber && !amsutil_lc2strncmp("text/html", ContentTypeOverride, 9)) {
	/* html-only: nothing better was available (a sibling
	   multipart/alternative branch above already prefers
	   text/plain when one exists). Deliberately dumb
	   strip-and-render shim -- see
	   revival/doc/mime-display-prompt.md -- scheduled for
	   replacement by the separate HTML-rendering objective's
	   htmlview work; kept small and isolated so it can be
	   deleted. Decoded lines are accumulated (respecting the same
	   \\enddata{text822 sentinel the plain-ASCII branch below
	   honors) rather than inserted one at a time, since the strip
	   pass needs to see the whole body at once. */
	int EncodingCode = ParseEncoding(ContentEncoding);
	char *msgcharset = NULL;
	char *rawbuf = NULL;
	long rawlen = 0, rawcap = 0;

	if (!FindParam(ContentTypeOverride, "charset", charsetbuf)) {
	    msgcharset = charsetbuf;
	}
	while (fgetsdecoding(LineBuf, sizeof(LineBuf), fp, EncodingCode)) {
	    if (!strncmp(LineBuf, "\\enddata{text822", 16)) {
		SawEndData = TRUE;
		break;
	    }
	    linelen = strlen(LineBuf);
	    if (rawlen + linelen + 1 > rawcap) {
		char *nb;
		rawcap = rawcap ? rawcap * 2 : 4000;
		if (rawcap < rawlen + linelen + 1) rawcap = rawlen + linelen + 1;
		nb = realloc(rawbuf, rawcap);
		if (!nb) break;
		rawbuf = nb;
	    }
	    bcopy(LineBuf, rawbuf + rawlen, linelen);
	    rawlen += linelen;
	}
	if (rawbuf) {
	    char *stripped = mimepart_HtmlToText(rawbuf, rawlen);
	    InsertDecodedText(d, &ShowPos, (unsigned char *) stripped, (long) strlen(stripped), msgcharset);
	    free(stripped);
	    free(rawbuf);
	}
    } else if (!AlternativeNumber && sfmttype
		&& !InsertProperObject(d, fp, &ShowPos, sfmttype, ContentEncoding, ContentDescription[0] ? ContentDescription : Subject)) {
	/* All was handled by InsertProperObject */
    } else if (!AlternativeNumber &&
		(!ContentTypeOverride[0]
		 || PlainAsciiText(ContentTypeOverride, currentcharset))) {
	int EncodingCode = ParseEncoding(ContentEncoding);
	char *msgcharset = NULL;
	/* a UTF-8 line's bytes can't contain a literal '\n' (0x0A) --
	   every continuation/lead byte of a multi-byte sequence is
	   >= 0x80 -- so converting charset per fgetsdecoding()-line is
	   exactly as correct as converting the whole decoded body at
	   once, without having to buffer the whole thing here */
	if (ContentTypeOverride[0] && !FindParam(ContentTypeOverride, "charset", charsetbuf)) {
	    msgcharset = charsetbuf;
	}
	while(fgetsdecoding(LineBuf, sizeof(LineBuf), fp, EncodingCode)) {
	    if (!strncmp(LineBuf, "\\enddata{text822", 16)) {
		SawEndData = TRUE;
		break;
	    }
	    linelen = strlen(LineBuf);
	    InsertDecodedText(d, &ShowPos, (unsigned char *) LineBuf, linelen, msgcharset);
	}
    } else {
	struct mailobj *mo = mailobj_New();
	struct text *t = text_New();
	struct environment *env;
	char Label[200];

	if (!mo || !t) {
	    message_DisplayString(NULL, 10, "Cannot create mailobj object");
	    return(FALSE);
	}
	text822_AlwaysInsertCharacters(d, ShowPos, "\n", 1);
	++ShowPos;
	env = text822_AlwaysAddView(d, ShowPos, "mailobjv", mo);
	++ShowPos;
	text822_AlwaysInsertCharacters(d, ShowPos, "\n", 1);
	++ShowPos;
	mailobj_SetTextInsertion(mo, d, env);
	mailobj_ReadAlienMail(mo, ContentTypeOverride, ContentEncoding, fp, FALSE);
	if (AlternativeNumber) {
	    sprintf(Label, "Alternative Version #%d ('%s' format)", AlternativeNumber, *sfmttype ? sfmttype : "text/plain");
	} else if (ContentDescription[0]) {
	    sprintf(Label, "%s ('%s' format)", ContentDescription, *sfmttype ? sfmttype : "text/plain");
	} else if (Subject[0]) {
	    sprintf(Label, "%s ('%s' format)", Subject, *sfmttype ? sfmttype : "text/plain");
	} else {
	    strcpy(Label, "Object of type '");
	    strncat(Label, ContentTypeOverride, sizeof(Label) - 25);
	    strcat(Label, "'");
	}
	mailobj_SetLabel(mo, 0, Label);
    }
    if (!SawEndData) {
	fgets(LineBuf, sizeof(LineBuf), fp); /* Just eat it */
	while (LineBuf[0] == '\n') {
	    LineBuf[0] = '\0';
	    fgets(LineBuf, sizeof(LineBuf), fp);
	}
	if (strncmp(LineBuf, "\\enddata{text822", 16)) {
	    /*	    fprintf(stderr, "Missing enddata in text822 -- saw %s instead\n", LineBuf); */
	}
    }
    if (!feof(fp)) {
	pos = ftell(fp);
	fgets(LineBuf, sizeof(LineBuf), fp);
	if (strncmp(LineBuf, "\\begindata{text822", 18)) {
	    fseek(fp, pos, 0);
	} else {
	    /* text822 is funny this way -- it always peeks at and reads any following text822 objects, allowing concatenation but disallowing embedding consecutive text822 objects. */
	    struct text *t = (struct text *) d;
	    pos = text822_GetLength(d);
	    text822_AlwaysInsertCharacters(d, pos, "\n.bp\n\n", 6);

	    /* Make sure there is no style at the end that grows to include the new stuff */
	    if (environment_Remove(t->rootEnvironment, pos, 6, environment_Style, TRUE)) {
		text_SetModified(t);
	    }
	    text_RegionModified(t, pos, 6);
	    text_NotifyObservers(t, 0);

	    et = environment_InsertStyle(((struct text *)d)->rootEnvironment, pos+1, FormatStyle, 1);
	    environment_SetLength(et, 4);
	    goto restart;
	}
    }
    pos = text822_GetLength(d);
    text822_AlwaysDeleteCharacters(d, pos--, 1);
    if (Mode & MODE822_ROT13) {
	RotateThirteen(d, *BodyStart);
    }
    if (pos < 3 && !AuxHeadText && !InsideRecursion) {
	text822_AlwaysInsertCharacters(d, 0, EmptyMsgString, strlen(EmptyMsgString));
	*BodyStart = 0;
    }
    text822_SetGlobalStyle(d, GlobalStyle);
    return(TRUE);
}

char *text822__ViewName(t)
struct text822 *t;
{
    return("textview"); /* t822view is not necessary */
}

long text822__Write(self, fp, writeID, level)
struct text822 *self;
FILE *fp;
long writeID;
int level;
{
    int bodystart, len;
    unsigned char ch;
    boolean SawNewline = FALSE, SawPrevNewline = FALSE;

    if (self->header.dataobject.writeID != writeID)  {
	self->header.dataobject.writeID = writeID;
	fprintf(fp, "\\begindata{%s, %ld}\n", class_GetTypeName(self), dataobject_UniqueID(&self->header.dataobject));
	len = text822_GetLength(self);
	for (bodystart = 0; bodystart <= len; ++bodystart) {
	    ch = text822_GetChar(self, bodystart);
	    if (ch == '\n') {
		if (SawNewline) {
		    if (SawPrevNewline) break;
		    SawPrevNewline = TRUE;
		} else {
		    fputc(ch, fp);
		}
		SawNewline = TRUE;
	    } else {
		fputc(ch, fp);
		SawNewline = FALSE;
	    }
	}
	fprintf(fp, "\n\\textdsversion{%d}\n", 12); /* BOGUS -- CAN'T GET RIGHT */
	if (((struct text *) self)->styleSheet->templateName)
	    fprintf(fp, "\\template{%s}\n", ((struct text *) self)->styleSheet->templateName);
	stylesheet_Write(((struct text *) self)->styleSheet, fp);
	text822_WriteSubString(self, bodystart, text822_GetLength(self) - bodystart, fp, TRUE);
	fprintf(fp, "\\enddata{%s,%d}\n", class_GetTypeName(self), self->header.dataobject.id);
	fflush(fp);
    }
    return self->header.dataobject.id;
}

static RotateThirteen(d, start)
struct text *d;
int start;
{
    register char *cp,*ecp;
    long len, lengotten;
    char *tbuf;

    if (start < 0) start = 0;
    len = text_GetLength(d) - start;
    while (len > 0) {
	tbuf = text_GetBuf(d, start, len, &lengotten);
	for (cp = tbuf, ecp = cp+lengotten; cp<ecp; ++cp) {
	    if ( (*cp >= 0x41 && *cp <= 0x5a) || 
		(*cp >= 0x61 && *cp <= 0x7a) )
		*cp = (((((*cp -1 ) & 0X1F) + 13)  % 26) + 1)
		  | (*cp & 0XE0);
	}
	text_ReplaceCharacters(d, start, lengotten, tbuf, lengotten);
	start += lengotten;
	len -= lengotten;
    }
}

void text822__Clear(self)
struct text822 *self;
{
    super_Clear(self);
    if (text822_ReadTemplate(self, "messages", FALSE)) {
	fprintf(stderr, "Could not read messages template!\n");
    }
    text822_SetGlobalStyle(self, GlobalStyle);
}

void text822__ClearCompletely(self)
struct text822 *self;
{
    super_ClearCompletely(self);
    if (text822_ReadTemplate(self, "messages", FALSE)) {
	fprintf(stderr, "Could not read messages template!\n");
    }
    text822_SetGlobalStyle(self, GlobalStyle);
}

long text822__ReadAsText(self, fp, id)
struct text822 *self;
FILE *fp;
long id;
{
    return(super_Read(self, fp, id));
}

void text822__ResetGlobalStyle(c, t)
struct classheader *c;
struct text822 *t;
{
    text822_SetGlobalStyle(t, GlobalStyle);
}

static char *paramend(s)
char *s;
{
    int inquotes=0;
    while (*s) {
	if (inquotes) {
	    if (*s == '"') {
		inquotes = 0;
	    } else if (*s == '\\') {
		++s; /* skip a char */
	    }
	} else if (*s == ';') {
	    return(s);
	} else if (*s == '"') {
	    inquotes = 1;
	}
	++s;
    }
    return(NULL);
}        

static FindParam(ct, paramname, ValueBuf)
char *ct, *paramname, *ValueBuf;
{
    char *s, *t, *t2, *eq, BigBuf[1000];

    strcpy(BigBuf, ct);
    s = strchr(BigBuf, ';');
    if (!s) return(1);
    *s++ = (char)0;
    do {
	t = paramend(s);
	if (t) *t++ = (char)0;
	eq = strchr(s, '=');
	if (eq) {
	    *eq++ = (char)0;
	    /* strip leading white space */
	    while (*s && isspace(*s)) ++s;
	    if (!amsutil_lc2strncmp(paramname, s, 8)) {
		/* strip leading white space */
		while (*eq && isspace(*eq)) ++eq;
		/* strip trailing white space */
		t2 = eq+strlen(eq) -1;
		while (isspace(*t2)) *t2-- = (char)0;
		if (*eq == '"') {
		    s = UnquoteString(eq);
		    if (s) eq = s;
		}
		strcpy(ValueBuf, eq);
		return(0);
	    }
	}
	s = t;
    } while (t);
    return(1);
}

static InsertProperObject(d, fp, ShowPos, ctype, encoding, descrip) 
struct text822 *d;
FILE *fp;
int *ShowPos;
char *ctype;
char *encoding;
char *descrip;
{
    int pos;

    if (ForceMetamail(ctype)) return(-1);
    pos = ftell(fp);
    while (encoding && isspace(*encoding)) ++encoding;
    if (!strncmp("image/", ctype, 6)) {
	struct dataobject *dob;
	if (!strncmp("gif", ctype + 6, 3))
	    dob = (struct dataobject *) class_NewObject("gif");
	else if (!strncmp("pbm", ctype + 6, 3) || !strncmp("pnm", ctype + 6, 3) || !strncmp("ppm", ctype + 6, 3) || !strncmp("pgm", ctype + 6, 3)) {
	    dob = (struct dataobject *) class_NewObject("pbm");
	    if(!dob) printf("Couldn't allocate pbm object\n");
	}
	else if (!strncmp("jpeg", ctype + 6, 4))
	    dob = (struct dataobject *) class_NewObject("jpeg");
	else
	    dob = (struct dataobject *) class_NewObject("raster");
	/* We don't use raster_New, etc. to avoid dependencies */
	if (!dob) return(-1);
	if (!dataobject_ReadOtherFormat(dob, fp, ctype, encoding, descrip)) {
	    fseek(fp, pos, 0);
	    dataobject_Destroy(dob);
	    return(-1);
	}
	text822_AlwaysAddView(d, *ShowPos, dataobject_ViewName(dob), dob);
	++*ShowPos;
	return(0);
    }
    if (!strncmp("audio/", ctype, 6)) {
	struct dataobject *dob = (struct dataobject *) class_NewObject("alink");
	/* We don't use alink_New, etc. to avoid dependencies */
	if (!dob) return(-1);
	if (!dataobject_ReadOtherFormat(dob, fp, ctype, encoding, descrip)) {
	    fseek(fp, pos, 0);
	    dataobject_Destroy(dob);
	    return(-1);
	}
	text822_AlwaysAddView(d, *ShowPos, dataobject_ViewName(dob), dob);
	++*ShowPos;
	return(0);
    }
    return(-1);
}

static ParseEncoding(enc)
char *enc;
{
    /* These codes are defined in mailobj.ch */
    /* strip leading white space */
    while (*enc && isspace(*enc)) ++enc;
    if (!amsutil_lc2strncmp("base64", enc, 6)) {
	return ENC_B64;
    } else if (!amsutil_lc2strncmp("quoted-printable", enc, 16)) {
	return ENC_QP;
    }
    return ENC_NONE;
}

static getcdecoding(fp, code)
FILE *fp;
int code;
{
    switch(code) {
	case ENC_B64:
	    return(getc64(fp));
	case ENC_QP:
	    return(getcqp(fp));
	default:
	    return(getc(fp));
    }
}

static ungetcdecoding(c, fp, code)
int c;
FILE *fp;
int code;
{
    switch(code) {
	case ENC_B64:
	    return(ungetc64(c, fp));
	case ENC_QP:
	    return(ungetcqp(c, fp));
	default:
	    return(ungetc(c, fp));
    }
}

static int charspending=0, nextpending=0;
static int pendingchars[80];
static FILE *lastfp = NULL;

static getc64(fp)
FILE *fp;
{
    int c1, c2, c3, c4;

    if (fp != lastfp) charspending = 0; /* bad hack */
    if (charspending) {
	--charspending;
	return(pendingchars[nextpending++]);
    }
    lastfp = fp;
    do {
	c1 = getc(fp);
    } while (c1 != EOF && isspace(c1));
    if (c1 == EOF) return(EOF);
    do {
	c2 = getc(fp);
    } while (c2 != EOF && isspace(c2));
    if (c2 == EOF) return(EOF);
    do {
	c3 = getc(fp);
    } while (c3 != EOF && isspace(c3));
    do {
	c4 = getc(fp);
    } while (c4 != EOF && isspace(c4));
    if (c3 == EOF) c3 = '=';
    if (c4 == EOF) c4 = '=';
    c1 = char64(c1);
    c2 = char64(c2);
    c1 = ((c1<<2) | ((c2&0x30)>>4));
    nextpending=0;
    charspending=0;
    if (c3 != '=') {
	c3 = char64(c3);
	pendingchars[0]= (((c2&0XF) << 4) | ((c3&0x3C) >> 2));
	++charspending;
	if (c4 != '=') {
	    c4 = char64(c4);
	    pendingchars[1] = (((c3&0x03) <<6) | c4);
	    ++charspending;
	}
    }
    return(c1);
}

static ungetc64(c, fp)
int c;
FILE *fp;
{
    int i;
    for (i=nextpending+charspending; i>nextpending; --i) {
	pendingchars[i] = pendingchars[i - 1];
    }
    pendingchars[nextpending] = c;
    ++charspending;
}

static getcqp(fp)
FILE *fp;
{
    int c1, c2;

    if (fp != lastfp) charspending = 0; /* bad hack */
    if (charspending > 0) {
	charspending--;
	return(pendingchars[charspending]);
    }
    lastfp = fp;
    c1 = getc(fp);
    if (c1 == '=') {
	c1 = getc(fp);
	if (c1 == EOF) return(EOF);
	if (c1 == '\n') {
	    /* soft break, LF form: ignore it */
	    return(getcqp(fp));
	} else if (c1 == '\r') {
	    int c1b = getc(fp);
	    if (c1b == '\n') {
		/* soft break, CRLF form: ignore both */
		return(getcqp(fp));
	    }
	    if (c1b != EOF) ungetc(c1b, fp);
	    return('X'); /* malformed escape: lenient passthrough */
	} else {
	    c2 = getc(fp);
	    c1 = hexchar(c1);
	    c2 = hexchar(c2);
            if (c1<0 || c2 < 0 || c1 > 15 || c2 > 15) return('X'); /* just don't crap out */
	    return(c1<<4 | c2);
	}
    } else {
	return(c1);
    }
}

static ungetcqp(c, fp)
int c;
FILE *fp;
{
    pendingchars[charspending++] = c;
}

static char *
fgetsdecoding(buf, size, fp, code)
char *buf;
int size, code;
FILE *fp;
{
    char *s=buf, *end = buf+size -1;
    int c;

    if (code == ENC_NONE) {
	return(fgets(s, size, fp));
    } else {
	while (s < end) {
	    c = getcdecoding(fp, code);
	    if (c == EOF) break;
	    *s++ = c;
	    if (c == '\n') break; /* this DID get copied */
	}
	*s = (char)0;
	if (s==buf) return(NULL);
	return(buf);
    }
}

static char basis_hex[] = "0123456789ABCDEF";
static char basis_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static hexchar(c)
int c;
{
    char *s;
    if (islower(c)) c = toupper(c);
    s = (char *) strchr(basis_hex, c);
    if (s) return(s-basis_hex);
    return(-1);
}

static char64(c)
int c;
{
    char *s = (char *) strchr(basis_64, c);
    if (s) return(s-basis_64);
    return(-1);
}

static char *UnquoteString(s)
char *s;
{
    char *ans, *t;

    if (*s != '"') return(s);
    ans = malloc(1+strlen(s));
    if (!ans) return(NULL);
    ++s;
    t = ans;
    while (*s) {
	if (*s == '\\') {
	    *t++ = *++s;
	} else if (*s == '"') {
	    break;
	} else {
	    *t++ = *s;
	}
	++s;
    }
    *t = (char)0;
    return(ans);
}

static PlainAsciiText(s, currentcharset)
char *s, *currentcharset;
{
    char *t, *semi;
    char Buf[1000];

    /* strip leading white space */
    while (*s && isspace(*s)) ++s;
    strncpy(Buf, s, (sizeof(Buf)));
    for (t=Buf; *t; ++t) {
	if (isupper(*t)) *t = tolower(*t);
    }
    /* Charset no longer gates this: ISO-8859-1/US-ASCII pass straight
       through, UTF-8 is converted to Latin-1, and anything else is
       still passed through as bytes (see mimepart.h) -- the caller
       does that conversion at the point of insertion, keyed off the
       same charset param this function used to reject on. currentcharset
       (this function's own idea of "what charset are we running in")
       is unused now for that reason, but is left in the signature to
       avoid touching the one call site's argument list. */
    semi = strchr(Buf, ';');
    if (semi) *semi=NULL;
    /* strip trailing white space */
    t = Buf+strlen(Buf) -1;
    while (isspace(*t)) *t-- = (char)0;
    if (strcmp(Buf, "text") && strcmp(Buf, "text/plain")) return(0);
    return(1);
}

static int ForceMetamail(ctype)
char *ctype;
{
    static char **ForceTypes = NULL;
    int i = 0, len = strlen(ctype), complen;

    if (!ForceTypes) {
	int ct=1;
	char *s, *RawForces = environ_GetProfile("messages.forcemetamail");
	if (!RawForces) RawForces = "";
	for (s=RawForces; *s; ++s) {
	    if (*s == ',') {
		++ct;
	    } else if (isupper(*s)) {
		*s = tolower(*s);
	    }
	}
	s = malloc(1+strlen(RawForces));
	if (!s) return(0);
	strcpy(s, RawForces);
	ForceTypes = (char **) malloc((1+ct) * sizeof(char *));
	if (!ForceTypes) return(0);
	ct = 0;
	while (1) {
	    ForceTypes[ct++] = s;
	    s = strchr(s, ',');
	    if (s) {
		*s++ = (char)0;
	    } else {
		break;
	    }
	}
	ForceTypes[ct] = NULL;
    }
    while (ForceTypes[i]) {
	complen = strlen(ForceTypes[i]);
	if (len < complen) complen = len;
	if (complen > 0 && !amsutil_lc2strncmp(ForceTypes[i], ctype, complen)) return(1);
	++i;
    }
    return(0);
}

/* Inserts already-CTE-decoded bytes as characters at *ShowPos,
   advancing it -- converting UTF-8 to Latin-1 first if charset says
   "utf-8" (mimepart_Utf8ToLatin1: exact for codepoints <= 0xFF, '?'
   above that); ISO-8859-1/US-ASCII/absent/anything else passes
   through as bytes unchanged, per the charset policy in
   revival/doc/mime-display-prompt.md. charset may be NULL. */
static int InsertDecodedText(d, ShowPos, bytes, len, charset)
struct text822 *d;
int *ShowPos;
unsigned char *bytes;
long len;
char *charset;
{
    long i, o;

    /* ATK's text world is LF-only; strip any stray '\r' in place
       (bytes is always ours to mutate here -- a stack line buffer, a
       part body about to be freed, or a strip-shim's own malloc'd
       result). Quoted-printable does not escape ordinary hard line
       breaks, only intentional soft breaks and 8-bit/control bytes,
       so a CRLF-terminated message's decoded lines still carry a
       literal '\r' before the '\n' unless stripped here -- left in,
       it shows up as an extra vertical gap per line in the ATK text
       widget. Since o <= i always, this is a safe in-place
       compaction, never an overlapping grow. */
    for (i = 0, o = 0; i < len; ++i) {
	if (bytes[i] != '\r') bytes[o++] = bytes[i];
    }
    len = o;

    if (charset && !amsutil_lc2strncmp("utf-8", charset, 5)) {
	long convlen;
	unsigned char *conv = mimepart_Utf8ToLatin1(bytes, len, &convlen);
	if (conv) {
	    text822_AlwaysInsertCharacters(d, *ShowPos, (char *) conv, convlen);
	    *ShowPos += convlen;
	    free(conv);
	    return(1);
	}
    }
    text822_AlwaysInsertCharacters(d, *ShowPos, (char *) bytes, len);
    *ShowPos += len;
    return(1);
}

/* Inserts one "[attachment: <filename> (<type/subtype>, <n> bytes)]"
   line (plus trailing newline) at *ShowPos, advancing it. Saving
   attachments is out of scope for this display path (see the mime-
   display task); this is a label only. */
static void InsertAttachmentLine(d, ShowPos, filename, ctype, nbytes)
struct text822 *d;
int *ShowPos;
char *filename, *ctype;
long nbytes;
{
    char Line[600];

    sprintf(Line, "[attachment: %.200s (%.100s, %ld bytes)]\n",
	    filename ? filename : "unnamed", ctype ? ctype : "unknown", nbytes);
    text822_AlwaysInsertCharacters(d, *ShowPos, Line, strlen(Line));
    *ShowPos += strlen(Line);
}
