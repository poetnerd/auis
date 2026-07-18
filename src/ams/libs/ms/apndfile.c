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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/ams/libs/ms/RCS/apndfile.c,v 2.14 1992/12/15 21:17:22 rr2b R6tape $";
#endif

#include <andrewos.h>
#include <ms.h>

MS_AppendFileToFolder(FileName, FolderName)
char           *FileName, *FolderName;          /* BOTH IN */
{
    debug(1, ("MS_AppendFileToFolder %s %s\n", FileName, FolderName));
    return (AppendFileToFolder(FileName, FolderName, TRUE));
}

AppendFileToFolder(FileName, FolderName, DoDelete)
char           *FileName, *FolderName;
int             DoDelete;
{
    int             errsave = 0;
    struct MS_Directory *Dir = NULL;

    CloseDirsThatNeedIt();
    if (ReadOrFindMSDir(FolderName, &Dir, MD_APPEND)) {
        errsave = mserrcode;
        if(Dir) CloseMSDir(Dir, MD_APPEND);
        return (errsave);
    }
    errsave = AppendFileToMSDir(FileName, Dir, DoDelete);
    mserrcode = CloseMSDir(Dir, MD_APPEND);
    return (errsave ? errsave : mserrcode);
}


AppendFileToMSDir(FileName, Dir, DoDelete)
char           *FileName;
struct MS_Directory *Dir;
int             DoDelete;
{
    return (AppendFileToMSDirInternal(FileName, Dir, DoDelete, FALSE));
}

AppendFileToMSDirPreservingFileName(FileName, Dir, DoDelete)
char           *FileName;
struct MS_Directory *Dir;
int             DoDelete;
{
    return (AppendFileToMSDirInternal(FileName, Dir, DoDelete, TRUE));
}

AppendFileToMSDirInternal(FileName, Dir, DoDelete, TreatAsAlien)
char           *FileName;
struct MS_Directory *Dir;
int             DoDelete;
int             TreatAsAlien;
{
    struct MS_Message *Msg;
    struct MS_CaptionTemplate CapTemplate;
    int             saveerr;
    char            NewFileName[1 + MAXPATHLEN];

    Msg = (struct MS_Message *) malloc(sizeof(struct MS_Message));
    if (Msg == NULL) {
        AMS_RETURN_ERRCODE(ENOMEM, EIN_MALLOC, EVIA_APPENDMESSAGETOMSDIR);
    }
    bzero(Msg, sizeof(struct MS_Message));
    Msg->OpenFD = -1;
    bzero(&CapTemplate, sizeof(struct MS_CaptionTemplate));
    CapTemplate.basictype = BASICTEMPLATE_NORMAL;
    CapTemplate.datetype = DATETYPE_FROMHEADER;
    if (ReadRawFile(FileName, Msg, DoDelete)
        || ParseMessageFromRawBody(Msg)
        || CheckAuthUid(Msg)
        || BuildDateField(Msg, DATETYPE_FROMHEADER)
        || BuildReplyField(Msg)
        || BuildAttributesField(Msg)
        || InventID(Msg)
        || BuildCaption(Msg, &CapTemplate, TRUE)) {
        saveerr = mserrcode;
        FreeMessage(Msg, TRUE);
        return (saveerr);
    }
    if (TreatAsAlien) {
        char           *s;

        s = strrchr(FileName, '/');
        if (s)
            *s++ = '\0';
        if (!s || strcmp(Dir->UNIXDir, FileName)
            || (strlen(s) >= AMS_IDSIZE)) {
            FreeMessage(Msg, TRUE);
            AMS_RETURN_ERRCODE(EINVAL, EIN_PARAMCHECK, EVIA_APPENDMESSAGETOMSDIR);
        }
        strcpy(AMS_ID(Msg->Snapshot), s);
        DoDelete = FALSE;              /* Cannot allow it! */
    }
    if (IsMessageAlreadyThere(Msg, Dir)) {
        if (DoDelete)
            unlink(FileName);
        return (0);
    }
    if (!TreatAsAlien) {
        sprintf(NewFileName, "%s/+%s", Dir->UNIXDir, AMS_ID(Msg->Snapshot));
        if (WritePureFile(Msg, NewFileName, FALSE, 0664)) {
            saveerr = mserrcode;
            FreeMessage(Msg, TRUE);
            return (saveerr);
        }
    }
    if (AppendMessageToMSDir(Msg, Dir)) {
        FreeMessage(Msg, TRUE);
        unlink(NewFileName);           /* The old copy of the file is still in
                                        * place */
        return (mserrcode);
    }
    FreeMessage(Msg, TRUE);
    if (DoDelete)
        unlink(FileName);              /* Errors here are funny; better an
                                        * orphan file than a bogus error
                                        * message, though */
    return (0);
}

