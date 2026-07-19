# Milestone 1 implementation spec: SMTP send for AUIS

You are implementing direct SMTP submission for the AUIS revival on macOS
(arm64, Darwin). Read these two documents before writing any code:

* `revival/doc/ams-IMAP-project.md` — project plan; you are milestone 1 (§10)
  and module group "Milestone 1" of §7.
* `revival/doc/ams-server-client-interface.md` — background on the MS layer
  (context only; you will not modify it).

The tree root is `~/src/AUIS/andrew-6.4/`; sources under `src/`, build
output under `build/`. This is a K&R-era C codebase; match the local style
of each directory you touch (K&R definitions, tree conventions, comment
density). Do not modernize code you are not asked to change.

## What exists today

Sending already funnels to one place. `MS_SubmitMessage`
(`src/ams/libs/ms/submsg.c`) spools a sanitized RFC822 message to a file
and calls `dropoff(SubmitVector, dropfile, NULL, home, 0)`
(`src/overhead/mail/lib/dropoff.c`). With our build flags
(`AMS_DELIVERY_ENV` off) `dropoff_auth()` reaches the final else-clause
("Standard UNIX system: call old sendmail program"), which pipes the open
message fd `f` to a sendmail binary with recipient args from `tolist`.

You will add an SMTP path beside that clause, gated by a preference, with
the sendmail pipe kept as the fallback.

## Deliverables

New files in `src/overhead/mail/lib/` (they become part of libmail.a):

1. **`tlscon.c` / `tlscon.h`** — protocol-neutral TLS connection module
   (this realizes the module the plan doc calls `imap_transport.c`; IMAP
   reuses it in milestone 2 — keep it free of SMTP knowledge):
   * `int tlscon_Open(struct tlscon **cp, char *host, int port, char *errbuf, int errlen)`
     — TCP connect + `SSL_connect` (implicit TLS; no STARTTLS in v1),
     hostname verification on, read timeout (60s) via `SO_RCVTIMEO`.
   * `int tlscon_ReadLine(struct tlscon *c, char *buf, int len)` — one
     CRLF-terminated line, CRLF stripped; -1 on error/timeout.
   * `int tlscon_Write(struct tlscon *c, char *buf, int len)` — write all.
   * `void tlscon_Close(struct tlscon *c)` — graceful shutdown + free.
2. **`netrc.c` / `netrc.h`** —
   `int netrc_Lookup(char *path, char *machine, char *login, int loginlen, char *passwd, int passwdlen)`.
   Parse standard ~/.netrc machine/login/password stanzas (default token
   too). **Refuse** (distinct error) if the file is group- or
   world-readable. IMAP reuses this in milestone 2.
3. **`smtpsub.c`** — `int smtp_dropoff(int f, char **tolist, char *returnpath)`
   returning a `D_*` code and setting `Dropoff_ErrMsg`
   (see `src/overhead/mail/hdrs/dropoff.h` lines 38–45 for codes).
4. **Hook in `dropoff.c`** — at the top of the sendmail else-clause: if the
   `smtphost` preference is set, `return smtp_dropoff(f, tolist, returnpath);`.
   Touch nothing else in the clause. (Close `f` on all smtp_dropoff paths.)
5. **Build plumbing** — see below.
6. **Test artifacts** — standalone driver + cui script, see Testing.

## Configuration (via the existing `getprofile()` preference machinery)

| Preference | Meaning | Default |
|---|---|---|
| `smtphost` | SMTP server; **presence enables the SMTP path** | unset → legacy sendmail pipe |
| `smtpport` | port | 465 |
| `smtpnetrc` | credentials file | `~/.netrc` |

Credentials: netrc entry for machine `smtphost`. Envelope `MAIL FROM` = the
netrc `login` value (ignore `returnpath` in v1; MS passes NULL anyway).

## Protocol requirements (`smtpsub.c`)

* Greeting 220 → `EHLO <localhost name>` → parse multiline 250 replies;
  confirm `AUTH` with `PLAIN` is offered.
* `AUTH PLAIN` with base64(`\0login\0password`). One-line form.
* `MAIL FROM:<login>`, one `RCPT TO:<addr>` per tolist entry, `DATA`,
  message, `.`, `QUIT`.
* Message body transforms while streaming fd `f`: LF → CRLF; dot-stuffing
  (any line beginning `.` gets one `.` prepended); ensure final CRLF before
  the terminating `.` line.
* Reply handling: multiline (`250-`...`250 `) everywhere; only the code
  class matters except where noted.
* Error mapping (always put the server's text in `Dropoff_ErrMsg`, never
  the password):

