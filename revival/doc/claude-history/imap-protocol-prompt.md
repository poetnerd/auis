# Milestone 3a spec: transport hardening + imap_prot.c

Scope: the first of three M3 sub-deliverables (see ams-IMAP-project.md §8
findings and §10). This one delivers a hardened transport and the real
IMAP protocol layer with its own committed test suite. It does NOT
include any sync/mirror work (`imap_sync.c` is M3b, against the API
defined here) and remains strictly read-only against the mailbox.

Read first: `revival/doc/ams-IMAP-project.md` (§8 for the spike
findings), `src/overhead/mail/lib/imapspike.c` (your own spike — the
protocol layer grows out of it), and the tlscon/netrc sources.

## Language level for new code

New files in this milestone (`imap_prot.c`, `imap_prot.h`, and any new
test drivers) are written in **ANSI C (C89 prototypes)** — full
prototypes in the header, prototype-style definitions in the .c file.
Rationale: every major bug class this revival has catalogued (LP64
truncation via undeclared functions, the fdplumb variadic-ABI
poisoning, the classpp stack-spill) came from call sites the compiler
could not check; new code gets that protection from birth, consistent
with the tree's ongoing ANSI migration. Existing files being *edited*
(tlscon.c etc.) keep their local K&R style, but any NEW declaration
added to an existing header (e.g. `tlscon_ReadLineAlloc`) is declared
with a full prototype.

**The scanf family (scanf/sscanf/fscanf) is banned in the new module.**
The spike demonstrated why: sscanf's return value counts conversions,
not matched literal text, and the tree's `%d`-into-`long` class is the
same disease. Parse numbers with `strtoul`(+endptr check — also exactly
right for 32-bit UIDs in an `unsigned long`, no truncation possible)
and keywords with explicit `strncmp`/`strcasecmp`.

## Part 1 — transport hardening (tlscon)

The spike's key finding: `tlscon_ReadLine`'s fixed ~4KB buffer overflows
on legitimate single-line IMAP responses, and a failed read leaves the
connection wedged with no recovery path.

Fix by addition, not mutation:

* **`extern int tlscon_ReadLineAlloc(struct tlscon *c, char **linep);`**
  — reads a full CRLF-terminated line of any length into a malloc'd
  buffer (caller frees), CRLF stripped. Grows as needed; no fixed cap
  (a sanity limit of ~16MB returning a distinct error is fine). Because
  it always consumes through the CRLF, the "partial line consumed,
  stream desynced" failure mode disappears: any error from this
  function means the connection itself is dead, which the protocol
  layer handles by reconnecting (Part 2).
* Existing functions (`tlscon_Open/_ReadLine/_ReadBytes/_Write/_Close`)
  keep byte-identical behavior — SMTP continues to use them unchanged.
* Both SMTP regression suites run after any tlscon edit:
  `revival/tests/smtp-protocol-tests wdc@fastmail.com` and
  `revival/tools/smtp-send-test wdc@fastmail.com`.

## Part 2 — imap_prot.c / imap_prot.h

New files: `src/overhead/mail/lib/imap_prot.c`, header
`src/overhead/mail/hdrs/imap_prot.h`, installed like tlscon.h/netrc.h.
ANSI C per above. This API is the contract `imap_sync.c` (M3b) will
code against — its shape is the durable deliverable; flag any place you
diverge from the sketch below and why.

### Connection lifecycle

```c
struct imapconn;    /* opaque outside imap_prot.c */

int imap_Open(struct imapconn **connp, const char *host, int port,
              char *errbuf, int errlen);
    /* connect (tlscon), read greeting, CAPABILITY */
int imap_Login(struct imapconn *conn, const char *login,
               const char *passwd);
    /* caller obtains credentials via netrc_Lookup; imap_prot.c never
       reads netrc itself. Redact passwd in any tracing. */
int imap_Capable(struct imapconn *conn, const char *cap);
    /* case-insensitive capability test, e.g. "ESEARCH", "CONDSTORE" */
void imap_Close(struct imapconn *conn);  /* LOGOUT if alive, teardown */
const char *imap_ErrMsg(struct imapconn *conn);  /* last error text */
```

Return codes (define in imap_prot.h): `IMAP_OK`, `IMAP_NO` (server said
NO), `IMAP_BAD` (server said BAD / protocol violation), `IMAP_DEAD`
(connection lost — see reconnect), `IMAP_UIDCHANGED` (see below).

### Reconnect contract (the resync design)

On `IMAP_DEAD`, the caller may call
`int imap_Reopen(struct imapconn *conn, const char *login,
const char *passwd);` — re-runs Open + Login and, if a mailbox was
examined at death, re-EXAMINEs it and verifies UIDVALIDITY matches
(mismatch → `IMAP_UIDCHANGED`; the sync layer treats that as
re-mirror). All API operations are UID-based precisely so a reconnect
never invalidates in-flight work — same idempotency philosophy the 1988
MS layer documented.

