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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/atkams/messages/lib/RCS/amsutil.c,v 1.14 1992/12/15 21:47:14 rr2b R6tape $";
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <andrewos.h>
#include <mailconf.h>
#include <class.h>
#include <environ.ih>
#include <amsutil.eh>
#include <proctbl.ih>
#include <ams.ih>
#include <ams.h> /* Could have been cui.h, but not yet necessary here */
#include <util.h>

extern char *StripWhiteEnds(char *string), *convlongto64(int num, int pad), *cvEng(int foo, int Capitalized, int MaxToSpellOut);
extern char **BreakDownResourcesIntoArray(char *reslist);
extern FILE *dbg_fopen(char *path, char *type);
extern int dbg_close(int fd), dbg_fclose(FILE *fp), dbg_vclose(int fd), dbg_vfclose(FILE *fp);
extern int fdplumb_SpillGutsToFile(FILE *fp, int ExtraNewLines);
/* ams/libs/shr -- no header declares these anywhere in the tree */
extern int LowerStringInPlace(char *string, int len), ReduceWhiteSpace(char *string), lc2strncmp(char *s1, char *s2, int len);
extern int BreakDownContentTypeField(char *HeadBuf, char *fmt, int fmtsz, char *vers, int verssz, char *resources, int resourcessz);
extern int GetBinaryOptions();	/* same-file forward reference, defined below */
extern long conv64tolong(char *xnum);

static struct OptionState MyOpts;
static char **KeyHeaders = NULL;

boolean amsutil__InitializeClass(struct classheader *c)
{
    GetBinaryOptions();
    KeyHeaders = amsutil_ParseKeyHeaders();
    return(TRUE);
}

char ** amsutil__GetKeyHeadsArray(struct classheader *c)
{
    return(KeyHeaders);
}

int amsutil__GetOptBit(struct classheader *c, int opt)
{
    return(GETOPTBIT(MyOpts.Opts, opt));
}

void amsutil__SetOptBit(struct classheader *c, int opt, int val)
{
    SETOPTBIT(MyOpts.Opts, opt, val);
}

int amsutil__GetPermOptBit(struct classheader *c, int opt)
{
    return(GETOPTBIT(MyOpts.PermOpts, opt));
}

void amsutil__SetPermOptBit(struct classheader *c, int opt, int val)
{
    SETOPTBIT(MyOpts.PermOpts, opt, val);
}

int amsutil__GetOptMaskBit(struct classheader *c, int opt)
{
    return(GETOPTBIT(MyOpts.OptMask, opt));
}

void amsutil__SetOptMaskBit(struct classheader *c, int opt, int val)
{
    SETOPTBIT(MyOpts.OptMask, opt, val);
}

void amsutil__BreakDownContentTypeField(struct classheader *c, char *override, char *fmttype, int fmttypelen, char *fmtvers, int fmtverslen, char *fmtresources, int fmtresourceslen)
{
    BreakDownContentTypeField(override, fmttype, fmttypelen, fmtvers, fmtverslen, fmtresources, fmtresourceslen);
}

char ** amsutil__BreakDownResourcesIntoArray(struct classheader *c, char *res)
{
    return(BreakDownResourcesIntoArray(res));
}

int amsutil__lc2strncmp(struct classheader *c, char *s1, char *s2, int len)
{
    return(lc2strncmp(s1, s2, len));
}

char * amsutil__StripWhiteEnds(struct classheader *c, char *s)
{
    return(StripWhiteEnds(s));
}

char * amsutil__cvEng(struct classheader *c, int num, int min, int max)
{
    return(cvEng(num, min, max));
}

char * amsutil__convlongto64(struct classheader *c, long t, long p)
{
    return(convlongto64(t, p));
}

long amsutil__conv64tolong(struct classheader *c, char *s64)
{
    return(conv64tolong(s64));
}

int amsutil__setprofilestring(struct classheader *c, char *prog, char *pref, char *val)
{
    return(setprofilestring(prog, pref, val));
}

