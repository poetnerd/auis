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
static char rcsid[]="$Header: /afs/cs.cmu.edu/project/atk-dist/auis-6.3/atkams/messages/lib/RCS/amsn.c,v 1.14 1994/03/31 16:05:30 rr2b Exp $";
#endif

/* Until I come up with a better scheme, new functions here have to be added to SIX files -- ams.ch, amss.ch, amsn.ch (all identical specs) and the corresponding c files */ 

#include <class.h>
#include <ams.ih>
#include <im.ih>
#include <message.ih>
#include <amsn.eh>
#include <mailconf.h>
#include <cui.h>
#include <util.h>
#include <mail.h>
#include <unscribe.h>

extern char ProgramName[];	/* Icky-poo! */

/* same-.do (stubs.o) cross-file references -- no header, defined in stubs.c */
extern int ChooseFromList(char **QVec, int def), GenericCompoundAction(struct view *v, char *prefix, char *orgcmds), GetBooleanFromUser(char *prompt, int defaultans),
	GetStringFromUser(char *prompt, char *buf, int len, int IsPassword), ReportError(char *text, int level, int Decode), ReportSuccess(char *text), SubtleDialogs(int Really),
	TildeResolve(char *old, char *new), WriteOutUserEnvironment(FILE *fp, Boolean IsAboutMessages), SetProgramVersion();

/* overhead/mail/lib -- no header anywhere */
extern int CheckAMSConfiguration();

/* ams/libs/cui -- no header anywhere declares the CUI_* function family
   (only the CUI_* global variables above are declared locally elsewhere) */
extern int CUI_BuildNickName(char *FullName, char *NickName), CUI_CheckMailboxes(char *ForWhat), CUI_CloneMessage(int cuid, char *OrigDirName, int Code),
	CUI_CreateNewMessageDirectory(), CUI_DeleteMessage(int cuid),
	CUI_DirectoriesToPurge(), CUI_DoesDirNeedPurging(char *Dname),
	CUI_EndConversation(), CUI_GenLocalTmpFileName(char *nmbuf), CUI_GenTmpFileName(char *nmbuf),
	CUI_GetAMSID(int cuid, char **id, char **dir), CUI_GetCuid(char *amsid, char *dirname, int *IsDup), CUI_GetFileFromVice(char *LocalFile, char *ViceFile),
	CUI_GetHeaderContents(int cuid, char *HeaderName, int HeaderTypeNumber, char *HeaderBuf, int lim), CUI_GetSnapshotFromCUID(int cuid, char *SnapshotBuf),
	CUI_HandleMissingFolder(char *OldName), CUI_MarkAsRead(int cuid), CUI_MarkAsUnseen(int cuid),
	CUI_NameReplyFile(int cuid, int code, char *FileName), CUI_PrefetchMessage(int cuid, int ReallyNext),
	CUI_PrintBodyFromCUIDWithFlags(int cuid, int flags, char *printer), CUI_PrintUpdates(char *dname, char *nickname),
	CUI_ProcessMessageAttributes(int cuid, char *Snapshot), CUI_PurgeDeletions(char *arg),
	CUI_PurgeMarkedDirectories(Boolean Ask, Boolean OfferQuit), CUI_ReallyGetBodyToLocalFile(int cuid, char *FileName, int *ShouldDelete, int MayFudge),
	CUI_RemoveDirectory(char *DirName), CUI_RenameDir(char *old, char *new), CUI_ReportAmbig(char *name, char *atype),
	CUI_ResendMessage(int cuid, char *Tolist), CUI_RewriteHeaderLine(char *text, char **newtext),
	CUI_RewriteHeaderLineInternal(char *text, char **newtext, int maxdealiases, int *numfound, int *externalct, int *formatct, int *stripct, int *trustct), CUI_SetClientVersion(char *Vers),
	CUI_SetPrinter(char *printername), CUI_StoreFileToVice(char *LocalFile, char *ViceFile), CUI_SubmitMessage(char *InFile, int DeliveryOpts),
	CUI_UndeleteMessage(int cuid);
/* these three are long-returning at their real definitions -- sourced from
   ams/libs/cui/cuilib.c, not guessed (this class's own .ch methods already
   declare their own amsn__CUI_* wrappers `long`, matching) */
extern long CUI_DisambiguateDir(char *shortname, char **longname), CUI_GetHeaders(), CUI_Initialize(int (*TimerFunction)(), char *rock);

/* ams/libs/ms -- no header anywhere declares the MS_* function family
   (ams/libs/hdrs/ms.h mentions FreeMessage only in a comment) */