/*
    MS_AppendFileToFolderWithId -- added for imapsync (ams/msclients/imapsync).

    Identical to MS_AppendFileToFolder/AppendFileToFolder/AppendFileToMSDir/
    AppendFileToMSDirInternal above, except the caller supplies the
    message's AMS id and AMS_DATE instead of letting InventID()/
    BuildDateField() invent them.  This lets a sync agent give a message a
    deterministic identity (e.g. derived from an IMAP UIDVALIDITY+UID pair)
    so that re-running the sync is idempotent.  Id must be an AMS_IDSIZE-1
    (18) character string; Date64 must be an AMS_DATESIZE-1 (6) character
    base-64 date string (see convlongto64() in overhead/mail/lib/genid.c).

    Everything else -- parsing, caption building, chain hashing, index
    update, the strictly-increasing-AMS_DATE-per-folder enforcement in
    AppendMessageToMSDir() -- is exactly the existing store code; this
    routine does not duplicate or bypass any of it.
*/

MS_AppendFileToFolderWithId(FileName, FolderName, Id, Date64)
char           *FileName, *FolderName, *Id, *Date64;    /* ALL IN */
{
    debug(1, ("MS_AppendFileToFolderWithId %s %s %s %s\n", FileName, FolderName, Id, Date64));
    return (AppendFileToFolderWithId(FileName, FolderName, TRUE, Id, Date64));
}

AppendFileToFolderWithId(FileName, FolderName, DoDelete, Id, Date64)
char           *FileName, *FolderName, *Id, *Date64;
int             DoDelete;
{
    int             errsave = 0;
    struct MS_Directory *Dir = NULL;

    CloseDirsThatNeedIt();
    if (ReadOrFindMSDir(FolderName, &Dir, MD_APPEND)) {
        errsave = mserrcode;
        if(Dir) CloseMSDir(Dir, MD_APPEND);
        return (errsave);
    }
    errsave = AppendFileToMSDirWithId(FileName, Dir, DoDelete, Id, Date64);
    mserrcode = CloseMSDir(Dir, MD_APPEND);
    return (errsave ? errsave : mserrcode);
}

AppendFileToMSDirWithId(FileName, Dir, DoDelete, Id, Date64)
char           *FileName;
struct MS_Directory *Dir;
int             DoDelete;
char           *Id, *Date64;
{
    struct MS_Message *Msg;
    struct MS_CaptionTemplate CapTemplate;
    int             saveerr;
    char            NewFileName[1 + MAXPATHLEN];

    Msg = (struct MS_Message *) malloc(sizeof(struct MS_Message));
    if (Msg == NULL) {
        AMS_RETURN_ERRCODE(ENOMEM, EIN_MALLOC, EVIA_APPENDMESSAGETOMSDIR);
    }
    bzero(Msg, sizeof(struct MS_Message));
    Msg->OpenFD = -1;
    bzero(&CapTemplate, sizeof(struct MS_CaptionTemplate));
    CapTemplate.basictype = BASICTEMPLATE_NORMAL;
    CapTemplate.datetype = DATETYPE_FROMHEADER;
    if (ReadRawFile(FileName, Msg, DoDelete)
        || ParseMessageFromRawBody(Msg)
        || CheckAuthUid(Msg)
        || BuildDateField(Msg, DATETYPE_FROMHEADER)
        || BuildReplyField(Msg)
        || BuildAttributesField(Msg)
        || InventID(Msg)
        || BuildCaption(Msg, &CapTemplate, TRUE)) {
        saveerr = mserrcode;
        FreeMessage(Msg, TRUE);
        return (saveerr);
    }
    /* Override the invented id/date with the caller's -- this is the
       one behavioral difference from AppendFileToMSDirInternal. Both
       fields are fixed-width within the snapshot buffer; NUL-terminate
       explicitly rather than relying on residue from InventID/
       BuildDateField's own writes. */
    strncpy(AMS_ID(Msg->Snapshot), Id, AMS_IDSIZE - 1);
    AMS_ID(Msg->Snapshot)[AMS_IDSIZE - 1] = '\0';
    strncpy(AMS_DATE(Msg->Snapshot), Date64, AMS_DATESIZE - 1);
    AMS_DATE(Msg->Snapshot)[AMS_DATESIZE - 1] = '\0';

    if (IsMessageAlreadyThere(Msg, Dir)) {
        if (DoDelete)
            unlink(FileName);
        return (0);
    }
    sprintf(NewFileName, "%s/+%s", Dir->UNIXDir, AMS_ID(Msg->Snapshot));
    if (WritePureFile(Msg, NewFileName, FALSE, 0664)) {
        saveerr = mserrcode;
        FreeMessage(Msg, TRUE);
        return (saveerr);
    }
    if (AppendMessageToMSDir(Msg, Dir)) {
        FreeMessage(Msg, TRUE);
        unlink(NewFileName);           /* The old copy of the file is still in
                                        * place */
        return (mserrcode);
    }
    FreeMessage(Msg, TRUE);
    if (DoDelete)
        unlink(FileName);              /* Errors here are funny; better an
                                        * orphan file than a bogus error
                                        * message, though */
    return (0);
}