char ** amsutil__ParseKeyHeaders(struct classheader *c)
{
    int numkeys = 0;
    char *s, *t;
    char **KeyHeads;

    s = (char *) environ_GetProfile("messages.keyheaders");
    if (!s) {
	s = "From:Date:Subject:To:CC:ReSent-From:ReSent-To";
    }

    for (t=s; t=strchr(t, ':'); ++t, ++numkeys) {
	;
    }
    KeyHeads = (char **) malloc((2+numkeys) * sizeof(char *));
    t = malloc(1+strlen(s));
    strcpy(t, s); /* permanent copy */
    LowerStringInPlace(t, strlen(t));
    KeyHeads[0] = t;
    numkeys = 1;
    for (s=t; s=strchr(s, ':'); ++s, ++numkeys) {
	*s++ = '\0';
	if (*s) {
	    KeyHeads[numkeys] = s;
	} else {
	    --numkeys;
	}
    }
    KeyHeads[numkeys] = NULL;
    return(KeyHeads);
}

long hatol(char *s)
{
    long n;
    char c;

    n = 0;
    while (c = *s) {
	if (c >= '0' && c <= '9') {
	    n = (16 * n) + c - '0';
	} else if (c >= 'a' && c <= 'f') {
	    n = (16 * n) + c - 'a' + 10;
	} /* ignore all other characters, including leading 0x and whitespace */
	++s;
    }
    return(n);
}

int GetBinaryOptions()
{
    int i;
    char *s, *t, *u;

    for (i=0; i<=(EXP_MAXUSED/32); ++i) {
	MyOpts.DefaultOpts[i] = 0; /* All defaults to zero */
    }
    s = environ_GetProfile("messages.binaryoptions");
    if (s) {
	long dum, dum2;
	int offset;

	offset = 0;
	while (s && offset <= (EXP_MAXUSED/32)) {
	    /* This is a pain, but it is portable */
	    t = strchr(s, ',');
	    if (t) *t++ = '\0';
	    u = strchr(s, '/');
	    if (u) *u++ = '\0';
	    dum = hatol(s);
	    dum2 = u ? hatol(u) : 0;
	    MyOpts.OptMask[offset] = dum2;
	    MyOpts.PermOpts[offset] = MyOpts.Opts[offset] = (dum & dum2) | (MyOpts.DefaultOpts[offset] & ~dum2);
	    s = t;
	    ++offset;
	}
    } else { /* first time for everything ... */
	char DumBuf[300];
	int bigmenus;

	/* First time with new option settings, get old preferences for
	   backwards compatibility; */

	for (i=0; i<=(EXP_MAXUSED/32); ++i) {
	    MyOpts.OptMask[i] = 0;
	    MyOpts.Opts[i] = MyOpts.DefaultOpts[i];
	}
	if (environ_GetProfileSwitch("messages.showclasses", 0)) {
	    SETOPTBIT(MyOpts.Opts, EXP_SHOWCLASSES, 1);
	    SETOPTBIT(MyOpts.Opts, EXP_FILEINTO, 1);
	    SETOPTBIT(MyOpts.Opts, EXP_FILEINTOMENU, 1);
	    SETOPTBIT(MyOpts.Opts, EXP_FILEICONCAPTIONS, 1);
	}
	if (environ_GetProfile("messages.crucialclasses") || environ_GetProfile("messages.maxclassmenu")) {
	    SETOPTBIT(MyOpts.Opts, EXP_FILEINTO, 1);
	    SETOPTBIT(MyOpts.Opts, EXP_FILEINTOMENU, 1);
	}
	s = environ_GetProfile("messages.density");
	if (s) {
	    s = amsutil_StripWhiteEnds(s);
	    if (*s == 'l' || *s == 'l') {
		SETOPTBIT(MyOpts.Opts, EXP_WHITESPACE, 1);
	    } else if (*s == 'm' || *s == 'M') {
		SETOPTBIT(MyOpts.Opts, EXP_WHITESPACE, 1);
	    } else if (*s != 'H' && *s != 'h') {
		/* Ignoring the stupid obsolete density preference, shich should go away anyway */
	    }
	}
	if (!environ_GetProfileSwitch("messages.wastespaceandtime", 1)) {
	    SETOPTBIT(MyOpts.Opts, EXP_SHOWNOHEADS, 1);
	}
	if (environ_GetProfileSwitch("messages.fixcaptionfont", 0)) {
	    SETOPTBIT(MyOpts.Opts, EXP_FIXCAPTIONS, 1);
	}
	if (environ_GetProfileSwitch("messages.purgeonquitting", 0)) {
	    SETOPTBIT(MyOpts.Opts, EXP_PURGEONQUIT, 1);
	}
	if (environ_GetProfileSwitch("messages.subscriptionexpert", 0)) {
	    SETOPTBIT(MyOpts.Opts, EXP_SUBSEXPERT, 1);
	}	
	if (environ_GetProfileInt("messages.mailfontbloat", 0)) {
	    SETOPTBIT(MyOpts.Opts, EXP_WHITESPACE, 1);
	}
	if (environ_GetProfileSwitch("messages.clearaftersending", 0)) {
	    SETOPTBIT(MyOpts.Opts, EXP_CLEARAFTER, 1);
	}
	if (environ_GetProfileSwitch("messages.hideaftersending", 0)) {
	    SETOPTBIT(MyOpts.Opts, EXP_HIDEAFTER, 1);
	}
	if (environ_GetProfileSwitch("messages.bcc", 0)) {
	    SETOPTBIT(MyOpts.Opts, EXP_KEEPBLIND, 1);
	}
	bigmenus = environ_GetProfileInt("messages.menulevel", 0);
	if (!bigmenus) {
	    bigmenus = environ_GetProfileSwitch("messages.bigmenus", 0);
	    if (bigmenus) bigmenus = 127;
	}
	if (bigmenus) {
	    if (bigmenus & 1) SETOPTBIT(MyOpts.Opts, EXP_SHOWMORENEXT, 1);
	    if (bigmenus & 4) SETOPTBIT(MyOpts.Opts, EXP_MARKASUNREAD, 1);
	    if (bigmenus & 8) SETOPTBIT(MyOpts.Opts, EXP_APPENDBYNAME, 1);
	    if (bigmenus & 16) SETOPTBIT(MyOpts.Opts, EXP_MARKEDEXTRAS, 1);
	    if (bigmenus & 32) SETOPTBIT(MyOpts.Opts, EXP_INSERTHEADER, 1);
	    if (bigmenus & 64) SETOPTBIT(MyOpts.Opts, EXP_SETQUITHERE, 1);
	}
	for (i=0; i<=(EXP_MAXUSED/32); ++i) {
	    MyOpts.PermOpts[i] = MyOpts.Opts[i];
	    MyOpts.OptMask[i] |= MyOpts.Opts[i] ^ MyOpts.DefaultOpts[i];
	}
	amsutil_BuildOptionPreference(DumBuf);
	setprofilestring("messages", "BinaryOptions", DumBuf);
    }
}