extern int MS_AppendFileToFolder(char *FileName, char *FolderName), MS_CheckAuthentication(int *Authenticated), MS_DebugMode(int level, int snap, int malloc),
	MS_DisambiguateFile(char *source, char *target, short AccessCode), MS_DomainHandlesFormatting(char *domname, int *codeP),
	MS_FastUpdateState(), MS_GetNewMessageCount(char *FullDirName, int *numnew, int *numtotal, char *LastOldDate, int InsistOnFetch), MS_GetNthSnapshot(char *DirName, int n, char *SnapshotBuf),
	MS_GetSearchPathEntry(int which, char *buf, int lim), MS_GetSubscriptionEntry(char *FullName, char *NickName, int *status),
	MS_NameChangedMapFile(char *MapFile, int MailOnly, int ListAll, int *NumChanged, int *NumUnavailable, int *NumMissingFolders, int *NumSlowpokes, int *NumFastFellas), MS_NameSubscriptionMapFile(char *Root, char *MapFile), MS_ParseDate(char *indate, int *year, int *month, int *day, int *hour, int *min, int *sec, int *wday, long *gtm),
	MS_PrefetchMessage(char *DirName, char *id, int GetNext), MS_SetAssociatedTime(char *FullName, char *newvalue), MS_SetCleanupZombies(),
	MS_SetSubscriptionEntry(char *FullName, char *NickName, int status), MS_UpdateState();
/* these three are long-returning at their real definitions */
extern long MS_GetDirInfo(char *DirName, int *ProtCode, int *MsgCount), MS_MatchFolderName(char *pat, char *filename), MS_UnlinkFile(char *FileName);

extern char *CUI_MachineName, CUI_MailDomain[], *CUI_Rock, CUI_VersionString[], *CUI_WhoIAm, *ap_Shorten(char *pathname), *DescribeProt(int ProtCode), *ams_genid(int IsFileName);

extern long CUI_DeliveryType, CUI_LastCallFinished, CUI_OnSameHost, CUI_SnapIsRunning, CUI_UseAmsDelivery, CUI_UseNameSep, mserrcode;

void amsn__RemoveErrorDialogWindow(struct amsn *self)
{
    extern void ams_RemoveErrorWindow();
    ams_RemoveErrorWindow();
}

boolean amsn__InitializeClass(struct classheader *c)
{
    CheckAMSConfiguration();
    SetProgramVersion();
    MS_SetCleanupZombies(0);
    return(TRUE);
}

void amsn__CUI_BuildNickName(struct amsn *self, char *shortname, char *longname)
{
    CUI_BuildNickName(shortname, longname);
}

int amsn__CUI_CheckMailboxes(struct amsn *self, char *forwhat)
{
    return(CUI_CheckMailboxes(forwhat));
}

long amsn__CUI_CloneMessage(struct amsn *self, int cuid, char *DirName, int code)
{
    return(CUI_CloneMessage(cuid, DirName, code));
}

long amsn__CUI_CreateNewMessageDirectory(struct amsn *self, char *dir, char *bodydir)
{
    return(CUI_CreateNewMessageDirectory(dir, bodydir));
}

long amsn__CUI_DeleteMessage(struct amsn *self, int cuid)
{
    return(CUI_DeleteMessage(cuid));
}

long amsn__CUI_DeliveryType(struct amsn *self)
{
    return(CUI_DeliveryType);
}

long amsn__CUI_DirectoriesToPurge(struct amsn *self)
{
    return(CUI_DirectoriesToPurge());
}

long amsn__CUI_DisambiguateDir(struct amsn *self, char *shortname, char **longname)
{
    return(CUI_DisambiguateDir(shortname, longname));
}

long amsn__CUI_DoesDirNeedPurging(struct amsn *self, char *name)
{
    return(CUI_DoesDirNeedPurging(name));
}

void amsn__CUI_EndConversation(struct amsn *self)
{
    CUI_EndConversation();
}

long amsn__CUI_GenLocalTmpFileName(struct amsn *self, char *name)
{
    return(CUI_GenLocalTmpFileName(name));
}

long amsn__CUI_GenTmpFileName(struct amsn *self, char *name)
{
    return(CUI_GenTmpFileName(name));
}

long amsn__CUI_GetFileFromVice(struct amsn *self, char *tmp_file, char *vfile)
{
    return(CUI_GetFileFromVice(tmp_file, vfile));
}

