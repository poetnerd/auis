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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/ams/libs/ms/RCS/altsnap.c,v 2.7 1992/12/15 21:17:22 rr2b R6tape $";
#endif

#include <stdio.h>
#include <andrewos.h>
#include <ms.h>

/* msjournal.c, this directory -- writeback capture (a no-op unless
   dirname is a mirrored folder; see the grammar note there).
   MSJournal_Record is genuinely variadic (va_start/va_arg in its own
   ANSI definition), so -- unlike every implicit-int K&R call in this
   file -- it needs a real prototype with "..." at every call site: on
   Apple's arm64 ABI a variadic callee expects its variable arguments
   on the stack, while a caller with no prototype in scope (or a fake
   K&R empty-parens one) passes them the normal-call way, in registers.
   That caller/callee mismatch silently hands the callee garbage --
   the same ABI hazard already documented for dbg_open() in
   overhead/util/hdrs/fdplumb.h, just triggered from the caller's side
   instead of the callee's. */
extern void MSJournal_Record(const char *dir, const char *fmt, ...);

MS_AlterSnapshot(dirname, id, NewSnapshot, Code)
char *dirname, *id, *NewSnapshot;
int Code;
{
    struct MS_Directory *Dir;
    int msgnum, errsave, i;
    char SnapshotDum[AMS_SNAPSHOTSIZE], *s, *t, DateBuf[AMS_DATESIZE];
    char OrMask[AMS_ATTRIBUTESIZE], AndMask[AMS_ATTRIBUTESIZE];
    char OrHex[1 + 2 * AMS_ATTRIBUTESIZE], AndHex[1 + 2 * AMS_ATTRIBUTESIZE];

    if (MSDebugging & 1) { /* Debugging SHOULD go to stdout -- nsb 5/16/86 */
	printf("MS_AlterSnapshot %s %s code %d\n", dirname, id, Code);
	fputs("Snapshot: 0x", stdout);
	for (i=0; i<AMS_SNAPSHOTSIZE; i++) fprintf(stdout, "%02x", (unsigned char) NewSnapshot[i]);
	fputs(")\n", stdout);
    }

    if (ReadOrFindMSDir(dirname, &Dir, MD_WRITE) != 0) {
	return(mserrcode);
    }
    if (GetSnapshotByID(Dir, id, &msgnum, SnapshotDum)) {
	errsave = mserrcode; 
	CloseMSDir(Dir, MD_WRITE);
	return(errsave);
    }
    switch(Code) {
	case ASS_REPLACE_ALL:
	    strcpy(DateBuf, AMS_DATE(SnapshotDum));
	    bcopy(NewSnapshot, SnapshotDum, AMS_SNAPSHOTSIZE);
	    strcpy(AMS_DATE(SnapshotDum), DateBuf);
	    break;
	case ASS_REPLACE_ATT_CAPT:
	    bcopy(AMS_CAPTION(NewSnapshot), AMS_CAPTION(SnapshotDum), AMS_CAPTIONSIZE);
	    /* DROP THROUGH TO NEXT CLAUSE */
	case ASS_REPLACE_ATTRIBUTES:
	    bcopy(AMS_ATTRIBUTES(NewSnapshot), AMS_ATTRIBUTES(SnapshotDum), AMS_ATTRIBUTESIZE);
	    break;
	case ASS_OR_ATTRIBUTES:
	    for (i=0, s=AMS_ATTRIBUTES(NewSnapshot), t = AMS_ATTRIBUTES(SnapshotDum); i<AMS_ATTRIBUTESIZE; ++i, ++s, ++t) {
		*t |= *s;
	    }
	    break;
	case ASS_AND_ATTRIBUTES:
	    for (i=0, s=AMS_ATTRIBUTES(NewSnapshot), t = AMS_ATTRIBUTES(SnapshotDum); i<AMS_ATTRIBUTESIZE; ++i, ++s, ++t) {
		*t &= *s;
	    }
	    break;
    }
    /* Writeback capture: express whatever this Code did to the
       attribute bytes as a single (or-mask, and-mask) pair --
       new = (old | or-mask) & and-mask -- so replay can recompute the
       server-flag delta without needing to know which of the five
       Codes produced it. The three REPLACE* variants force the result
       to equal NewSnapshot's attribute bytes regardless of old_attrs,
       which this formula gets right by setting or-mask = and-mask =
       those bytes (old|new, then &new, leaves exactly new). */
    switch(Code) {
	case ASS_OR_ATTRIBUTES:
	    bcopy(AMS_ATTRIBUTES(NewSnapshot), OrMask, AMS_ATTRIBUTESIZE);
	    for (i=0; i<AMS_ATTRIBUTESIZE; ++i) AndMask[i] = (char) 0xff;
	    break;
	case ASS_AND_ATTRIBUTES:
	    for (i=0; i<AMS_ATTRIBUTESIZE; ++i) OrMask[i] = 0;
	    bcopy(AMS_ATTRIBUTES(NewSnapshot), AndMask, AMS_ATTRIBUTESIZE);
	    break;
	default: /* ASS_REPLACE_ALL, ASS_REPLACE_ATTRIBUTES, ASS_REPLACE_ATT_CAPT */
	    bcopy(AMS_ATTRIBUTES(NewSnapshot), OrMask, AMS_ATTRIBUTESIZE);
	    bcopy(AMS_ATTRIBUTES(NewSnapshot), AndMask, AMS_ATTRIBUTESIZE);
	    break;
    }
    debug(4, ("Altering snapshot of  message %d %s\n", msgnum, AMS_CAPTION(SnapshotDum)));
    if (RewriteSnapshotInDirectory(Dir, msgnum, SnapshotDum)) {
	errsave = mserrcode;
	CloseMSDir(Dir, MD_WRITE);
	return(errsave);
    }
    if (CacheDirectoryForClosing(Dir, MD_WRITE)) {
	return(mserrcode);
    }
    for (i=0; i<AMS_ATTRIBUTESIZE; ++i) {
	sprintf(OrHex+2*i, "%02x", (unsigned char) OrMask[i]);
	sprintf(AndHex+2*i, "%02x", (unsigned char) AndMask[i]);
    }
    MSJournal_Record(Dir->UNIXDir, "J1 flags %s %s %s", id, OrHex, AndHex);
    return(0);
}
