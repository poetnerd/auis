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
	imap_prot.h -- IMAP protocol layer (milestone 3a).  Grown out of
		      the milestone 2 spike (imapspike.c, left untouched
		      as the committed spike artifact) on top of the
		      milestone 3a-hardened tlscon (tlscon_ReadLineAlloc).

	ANSI C (C89 prototypes): this is the tree's first born-ANSI
	module.  Every declaration here is a full prototype; imap_prot.c
	is written in prototype style throughout.  No scanf/sscanf/fscanf
	is used anywhere in the implementation -- numbers are parsed with
	strtoul (+ endptr check), keywords with strncmp/strcasecmp.  See
	revival/doc/imap-protocol-prompt.md for the rationale (the
	milestone 2 spike's sscanf-return-value bug is the case study).

	This is the API imap_sync.c (milestone 3b) codes against; its
	shape is the durable deliverable of this milestone.  Divergences
	from the spec's Part 2 sketch are called out inline below with
	"DIVERGENCE:" and are also summarized in the milestone's Gate 2
	report.

	Strictly read-only against the mailbox in this milestone: EXAMINE
	only (never SELECT -- imap_Select is declared for API-shape
	stability but is not implemented until the writeback milestone),
	BODY.PEEK[] only, no STORE/APPEND/CREATE/DELETE/EXPUNGE anywhere.

	Optional protocol trace: if the environment variable IMAP_TRACE
	is set, every line sent/received is written to stderr as "C: "/
	"S: " (mirroring smtpsub.c's AMS_SMTP_TRACE), with the LOGIN
	command's password argument always redacted regardless.
*/

#include <stdio.h>	/* FILE * */
#include <stddef.h>	/* size_t */

struct imapconn;	/* opaque outside imap_prot.c */

/* Return codes.  Most functions below return one of these; IMAP_OK is
   zero so "if (imap_Whatever(...))" tests for any non-success. */
#define IMAP_OK		0	/* tagged OK */
#define IMAP_NO		1	/* tagged NO -- server declined the command */
#define IMAP_BAD	2	/* tagged BAD, or a protocol/parse violation */
#define IMAP_DEAD	3	/* connection lost; see imap_Reopen */
#define IMAP_UIDCHANGED	4	/* imap_Reopen only: reconnected, but the
				   examined mailbox's UIDVALIDITY changed --
				   the sync layer should treat this as a
				   re-mirror trigger, not a plain retry */

/* ---- Connection lifecycle ---- */

/* Connects via tlscon (implicit TLS), reads the greeting, and issues
   CAPABILITY. On success *connp is a fresh connection and 0
   (IMAP_OK) is returned. On failure a printable reason is left in
   errbuf (imap_ErrMsg is also usable once *connp exists, but a
   connect-level failure may occur before *connp can be set, hence the
   separate errbuf here -- matching tlscon_Open's own convention). */
int imap_Open(struct imapconn **connp, const char *host, int port,
              char *errbuf, int errlen);

/* Sends LOGIN with the given credentials. The caller obtains them via
   netrc_Lookup(); imap_prot.c never reads netrc itself. The password
   is redacted in any trace output (see IMAP_TRACE above); imap_Login
   also scrubs its own copies of the command line and password from
   memory before returning. Captures the server's post-LOGIN
   CAPABILITY response code, if present, replacing the pre-auth
   CAPABILITY list (post-auth capabilities can legitimately differ --
   observed live against Fastmail). */
int imap_Login(struct imapconn *conn, const char *login,
               const char *passwd);

/* Case-insensitive, whole-token capability test against the most
   recently captured CAPABILITY list (e.g. imap_Capable(conn,
   "ESEARCH")). Returns nonzero (true) if present, 0 if absent -- not
   one of the IMAP_* codes above, since this never talks to the
   server. */
int imap_Capable(struct imapconn *conn, const char *cap);

/* Sends LOGOUT if the connection is alive, then tears down the tlscon
   and frees conn. Safe to call on an already-dead connection (no
   LOGOUT is sent in that case). conn must not be used afterward. */
void imap_Close(struct imapconn *conn);

/* Returns a pointer to the connection's last error text (valid until
   the next imap_prot.c call on the same conn, or until imap_Close).
   Never NULL; an unused connection returns an empty string. */
const char *imap_ErrMsg(struct imapconn *conn);

/* DIVERGENCE from the sketch (additive, test-support only): forces
   the connection into the same state a dropped network link would --
   tears down the underlying tlscon and marks conn dead -- without
   touching tlscon.c/.h further (Part 1 of this milestone added only
   tlscon_ReadLineAlloc). This is the "test-only hook" the spec's
   Part 3 case 9 anticipates as an alternative to a direct fd close;
   it lives here rather than in tlscon because struct imapconn (unlike
   struct tlscon) is this milestone's own opaque type, so no existing
   module needed touching to provide it. Every other imap_prot.c call
   already checks connection liveness first and returns IMAP_DEAD, so
   this is the only extra surface needed for the reconnect drill. */
int imap_TestForceClose(struct imapconn *conn);

/* ---- Reconnect (the resync design) ---- */

/* On IMAP_DEAD, the caller may call this to re-run Open + Login on
   the same conn (same host/port, remembered from imap_Open). If a
   mailbox was under EXAMINE at the time of death, it is re-EXAMINEd
   and the new UIDVALIDITY is compared against the one recorded at the
   original EXAMINE: a mismatch returns IMAP_UIDCHANGED (the
   connection itself is fine and usable -- this is a signal for the
   sync layer, not a failure). All API operations are UID-based
   precisely so a reconnect never invalidates in-flight work, the same
   idempotency philosophy the 1988 MS layer documented. */
int imap_Reopen(struct imapconn *conn, const char *login,
                 const char *passwd);

/* ---- Mailbox operations (read-only in this milestone) ---- */

int imap_List(struct imapconn *conn, const char *ref,
              const char *pattern,
              int (*cb)(const char *name, const char *delim,
                        const char *flags, void *rock),
              void *rock);

struct imap_mboxinfo {
    long exists;
    unsigned long uidvalidity, uidnext;
    unsigned long long highestmodseq;	/* 0 if the server has no CONDSTORE */
};

/* EXAMINE only (read-only select; never SELECT). *out is filled in on
   success. Unsolicited "* n EXISTS" updates seen during any later
   command on this conn update conn's own copy of mboxinfo.exists in
   place (see the Parsing requirements note in the spec) -- callers
   that want the freshest count can re-EXAMINE, but simple drift
   tracking happens automatically. */
int imap_Examine(struct imapconn *conn, const char *mailbox,
                  struct imap_mboxinfo *out);

/* DIVERGENCE from the sketch, exactly as the spec pre-authorizes:
   "imap_Select is declared in the header but may return a
   not-implemented error until the writeback milestone." Declared now
   so imap_sync.c's API shape is stable; calling it in M3a always
   returns IMAP_BAD with imap_ErrMsg() explaining why. */
int imap_Select(struct imapconn *conn, const char *mailbox,
                 struct imap_mboxinfo *out);

/* *uidsp is a malloc'd ascending array of *countp UIDs; caller frees
   *uidsp. Uses "UID SEARCH RETURN (ALL) <criteria>" (ESEARCH, RFC
   4731) when imap_Capable(conn, "ESEARCH") -- Fastmail advertises it
   -- decoding its compressed sequence-set (e.g. "1:5,7,9:3939") into
   individual UIDs; falls back to plain "UID SEARCH <criteria>"
   (space-separated UID list on one potentially very long line, which
   Part 1's tlscon_ReadLineAlloc is what makes survivable) otherwise.
   "UID SEARCH ALL" on the full INBOX -- the exact case that broke the
   milestone 2 spike -- is this module's primary acceptance case (see
   revival/tests/imap-protocol-tests case 5). *countp is 0 and *uidsp
   is NULL (not malloc'd) if nothing matched. */
int imap_UidSearch(struct imapconn *conn, const char *criteria,
                    unsigned long **uidsp, long *countp);

/* ---- Fetch ---- */

struct imap_envelope {
    char *date, *subject, *from, *sender, *replyto,
         *to, *cc, *bcc, *inreplyto, *messageid;
};

/* Frees every non-NULL field of *e and zeroes them; does not free e
   itself (matching struct imap_envelope being stack-allocated at
   every call site in this module). */
void imap_FreeEnvelope(struct imap_envelope *e);

/* Issues "UID FETCH <uidset> (FLAGS INTERNALDATE ENVELOPE)" (uidset
   e.g. "301:400" or a single UID) and invokes cb once per untagged
   FETCH response with that message's UID (always present -- UID
   FETCH's server-side contract guarantees it even though it wasn't
   explicitly requested as a data item), FLAGS text (the parenthesized
   list's contents, unparsed), INTERNALDATE text, and a fully-parsed
   ENVELOPE. A literal appearing inside ENVELOPE (e.g. a subject sent
   as "{n}" + raw bytes rather than a quoted string) is handled
   transparently by the tokenizer -- closing the spike's known stub.
   *env and its fields are only valid for the duration of the
   callback; imap_prot.c frees them via imap_FreeEnvelope immediately
   after cb returns. Unsolicited EXISTS/EXPUNGE/FETCH lines seen while
   awaiting this command's tagged completion are routed per the
   Parsing requirements note (EXISTS updates conn's mboxinfo; the rest
   are consumed without desyncing the stream but otherwise ignored).
   Batched fetches (a wide uidset) are how imap_sync.c is expected to
   pull metadata -- the callback fires per message rather than
   building a list in memory. */
int imap_UidFetchMeta(struct imapconn *conn, const char *uidset,
    int (*cb)(unsigned long uid, const char *flags,
              const char *internaldate,
              const struct imap_envelope *env, void *rock),
    void *rock);

/* Issues "UID FETCH <uid> (BODY.PEEK[])" and streams the literal
   directly to out via tlscon_ReadBytes in bounded chunks -- the whole
   body is never resident in memory at once, regardless of message
   size. *sizep receives the literal's byte count (which is exactly
   what was written to out) on success. */
int imap_UidFetchBody(struct imapconn *conn, unsigned long uid,
                       FILE *out, long *sizep);

/* DIVERGENCE from the sketch (additive, test-support only): parses a
   single canned ENVELOPE response -- bytes exactly as they would
   appear on the wire, including a real embedded "{n}\r\n" + n-byte
   literal if desired -- with no network connection at all, driving
   the identical literal-aware tokenizer imap_UidFetchMeta uses. This
   is how revival/tests/imap-protocol-tests case 8 (a literal inside
   ENVELOPE, e.g. a subject sent as a literal rather than a quoted
   string -- the milestone 2 spike's known stub) is tested: the spec
   calls for testing this "on a canned response string, not live",
   and imap_prot.c has no other seam for injecting canned bytes since
   struct tlscon only ever wraps a real TLS socket. `canned` must
   begin with ENVELOPE's own opening "(" (i.e. omit any leading
   "* n FETCH (... ENVELOPE " prefix); `cannedlen` is its exact byte
   length (the buffer need not be NUL-terminated). Returns 0 on
   success (*out is filled in; caller frees it via imap_FreeEnvelope),
   -1 on a parse error. */
int imap_TestParseEnvelope(const char *canned, size_t cannedlen,
                            struct imap_envelope *out);