long amsn__CUI_GetHeaderContents(struct amsn *self, int cuid, char *hdrname, int hdrnum, char *hdrbuf, int lim)
{
    return(CUI_GetHeaderContents(cuid, hdrname, hdrnum, hdrbuf, lim));
}

long amsn__CUI_GetHeaders(struct amsn *self, char *dirname, char *date64, char *headbuf, int lim, long startbyte, long *nbytes, long *status, int RegisterCuids)
{
    return(CUI_GetHeaders(dirname, date64, headbuf, lim, startbyte, nbytes, status, RegisterCuids));
}

long amsn__CUI_GetSnapshotFromCUID(struct amsn *self, int cuid, char *Sbuf)
{
    return(CUI_GetSnapshotFromCUID(cuid, Sbuf));
}

long amsn__CUI_HandleMissingFolder(struct amsn *self, char *dname)
{
    return(CUI_HandleMissingFolder(dname));
}

static int TimerInit();
long amsn__CUI_Initialize(struct amsn *self, procedure TimerFunction, char *rock)
{
    message_DisplayString(NULL, 10, "Initializing Internal Message Server...");
    im_ForceUpdate();
    if (!TimerFunction) TimerFunction = TimerInit;
    strcpy(ProgramName, im_GetProgramName());
    return(CUI_Initialize(TimerFunction, rock));
}

long amsn__CUI_LastCallFinished(struct amsn *self)
{
    return(CUI_LastCallFinished);
}

char * amsn__CUI_MachineName(struct amsn *self)
{
    return(CUI_MachineName);
}

char * amsn__CUI_MailDomain(struct amsn *self)
{
    return(CUI_MailDomain);
}

long amsn__CUI_MarkAsRead(struct amsn *self, int cuid)
{
    return(CUI_MarkAsRead(cuid));
}

long amsn__CUI_MarkAsUnseen(struct amsn *self, int cuid)
{
    return(CUI_MarkAsUnseen(cuid));
}

long amsn__CUI_NameReplyFile(struct amsn *self, int cuid, int code, char *fname)
{
    return(CUI_NameReplyFile(cuid, code, fname));
}

long amsn__CUI_OnSameHost(struct amsn *self)
{
    return(CUI_OnSameHost);
}

long amsn__CUI_PrefetchMessage(struct amsn *self, int cuid, int ReallyNext)
{
    return(CUI_PrefetchMessage(cuid, ReallyNext));
}

long amsn__CUI_PrintBodyFromCUIDWithFlags(struct amsn *self, int cuid, int flags, char *printer)
{
    return(CUI_PrintBodyFromCUIDWithFlags(cuid, flags, printer));
}

void amsn__CUI_PrintUpdates(struct amsn *self, char *dname, char *nickname)
{
    CUI_PrintUpdates(dname, nickname);
}

long amsn__CUI_ProcessMessageAttributes(struct amsn *self, int cuid, char *snapshot)
{
    return(CUI_ProcessMessageAttributes(cuid, snapshot));
}

long amsn__CUI_PurgeDeletions(struct amsn *self, char *dirname)
{
    return(CUI_PurgeDeletions(dirname));
}

long amsn__CUI_PurgeMarkedDirectories(struct amsn *self, boolean ask, boolean OfferQuit)
{
    return(CUI_PurgeMarkedDirectories(ask, OfferQuit));
}

long amsn__CUI_ReallyGetBodyToLocalFile(struct amsn *self, int cuid, char *fname, int *ShouldDelete, int MayFudge)
{
    return(CUI_ReallyGetBodyToLocalFile(cuid, fname, ShouldDelete, MayFudge));
}

long amsn__CUI_RemoveDirectory(struct amsn *self, char *dirname)
{
    return(CUI_RemoveDirectory(dirname));
}
long amsn__CUI_RenameDir(struct amsn *self, char *oldname, char *newname)
{
    return(CUI_RenameDir(oldname, newname));
}

void amsn__CUI_ReportAmbig(struct amsn *self, char *name, char *atype)
{
    CUI_ReportAmbig(name, atype);
}

long amsn__CUI_ResendMessage(struct amsn *self, int cuid, char *tolist)
{
    return(CUI_ResendMessage(cuid, tolist));
}

long amsn__CUI_RewriteHeaderLine(struct amsn *self, char *addr, char **newaddr)
{
    return(CUI_RewriteHeaderLine(addr, newaddr));
}

