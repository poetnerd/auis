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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/ams/libs/ms/RCS/uattrs.c,v 2.8 1992/12/15 21:21:37 rr2b R6tape $";
#endif


 

#include <ms.h>
#include <string.h>
extern int CacheDirectoryForClosing();
extern int CloseMSDir();
extern int DestructivelyWriteDirectoryHead();
extern int GetSnapshotByNumber();
extern int ReadOrFindMSDir();
extern int RewriteSnapshotInDirectory();

extern char *permanentmalloc();
static char *UnusedAttrName = UNUSEDATTRNAME;

int MS_GetDirAttributes(char *Dirname, int *AttrCt, char *Attrs, int SepChar, int ShowEmpty)
{
    struct MS_Directory *Dir;
    int i;
    char SepString[2];

    if (SepChar == 0) AMS_RETURN_ERRCODE(EINVAL, EIN_PARAMCHECK, EVIA_GETDIRATTRS);
    if (ReadOrFindMSDir(Dirname, &Dir, MD_READ) != 0) {
	return(mserrcode);
    }
    *AttrCt = Dir->AttrCount;
    Attrs[0] = '\0';
    SepString[0] = (char) SepChar;
    SepString[1] = '\0';
    for(i=0; i < Dir->AttrCount; ++i) {
	if (ShowEmpty || strcmp(Dir->AttrNames[i], UnusedAttrName)) {
	    strcat(Attrs, Dir->AttrNames[i]);
	    strcat(Attrs, SepString);
	} else {
	    --*AttrCt;
	}
    }
    CacheDirectoryForClosing(Dir, MD_READ);
    return(0);
}

int MS_AddAttribute(char *Dirname, char *Newname, int *AttNum)
{
    int i, errsave;
    Boolean Reusing = FALSE;
    char *s;
    struct MS_Directory *Dir;

    if (ReadOrFindMSDir(Dirname, &Dir, MD_WRITE) != 0) {
	return(mserrcode);
    }
    for (i=0; i<Dir->AttrCount; ++i) {
	if (!strcmp(Dir->AttrNames[i], Newname)) {
	    CloseMSDir(Dir, MD_WRITE);
	    AMS_RETURN_ERRCODE(EMSATTREXISTS, EIN_PARAMCHECK, EVIA_ADDATTR);
	}
    }
    for (i=0; i<Dir->AttrCount; ++i) {
	if (!strcmp(Dir->AttrNames[i], UnusedAttrName)) {
	    Reusing = TRUE;
	    break;
	}
    }
    if (!Reusing) {
	if (i >= AMS_NUM_UATTRS) {
	    CloseMSDir(Dir, MD_WRITE);
	    AMS_RETURN_ERRCODE(ERANGE, EIN_PARAMCHECK, EVIA_ADDATTR);
	}
	++Dir->AttrCount;
    }
    s = permanentmalloc(1+strlen(Newname));
    if (!s) {
	CloseMSDir(Dir, MD_WRITE);
	AMS_RETURN_ERRCODE(ENOMEM, EIN_MALLOC, EVIA_ADDATTR);
    }
    strcpy(s, Newname);
    if (!Dir->AttrNames) {
	Dir->AttrNames = (char **) permanentmalloc(AMS_NUM_UATTRS * sizeof(char *));
	if (!Dir->AttrNames) {
	    CloseMSDir(Dir, MD_WRITE);
	    AMS_RETURN_ERRCODE(ENOMEM, EIN_MALLOC, EVIA_ADDATTR);
	}
    }
    Dir->AttrNames[i] = s;
    if (Dir->DBMajorVersion < 4) {
	Dir->DBMajorVersion = 4;
    }
    if (DestructivelyWriteDirectoryHead(Dir)) {
	errsave = mserrcode;
	CloseMSDir(Dir, MD_WRITE);
	return(errsave);
    }
    if (CacheDirectoryForClosing(Dir, MD_WRITE)) {
	return(mserrcode);
    }
    *AttNum = i;
    return(0);
}

int MS_DeleteAttr(char *DirName, char *AttrName)
{
    int i, errsave;
    char snapshot[AMS_SNAPSHOTSIZE];
    struct MS_Directory *Dir;

    if (ReadOrFindMSDir(DirName, &Dir, MD_WRITE) != 0) {
	return(mserrcode);
    }
    for (i=0; i<Dir->AttrCount; ++i) {
	if (!strcmp(Dir->AttrNames[i], AttrName)) {
	    break;
	}
    }
    if (i>=Dir->AttrCount) {
	CloseMSDir(Dir, MD_WRITE);
	AMS_RETURN_ERRCODE(EINVAL, EIN_PARAMCHECK, EVIA_DELATTR);
    }
    if (strlen(AttrName) < strlen(UnusedAttrName)) {
	Dir->AttrNames[i] = permanentmalloc(1+strlen(UnusedAttrName));
	if (!Dir->AttrNames[i]) {
	    CloseMSDir(Dir, MD_WRITE);
	    AMS_RETURN_ERRCODE(ENOMEM, EIN_MALLOC, EVIA_DELATTR);
	}
    }
    strcpy(Dir->AttrNames[i], UnusedAttrName);
    for (i=0; i<Dir->MessageCount; ++i) {
	if (GetSnapshotByNumber(Dir, i, snapshot)) {
	    CloseMSDir(Dir, MD_WRITE);
	    return(mserrcode);
	}
	AMS_UNSET_ATTRIBUTE(snapshot, AMS_ATT_UATTR(i));
	if (RewriteSnapshotInDirectory(Dir, i, snapshot)) {
	    CloseMSDir(Dir, MD_WRITE);
	    return(mserrcode);
	}
    }
    if (DestructivelyWriteDirectoryHead(Dir)) {
	errsave = mserrcode;
	CloseMSDir(Dir, MD_WRITE);
	return(errsave);
    }
    return(CacheDirectoryForClosing(Dir, MD_WRITE));
}
