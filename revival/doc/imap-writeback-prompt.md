# Milestone 4 spec: writeback — change journal + replay to IMAP

Scope: milestone 4 of the AMS-over-IMAP plan. Local mutations made in
messages/cui against a mirrored folder (read, delete, purge, move,
copy-in) get captured in a per-folder change journal and pushed to the
IMAP server by `imapsync` on its next run. Until now sync is one-way
(server → local); this makes it two-way. Conflict policy, per the plan
of record: **server wins; the journal replays on top.**

Read first, in order:
1. `revival/doc/ams-IMAP-project.md` — §2A (journal at the four
   mutation points), §3 (deterministic ids f(UIDVALIDITY,UID)), §10.
2. `revival/doc/claude-history/imap-sync-prompt.md` — what 3b built; this spec
   extends that program and its conventions (state file, scratch
   roots, test style). The `.MS_IMAPSync` state file and the
   deterministic base32hex id format are load-bearing here.
3. `src/overhead/mail/hdrs/imap_prot.h` — the protocol layer. It is
   read-only today; this milestone authorizes additive write-side
   entry points (below).
4. `src/ams/libs/ms/` — `altsnap.c` (or wherever `MS_AlterSnapshot`
   lives), `clonemsg.c`, `purgedel.c`, `apndfile.c`: the four
   mutation points named by the plan doc. Read how each learns the
   folder directory path.

Language level: new files are born-ANSI (C89 prototypes, scanf
banned), same policy as `imap_prot.c`/`imapsync`. Hooks added inside
existing K&R files match those files' style.

## Design decisions (made; not yours to re-open)

1. **Capture at the MS layer, per-folder journal file.** New module
   `src/ams/libs/ms/msjournal.c`: `MSJournal_Record(dir, fmt, ...)`
   appends one text line to `.MS_Journal` inside the folder directory
   (sibling of `.MS_MsgDir`). Append = one `write()` on an
   `O_APPEND` fd per record; open/close per record is fine at these
   rates. A folder is journaled iff its directory contains
   `.MS_IMAPSync` (stat once per dir per process, cache the answer).
   Non-mirrored folders never journal — zero behavior change for
   local mail.
2. **Suppression.** `MSJournal_Suppress(int on)` — a process-global
   flag; `imapsync` sets it at startup so its own mirror writes
   (append, flags) never journal. Nothing else calls it.
3. **Capture points and records** (fields space-separated, one line,
   `J1` version tag first; document exact grammar in the code):
   * `MS_AlterSnapshot` → `J1 flags <id> <or-spec> <and-spec>` —
     record the attribute changes as passed (enough to recompute the
     IMAP flag delta for the three server-mapped attributes UNSEEN /
     DELETED / REPLIEDTO; other attributes are local-only, still
     journaled, ignored at replay).
   * `MS_PurgeDeletedMessages` → one `J1 purge <id>` per purged
     message (capture inside the loop, after the store's own purge
     succeeds).
   * `MS_AppendFileToFolder` (native-id append into a mirrored
     folder — e.g. a user copies/moves a message in from a local
     folder; `MS_CloneMessage`'s dest-side lands here or in its own
     copy loop — find where and hook whichever primitive actually
     writes) → `J1 append <id>`.
   * `MS_CloneMessage` with delete-original (COPYDEL) out of a
     mirrored source → the source side records `J1 purge <id>`.
     Clone between two mirrored folders is NOT special-cased: it
     decomposes into `append` at the dest + (if COPYDEL) `purge` at
     the source. UID COPY/MOVE optimization is explicitly deferred.
4. **Replay ordering: journal first, then mirror** (per folder, in
   the existing per-folder pass). Server-wins falls out: our pushes
   land, then the mirror pass re-reads server truth including them.
5. **Journal handoff, crash-safe.** At replay start, if `.MS_Journal`
   exists, `rename()` it to `.MS_Journal.replaying` (skip if a
   previous `.replaying` already exists — resume that one first;
   messages may keep appending to a fresh `.MS_Journal` meanwhile,
   picked up next run). Replay `.replaying` line by line; after each
   line's server op gets its tagged OK, advance a `journal_offset`
   cursor in `.MS_IMAPSync` (atomic rewrite, as 3b does). When the
   file is exhausted, delete it and zero the cursor. Worst case after
   a crash is one op re-sent: flags STORE is idempotent, re-purge of
   a gone uid is a no-op, re-append can duplicate — accepted and
   documented, not defended against in v1.
