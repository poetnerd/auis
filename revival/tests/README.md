# revival/tests

Committed, repeatable test scripts for the AUIS revival (as opposed to
`revival/tools`, which holds one-off developer utilities).

## smtp-protocol-tests

`smtp-protocol-tests [RECIPIENT]` -- the milestone 1 SMTP dropoff
module's Gate A protocol suite (see `revival/doc/claude-history/smtp-send-prompt.md`),
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
suite (see `revival/doc/claude-history/imap-protocol-prompt.md`), packaged as a
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

## imap-sync-tests

`imap-sync-tests` -- the milestone 3b `imapsync` one-way IMAP mirror's
full acceptance suite (see `revival/doc/claude-history/imap-sync-prompt.md`), packaged
as a durable regression test. It builds `imaptest.test` (used only as
an independent, cross-checking client -- never to drive imapsync
itself), `imapsync` (`src/ams/msclients/imapsync`), and
`smtptest.test`, then exercises all six spec cases against a fresh
`mktemp -d` scratch mspath root, INBOX only:

1. Fresh mirror: message count matches EXAMINE's EXISTS (checked
   independently via `imaptest.test examine`, never hardcoded); a
   message with subject/body containing `STAGE-3-CUI-TEST` (left by
   `smtp-protocol-tests`/`smtp-send-test`'s live sends) is spot-checked
   for a present caption, a readable body file, and an id matching the
   documented synthesized format (`IMAP` + 14 base32hex characters --
   see the case-insensitive-filesystem note below).
2. Idempotency: an immediate second run must not alter or remove any
   already-mirrored message, and any local growth must be exactly
   accounted for by genuinely new server uids that arrived in the same
   window (checked via an independent EXAMINE on each side of the
   re-run) -- a live mailbox can grow between case 1 and this re-run,
   and that is new mail, not an idempotency violation.
3. Incremental sync: sends one marker message to self via the committed
   SMTP path (`smtptest.test`, `wdc@fastmail.com` only), then re-runs
   imapsync with bounded polling (tolerant of delivery delay) until
   exactly one new message appears and it is the marker.
4. Flags mapping: the newly-arrived marker has `AMS_ATT_UNSEEN` set
   locally; an old, server-seen message does not (checked by decoding
   `.MS_MsgDir` snapshot bytes directly, independent of imapsync's own
   code).
5. UIDVALIDITY change drill: corrupts the state file's `uidvalidity`,
   re-runs, and checks for a loud full re-mirror and a correct final
   state. A single message's body fetch or append can legitimately fail
   mid-drill (e.g. a message expunged server-side between SEARCH and
   FETCH); imapsync treats that as a per-message warn-and-skip rather
   than a whole-folder failure, so this case still expects exit 0.
6. cui browse: with mspath extended to the scratch root via a scratch
   `PROFILES` file (never the real `~/preferences`), cui lists INBOX and
   displays the marker's caption and body. Uses `headers INBOX since
   <date>` (bracketing the marker's own `Date:` header, not a fixed
   value) rather than a bare `headers INBOX`, since cui numbers messages
   by session-local display order, not mailbox position -- a bare
   `headers` would bury the marker's number thousands of messages deep
   in a live INBOX this size.

Because case 1 mirrors the *entire* live INBOX and case 5 re-mirrors it
again from scratch, this suite can take many minutes to run. Skips
cleanly (rather than failing) when `~/.netrc` has no usable `machine
imap.fastmail.com` stanza. The real `~/.IMAP/fastmail` mirror root is
never touched -- only the scratch root, removed on exit unless
`IMAPSYNC_KEEP_SCRATCH` is set.

Note for anyone re-running this on a case-insensitive filesystem (macOS
default APFS, as used for this revival): `imapsync`'s synthesized ids
use a fixed 32-character alphabet with no lowercase letters at all
specifically so that two distinct ids can never collide as the same
filename after case-folding -- a real bug found and fixed during this
milestone's Gate 1 testing (see `imap_sync.c`'s `synth_id()` comment).

```
revival/tests/imap-sync-tests
```

## imap-writeback-tests

`imap-writeback-tests` -- milestone 4's Gate 1 capture/suppression
suite for `msjournal.c` and its four hook sites in `src/ams/libs/ms/`
(`altsnap.c`, `purge.c`, `clonemsg.c`, `apndfile.c`). See
`revival/doc/imap-writeback-prompt.md`. Gate 1 scope only: local
capture into a per-folder `.MS_Journal` file and the
`MSJournal_Suppress(1)` `imapsync` sets at startup -- no IMAP write
code exists anywhere in the tree yet, so this suite's only server
traffic is `imapsync`'s own read-only EXAMINE/UID FETCH/UID SEARCH,
exactly as in `imap-sync-tests`. Every mutation under test (delete,
undelete, purge) is a pure local store operation, driven through the
real, unmodified `cui` binary against a scratch-mirrored copy of a
small, pre-existing real folder (`INBOX/1-Admin/keys`, chosen for its
low message count so the whole suite runs in seconds rather than the
many minutes a full INBOX mirror takes) -- never INBOX, and the
server-side copy of that folder is never modified (imapsync issues no
write commands in this milestone).

Case 1 (the only case at Gate 1):

1. Fresh mirror of the small folder into a `mktemp -d` scratch root
   (never the real `~/.IMAP`), confirming `.MS_Journal` does not exist
   right after -- imapsync's own append writes must never journal.
2. A scripted `cui` session (scratch `PROFILES`, never the real
   `~/preferences`) attempts `type 1` (mark-read; a no-op here since
   this real, long-read mailbox already has the message `\Seen`
   server-side -- proving the hook adds no spurious record when
   nothing actually changes), then `delete 2` (the `ASS_OR_ATTRIBUTES`
   path), `undelete 2` (the `ASS_AND_ATTRIBUTES` path), `delete 2`
   again, and `purge`. `.MS_Journal` is asserted to contain exactly the
   four expected lines, byte for byte, independently recomputed in
   Python from the documented OR/AND-mask grammar (not by re-trusting
   `msjournal.c`'s own encoding).
3. Suppression, two ways: an ordinary immediate re-run of `imapsync`
   (which may legitimately skip its own flags-refresh pass via the
   HIGHESTMODSEQ shortcut -- a weak proof by itself), and then a
   *forced* re-run with the scratch `.MS_IMAPSync` state file's
   `highestmodseq` deliberately invalidated, so the flags-refresh pass
   genuinely re-applies real `MS_AlterSnapshot` calls from real FETCH
   data against every already-mirrored message -- confirmed via the
   `-v` log's "refreshing flags for uids" line -- and `.MS_Journal`
   must still come out byte-identical afterward.

Skips cleanly (rather than failing) when `~/.netrc` has no usable
`machine imap.fastmail.com` stanza. Leaves its scratch root in place on
exit (path printed) for inspection.

```
revival/tests/imap-writeback-tests
```
