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

	Per-run work, in order: fresh/incremental mirror of new mail (with
	\Seen/\Answered/\Deleted applied from the same FETCH that pulled
	each new message's INTERNALDATE); a flags refresh pass over
	previously-mirrored messages, skipped when HIGHESTMODSEQ shows
	nothing changed since the last run (a modseq comparison only -- the
	underlying FETCH has no CHANGEDSINCE modifier available to it); and,
	only with -full-check, a full UID list comparison that marks
	AMS_ATT_DELETED locally for any previously-mirrored message no
	longer present on the server (never purges -- that stays a human or
	later-milestone decision). A UIDVALIDITY change wipes and re-mirrors
	a folder from scratch, loudly.

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
#include <ctype.h>
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
extern int MS_AlterSnapshot(char *dirname, char *id, char *newsnapshot, int code);

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

/* Metadata UIDs per UID FETCH round trip.  imap_UidFetchMeta() sizes
   its command buffer to the uidset it is given, so this is a tuning
   knob, not a protocol-safety ceiling: bigger batches mean fewer round
   trips on a large mirror, at the cost of larger per-batch stack
   arrays in meta_batch_rock below. */
#define FETCH_BATCH 100

#define SF_OK 0
#define SF_FAIL 1
#define SF_DEAD 2	/* connection died and could not be recovered */

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
    char flags[FETCH_BATCH][256];
    int got[FETCH_BATCH];
};

