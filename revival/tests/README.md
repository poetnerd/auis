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

Cases 1-9 are strictly read-only against the mailbox: only `EXAMINE`
and `BODY.PEEK[]` ever reach the wire.

Cases 10-14 (added for the writeback milestone's Gate 2) exercise the
new write entry points -- `imap_Create`/`imap_UidStoreFlags`/
`imap_UidExpunge`/`imap_Append` -- via `imaptest.test`'s
`create`/`storeflags`/`expunge`/`append` subcommands: create (or
confirm) a dedicated mailbox `Revival/WritebackTest`, append a test
message into it (verifying the `APPENDUID` response code), a flags
round-trip (`+FLAGS.SILENT`/`-FLAGS.SILENT`, each verified via a real
FETCH), and a `UID EXPUNGE` round-trip (verified gone via a real
FETCH). Case 14 proves the mailbox guard itself: all four write
subcommands refuse, at the C level (`imaptest.c`'s `guard_mailbox()`),
to run against `INBOX` -- nonzero exit, no wire traffic, regardless of
what a caller passes. This suite's own Python driver carries an
independent copy of that same assertion (`imaptest_write()`), so the
"only touch the test mailbox" rule has two enforcement points, not
one. INBOX itself is never written to anywhere in this suite.

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
`mktemp -d` scratch mspath root, INBOX only (cases W2/W3/W4/W7, added
for the writeback milestone's Gate 2, follow further down against a
second, separate scratch root and mailbox):

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

Cases W2/W3/W4/W7 (added for the writeback milestone's Gate 2) exercise
`imap_sync.c`'s `replay_folder()` -- flags replay, purge replay, the
25-purge safety valve, and the stale-UIDVALIDITY skip -- against their
own fresh scratch root and mailbox, entirely separate from cases 1-6's
INBOX mirror above (own tempdir, own `imapsync` run, no dependency on
the long INBOX pass having completed first):

* **W2 flags round-trip**: appends a test message into
  `Revival/WritebackTest` via `imap_Append`, mirrors it, fabricates a
  `J1 flags` journal line forcing `AMS_ATT_UNSEEN` off (the
  `ASS_AND_ATTRIBUTES` mask shape `CUI_MarkAsRead` itself would
  produce), replays, and verifies `\Seen` landed server-side via a real
  `imap_prot` FLAGS fetch -- then confirms the journal is fully
  consumed (deleted, cursor zeroed) and a second replay run is a no-op.
* **W3 purge round-trip**: same setup, but fabricates a `J1 purge`
  line and first unlinks the local body file directly (simulating what
  `MS_PurgeDeletedMessages` itself would already have done -- replay
  only ever pushes the removal to the server, it never touches local
  body files, so the local half of "purge locally" has to be done
  explicitly here for a meaningful check). Verifies the uid is gone
  server-side (a real FETCH finds nothing) and that the local mirror's
  message count matches the server's `EXISTS` after the same run's
  mirror pass.
* **W4 safety valve**: fabricates a journal with 26 `J1 purge` lines
  (well past the 25-purge limit) and confirms `imapsync` refuses to
  replay any of it without `-force-purges` (journal left untouched,
  loud log line present, nonzero exit) -- then confirms `-force-purges`
  overrides the refusal and clears it (all 26 fabricated uids are
  harmless no-ops server-side, since none of them actually exist).
* **W7 stale-journal drill**: fabricates a `J1 purge` line whose id
  encodes a UIDVALIDITY one more than the folder's real one, and
  confirms it is skipped with the documented loud log line, the journal
  is still fully consumed (cursor advances past the bad line rather
  than getting stuck on it), and a real, correctly-mirrored message
  elsewhere in the folder is left untouched.

These four cases fabricate well-formed journal lines directly
(following `msjournal.c`'s documented `"J1 ..."` grammar exactly) rather
than driving a real `cui` session to produce them -- `imap-writeback-tests`
(below) already proves real capture end-to-end, so these cases test
replay in isolation, faster and more deterministically. ALL destructive
traffic in these four cases -- `STORE`/`EXPUNGE`/`APPEND`/`CREATE`, and
every `imapsync` invocation that might replay a journal -- is confined
to `Revival/WritebackTest`, created if absent; `INBOX` is never written
to. Enforced twice, independently: `imaptest.c`'s own `guard_mailbox()`
refuses at the C level regardless of caller, and this file's own
`run_imapsync_wb()`/`imaptest_wb()` wrappers refuse, themselves, to be
pointed at any other folder name. A best-effort cleanup pass at the end
(`expunge 1:*`) leaves `Revival/WritebackTest` empty again rather than
accumulating each run's test messages.

```
revival/tests/imap-sync-tests
```

## imap-writeback-tests

`imap-writeback-tests` -- milestone 4's Gate 1 capture/suppression
suite for `msjournal.c` and its four hook sites in `src/ams/libs/ms/`
(`altsnap.c`, `purge.c`, `clonemsg.c`, `apndfile.c`). See
`revival/doc/imap-writeback-prompt.md`. Gate 1 scope: local capture
into a per-folder `.MS_Journal` file and the `MSJournal_Suppress(1)`
`imapsync` sets at startup. As of Gate 2, `imapsync`'s own
`replay_folder()` also *acts on* a pending `.MS_Journal` on every
ordinary run, so this suite's "suppression" re-run step is now a real
write path, not read-only. **This suite retargets to the dedicated
sandbox mailbox `Revival/WritebackTest`** (see the 2026-07-23 incident
in `imap-writeback-REPORT.md`: an earlier version mirrored a real,
pre-existing folder, `INBOX/1-Admin/keys`, and once Gate 2's replay
code existed, this suite's own case 1 + suppression re-run permanently
deleted one real message from it via a genuine `UID STORE`/`UID
EXPUNGE`). Every write this suite performs -- the `cui`-driven local
mutations, the journal they produce, every `imapsync` run that might
replay that journal, and this suite's own `APPEND` seeding/`EXPUNGE`
cleanup -- is confined to `Revival/WritebackTest`, enforced by
`assert_folder_is_sandbox()` (checked against a hardcoded literal, not
the mutable `FOLDER` constant, so a future edit can't silently repoint
this suite at a real folder again) in addition to `imaptest.c`'s own
`guard_mailbox()` at the C level. The sandbox starts empty, so this
suite seeds it with two throwaway `\Seen`-flagged messages of its own
up front (via `imap_Append`/`imaptest.test append`, the same mechanism
`imap-sync-tests`' W-cases use) and expunges it clean again on exit,
pass or fail.

Case 1 (the only case at Gate 1):

1. Provision/seed: create `Revival/WritebackTest` if absent, `APPEND`
   two throwaway messages (pre-marked `\Seen`) so the fresh mirror has
   the two messages this case needs.
2. Fresh mirror into a `mktemp -d` scratch root (never the real
   `~/.IMAP`), confirming `.MS_Journal` does not exist right after --
   imapsync's own append writes must never journal.
3. A scripted `cui` session (scratch `PROFILES`, never the real
   `~/preferences`) attempts `type 1` (mark-read; a no-op since message
   1 was seeded `\Seen` already -- proving the hook adds no spurious
   record when nothing actually changes), then `delete 2` (the
   `ASS_OR_ATTRIBUTES` path), `undelete 2` (the `ASS_AND_ATTRIBUTES`
   path), `delete 2` again, and `purge`. `.MS_Journal` is asserted to
   contain exactly the four expected lines, byte for byte,
   independently recomputed in Python from the documented OR/AND-mask
   grammar (not by re-trusting `msjournal.c`'s own encoding).
4. Suppression, two ways: an ordinary immediate re-run of `imapsync`,
   and then a *forced* re-run with the scratch `.MS_IMAPSync` state
   file's `highestmodseq` deliberately invalidated so the flags-refresh
   pass genuinely re-applies real `MS_AlterSnapshot` calls from real
   FETCH data. **Known, separate issue (not a safety concern): since
   Gate 2, both of these re-runs really do replay and consume the
   pending journal (real `STORE`/`EXPUNGE` against the sandbox), so the
   "byte-identical afterward" assertions these two sub-cases make now
   fail (comparing real journal content against the post-replay
   consumed/deleted file) -- expect "1 passed, 2 failed" live.
   Reconciling these assertions with real replay is a design decision
   left to a future gate, tracked in `imap-writeback-REPORT.md`.**
5. Cleanup: expunge everything this run added to `Revival/WritebackTest`
   (`expunge 1:*`), run in a `finally` block so it happens whether the
   cases above pass or fail.

Skips cleanly (rather than failing) when `~/.netrc` has no usable
`machine imap.fastmail.com` stanza. Leaves its scratch root in place on
exit (path printed) for inspection.

```
revival/tests/imap-writeback-tests
```