long amsn__CUI_RewriteHeaderLineInternal(struct amsn *self, char *addr, char **newaddr, int maxdealiases, int *numfound, int *externalcount, int *formatct, int *stripct, int *trustct)
{
    return(CUI_RewriteHeaderLineInternal(addr, newaddr, maxdealiases, numfound, externalcount, formatct, stripct, trustct));
}

char * amsn__CUI_Rock(struct amsn *self)
{
    return(CUI_Rock);
}

void amsn__CUI_SetClientVersion(struct amsn *self, char *vers)
{
    CUI_SetClientVersion(vers);
}

long amsn__CUI_SetPrinter(struct amsn *self, char *printername)
{
    return(CUI_SetPrinter(printername));
}

long amsn__CUI_SnapIsRunning(struct amsn *self)
{
    return(CUI_SnapIsRunning);
}

long amsn__CUI_StoreFileToVice(struct amsn *self, char *localfile, char *vicefile)
{
    return(CUI_StoreFileToVice(localfile, vicefile));
}

long amsn__CUI_SubmitMessage(struct amsn *self, char *infile, long DeliveryOpts)
{
    return(CUI_SubmitMessage(infile, DeliveryOpts));
}

long amsn__CUI_UndeleteMessage(struct amsn *self, int cuid)
{
    return(CUI_UndeleteMessage(cuid));
}

long amsn__CUI_UseAmsDelivery(struct amsn *self)
{
    return(CUI_UseAmsDelivery);
}

long amsn__CUI_UseNameSep(struct amsn *self)
{
    return(CUI_UseNameSep);
}

char * amsn__CUI_VersionString(struct amsn *self)
{
    return(CUI_VersionString);
}

char* amsn__CUI_WhoIAm(struct amsn *self)
{
    return(CUI_WhoIAm);
}

int amsn__CUI_GetCuid(struct amsn *self, char *id, char *fullname, int *isdup)
{
    return(GetCuid(id, fullname, isdup));
}

long amsn__MS_AppendFileToFolder(struct amsn *self, char *filename, char *foldername)
{
    return(MS_AppendFileToFolder(filename, foldername));
}

long amsn__MS_CheckAuthentication(struct amsn *self, long *auth)
{
    return(MS_CheckAuthentication(auth));
}

long amsn__MS_DebugMode(struct amsn *self, int mslevel, int snaplevel, int malloclevel)
{
    return(MS_DebugMode(mslevel, snaplevel, malloclevel));
}

long amsn__MS_DisambiguateFile(struct amsn *self, char *source, char *target, long MustBeDir)
{
    return(MS_DisambiguateFile(source, target, MustBeDir));
}

int amsn__MS_FastUpdateState(struct amsn *self)
{
    return(MS_FastUpdateState());
}

long amsn__MS_GetDirInfo(struct amsn *self, char *dirname, int *protcode, int *msgcount)
{
    return(MS_GetDirInfo(dirname, protcode, msgcount));
}

long amsn__MS_GetNewMessageCount(struct amsn *self, char *dirname, int *numnew, int *numtotal, char *lastolddate, long InsistOnFetch)
{
    return(MS_GetNewMessageCount(dirname, numnew, numtotal, lastolddate, InsistOnFetch));
}

long amsn__MS_GetNthSnapshot(struct amsn *self, char *dirname, long which, char *snapshotbuf)
{
    return(MS_GetNthSnapshot(dirname, which, snapshotbuf));
}

long amsn__MS_GetSearchPathEntry(struct amsn *self, long which, char *buf, long buflim)
{
    return(MS_GetSearchPathEntry(which, buf, buflim));
}

long amsn__MS_GetSubscriptionEntry(struct amsn *self, char *fullname, char *nickname, int *status)
{
    return(MS_GetSubscriptionEntry(fullname, nickname, status));
}

long amsn__MS_NameChangedMapFile(struct amsn *self, char *mapfile, long mailonly, long listall, int *numchanged, int *numunavailable, int *nummissing, int *numslowpokes, int *numfastfellas)
{
    return(MS_NameChangedMapFile(mapfile, mailonly, listall, numchanged, numunavailable, nummissing, numslowpokes, numfastfellas));
}

long amsn__MS_NameSubscriptionMapFile(struct amsn *self, char *root, char *mapfile)
{
    return(MS_NameSubscriptionMapFile(root, mapfile));
}

long amsn__MS_MatchFolderName(struct amsn *self, char *pat, char *filename)
{
    return(MS_MatchFolderName(pat, filename));
}

long amsn__MS_ParseDate(struct amsn *self, char *indate, int *year, int *month, int *day, int *hour, int *min, int *sec, int *wday, long *gtm)
{
    return(MS_ParseDate(indate, year, month, day, hour, min, sec, wday, gtm));
}