void amsutil__BuildOptionPreference(struct classheader *c, char *buf)
{
    int whichbyte;
    char MyBuf[50];

    *buf = '\0';
    for (whichbyte = 0; whichbyte <= (EXP_MAXUSED/32); ++whichbyte) {
	sprintf(MyBuf, "0x%lx/0x%lx", MyOpts.PermOpts[whichbyte], MyOpts.OptMask[whichbyte]);
	if (*buf) strcat(buf, ", ");
	strcat(buf, MyBuf);
    }
}

void amsutil__ReduceWhiteSpace(struct classheader *c, char *s)
{
    ReduceWhiteSpace(s);
}

void amsutil__LowerStringInPlace(struct classheader *c, char *s, int slen)
{
    LowerStringInPlace(s, slen);
}

int amsutil__dbg_open(struct classheader *c, char *name, int flags, int mode)
{
    return(dbg_open(name, flags, mode));
}

FILE * amsutil__dbg_fopen(struct classheader *c, char *name, char *mode)
{
    return(dbg_fopen(name, mode));
}

int amsutil__dbg_close(struct classheader *c, int fd)
{
    return(dbg_close(fd));
}

int amsutil__dbg_vclose(struct classheader *c, int fd)
{
    return(vclose(fd));
}

int amsutil__dbg_fclose(struct classheader *c, FILE *fp)
{
    return(dbg_fclose(fp));
}