### Mailbox operations (read-only in this milestone)

```c
int imap_List(struct imapconn *conn, const char *ref,
              const char *pattern,
              int (*cb)(const char *name, const char *delim,
                        const char *flags, void *rock),
              void *rock);

struct imap_mboxinfo {
    long exists;
    unsigned long uidvalidity, uidnext;
    unsigned long long highestmodseq;   /* 0 if no CONDSTORE */
};
int imap_Examine(struct imapconn *conn, const char *mailbox,
                 struct imap_mboxinfo *out);
    /* EXAMINE only; imap_Select is declared in the header but may
       return a not-implemented error until the writeback milestone. */

int imap_UidSearch(struct imapconn *conn, const char *criteria,
                   unsigned long **uidsp, long *countp);
    /* malloc'd ascending uid array, caller frees. MUST use ESEARCH
       when advertised (Fastmail does); plain SEARCH fallback must
       survive arbitrarily long response lines (Part 1 makes that
       possible). "UID SEARCH ALL" on the full 3,939-message INBOX is
       an acceptance case — the exact case that broke the spike. */
```

### Fetch

```c
struct imap_envelope {
    char *date, *subject, *from, *sender, *replyto,
         *to, *cc, *bcc, *inreplyto, *messageid;
};
void imap_FreeEnvelope(struct imap_envelope *e);

int imap_UidFetchMeta(struct imapconn *conn, const char *uidset,
    int (*cb)(unsigned long uid, const char *flags,
              const char *internaldate,
              const struct imap_envelope *env, void *rock),
    void *rock);
    /* FLAGS INTERNALDATE ENVELOPE for a uidset (e.g. "301:400").
       Batched fetches are how sync will pull metadata — the callback
       fires per message. */

int imap_UidFetchBody(struct imapconn *conn, unsigned long uid,
                      FILE *out, long *sizep);
    /* BODY.PEEK[] streamed literal-to-FILE via tlscon_ReadBytes in
       chunks — never the whole body in memory. Returns byte count in
       *sizep; sync writes message-body files with exactly this. */
```

### Parsing requirements

* Tagged/untagged routing and mid-line literals as in the spike.
* **Close the spike's known stub**: a literal appearing inside ENVELOPE
  (e.g. a subject sent as `{n}` + bytes) must parse correctly — weave
  `tlscon_ReadBytes` into token-level parsing. Test on a canned
  response string, not live (forcing it live would need a mutating
  APPEND).
* No scanf family anywhere in the module (see Language level above).
* Unsolicited untagged updates (EXISTS/EXPUNGE/FETCH) during other
  commands: record EXISTS changes in the conn's mboxinfo, ignore the
  rest for now, but route them without desyncing.

## Part 3 — test suite

`revival/tests/imap-protocol-tests` — same conventions as
smtp-protocol-tests (usage comment, pass/fail lines, nonzero exit).
This suite is live-only except case 8 (it needs the real server); it
must say so when run without credentials and exit cleanly. Read-only
discipline throughout: EXAMINE and BODY.PEEK[] only. Cases at minimum:

1. Open + greeting + CAPABILITY (assert IMAP4rev1 present)
2. Login via netrc (machine imap.fastmail.com)
3. List: INBOX present among results
4. Examine INBOX: exists > 0, uidvalidity != 0; highestmodseq != 0
   (Fastmail has CONDSTORE)
5. **UID SEARCH ALL on full INBOX succeeds** (the spike-breaking case);
   count matches EXAMINE's exists
6. UID SEARCH SUBJECT "STAGE-3-CUI-TEST" finds >= 1; fetch meta for
   one: envelope subject contains the marker
7. Fetch body of that message to a temp file; size matches the
   literal's byte count; file contains the marker
8. Canned-response unit test: ENVELOPE containing a literal parses
   correctly (no live traffic)
9. Reconnect drill: force-close the underlying socket (a test-only
   hook or direct fd close), verify next call returns IMAP_DEAD, and
   imap_Reopen restores a working session with matching UIDVALIDITY

Also update `revival/tests/README.md` with the new suite.

## Gates

* **Gate 1 (STOP and report):** Part 1 done — tlscon_ReadLineAlloc in
  place, both SMTP suites pass, and a spike-style probe shows
  `UID SEARCH ALL` on the full INBOX no longer kills the connection.
* **Gate 2 (STOP and report, end of M3a):** full deliverable — diffs,
  imap_prot.h as designed (with divergences flagged), test suite 9/9,
  SMTP suites still green, fossil status.

## Ground rules

Standard set: no fossil commits; no edits to ~/preferences or ~/.netrc
(read netrc only via netrc_Lookup at runtime); password redacted
everywhere; mailbox strictly read-only; sends (SMTP suites only) go
only to wdc@fastmail.com; no concurrent builds; compile-verify every
touched file with no new warning categories; if something needs
touching outside overhead/mail/{lib,hdrs} and revival/tests, stop and
report instead.
