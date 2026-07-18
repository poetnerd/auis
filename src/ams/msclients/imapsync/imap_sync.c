/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	imap_sync.c -- imapsync, a one-way IMAP-to-local-store mirror.

	Mirrors the folders named by -folders (default: the "imapsyncfolders"
	preference, or "INBOX" if that is unset) from a single IMAP account
	into a local mspath-shaped root (-root, default ~/.IMAP/fastmail),
	one UNIX directory per IMAP folder, using the existing AMS message
	store code (MS_CreateNewMessageDirectory, MS_AppendFileToFolderWithId)
	to do all the actual directory/snapshot/caption/chain maintenance --
	this program never hand-writes .MS_MsgDir or snapshot bytes.

	Message identity: the AMS id for a synced message is synthesized
	deterministically from the folder's UIDVALIDITY and the message's
	UID (see synth_id() below) so that re-running imapsync is idempotent
	-- no separate id-mapping database is needed.  Per-folder sync
	progress (uidvalidity, highest-synced uid, highestmodseq) lives in a
	small text file ".MS_IMAPSync" inside each mirrored folder directory.

	Implemented so far: fresh mirror + idempotent incremental re-run.
	Flags mapping, server-side expunge detection, and the CONDSTORE-skip
	optimization are follow-on work and are NOT implemented here.

	IMAP usage is strictly read-only: only imap_Examine/imap_UidSearch/
	imap_UidFetchMeta/imap_UidFetchBody are called (EXAMINE and
	BODY.PEEK[] on the wire), so a sync run can never alter or destroy
	anything on the server.

	ANSI C (C89 prototypes), scanf family banned -- same policy as
	imap_prot.c.  Numbers are parsed with strtoul/strtoull (+ endptr
	where it matters), keywords with strcmp/strncasecmp.
*/

#ifndef NORCSID
#define NORCSID
#endif

#include <andrewos.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/param.h>		/* MAXPATHLEN */
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include <netrc.h>
#include <imap_prot.h>
#include <ams.h>		/* AMS_IDSIZE, AMS_DATESIZE, mserrcode/AMS_ERR* macros */

/* ---- ANSI declarations for the K&R library entry points this program
   calls, at the "extern declaration site" the ground rules mention.
   Neither ms.h nor cui.h prototypes any MS_ call anywhere in the tree
   (confirmed by grep) -- callers either take the implicit-int default or,
   like ams/msclients/vui/vui.c, write an ad hoc local extern.  Since this
   file is new ANSI code, the local externs below are given full
   prototypes rather than K&R empty-parens ones. ---- */

/* NOTE on return types: MS_Initialize/MS_UpdateState/
   MS_CreateNewMessageDirectory (existing) and MS_AppendFileToFolderWithId
   (added for imapsync) are all K&R functions defined with no
   explicit return type, i.e. their *actual* compiled return type is
   `int`, even though the value they return is conceptually the packed
   `long mserrcode` (the values involved never exceed 24 bits, so the
   long->int truncation inside their own `return` statements is lossless
   -- see AMS_RETURN_ERRCODE in mserrno.h). Declaring these prototypes as
   returning `long` here would be a real bug: on LP64 arm64 the callee
   only populates the low 32 bits of the return register, and a caller
   that reads it as 64-bit `long` can pick up garbage in the upper 32
   bits -- exactly the register-garbage class documented in this repo's
   LP64 bug notes, just via a mismatched prototype instead of a missing
   one. Match the real (int) return type here; the `long` locals these
   are assigned to below get a normal, safe widening conversion. */
extern int MS_Initialize(int *maxbufsize, int usingsnap);
extern int MS_UpdateState(void);
extern int MS_CreateNewMessageDirectory(char *dirname, int overwrite, char *bodydirname);
extern int MS_AppendFileToFolderWithId(char *filename, char *foldername,
                                        char *id, char *date64);

/* overhead/mail/lib/genid.c -- the tree's base-64 helpers (mail.h has
   only a K&R "extern char *convlongto64();"; declared ANSI here). Each
   returns a pointer into a static 7-byte buffer that is overwritten on
   the *next* call -- every call site below copies the result out
   immediately, before making another call, to avoid aliasing. */
extern char *convlongto64(long num, int pad);

/* getprofile() -- the tree's preference lookup (declared K&R all over
   the tree, e.g. ams/libs/ms/util.c); used only for the "imapsyncfolders"
   default when -folders is not given. */
extern char *getprofile(char *name);