int amsutil__dbg_vfclose(struct classheader *c, FILE *fp)
{
    return(dbg_vfclose(fp));
}

void amsutil__fdplumb_SpillGutsToFile(struct classheader *c, FILE *fp, boolean doublenewlines)
{
#ifdef PLUMBFDLEAKS
    fdplumb_SpillGutsToFile(fp, doublenewlines);
#else /* #ifdef PLUMBFDLEAKS */
    fprintf(fp, "FD plumbing not compiled in!\n\n\n");
#endif /* #ifdef PLUMBFDLEAKS */
}

void amsutil__fdplumb_SpillGuts(struct classheader *c)
{
#ifdef PLUMBFDLEAKS
    fdplumb_SpillGuts();
#else /* #ifdef PLUMBFDLEAKS */
    printf("FD plumbing not compiled in!\n\n\n");
#endif /* #ifdef PLUMBFDLEAKS */
}

char * amsutil__GetDefaultFontName(struct classheader *c)
{
    static char *myfontname = NULL;

    if (!myfontname) {
	myfontname = environ_GetProfile("fontfamily");
	if (!myfontname || !*myfontname) {
	    myfontname = "andy";
	} else {
	    char *t;

	    t = (char *) malloc(1+strlen(myfontname));
	    if (t) {
		strcpy(t, myfontname);
		myfontname = t;
		amsutil_ReduceWhiteSpace(myfontname);
		for (t=myfontname; *t; ++t) {
		    if (isupper(*t)) *t = tolower(*t);
		}
	    } else {
		myfontname = "andy";
	    }
	}
    }
    return(myfontname);
}

static char *SubsChooseVec[] = {
    "",
    "Yes (Normal subscription; shows new notices)",
    "No (no subscription)",
    "Ask subscription (shows new notices optionally)",
    "ShowAll subscription (shows all notices)",
    "Print subscription (prints new notices)",
    0,
};

static char *Ssubsvec[] = {
    "",
    "Yes", 
    "No",
    "Ask",
    "ShowAll",
    "Print",
    0,
};

static char *BabySubsVec[] = {
    "",
    "Yes",
    "No",
    "Other",
    0,
};

int amsutil__ChooseNewStatus(struct classheader *c, char *nickname, int GivenDefault, boolean ShowAllChoices)
{
    int ans, defaultans;
    char QBuf[200];
    boolean IsExpert = amsutil_GetOptBit(EXP_SUBSEXPERT);

    sprintf(QBuf, "Subscribe to %s?", nickname);
    switch(GivenDefault) {
	case AMS_ALWAYSSUBSCRIBED:
	    defaultans = 1;
	    break;
	case AMS_UNSUBSCRIBED:
	    defaultans = 2;
	    break;
	case AMS_ASKSUBSCRIBED:
	    defaultans = 3;
	    break;
	case AMS_SHOWALLSUBSCRIBED:
	    defaultans = 4;
	    break;
	case AMS_PRINTSUBSCRIBED:
	    defaultans = 5;
	    break;
	default:
	    defaultans = 0;
	    break;
    }
    if (IsExpert) {
	Ssubsvec[0] = QBuf;
	ans = ams_ChooseFromList(ams_GetAMS(), Ssubsvec, defaultans);
    } else {
	BabySubsVec[0] = QBuf;
	if (ShowAllChoices) {
	    ans = 3;
	} else {
	    ans = ams_ChooseFromList(ams_GetAMS(), BabySubsVec, (defaultans > 2) ? 3 : defaultans);
	}
	if (ans > 2) {
	    SubsChooseVec[0] = QBuf;
	    ans = ams_ChooseFromList(ams_GetAMS(), SubsChooseVec, defaultans);
	}
    }
    switch (ans) {
	case 1:
	    return AMS_ALWAYSSUBSCRIBED;
	case 2:
	    return AMS_UNSUBSCRIBED;
	case 3:
	    return AMS_ASKSUBSCRIBED;
	case 4:
	    return AMS_SHOWALLSUBSCRIBED;
	case 5:
	    return AMS_PRINTSUBSCRIBED;
	default:
	    return AMS_ALWAYSSUBSCRIBED;
    }
}