static int meta_batch_cb(unsigned long uid, const char *flags,
                          const char *internaldate,
                          const struct imap_envelope *env, void *rockp)
{
    struct meta_batch_rock *r = (struct meta_batch_rock *) rockp;
    int i;

    (void) env;
    for (i = 0; i < r->count; ++i) {
        if (r->uids[i] == uid) {
            strncpy(r->internaldate[i], internaldate != NULL ? internaldate : "",
                    sizeof(r->internaldate[i]) - 1);
            r->internaldate[i][sizeof(r->internaldate[i]) - 1] = '\0';
            strncpy(r->flags[i], flags != NULL ? flags : "",
                    sizeof(r->flags[i]) - 1);
            r->flags[i][sizeof(r->flags[i]) - 1] = '\0';
            r->got[i] = 1;
            break;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* flags mapping: IMAP system flags -> AMS attributes                   */
/* ---------------------------------------------------------------- */

/* flagstext is the parenthesized FLAGS list's contents, unparsed, e.g.
   "\Seen \Answered" or "" -- tokenize on whitespace, compare
   case-insensitively (IMAP flag atoms are technically case-sensitive
   per RFC 3501, but real servers vary and case-insensitive compare
   costs nothing here). */
static void parse_flags(const char *flagstext, int *seen, int *answered, int *deleted)
{
    char buf[256];
    char *tok, *save;

    *seen = 0;
    *answered = 0;
    *deleted = 0;
    if (flagstext == NULL) return;

    strncpy(buf, flagstext, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    for (tok = strtok_r(buf, " ", &save); tok != NULL; tok = strtok_r(NULL, " ", &save)) {
        if (strcasecmp(tok, "\\Seen") == 0) *seen = 1;
        else if (strcasecmp(tok, "\\Answered") == 0) *answered = 1;
        else if (strcasecmp(tok, "\\Deleted") == 0) *deleted = 1;
    }
}

/* Maps \Seen/\Answered/\Deleted onto AMS_ATT_UNSEEN (inverted)/
   AMS_ATT_REPLIEDTO/AMS_ATT_DELETED via two MS_AlterSnapshot calls: one
   ASS_OR_ATTRIBUTES (bits to turn on) and one ASS_AND_ATTRIBUTES (a
   keep-mask -- a 0 bit there forces the corresponding attribute off).
   Both calls only ever touch the attributes field of the target
   snapshot (see MS_AlterSnapshot's own switch in
   ams/libs/ms/altsnap.c) -- the rest of orbuf/andbuf is never
   examined, so it is left zeroed/undefined. Returns 0 on success. */
static long apply_flags(const char *dirpath, const char *id, const char *flagstext)
{
    char orbuf[AMS_SNAPSHOTSIZE];
    char andbuf[AMS_SNAPSHOTSIZE];
    int seen, answered, deleted;
    long rc;

    parse_flags(flagstext, &seen, &answered, &deleted);

    memset(orbuf, 0, sizeof(orbuf));
    memset(andbuf, 0xFF, sizeof(andbuf));

    if (!seen) AMS_SET_ATTRIBUTE(orbuf, AMS_ATT_UNSEEN);
    else       AMS_UNSET_ATTRIBUTE(andbuf, AMS_ATT_UNSEEN);

    if (answered) AMS_SET_ATTRIBUTE(orbuf, AMS_ATT_REPLIEDTO);
    else          AMS_UNSET_ATTRIBUTE(andbuf, AMS_ATT_REPLIEDTO);

    if (deleted) AMS_SET_ATTRIBUTE(orbuf, AMS_ATT_DELETED);
    else         AMS_UNSET_ATTRIBUTE(andbuf, AMS_ATT_DELETED);

    rc = MS_AlterSnapshot((char *) dirpath, (char *) id, orbuf, ASS_OR_ATTRIBUTES);
    if (rc != 0) return rc;
    return MS_AlterSnapshot((char *) dirpath, (char *) id, andbuf, ASS_AND_ATTRIBUTES);
}

struct flags_refresh_rock {
    const char *localpath;
    const char *foldername;
    unsigned long uidvalidity;
    int *anyerror;
};

static int flags_refresh_cb(unsigned long uid, const char *flags,
                             const char *internaldate,
                             const struct imap_envelope *env, void *rockp)
{
    struct flags_refresh_rock *r = (struct flags_refresh_rock *) rockp;
    char id[AMS_IDSIZE];
    long rc;

    (void) internaldate;
    (void) env;

    synth_id(r->uidvalidity, uid, id);
    rc = apply_flags(r->localpath, id, flags);
    if (rc != 0) {
        mserrcode = rc;
        if (AMS_ERRNO == EMSNOSUCHMESSAGE) {
            /* Expected, not an error: this uid falls inside the
               1:prior_highestuid refresh range but was never actually
               mirrored locally (e.g. it was already expunged from the
               server before this account's very first sync ever
               reached it). Every real mailbox accumulates a few of
               these over time; only a message this run actually wrote
               to disk can be missing for any other reason. */
            vlog("uid %lu (id %s) in %s: not locally mirrored (EMSNOSUCHMESSAGE); skipping", uid, id, r->foldername);
        } else {
            fprintf(stderr, "imapsync: apply_flags uid %lu (id %s) in %s: ", uid, id, r->foldername);
            report_mserr("MS_AlterSnapshot", rc);
            *r->anyerror = 1;
        }
    }
    return 0;
}

/* Refetches FLAGS (in batches) for every uid in [lo, hi] and applies
   the mapping above to each -- used to refresh already-mirrored
   messages whose flags may have changed since the last run. Does
   nothing if lo > hi (nothing to refresh). */
static int refresh_flags(struct imapconn *conn, const char *localpath,
                          const char *foldername, unsigned long uidvalidity,
                          unsigned long lo, unsigned long hi, int *anyerror)
{
    unsigned long batchlo;
    char criteria[64];
    struct flags_refresh_rock rock;
    int rc;

    if (lo > hi) return SF_OK;

    rock.localpath = localpath;
    rock.foldername = foldername;
    rock.uidvalidity = uidvalidity;
    rock.anyerror = anyerror;

    for (batchlo = lo; batchlo <= hi; batchlo += FETCH_BATCH) {
        unsigned long batchhi = batchlo + FETCH_BATCH - 1;

        if (batchhi > hi) batchhi = hi;
        snprintf(criteria, sizeof(criteria), "%lu:%lu", batchlo, batchhi);
        rc = imap_UidFetchMeta(conn, criteria, flags_refresh_cb, &rock);
        if (rc == IMAP_DEAD) return SF_DEAD;
        if (rc != IMAP_OK) {
            fprintf(stderr, "imapsync: FLAGS refresh batch %s in %s failed: %d (%s) %s\n",
                    criteria, foldername, rc, imap_rcname(rc), imap_ErrMsg(conn));
            *anyerror = 1;
            /* try the next batch anyway */
        }
    }
    return SF_OK;
}

/* ---------------------------------------------------------------- */
/* server-side expunge detection (-full-check only)                     */
/* ---------------------------------------------------------------- */

static int base32hex_digit(char c)
{
    const char *p;

    c = (char) toupper((unsigned char) c);
    p = strchr(Base32Hex, c);
    return (p != NULL) ? (int) (p - Base32Hex) : -1;
}

static int decode_base32hex7(const char *s, unsigned long *out)
{
    unsigned long v = 0;
    int i, d;

    for (i = 0; i < 7; ++i) {
        d = base32hex_digit(s[i]);
        if (d < 0) return -1;
        v = (v << 5) | (unsigned long) d;
    }
    *out = v & 0xFFFFFFFFUL;
    return 0;
}

/* Inverse of synth_id(): recovers (uidvalidity, uid) from an 18-char id
   this module generated. Returns -1 (and leaves the outputs untouched)
   if id isn't in the expected format -- e.g. a locally
   invented id from some other source, which this module never writes
   but which the store's own hidden-file recovery could in principle
   introduce; such ids are simply not ours to reconcile against IMAP
   UIDs and are left alone. */
static int decode_id(const char *id, unsigned long *uidvalidity, unsigned long *uid)
{
    if (strlen(id) != (size_t) (AMS_IDSIZE - 1)) return -1;
    if (strncmp(id, "IMAP", 4) != 0) return -1;
    if (decode_base32hex7(id + 4, uidvalidity) != 0) return -1;
    if (decode_base32hex7(id + 11, uid) != 0) return -1;
    return 0;
}

static int uid_present(const unsigned long *sorted, long count, unsigned long want)
{
    long lo = 0, hi = count - 1, mid;

    while (lo <= hi) {
        mid = lo + (hi - lo) / 2;
        if (sorted[mid] == want) return 1;
        if (sorted[mid] < want) lo = mid + 1;
        else hi = mid - 1;
    }
    return 0;
}

/* For every locally-mirrored message (any "+<id>" file in localpath
   whose id decodes to this folder's current uidvalidity) whose uid is
   no longer present in a full server-side UID list, sets
   AMS_ATT_DELETED locally. Does not purge -- v1 leaves that to a human
   or a future milestone, matching how AMS's own mark-then-purge model
   already works. This is the expensive full-mailbox check, hence
   opt-in via -full-check rather than run on every pass. */
static int check_expunged(struct imapconn *conn, const char *localpath,
                           const char *foldername, unsigned long uidvalidity,
                           int *anyerror)
{
    unsigned long *serveruids = NULL;
    long servercount = 0;
    DIR *d;
    struct dirent *ent;
    int rc;
    long marked = 0;

    rc = imap_UidSearch(conn, "ALL", &serveruids, &servercount);
    if (rc == IMAP_DEAD) return SF_DEAD;
    if (rc != IMAP_OK) {
        fprintf(stderr, "imapsync: full-check UID SEARCH ALL in %s failed: %d (%s) %s\n",
                foldername, rc, imap_rcname(rc), imap_ErrMsg(conn));
        *anyerror = 1;
        return SF_FAIL;
    }

    d = opendir(localpath);
    if (d == NULL) {
        fprintf(stderr, "imapsync: full-check could not open %s: %s\n", localpath, strerror(errno));
        if (serveruids != NULL) free(serveruids);
        *anyerror = 1;
        return SF_FAIL;
    }
    while ((ent = readdir(d)) != NULL) {
        unsigned long ev, euid;
        char orbuf[AMS_SNAPSHOTSIZE];
        long arc;

        if (ent->d_name[0] != '+') continue;
        if (decode_id(ent->d_name + 1, &ev, &euid) != 0) continue;
        if (ev != uidvalidity) continue;	/* stale generation; ignore */
        if (uid_present(serveruids, servercount, euid)) continue;

        memset(orbuf, 0, sizeof(orbuf));
        AMS_SET_ATTRIBUTE(orbuf, AMS_ATT_DELETED);
        arc = MS_AlterSnapshot((char *) localpath, ent->d_name + 1, orbuf, ASS_OR_ATTRIBUTES);
        if (arc != 0) {
            fprintf(stderr, "imapsync: full-check mark-deleted uid %lu (id %s) in %s: ",
                    euid, ent->d_name + 1, foldername);
            report_mserr("MS_AlterSnapshot", arc);
            *anyerror = 1;
            continue;
        }
        ++marked;
        vlog("full-check: uid %lu (id %s) no longer on server; marked AMS_ATT_DELETED locally",
             euid, ent->d_name + 1);
    }
    closedir(d);
    if (serveruids != NULL) free(serveruids);
    if (marked > 0) loudlog("imapsync: full-check marked %ld message(s) deleted in \"%s\" "
                             "(server-expunged; not purged)", marked, foldername);
    return SF_OK;
}

/* ---------------------------------------------------------------- */
/* per-folder sync                                                      */
/* ---------------------------------------------------------------- */

static int sync_folder_once(struct imapconn *conn, const char *root,
                             const char *foldername, int full_check, int *anyerror)
{
    char localpath[MAXPATHLEN + 1];
    char statepath[MAXPATHLEN + 1];
    struct imap_mboxinfo info;
    struct sync_state state;
    unsigned long prior_highestuid;
    unsigned long long prior_highestmodseq;
    int rc;
    char criteria[64];
    unsigned long *uids = NULL;
    long count = 0;
    long batchstart;
    unsigned long first_skipped_uid = 0;
    long skipped_count = 0;

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

    /* Captured before the new-mail pass below mutates state: this is
       what a flags refresh needs to know which uids were already
       mirrored coming into this run, and whether anything has changed
       since the last run at all (the CONDSTORE/HIGHESTMODSEQ skip). */
    prior_highestuid = state.highestuid;
    prior_highestmodseq = state.highestmodseq;

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

    /* RFC 3501 range semantics: "UID <n>:*" always includes the highest
       existing uid, even when every uid is below n -- so an idempotent
       re-run with no new mail can get the top already-mirrored message
       echoed back as a "candidate" (observed live: search 77742:*
       returned 77741 after a transient arrival+expunge bumped UIDNEXT).
       Filter anything at or below the watermark before fetching. */
    {
        long in, out;

        for (in = 0, out = 0; in < count; ++in) {
            if (uids[in] > state.highestuid) uids[out++] = uids[in];
            else vlog("ignoring candidate uid %lu <= watermark %lu (n:* range echo)",
                      uids[in], state.highestuid);
        }
        count = out;
    }

    for (batchstart = 0; batchstart < count; batchstart += FETCH_BATCH) {
        struct meta_batch_rock rock;
        char uidset[FETCH_BATCH * 12 + 16];
        int n, i;
        char *up;

        n = (int) ((count - batchstart) < FETCH_BATCH ? (count - batchstart) : FETCH_BATCH);
        rock.uids = uids + batchstart;
        rock.count = n;
        for (i = 0; i < n; ++i) { rock.got[i] = 0; rock.internaldate[i][0] = '\0'; rock.flags[i][0] = '\0'; }

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

            /* Already mirrored?  Ids are deterministic, so the target
               body file's existence is authoritative.  This matters when
               re-covering ground (a lost/corrupt state file forces a
               full re-scan over existing content): the store's own
               duplicate check keys on the RFC-822 Message-ID header,
               which a message can lack, and an append that gets past it
               into an existing "+<id>" file fails EEXIST instead of
               no-op'ing.  Check here and skip cleanly. */
            {
                char bodypath[MAXPATHLEN + 1];
                struct stat stb;

                snprintf(bodypath, sizeof(bodypath), "%s/+%s", localpath, id);
                if (stat(bodypath, &stb) == 0) {
                    vlog("uid %lu (id %s) already mirrored; skipping", uid, id);
                    if (uid > state.highestuid && (first_skipped_uid == 0 || uid < first_skipped_uid))
                        state.highestuid = uid;
                    continue;
                }
            }

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
                loudlog("imapsync: WARN: FETCH BODY uid %lu in %s failed: %d (%s) %s -- "
                        "skipping this message, will retry it next run",
                        uid, foldername, rc, imap_rcname(rc), imap_ErrMsg(conn));
                unlink(tmppath);
                if (first_skipped_uid == 0 || uid < first_skipped_uid) first_skipped_uid = uid;
                ++skipped_count;
                continue;
            }
            if (bodysize == 0) {
                /* A zero-length BODY.PEEK[] fetch for a message that
                   genuinely exists in the SEARCH result is a known
                   server-side race (seen in practice for a message that
                   is expunged, moved, or still being delivered between
                   the SEARCH and this FETCH) rather than a local bug --
                   the store itself refuses to append an empty message
                   body (its own "young mail" sanity check), so there is
                   nothing useful to append here. Skip and retry next
                   run rather than failing the whole folder pass. */
                loudlog("imapsync: WARN: uid %lu in %s fetched a zero-length body -- "
                        "skipping this message, will retry it next run",
                        uid, foldername);
                unlink(tmppath);
                if (first_skipped_uid == 0 || uid < first_skipped_uid) first_skipped_uid = uid;
                ++skipped_count;
                continue;
            }

            apprc = MS_AppendFileToFolderWithId(tmppath, (char *) localpath, id, date64);
            if (apprc != 0) {
                fprintf(stderr, "imapsync: WARN: MS_AppendFileToFolderWithId uid %lu (id %s) in %s: ", uid, id, foldername);
                report_mserr("MS_AppendFileToFolderWithId", apprc);
                loudlog("imapsync: WARN: skipping uid %lu, will retry it next run", uid);
                unlink(tmppath);
                if (first_skipped_uid == 0 || uid < first_skipped_uid) first_skipped_uid = uid;
                ++skipped_count;
                continue;
            }

            vlog("appended uid %lu -> id %s date64 %s", uid, id, date64);

            apprc = apply_flags(localpath, id, rock.flags[i]);
            if (apprc != 0) {
                fprintf(stderr, "imapsync: apply_flags uid %lu (id %s) in %s: ", uid, id, foldername);
                report_mserr("MS_AlterSnapshot", apprc);
                *anyerror = 1;
                /* the message itself is appended and correctly identified;
                   a flags-application miss is logged but not fatal to the
                   run -- it will be caught by the next refresh pass. */
            }

            /* Never advance the persisted watermark past a uid this same
               pass has already skipped: once first_skipped_uid is set,
               every later (necessarily higher, since candidates are
               processed in ascending order) successful uid is still
               appended locally -- that work is not wasted -- but the
               watermark stays capped at (first_skipped_uid - 1) so the
               next run's "UID SEARCH UID <highestuid+1>:*" sees the
               skipped uid again. Re-appending an already-mirrored uid is
               a deterministic no-op (same synthesized id -> the store's
               already-there path), so re-covering this range costs
               nothing but a little wasted fetch/search time. */
            if (uid > state.highestuid && (first_skipped_uid == 0 || uid < first_skipped_uid))
                state.highestuid = uid;
            state.uidvalidity = info.uidvalidity;
            /* Deliberately the PRIOR modseq, not info.highestmodseq: these
               mid-pass writes exist only for crash safety, and persisting
               the current modseq here would make a run that dies before
               the flags-refresh pass below look, to the next run's
               HIGHESTMODSEQ comparison, as though the refresh had already
               happened. Only the end-of-pass write below may advance it. */
            state.highestmodseq = prior_highestmodseq;
            if (write_state(statepath, &state) != 0) {
                fprintf(stderr, "imapsync: could not write %s: %s\n", statepath, strerror(errno));
                *anyerror = 1;
            }
        }
    }

    if (skipped_count > 0) {
        /* Belt-and-suspenders: the gated update above should already
           keep state.highestuid below first_skipped_uid, but cap it
           explicitly here too in case a future edit adds another path
           that advances state.highestuid without going through that
           check. */
        if (state.highestuid >= first_skipped_uid) state.highestuid = first_skipped_uid - 1;
        loudlog("imapsync: skipped %ld message(s) in \"%s\" due to an empty or failed body "
                "fetch/append (lowest skipped uid %lu); the highestuid watermark is capped at "
                "%lu so they will be retried next run", skipped_count, foldername,
                first_skipped_uid, state.highestuid);
    }

    if (uids != NULL) free(uids);

    /* Flags refresh for already-mirrored messages (uids 1..prior_highestuid
       -- freshly-appended messages above that already got their flags
       applied inline, from the same UID FETCH that pulled INTERNALDATE).
       CONDSTORE/HIGHESTMODSEQ skip: this is a modseq COMPARISON only --
       imap_UidFetchMeta() has no CHANGEDSINCE modifier to pass through to
       the FETCH itself, so "nothing changed" is decided purely by
       comparing the last-seen and current HIGHESTMODSEQ, not by asking
       the server to filter the FETCH. */
    if (prior_highestuid == 0) {
        vlog("no previously-mirrored messages in \"%s\"; nothing to refresh", foldername);
    } else if (info.highestmodseq != 0 && prior_highestmodseq == info.highestmodseq) {
        vlog("HIGHESTMODSEQ unchanged (%llu) for \"%s\"; skipping flags refresh",
             info.highestmodseq, foldername);
    } else {
        vlog("refreshing flags for uids 1:%lu in \"%s\" (HIGHESTMODSEQ was %llu, now %llu)",
             prior_highestuid, foldername, prior_highestmodseq, info.highestmodseq);
        rc = refresh_flags(conn, localpath, foldername, info.uidvalidity, 1, prior_highestuid, anyerror);
        if (rc == SF_DEAD) return SF_DEAD;
    }

    if (full_check) {
        rc = check_expunged(conn, localpath, foldername, info.uidvalidity, anyerror);
        if (rc == SF_DEAD) return SF_DEAD;
    }

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
                        const char *root, const char *foldername, int full_check, int *anyerror)
{
    int result;
    int rc;

    result = sync_folder_once(conn, root, foldername, full_check, anyerror);
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
    result = sync_folder_once(conn, root, foldername, full_check, anyerror);
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
    fprintf(stderr, "usage: %s [-root <dir>] [-folders <comma,list>] [-v] [-full-check]\n", prog);
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
    int full_check = 0;
    int i;
    char *foldercopy, *tok, *save;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "-folders") == 0 && i + 1 < argc) {
            folderlist = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            Verbose = 1;
        } else if (strcmp(argv[i], "-full-check") == 0) {
            full_check = 1;
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
        result = sync_folder(conn, login, passwd, rootbuf, tok, full_check, &anyerror);
        if (result != SF_OK) anyerror = 1;
    }
    free(foldercopy);

    memset(passwd, 0, sizeof(passwd));
    MS_UpdateState();
    imap_Close(conn);

    return anyerror ? 1 : 0;
}