/* ---- Client-provided glue that libmssrv.a expects any direct linker
   (cui gets these from libcui.a/andmchs.c; a minimal client like
   ams/msclients/nns/nns.c -- imapsync's model, see the Imakefile note --
   provides its own dummies) to define. K&R signatures deliberately
   match nns.c's exact versions: these are called from K&R object code
   with no prototype in scope, so matching the established, working
   parameter types avoids introducing a new prototype/definition
   mismatch. ---- */

int MSDebugging = 0;
unsigned char *SnapVersionString = NULL;

BizarreError(text, level)		/* dummy, as in nns.c -- libmssrv.a needs it */
char *text;
int level;
{
    fprintf(stderr, "imapsync: %s (%d)\n", text, level);
}

Machine_HandleClientSignal(signum, ActNormal)		/* ditto */
int signum, *ActNormal;
{
    fprintf(stderr, "imapsync: Machine_HandleClientSignal signum=%d\n", signum);
}

#define DEFAULT_HOST "imap.fastmail.com"
#define DEFAULT_PORT 993
#define DEFAULT_NETRC_MACHINE "imap.fastmail.com"
#define DEFAULT_FOLDERS "INBOX"
#define DEFAULT_ROOT_TAIL "/.IMAP/fastmail"

/* 40, not more: imap_UidFetchMeta() (overhead/mail/lib/imap_prot.c)
   builds its command into a fixed `char cmdtext[512]` and snprintf()s
   "UID FETCH <uidset> (FLAGS INTERNALDATE ENVELOPE)" into it --
   silently truncating for uidsets much past ~470 bytes (a ~100-uid
   batch of 5-digit UIDs already overflows), producing a malformed
   command the server correctly rejects ("BAD Missing required argument
   to Uid").  40 UIDs/batch is provably safe for that ceiling even in
   the worst case (40 * 11 bytes/uid, allowing up to 10-digit UIDs +
   comma, = 440 bytes, comfortably under the ~470 available after
   "UID FETCH " + " (FLAGS INTERNALDATE ENVELOPE)" + NUL).  Do not
   raise this without first widening cmdtext (or switching it to a
   growable buffer) in imap_prot.c itself. */
#define FETCH_BATCH 40

static int Verbose = 0;

/* ---------------------------------------------------------------- */
/* small utilities                                                  */
/* ---------------------------------------------------------------- */

static void vlog(const char *fmt, ...)
{
    va_list ap;

    if (!Verbose) return;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void loudlog(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/* Takes the MS_ call's return value explicitly and assigns it to the
   global mserrcode itself -- per the interface doc's "by convention,
   all clients set the global variable mserrcode to the result of each
   message server call" -- rather than trusting mserrcode to still hold
   it. It usually would, but MS_AppendFileToFolder{,WithId}'s own wrapper
   (AppendFileToFolder{,WithId} in apndfile.c) does
   "errsave = AppendFileToMSDir...(...); mserrcode = CloseMSDir(...);
   return (errsave ? errsave : mserrcode);" -- so on a failure inside the
   append itself, the *returned* value (errsave) and the *global*
   mserrcode (now whatever CloseMSDir most recently set it to, often 0)
   can legitimately differ by the time a caller gets around to looking.
   Decode the value the call actually returned, not a global that may
   already have moved on. */
static void report_mserr(const char *what, long code)
{
    mserrcode = code;
    fprintf(stderr, "imapsync: %s failed: mserrcode=%ld (errno=%ld cause=%ld via=%ld: %s)\n",
            what, mserrcode, AMS_ERRNO, AMS_ERRCAUSE, AMS_ERRVIA,
            (AMS_ERRNO > 0 && AMS_ERRNO < 128) ? strerror((int) AMS_ERRNO) : "(ms-specific code)");
}

static const char *imap_rcname(int rc)
{
    switch (rc) {
    case IMAP_OK:		return "IMAP_OK";
    case IMAP_NO:		return "IMAP_NO";
    case IMAP_BAD:		return "IMAP_BAD";
    case IMAP_DEAD:		return "IMAP_DEAD";
    case IMAP_UIDCHANGED:	return "IMAP_UIDCHANGED";
    default:			return "???";
    }
}

/* mkdir -p, roughly -- creates every missing component of path. */
static int mkdir_p(const char *path)
{
    char buf[MAXPATHLEN + 1];
    char *p;
    struct stat st;

    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    for (p = buf + 1; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (stat(buf, &st) != 0) {
                if (mkdir(buf, 0777) != 0 && errno != EEXIST) return -1;
            }
            *p = '/';
        }
    }
    if (stat(buf, &st) != 0) {
        if (mkdir(buf, 0777) != 0 && errno != EEXIST) return -1;
    }
    return 0;
}

/* Ensure <path>/.MS_MsgDir exists, creating it (and path itself) via the
   store's own MS_CreateNewMessageDirectory if not.  Idempotent: does
   nothing if the folder is already initialized. */
static int ensure_folder_dir(const char *path)
{
    char msgdirpath[MAXPATHLEN + 1];
    struct stat st;
    long rc;

    if (mkdir_p(path) != 0) {
        fprintf(stderr, "imapsync: could not create directory %s: %s\n", path, strerror(errno));
        return -1;
    }
    snprintf(msgdirpath, sizeof(msgdirpath), "%s/.MS_MsgDir", path);
    if (stat(msgdirpath, &st) == 0) return 0;	/* already a message directory */

    rc = MS_CreateNewMessageDirectory((char *) path, 0, (char *) path);
    if (rc != 0) {
        report_mserr("MS_CreateNewMessageDirectory", rc);
        return -1;
    }
    return 0;
}

/* Delete every file directly inside path (used only on a UIDVALIDITY
   change, right before MS_CreateNewMessageDirectory re-initializes
   .MS_MsgDir with Overwrite=1).  Non-recursive: imapsync never creates
   subdirectories inside a mirrored folder. */
static int wipe_folder_contents(const char *path)
{
    DIR *d;
    struct dirent *ent;
    char entpath[MAXPATHLEN + 1];

    d = opendir(path);
    if (d == NULL) return (errno == ENOENT) ? 0 : -1;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        snprintf(entpath, sizeof(entpath), "%s/%s", path, ent->d_name);
        (void) unlink(entpath);
    }
    closedir(d);
    return 0;
}

