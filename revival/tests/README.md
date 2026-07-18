# revival/tests

Committed, repeatable test scripts for the AUIS revival (as opposed to
`revival/tools`, which holds one-off developer utilities).

## smtp-protocol-tests

`smtp-protocol-tests [RECIPIENT]` -- the milestone 1 SMTP dropoff
module's Gate A protocol suite (see `revival/doc/smtp-send-prompt.md`),
packaged as a durable regression test. It builds `smtptest.test` and
`dropoff.test` from `src/overhead/mail/lib`, generates its whole
message/PROFILES/netrc corpus fresh into a temp directory on each run,
and exercises the seven Gate A cases: simple send, dot-stuffing,
LF-only input, two recipients, a bad recipient, a bad password, and
the legacy sendmail path when `smtphost` is unset.

Run with a recipient address to exercise the five live-send cases
against the real `smtp.fastmail.com` (mail goes only to that address);
run with no argument to run just the two deterministic, offline cases
(bad password, `smtphost` unset). Never touches `~/preferences` or the
real `~/.netrc`'s contents; never prints a password.

```
revival/tests/smtp-protocol-tests wdc@fastmail.com
```

For the real end-to-end acceptance counterpart -- driving the actual
`cui` UI through mail composition and submission, rather than the SMTP
module directly -- see `revival/tools/smtp-send-test`.

## imap-protocol-tests

`imap-protocol-tests` -- the milestone 3a IMAP protocol module's Gate 2
suite (see `revival/doc/imap-protocol-prompt.md`), packaged as a
durable regression test. It builds `imaptest.test` from
`src/overhead/mail/lib` (a standalone ANSI C driver exercising
`imap_prot.c`'s API) and exercises nine cases: open + greeting +
CAPABILITY, login via netrc, LIST (INBOX present), EXAMINE INBOX
(exists/uidvalidity/highestmodseq), `UID SEARCH ALL` on the full
INBOX (the exact oversized-response case that broke the milestone 2
spike before `tlscon_ReadLineAlloc`), `UID SEARCH SUBJECT` + fetch
metadata, fetch body (streamed to a file, size and content verified),
a canned-response unit test proving a literal appearing inside
ENVELOPE parses correctly with no live traffic, and a reconnect drill
(force-close -> `IMAP_DEAD` -> `imap_Reopen` restores a working
session with a verified matching UIDVALIDITY).

Live cases (all but the canned-response case) run automatically
whenever `~/.netrc` has a usable `machine imap.fastmail.com` stanza;
otherwise the suite runs only the offline canned-response case and
says so cleanly rather than failing. All numeric assertions are
compared against what the server reports at runtime (e.g. EXAMINE's
own EXISTS count) -- never a hardcoded mailbox size, since the real
mailbox's message count drifts with ordinary use. The subject-search
and fetch cases depend on a message already present in INBOX with a
subject containing `STAGE-3-CUI-TEST` -- left there by
`smtp-protocol-tests`/`smtp-send-test`'s own live sends -- and skip
cleanly (rather than failing) if it isn't found.

Strictly read-only against the mailbox throughout: only `EXAMINE` and
`BODY.PEEK[]` ever reach the wire; no `STORE`/`APPEND`/`CREATE`/
`DELETE`/`EXPUNGE` anywhere in `imap_prot.c`.

```
revival/tests/imap-protocol-tests
```
