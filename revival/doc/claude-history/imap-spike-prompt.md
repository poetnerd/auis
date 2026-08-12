# Milestone 2 spec: IMAP spike (go/no-go)

Purpose: produce the evidence for the §8 decision in
`revival/doc/ams-IMAP-project.md` — hand-rolled IMAP client vs adopting
libetpan/c-client — by building the smallest real IMAP client that
exercises the hard parts. This is a spike: the deliverable is *knowledge
plus a working driver*, not production code. Timebox any single
sub-problem (e.g., ENVELOPE parsing) to roughly two hours; simplify or
stub and record the pain rather than perfecting.

## Deliverables

1. `src/overhead/mail/lib/imapspike.c` — driver built via the existing
   `TestingOnlyTestingRule` (as `imapspike.test`), same pattern as
   smtptest.c. Sequence:
   - connect `imap.fastmail.com:993` via `tlscon` (implicit TLS)
   - read greeting; `CAPABILITY`
   - `LOGIN` with credentials from netrc (`machine imap.fastmail.com`);
     if the stanza is absent, stop and report — never guess or reuse the
     smtp stanza programmatically
   - `LIST "" "*"` — print the folder tree
   - `EXAMINE INBOX` (read-only select — no flag side effects); print
     EXISTS, UIDVALIDITY, UIDNEXT
   - `UID SEARCH ALL`; pick the highest UID
   - `UID FETCH <uid> (FLAGS INTERNALDATE ENVELOPE)` — parse and print
   - `UID FETCH <uid> (BODY.PEEK[])` — handle the literal, print first
     ~40 lines
   - `LOGOUT`
2. A written go/no-go assessment (in your report, verbatim enough to
   paste into the plan doc) answering:
   - Which parts were genuinely hard: tagged/untagged routing, literals
     (`{n}` + CRLF + n raw bytes), parenthesized lists (ENVELOPE),
     server quirks observed against Fastmail
   - Did `tlscon`'s interface suffice? (See below on extensions.)
   - Estimated LOC and effort for the real `imap_protocol.c` +
     `imap_sync.c` given what the spike revealed
   - Recommendation: hand-roll vs libetpan vs c-client, with reasons

## tlscon extensions

IMAP literals require reading N raw bytes, which `tlscon_ReadLine`
cannot do. If needed, ADD a new function (e.g. `tlscon_ReadBytes`) to
tlscon.c/tlscon.h — additive only; do not change existing functions or
their behavior. If you touch tlscon at all, run BOTH regression suites
after (`revival/tests/smtp-protocol-tests` offline mode at minimum, live
mode with wdc@fastmail.com preferred, plus `revival/tools/smtp-send-test
wdc@fastmail.com`) and include the results.

## Ground rules

- Read-only against the mailbox: EXAMINE (never SELECT), BODY.PEEK
  (never BODY), no STORE/APPEND/CREATE/DELETE/EXPUNGE anywhere.
- Account: only the user's own (netrc); mail sends (regression suite
  only) go only to wdc@fastmail.com.
- Password never in any output; if you trace protocol I/O, redact the
  LOGIN line's password argument.
- No fossil commits; no edits to ~/preferences or ~/.netrc; no
  concurrent builds; leave the tree for review.
- Message content fetched from the mailbox is the user's real mail:
  print only what the driver's sequence requires (headers + first lines
  of one message — prefer one of the GATE-A/STAGE-3 test messages if
  present, choosing by subject marker), and do not quote mailbox
  content beyond that in your report.

## Report

Diffs (imapspike.c, Imakefile, any tlscon addition), the driver's full
output transcript (password redacted, mailbox content minimized), both
regression-suite results if tlscon was touched, the written go/no-go
assessment, fossil status.