/* ---------------------------------------------------------------- */
/* synthesized AMS id                                                   */
/* ---------------------------------------------------------------- */

/* WHY NOT BASE64: the store names message body files "+<id>" on disk,
   and macOS's default APFS (like HFS+ and NTFS) is case-insensitive but
   case-preserving. The original design here used the tree's own
   convlongto64() -- 64-char mixed-case base64 -- for id synthesis,
   which produces ids differing ONLY in letter case for many distinct
   (uidvalidity, uid) pairs (e.g. ...GvA vs ...Gva). Two ids differing
   only by case are DISTINCT strings but name the SAME file on a
   case-insensitive filesystem, so a later message's WritePureFile()
   open(O_CREAT|O_EXCL) collided with an EARLIER, case-differing
   message's file -- reproduced and confirmed directly (touch +IMAPAb;
   ls +IMAPaB finds it), and confirmed live: uid 77516 -> ...GvA and
   uid 77542 -> ...Gva collided, as did uid 77585 -> ...GwF and uid
   77611 -> ...Gwf, in every run where the lower uid of each pair was
   appended first. The same hazard exists in principle for the store's
   own ams_genid()-invented ids (also mixed-case base64), though the
   odds are far lower there since those ids differ in many characters
   at once (time+host+pid+counter).

   Fix: encode with a 32-character (5 bits/char) alphabet containing NO
   case pairs at all -- base32hex ("0123456789ABCDEFGHIJKLMNOPQRSTUV",
   RFC 4648 base32hex, entirely uppercase digits/letters, itself a
   subset of the id charset ams_genid() draws from) -- so case-folding
   the whole id string is now a no-op by construction: two DIFFERENT
   ids can never compare equal case-insensitively, because there is no
   lowercase anywhere to fold. Fixed-width, no variable-length encoding:

       out[0..3]    literal "IMAP"                (recognizable prefix)
       out[4..10]   uidvalidity, 7 base32hex chars (32 bits, zero-padded
                                                     to the 35-bit field)
       out[11..17]  uid, 7 base32hex chars         (ditto)

   4 + 7 + 7 = 18 = AMS_IDSIZE - 1.  Deterministic: the same
   (uidvalidity, uid) always yields the same id, which is what makes
   re-running imapsync idempotent at the store layer (in addition to the
   uid-watermark check that normally avoids even trying to re-append an
   already-synced message). */

static const char Base32Hex[33] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";

/* Encodes a 32-bit value into exactly 7 base32hex characters (35 bits
   of room; the top 3 are always zero for a 32-bit input), MSB-first.
   out must have room for 8 bytes (7 chars + NUL). */