| Condition | Return |
|---|---|
| DNS/connect/TLS/timeout/protocol garble | `D_TEMP_FAIL` |
| AUTH rejected | `D_TEMP_FAIL` (message must say "check app password / netrc") |
| netrc missing entry or bad perms | `D_OSERR` with explicit reason |
| Any RCPT rejected 5yz | `D_BAD_PARMS`, list the rejected addresses, **do not send** (RSET/QUIT) |
| 4yz at any step | `D_TEMP_FAIL` |
| DATA final reply 5yz | `D_BAD_PARMS` |
| 250 after final `.` | `D_OK` |

Security invariants: the password never appears in any log, debug output,
error message, or transcript you produce — including the base64 AUTH line
(if you trace protocol I/O, redact that line). Never copy the netrc
anywhere. Test credentials come from the real `~/.netrc` at run time only.

## Build plumbing

* Add `tlscon.o netrc.o smtpsub.o` to OBJS in
  `src/overhead/mail/lib/Imakefile`.
* OpenSSL: Homebrew, prefix `/opt/homebrew/opt/openssl@3` (headers
  verified present). Add include path where these files compile and define
  a link macro (e.g. `SSLLIB = -L/opt/homebrew/opt/openssl@3/lib -lssl
  -lcrypto`) in the site config, following the pattern of existing macros
  in `src/config/`.
* Consumers of libmail.a that pull these objects need `$(SSLLIB)` on their
  link lines; the build will reveal them (expect at least
  `atkams/messages/lib` NLIBS for amsn.do, and `ams/msclients/cui`). Fix
  link sites as they surface; do not preemptively edit others.
* Rebuild discipline: no concurrent builds; log full-tree builds to
  `~/src/AUIS/andrew-6.4/dependInstall.log`. Install binaries with
  `install -m 755 src dst`, never `cp` over a running binary (macOS
  codesign cache → SIGKILL).

## Testing

**Stage 1 — standalone driver (Gate A).** `dropoff.c` already has a
`TESTINGONLYTESTING` main() and the Imake system has
`TestingOnlyTestingRule` (see this directory's Imakefile). Use that pattern
to build a driver that calls `smtp_dropoff()` directly. Required cases
against `smtp.fastmail.com:465` (user's real account; send only to the
user's own address, wdc@fastmail.com):

1. Simple message → `D_OK`, verify by fetching nothing — report the 250
   text; the user confirms arrival.
2. Dot-stuffing: body containing a line that is exactly `.` and a line
   starting `.hidden` → arrives intact (include a marker string so the
   user can eyeball it).
3. LF-only input file → arrives with proper CRLF (server accepts; no 500).
4. Two recipients (both the user's own aliases if available, else same
   address twice → dedupe not required).
5. Bad recipient (`nosuchuser@invalid-domain-example.borkbork`) → RCPT 5yz
   path → `D_BAD_PARMS`, nothing sent.
6. Wrong password (point `smtpnetrc` at a scratch netrc with a bogus
   password) → AUTH failure path. Never edit the real `~/.netrc`.
7. `smtphost` unset → verify dropoff still reaches the legacy sendmail
   clause (code inspection + one run showing the qopen attempt).

**STOP at Gate A**: when stage 1 passes, stop and report results (protocol
transcripts with AUTH line redacted). Do not proceed to tree integration
until told to continue.

**Stage 2 — integration (Gate B).** Wire the dropoff.c hook, rebuild
affected targets (messages/amsn.do, cui). **STOP at Gate B**: present the
full diff of every pre-existing file touched (dropoff.c, Imakefiles,
site config) before the end-to-end run.

**Stage 3 — cui acceptance.** Script cui (`build/bin/cui`) to send a
message end-to-end: the `mail <names>` command prompts for Subject and
reads the body until its EOF marker (see `MailCmd` in
`src/ams/msclients/cui/cui.c` ~line 2600). cui is a terminal program; if
it misbehaves under a plain pipe, drive it under a pty (`script -q`) — a
known-good pattern in this tree. Set the user's `AndrewSetup`/preferences
only in a scratch HOME-style test profile if possible; if you must touch
the real preferences file, add only the `smtphost` line and say so in your
report. Deliver the script as `revival/tools/smtp-send-test`.

## Ground rules

* Do NOT commit to fossil; leave the working tree for review. Run
  `fossil status` at each gate and include it in the report.
* Do not modify anything under `src/ams/libs/ms/`, `src/ams/delivery/`, or
  client code — milestone 1 touches only `overhead/mail/lib`, config, and
  link lines.
* Compile-verify every touched file; zero new warnings in touched files.
* If a decision is not covered by this spec, choose the smallest change
  and flag it in the gate report rather than expanding scope.