long amsn__MS_PrefetchMessage(struct amsn *self, char *dirname, char *id, long getnext)
{
    return(MS_PrefetchMessage(dirname, id, getnext));
}

long amsn__MS_SetAssociatedTime(struct amsn *self, char *fullname, char *newvalue)
{
    return(MS_SetAssociatedTime(fullname, newvalue));
}

void amsn__MS_SetCleanupZombies(struct amsn *self, long doclean)
{
    MS_SetCleanupZombies(doclean);
}

long amsn__MS_SetSubscriptionEntry(struct amsn *self, char *fullname, char *nickname, long status)
{
    return(MS_SetSubscriptionEntry(fullname, nickname, status));
}

long amsn__MS_UnlinkFile(struct amsn *self, char *filename)
{
    return(MS_UnlinkFile(filename));
}

int amsn__MS_UpdateState(struct amsn *self)
{
    return(MS_UpdateState());
}

void amsn__ReportSuccess(struct amsn *self, char *s)
{
    ReportSuccess(s);
}


long amsn__MS_DomainHandlesFormatting(struct amsn *self, char *domname, long *retval)
{
    return(MS_DomainHandlesFormatting(domname, retval));
}

void amsn__ReportError(struct amsn *self, char *s, int level, int decode, long err)
{
    if (decode) mserrcode = err;
    ReportError(s, level, decode);
}

int amsn__GenericCompoundAction(struct amsn *self, struct view *v, char *prefix, char *cmds)
{
    return(GenericCompoundAction(v, prefix, cmds));
}

int amsn__GetBooleanFromUser(struct amsn *self, char *prompt, int defaultans)
{
    return(GetBooleanFromUser(prompt, defaultans));
}

int amsn__GetStringFromUser(struct amsn *self, char *prompt, char *buf, int len, int ispass)
{
    return(GetStringFromUser(prompt, buf, len, ispass));
}

int amsn__TildeResolve(struct amsn *self, char *in, char *out)
{
    return(TildeResolve(in, out));
}

int amsn__OnlyMail(struct amsn *self)
{
    return(AMS_OnlyMail);
}

char* amsn__ap_Shorten(struct amsn *self, char *fname)
{
    return(ap_Shorten(fname));
}

int amsn__fwriteallchars(struct amsn *self, char *s, int len, FILE *fp)
{
    return(fwriteallchars(s, len, fp));
}

long amsn__mserrcode(struct amsn *self)
{
    return(mserrcode);
}

int amsn__vdown(struct amsn *self, int errnum)
{
    return(vdown(errnum));
}

int amsn__AMS_ERRNO(struct amsn *self)
{
    return(AMS_ERRNO);
}

void amsn__SubtleDialogs(struct amsn *self, boolean besubtle)
{
    SubtleDialogs(besubtle);
}

char * amsn__DescribeProt(struct amsn *self, int code)
{
    return(DescribeProt(code));
}

int amsn__ChooseFromList(struct amsn *self, char **QVec, int defans)
{
    return(ChooseFromList(QVec, defans));
}
int amsn__CUI_GetAMSID(struct amsn *self, int cuid, char **id, char **dir)
{
    return(CUI_GetAMSID(cuid, id, dir));
}

char * amsn__MessagesAutoBugAddress(struct amsn *self)
{
    return(MessagesAutoBugAddress);
}

int amsn__UnScribe(struct amsn *self, int ucode, struct ScribeState *ss, char *LineBuf, int ct, FILE *fout)
{
    return(UnScribe(ucode, ss, LineBuf, ct, fout));
}

int amsn__UnScribeFlush(struct amsn *self, int ucode, struct ScribeState *ss, FILE *fout)
{
    return(UnScribeFlush(ucode, ss, fout));
}

int amsn__UnScribeInit(struct amsn *self, char *vers, struct ScribeState *ss)
{
    return(UnScribeInit(vers, ss));
}

void amsn__WriteOutUserEnvironment(struct amsn *self, FILE *fp, boolean IsAboutMessages)
{
    WriteOutUserEnvironment(fp, IsAboutMessages);
}

char * amsn__ams_genid(struct amsn *self, boolean isfilename)
{
    return(ams_genid(isfilename));
}

int amsn__CheckAMSUseridPlusWorks(struct amsn *self, char *dom)
{
    return(CheckAMSUseridPlusWorks(dom));
}

static int TimerInit() {
    ams_TimerInit();
}