static void encode_base32hex7(unsigned long v, char *out)
{
    int i;

    for (i = 0; i < 7; ++i) {
        out[i] = Base32Hex[(v >> ((6 - i) * 5)) & 0x1FUL];
    }
    out[7] = '\0';
}

/* out must have room for AMS_IDSIZE bytes (18 chars + NUL). */
static void synth_id(unsigned long uidvalidity, unsigned long uid, char *out)
{
    char field[8];

    memcpy(out, "IMAP", 4);
    encode_base32hex7(uidvalidity, field);
    memcpy(out + 4, field, 7);
    encode_base32hex7(uid, field);
    memcpy(out + 11, field, 7);
    out[AMS_IDSIZE - 1] = '\0';
}

/* ---------------------------------------------------------------- */
/* INTERNALDATE parsing -> AMS_DATE (base64 unix time)                  */
/* ---------------------------------------------------------------- */

static const char *Months3[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* Parses an RFC 3501 INTERNALDATE string, e.g.
   "18-Jul-2026 12:34:56 +0000" (already dequoted by imap_prot.c's
   tokenizer) into a UTC time_t.  Hand-rolled (no scanf/sscanf): strtoul
   plus manual delimiter checks, matching imap_prot.c's own conventions.
   Returns 0 on success, -1 on a parse error. */
static int parse_internaldate(const char *s, time_t *outp)
{
    char buf[64];
    char *p, *end;
    struct tm tmv;
    int day, mon, year, hour, min, sec, tzsign, tzhour, tzmin;
    long tzoffset;
    time_t t;
    char tzb[3];

    if (s == NULL) return -1;
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    p = buf;
    while (*p == ' ') ++p;

    day = (int) strtoul(p, &end, 10);
    if (end == p || *end != '-') return -1;
    p = end + 1;

    if (strlen(p) < 4 || p[3] != '-') return -1;
    for (mon = 0; mon < 12; ++mon) {
        if (strncasecmp(p, Months3[mon], 3) == 0) break;
    }
    if (mon == 12) return -1;
    p += 4;

    year = (int) strtoul(p, &end, 10);
    if (end == p || *end != ' ') return -1;
    p = end + 1;

    hour = (int) strtoul(p, &end, 10);
    if (end == p || *end != ':') return -1;
    p = end + 1;

    min = (int) strtoul(p, &end, 10);
    if (end == p || *end != ':') return -1;
    p = end + 1;

    sec = (int) strtoul(p, &end, 10);
    if (end == p || *end != ' ') return -1;
    p = end + 1;

    if (*p != '+' && *p != '-') return -1;
    tzsign = (*p == '-') ? -1 : 1;
    ++p;
    if (strlen(p) < 4) return -1;
    tzb[0] = p[0]; tzb[1] = p[1]; tzb[2] = '\0';
    tzhour = (int) strtoul(tzb, NULL, 10);
    tzb[0] = p[2]; tzb[1] = p[3]; tzb[2] = '\0';
    tzmin = (int) strtoul(tzb, NULL, 10);
    tzoffset = tzsign * (tzhour * 3600L + tzmin * 60L);

    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_mday = day;
    tmv.tm_mon = mon;
    tmv.tm_year = year - 1900;
    tmv.tm_hour = hour;
    tmv.tm_min = min;
    tmv.tm_sec = sec;
    tmv.tm_isdst = 0;

    t = timegm(&tmv);
    if (t == (time_t) -1) return -1;
    t -= tzoffset;

    *outp = t;
    return 0;
}

/* ---------------------------------------------------------------- */
/* .MS_IMAPSync per-folder state file                                   */
/* ---------------------------------------------------------------- */

struct sync_state {
    unsigned long uidvalidity;
    unsigned long highestuid;
    unsigned long long highestmodseq;
};

static void load_state(const char *path, struct sync_state *st)
{
    FILE *f;
    char line[256];
    char *sp, *val, *nl;

    st->uidvalidity = 0;
    st->highestuid = 0;
    st->highestmodseq = 0;

    f = fopen(path, "r");
    if (f == NULL) return;
    while (fgets(line, sizeof(line), f) != NULL) {
        nl = strchr(line, '\n');
        if (nl != NULL) *nl = '\0';
        sp = strchr(line, ' ');
        if (sp == NULL) continue;
        *sp = '\0';
        val = sp + 1;
        if (strcmp(line, "uidvalidity") == 0) st->uidvalidity = strtoul(val, NULL, 10);
        else if (strcmp(line, "highestuid") == 0) st->highestuid = strtoul(val, NULL, 10);
        else if (strcmp(line, "highestmodseq") == 0) st->highestmodseq = strtoull(val, NULL, 10);
    }
    fclose(f);
}

static int write_state(const char *path, const struct sync_state *st)
{
    char tmppath[MAXPATHLEN + 1];
    FILE *f;

    snprintf(tmppath, sizeof(tmppath), "%s.tmp", path);
    f = fopen(tmppath, "w");
    if (f == NULL) return -1;
    fprintf(f, "uidvalidity %lu\n", st->uidvalidity);
    fprintf(f, "highestuid %lu\n", st->highestuid);
    fprintf(f, "highestmodseq %llu\n", st->highestmodseq);
    if (fclose(f) != 0) return -1;
    if (rename(tmppath, path) != 0) return -1;
    return 0;
}

/* ---------------------------------------------------------------- */
/* folder name -> local path (delimiter from LIST -> '/')               */
/* ---------------------------------------------------------------- */

struct list_delim_rock {
    const char *want;
    char delim;
    int found;
};

static int list_delim_cb(const char *name, const char *delim, const char *flags, void *rockp)
{
    struct list_delim_rock *r = (struct list_delim_rock *) rockp;

    (void) flags;
    if (strcmp(name, r->want) == 0) {
        r->delim = (delim != NULL && delim[0] != '\0') ? delim[0] : '\0';
        r->found = 1;
    }
    return 0;
}

/* local must have room for MAXPATHLEN+1 bytes. */
static int folder_local_path(struct imapconn *conn, const char *root,
                              const char *foldername, char *local)
{
    struct list_delim_rock rock;
    int rc;
    char mapped[MAXPATHLEN + 1];
    const char *s;
    char *d;

    rock.want = foldername;
    rock.delim = '\0';
    rock.found = 0;
    rc = imap_List(conn, "", foldername, list_delim_cb, &rock);
    if (rc != IMAP_OK) return rc;
    if (!rock.found) {
        fprintf(stderr, "imapsync: folder \"%s\" not found on server\n", foldername);
        return IMAP_NO;
    }

    d = mapped;
    for (s = foldername; *s != '\0' && (d - mapped) < (long) sizeof(mapped) - 1; ++s) {
        *d++ = (rock.delim != '\0' && *s == rock.delim) ? '/' : *s;
    }
    *d = '\0';

    /* MS_TREEROOT (ams.h) requires a path component literally named
       ".MESSAGES" (or ".MESSAGES*") somewhere above any MS message
       directory -- FindTreeRoot (ams/libs/shr/findroot.c), called from
       MS_CreateNewMessageDirectory's EnsureInSubscriptionMap step, fails
       EINVAL without it. This is exactly how the personal mail root is
       built (MAILSEARCHPATHTEMPLATE = "/.MESSAGES" in
       ams/libs/ms/util.c: $HOME + "/.MESSAGES"), so the IMAP mirror
       tree needs the same marker component beneath the account root:
       <root>/.MESSAGES/<folder>, not <root>/<folder> directly -- the
       on-disk shape a real AMS tree already requires everywhere else. */
    snprintf(local, MAXPATHLEN + 1, "%s/.MESSAGES/%s", root, mapped);
    return IMAP_OK;
}

/* ---------------------------------------------------------------- */
/* fetch-meta batch callback                                            */
/* ---------------------------------------------------------------- */

struct meta_batch_rock {
    unsigned long *uids;	/* batch slice, ascending */
    int count;
    char internaldate[FETCH_BATCH][128];
    int got[FETCH_BATCH];
};

static int meta_batch_cb(unsigned long uid, const char *flags,
                          const char *internaldate,
                          const struct imap_envelope *env, void *rockp)
{
    struct meta_batch_rock *r = (struct meta_batch_rock *) rockp;
    int i;

    (void) flags;
    (void) env;
    for (i = 0; i < r->count; ++i) {
        if (r->uids[i] == uid) {
            strncpy(r->internaldate[i], internaldate != NULL ? internaldate : "",
                    sizeof(r->internaldate[i]) - 1);
            r->internaldate[i][sizeof(r->internaldate[i]) - 1] = '\0';
            r->got[i] = 1;
            break;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* per-folder sync                                                      */
/* ---------------------------------------------------------------- */

#define SF_OK 0
#define SF_FAIL 1
#define SF_DEAD 2	/* connection died and could not be recovered */

static int sync_folder_once(struct imapconn *conn, const char *root,
                             const char *foldername, int *anyerror)
{
    char localpath[MAXPATHLEN + 1];
    char statepath[MAXPATHLEN + 1];
    struct imap_mboxinfo info;
    struct sync_state state;
    int rc;
    char criteria[64];
    unsigned long *uids = NULL;
    long count = 0;
    long batchstart;

    rc = folder_local_path(conn, root, foldername, localpath);
    if (rc == IMAP_DEAD) return SF_DEAD;
    if (rc != IMAP_OK) { *anyerror = 1; return SF_FAIL; }

    rc = imap_Examine(conn, foldername, &info);
    if (rc == IMAP_DEAD) return SF_DEAD;
    if (rc != IMAP_OK) {
        fprintf(stderr, "imapsync: EXAMINE %s failed: %d (%s) %s\n",
                foldername, rc, imap_rcname(rc), imap_ErrMsg(conn));
        *anyerror = 1;
        return SF_FAIL;
    }
    vlog("EXAMINE %s: exists=%ld uidvalidity=%lu uidnext=%lu highestmodseq=%llu",
         foldername, info.exists, info.uidvalidity, info.uidnext, info.highestmodseq);

    if (ensure_folder_dir(localpath) != 0) { *anyerror = 1; return SF_FAIL; }

    snprintf(statepath, sizeof(statepath), "%s/.MS_IMAPSync", localpath);
    load_state(statepath, &state);

    if (state.uidvalidity != 0 && state.uidvalidity != info.uidvalidity) {
        loudlog("imapsync: UIDVALIDITY changed for folder \"%s\": was %lu, now %lu -- "
                "removing mirrored messages and re-mirroring from scratch",
                foldername, state.uidvalidity, info.uidvalidity);
        if (wipe_folder_contents(localpath) != 0) {
            fprintf(stderr, "imapsync: could not clear %s for re-mirror: %s\n",
                    localpath, strerror(errno));
            *anyerror = 1;
            return SF_FAIL;
        }
        rc = MS_CreateNewMessageDirectory((char *) localpath, 1, (char *) localpath);
        if (rc != 0) {
            report_mserr("MS_CreateNewMessageDirectory (re-mirror)", rc);
            *anyerror = 1;
            return SF_FAIL;
        }
        state.uidvalidity = 0;
        state.highestuid = 0;
        state.highestmodseq = 0;
    }

    snprintf(criteria, sizeof(criteria), "UID %lu:*", state.highestuid + 1);
    rc = imap_UidSearch(conn, criteria, &uids, &count);
    if (rc == IMAP_DEAD) return SF_DEAD;
    if (rc != IMAP_OK) {
        fprintf(stderr, "imapsync: UID SEARCH %s in %s failed: %d (%s) %s\n",
                criteria, foldername, rc, imap_rcname(rc), imap_ErrMsg(conn));
        *anyerror = 1;
        return SF_FAIL;
    }
    vlog("UID SEARCH %s in %s: %ld candidate uid(s)", criteria, foldername, count);

    for (batchstart = 0; batchstart < count; batchstart += FETCH_BATCH) {
        struct meta_batch_rock rock;
        char uidset[FETCH_BATCH * 12 + 16];
        int n, i;
        char *up;

        n = (int) ((count - batchstart) < FETCH_BATCH ? (count - batchstart) : FETCH_BATCH);
        rock.uids = uids + batchstart;
        rock.count = n;
        for (i = 0; i < n; ++i) { rock.got[i] = 0; rock.internaldate[i][0] = '\0'; }

        up = uidset;
        uidset[0] = '\0';
        for (i = 0; i < n; ++i) {
            int wrote = snprintf(up, sizeof(uidset) - (up - uidset), "%s%lu",
                                  (i == 0) ? "" : ",", rock.uids[i]);
            if (wrote < 0) break;
            up += wrote;
        }

        rc = imap_UidFetchMeta(conn, uidset, meta_batch_cb, &rock);
        if (rc == IMAP_DEAD) { if (uids != NULL) free(uids); return SF_DEAD; }
        if (rc != IMAP_OK) {
            fprintf(stderr, "imapsync: UID FETCH meta batch in %s failed: %d (%s) %s\n",
                    foldername, rc, imap_rcname(rc), imap_ErrMsg(conn));
            *anyerror = 1;
            continue;	/* try the next batch anyway */
        }

        for (i = 0; i < n; ++i) {
            unsigned long uid = rock.uids[i];
            time_t t;
            char date64[8];
            char id[AMS_IDSIZE];
            char tmppath[MAXPATHLEN + 1];
            int fd;
            FILE *outf;
            long bodysize;
            long apprc;

            if (!rock.got[i]) {
                fprintf(stderr, "imapsync: no metadata for uid %lu in %s; skipping\n", uid, foldername);
                *anyerror = 1;
                continue;
            }
            if (parse_internaldate(rock.internaldate[i], &t) != 0) {
                fprintf(stderr, "imapsync: could not parse INTERNALDATE \"%s\" for uid %lu in %s; skipping\n",
                        rock.internaldate[i], uid, foldername);
                *anyerror = 1;
                continue;
            }
            strncpy(date64, convlongto64((long) t, 0), 6);
            date64[6] = '\0';
            synth_id(info.uidvalidity, uid, id);
            vlog("uid %lu -> id %s date64 %s", uid, id, date64);

            snprintf(tmppath, sizeof(tmppath), "%s/.imapsync-fetch-XXXXXX", localpath);
            fd = mkstemp(tmppath);
            if (fd < 0) {
                fprintf(stderr, "imapsync: mkstemp in %s failed: %s\n", localpath, strerror(errno));
                *anyerror = 1;
                continue;
            }
            outf = fdopen(fd, "wb");
            if (outf == NULL) {
                fprintf(stderr, "imapsync: fdopen failed: %s\n", strerror(errno));
                close(fd);
                unlink(tmppath);
                *anyerror = 1;
                continue;
            }

            rc = imap_UidFetchBody(conn, uid, outf, &bodysize);
            fclose(outf);
            if (rc == IMAP_DEAD) { unlink(tmppath); if (uids != NULL) free(uids); return SF_DEAD; }
            if (rc != IMAP_OK) {
                fprintf(stderr, "imapsync: FETCH BODY uid %lu in %s failed: %d (%s) %s\n",
                        uid, foldername, rc, imap_rcname(rc), imap_ErrMsg(conn));
                unlink(tmppath);
                *anyerror = 1;
                continue;
            }

            apprc = MS_AppendFileToFolderWithId(tmppath, (char *) localpath, id, date64);
            if (apprc != 0) {
                fprintf(stderr, "imapsync: MS_AppendFileToFolderWithId uid %lu (id %s) in %s: ", uid, id, foldername);
                report_mserr("MS_AppendFileToFolderWithId", apprc);
                unlink(tmppath);
                *anyerror = 1;
                continue;
            }

            vlog("appended uid %lu -> id %s date64 %s", uid, id, date64);
            if (uid > state.highestuid) state.highestuid = uid;
            state.uidvalidity = info.uidvalidity;
            state.highestmodseq = info.highestmodseq;
            if (write_state(statepath, &state) != 0) {
                fprintf(stderr, "imapsync: could not write %s: %s\n", statepath, strerror(errno));
                *anyerror = 1;
            }
        }
    }

    if (uids != NULL) free(uids);

    state.uidvalidity = info.uidvalidity;
    state.highestmodseq = info.highestmodseq;
    if (write_state(statepath, &state) != 0) {
        fprintf(stderr, "imapsync: could not write final %s: %s\n", statepath, strerror(errno));
        *anyerror = 1;
    }

    return SF_OK;
}

/* Wraps sync_folder_once with the reconnect contract: one imap_Reopen
   retry per folder pass. */
static int sync_folder(struct imapconn *conn, const char *login, const char *passwd,
                        const char *root, const char *foldername, int *anyerror)
{
    int result;
    int rc;

    result = sync_folder_once(conn, root, foldername, anyerror);
    if (result != SF_DEAD) return result;

    loudlog("imapsync: connection to server lost while syncing \"%s\"; reconnecting...", foldername);
    rc = imap_Reopen(conn, login, passwd);
    if (rc != IMAP_OK && rc != IMAP_UIDCHANGED) {
        fprintf(stderr, "imapsync: reconnect failed: %d (%s) %s\n",
                rc, imap_rcname(rc), imap_ErrMsg(conn));
        *anyerror = 1;
        return SF_FAIL;
    }
    /* IMAP_UIDCHANGED funnels into the ordinary UIDVALIDITY-mismatch path
       inside sync_folder_once (its own EXAMINE will see the new value and
       compare it against the persisted state). One retry only. */
    result = sync_folder_once(conn, root, foldername, anyerror);
    if (result == SF_DEAD) {
        fprintf(stderr, "imapsync: connection to server lost again while syncing \"%s\"; giving up\n",
                foldername);
        *anyerror = 1;
        return SF_FAIL;
    }
    return result;
}

/* ---------------------------------------------------------------- */
/* main                                                                  */
/* ---------------------------------------------------------------- */

static char *expand_home(const char *path, char *buf, size_t buflen)
{
    const char *home;

    if (path[0] != '~') {
        strncpy(buf, path, buflen - 1);
        buf[buflen - 1] = '\0';
        return buf;
    }
    home = getenv("HOME");
    if (home == NULL) home = "";
    snprintf(buf, buflen, "%s%s", home, path + 1);
    return buf;
}

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [-root <dir>] [-folders <comma,list>] [-v]\n", prog);
}

int main(int argc, char **argv)
{
    char rootbuf[MAXPATHLEN + 1];
    const char *root = NULL;
    const char *folderlist = NULL;
    char netrcpath[MAXPATHLEN + 1];
    char login[256], passwd[256];
    int nrc, rc;
    int dummy;
    struct imapconn *conn;
    char errbuf[512];
    int anyerror = 0;
    int i;
    char *foldercopy, *tok, *save;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "-folders") == 0 && i + 1 < argc) {
            folderlist = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            Verbose = 1;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (root == NULL) {
        char defaultroot[MAXPATHLEN + 1];
        snprintf(defaultroot, sizeof(defaultroot), "~%s", DEFAULT_ROOT_TAIL);
        expand_home(defaultroot, rootbuf, sizeof(rootbuf));
    } else {
        expand_home(root, rootbuf, sizeof(rootbuf));
    }

    if (folderlist == NULL) {
        folderlist = getprofile("imapsyncfolders");
        if (folderlist == NULL || folderlist[0] == '\0') folderlist = DEFAULT_FOLDERS;
    }

    expand_home("~/.netrc", netrcpath, sizeof(netrcpath));

    if (mkdir_p(rootbuf) != 0) {
        fprintf(stderr, "imapsync: could not create root %s: %s\n", rootbuf, strerror(errno));
        return 1;
    }
    vlog("root: %s", rootbuf);
    vlog("folders: %s", folderlist);

    dummy = 0;
    rc = MS_Initialize(&dummy, 0);
    if (rc != 0) {
        report_mserr("MS_Initialize", rc);
        return 1;
    }

    nrc = netrc_Lookup(netrcpath, DEFAULT_NETRC_MACHINE, login, sizeof(login), passwd, sizeof(passwd));
    if (nrc != NETRC_OK) {
        fprintf(stderr, "imapsync: netrc lookup for \"%s\" in %s failed (%d)\n",
                DEFAULT_NETRC_MACHINE, netrcpath, nrc);
        return 1;
    }

    rc = imap_Open(&conn, DEFAULT_HOST, DEFAULT_PORT, errbuf, sizeof(errbuf));
    if (rc != IMAP_OK) {
        fprintf(stderr, "imapsync: connect to %s:%d failed: %s\n", DEFAULT_HOST, DEFAULT_PORT, errbuf);
        memset(passwd, 0, sizeof(passwd));
        return 1;
    }
    rc = imap_Login(conn, login, passwd);
    if (rc != IMAP_OK) {
        fprintf(stderr, "imapsync: login failed: %d (%s) %s\n", rc, imap_rcname(rc), imap_ErrMsg(conn));
        memset(passwd, 0, sizeof(passwd));
        imap_Close(conn);
        return 1;
    }

    foldercopy = malloc(strlen(folderlist) + 1);
    if (foldercopy == NULL) {
        fprintf(stderr, "imapsync: out of memory\n");
        memset(passwd, 0, sizeof(passwd));
        imap_Close(conn);
        return 1;
    }
    strcpy(foldercopy, folderlist);

    save = NULL;
    for (tok = strtok_r(foldercopy, ",", &save); tok != NULL; tok = strtok_r(NULL, ",", &save)) {
        int result;

        while (*tok == ' ') ++tok;
        if (*tok == '\0') continue;

        vlog("--- syncing folder \"%s\" ---", tok);
        result = sync_folder(conn, login, passwd, rootbuf, tok, &anyerror);
        if (result != SF_OK) anyerror = 1;
    }
    free(foldercopy);

    memset(passwd, 0, sizeof(passwd));
    MS_UpdateState();
    imap_Close(conn);

    return anyerror ? 1 : 0;
}