6. **Replay mapping.** Deterministic ids decode to (UIDVALIDITY,
   UID); an id whose UIDVALIDITY doesn't match the folder's current
   one is dropped with a loud log line (server won; the re-mirror
   already handled it).
   * `flags` → `UID STORE <uid> ±FLAGS.SILENT (...)` for the mapped
     attributes only.
   * `purge` → `UID STORE <uid> +FLAGS.SILENT (\Deleted)` then
     `UID EXPUNGE <uid>` (UIDPLUS — Fastmail advertises it; if the
     server lacks UIDPLUS, log and skip purges rather than falling
     back to bare EXPUNGE, which expunges more than asked).
   * `append` (id is native/non-deterministic) → `APPEND` the local
     body file (flags from the local snapshot, INTERNALDATE from
     AMS_DATE), then **delete the local native-id copy through the
     store's own purge path (suppressed)** — the message returns on
     the same run's mirror pass under its proper deterministic id.
     One-run flicker of identity, zero duplicate risk, no id rewrite
     surgery in the store. Document this behavior for the user.
7. **Safety valve.** Replay refuses to purge more than 25 messages
   in one folder in one run unless invoked with `-force-purges`
   (a corrupt or stale journal must not mass-expunge a mailbox).
8. **UIDVALIDITY change** (detected by the existing 3b path): discard
   both journal files for that folder, log loudly, re-mirror. Ids in
   them reference dead uids; the guard in (6) makes this belt+braces.

## imap_prot additions (additive only; existing entry points frozen)

`imap_UidStoreFlags`, `imap_UidExpunge`, `imap_Append` — same
conventions as the existing entry points (tagged dispatch, IMAP_DEAD
contract, growable reads). Check CAPABILITY for UIDPLUS; expose it the
way ESEARCH/CONDSTORE detection is exposed today. Extend
`revival/tests/imap-protocol-tests` for each new entry point —
**write operations in tests run only against the dedicated test
mailbox (below), never INBOX.**

## Testing

Extend `revival/tests/imap-sync-tests` (update its README). All write
operations — STORE, EXPUNGE, APPEND, and any folder the journal replay
touches — are confined to a dedicated mailbox `Revival/WritebackTest`
(create via IMAP if absent; the test driver must refuse to run replay
cases against any other folder name — assert it). INBOX stays strictly
read-only in every suite. Local side always a scratch root + scratch
PROFILES, per 3b conventions. Cases at minimum:

1. Capture: scripted cui marks a mirrored message read, marks one
   deleted, purges; `.MS_Journal` contains exactly the expected
   records. An imapsync mirror run right after adds zero records
   (suppression proven).
2. Flags round-trip: replay pushes \Seen; verify server-side via an
   imap_prot FLAGS fetch; a second replay run is a no-op (journal
   gone, cursor zeroed).
3. Purge round-trip: purge locally, replay, verify uid gone from the
   server, message count consistent after the same run's mirror pass.
4. Safety valve: fabricate a journal with 26 purges, verify refusal
   without `-force-purges`.
5. Append round-trip: copy a local message into the mirrored folder
   via cui, replay+mirror in one run, verify it exists server-side
   and locally under a deterministic id (and only once).
6. Crash resume: kill replay between ops (or simulate via a truncated
   run), re-run, verify completion with no duplicate flags damage and
   at most the documented duplicate-append exposure.
7. Stale-journal drill: journal entry whose id has the wrong
   UIDVALIDITY is skipped with the loud log line.

`imap-protocol-tests` and both SMTP suites must still pass at every
gate.

## Gates

* **Gate 1 (STOP and report):** capture + suppression only — no IMAP
  write code at all. Test case 1 passing, journal grammar documented,
  diffs of the four hook sites shown (they are the riskiest edits in
  this task: they sit in 35-year-old store code every AMS client
  runs — smallest possible footprint, no behavior change when the
  folder isn't mirrored or suppression is on).
* **Gate 2 (STOP and report):** imap_prot write entry points + flags
  and purge replay; cases 2–4, 7 passing; all prior suites green.
* **Gate 3 (STOP and report, end of M4):** append writeback; cases
  5–6; full suite; a scripted-cui end-to-end acceptance (mark-read +
  purge + copy-in against `Revival/WritebackTest`, then a fresh
  mirror shows convergence). messages-GUI hand test is the user's,
  not yours — provide the two-line instruction for it.

## Ground rules

No fossil commits — session ends with `<task>-session.diff` +
`<task>-REPORT.md` in the tree root. Never write outside:
`src/ams/libs/ms/` (msjournal.c + the four hook sites + extern
declaration site), `src/overhead/mail/lib|hdrs` (imap_prot additions),
`src/ams/msclients/imapsync/`, `revival/tests/`, scratch roots.
Destructive IMAP ops only against `Revival/WritebackTest`; INBOX
read-only; real `~/.IMAP`, `~/preferences`, `~/.netrc` never touched
(netrc via netrc_Lookup only, password redacted everywhere). No
concurrent builds; compile-verify every touched file against its
`fossil cat` original for new warnings. If capture turns out to need
more than the four hook sites + one new module, or replay needs an
imap_prot change that isn't purely additive, STOP and report — design
conversation, not judgment call.

Maintain a breadcrumb handoff file at `<scratchpad>/M4-HANDOFF.md`
(coordinator supplies the path) updated after each meaningful step, so
a usage-limit interruption can be recovered by a fresh instance from
spec + fossil diff + handoff.
