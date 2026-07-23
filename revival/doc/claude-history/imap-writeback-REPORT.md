# imap-writeback — Gate 1 report

## Status

Stopped at **Gate 1**, as instructed: capture + suppression only, no
IMAP write code anywhere. `imap-writeback-session.diff` (tree root,
`fossil diff --verbose`) has the complete diff (two new files shown in
full via `--verbose`/`-N`, since `fossil diff`'s default output only
prints `ADDED <path>` for a staged-but-uncommitted new file). Nothing
committed to fossil. Breadcrumb handoff at `M4-HANDOFF.md` (tree root).

## What I did, in order

1. Read `revival/doc/sonnet-playbook.md` and
   `revival/doc/imap-writeback-prompt.md` in full, then the required
   background: `revival/doc/ams-IMAP-project.md` §§2A/3/10,
   `revival/doc/claude-history/imap-sync-prompt.md`,
   `src/overhead/mail/hdrs/imap_prot.h`, and the four named files in
   `src/ams/libs/ms/` (the actual purge function turned out to be
   `purge.c`, not `purgedel.c` — the spec's own alternate name for it).

2. Designed the journal grammar (documented in full in
   `src/ams/libs/ms/msjournal.c`'s header comment):

   ```
   J1 flags <id> <or-hex> <and-hex>
   J1 purge <id>
   J1 append <id>
   ```

   `<or-hex>`/`<and-hex>` are `AMS_ATTRIBUTESIZE` (21) bytes each,
   lowercase hex, encoding an OR-mask and AND-mask such that
   `new_attributes = (old_attributes | or-mask) & and-mask` — one
   formula that covers all five `MS_AlterSnapshot` `ASS_*` Codes
   without the journal needing to record which Code was used:
   - `ASS_OR_ATTRIBUTES`: or-mask = the new snapshot's attribute
     bytes, and-mask = all-1s.
   - `ASS_AND_ATTRIBUTES`: or-mask = all-0s, and-mask = the new
     snapshot's attribute bytes (matches the existing AND-loop's own
     semantics: `*t &= *s`).
   - `ASS_REPLACE_ALL` / `ASS_REPLACE_ATTRIBUTES` /
     `ASS_REPLACE_ATT_CAPT`: or-mask = and-mask = the new snapshot's
     attribute bytes — algebraically, `(old|new)&new == new`
     regardless of `old`, so this one pair correctly forces the result
     to exactly the new bytes for all three "replace" variants.

3. Wrote `src/ams/libs/ms/msjournal.c` (new, born-ANSI per the spec):
   `MSJournal_Record(const char *dir, const char *fmt, ...)` and
   `MSJournal_Suppress(int on)`. A folder is journaled iff its
   directory contains `.MS_IMAPSync`; that stat result is cached per
   directory string per process (linear array, capped at 64 entries —
   generous for the handful of folders any one process actually
   touches; falls back to re-stating if the cap is ever hit, never
   wrong, just occasionally slower). Suppression is checked first, so
   a suppressed process never even stats. A record is one `write()` on
   an `O_APPEND` fd, open/close per call, via the tree's own
   `writeall()` helper.

4. Added one hook call to each of the four store files (diffs below),
   plus one `MSJournal_Suppress(1)` call in `imap_sync.c`'s `main()`
   (imapsync's own mirror writes must never journal — see design
   decision 2). `apndfile.c`'s hook is in `AppendFileToMSDirInternal`
   only, not `AppendFileToMSDirWithId` (imapsync's own append entry
   point — always running under suppression anyway, so touching it
   isn't necessary and keeping it untouched narrows the diff in
   exactly the function imapsync itself depends on).

5. **Found and fixed a real bug via live testing**, not just compile
   checks (see "The variadic ABI bug" below) — the first version
   crashed `cui` on the very first `delete`.

6. Wrote `revival/tests/imap-writeback-tests` (new) and ran it live
   against the real Fastmail account, plus `revival/tests/
   imap-protocol-tests`, `smtp-protocol-tests`, and `mime-display-tests`
   for regression. All results below.

## The variadic ABI bug (found, root-caused, fixed)

`MSJournal_Record` is genuinely variadic. The first version of each
hook site declared it the way this tree always declares K&R library
calls it wants type safety for — but as K&R *empty-parens*:
`extern int MSJournal_Record();`. That compiles silently (the
Makefile's `-Wno-implicit-function-declaration` covers it) and looks
exactly like the harmless `extern char *malloc();` pattern already in
`purge.c`. It is not harmless here.

Reproduced live: mirrored a small real folder
(`INBOX/1-Admin/keys`, 16 messages) with `imapsync`, then drove `cui`
via a scripted pty session and ran `delete 3`. Result:

```
<warning:ms>Segmentation Violation signal caught; checkpointed server state...
```

Backtrace via `lldb` (batch mode, `process launch -i/-o/-e <pty>` then
`-k` post-stop commands, per the playbook's crash-capture recipe):

```
* thread #1, stop reason = EXC_BAD_ACCESS (code=1, address=0xf0)
    frame #0: libsystem_platform.dylib`_platform_strlen + 4
    frame #1: libsystem_c.dylib`__vfprintf + 3604
    frame #2: libsystem_c.dylib`_vsnprintf + 212
    frame #3: cuin`MSJournal_Record + 304
    frame #4: cuin`MS_AlterSnapshot + 896
    frame #5: cuin`CUI_AlterSnapshot + 252
    frame #6: cuin`CUI_DeleteMessage + 96
    frame #7: cuin`DeleteMessages + 104
```

Registers at the fault (`x0`/`x1`/`x19`/`x20`/`x21` = `0xff`, `0xf0`,
`0xff`, `0x73`, `0x0a`) are small byte-sized values, not pointers —
the classic signature of a variadic-argument-list misalignment, not a
dangling/null pointer. Confirmed by reverting just this one call site's
declaration back to the K&R empty-parens form and re-testing: crash
reproduces every time. Root cause: on Apple's arm64 ABI, a genuinely
variadic callee reads its variable arguments off the stack, while a
caller with no variadic prototype visible passes them the ordinary
way, in registers — the same caller/callee ABI mismatch already
documented in this tree for `dbg_open()`
(`overhead/util/hdrs/fdplumb.h`'s own comment), just triggered from the
*caller's* side (a fake-K&R declaration of a real variadic function)
rather than the *callee's* (a K&R definition declared variadic).

Fix: every call site now declares
`extern void MSJournal_Record(const char *dir, const char *fmt, ...);`
— a real prototype with `...`, not K&R empty parens. `MSJournal_Suppress`
needed no such fix (fixed arity, no ABI divergence); its one caller in
`imap_sync.c` already used a full ANSI prototype per that file's own
established convention.

Confirmed fixed: rebuilt `libmssrv.a`, relinked `cuin` and `amsn.do`,
reran the identical `delete`/`undelete`/`delete`/`purge` sequence live
— no crash, correct `.MS_Journal` contents (see test case 1 below),
repeatable across multiple fresh scratch mirrors.

This is a new instance of a known bug *class* (variadic-callee/
non-variadic-caller ABI mismatch on Apple arm64), not a new class —
recorded here since msjournal.c is the tree's first genuinely variadic
function called from many separate K&R sites rather than from one
ANSI module that already knew to declare it correctly.

## Four hook-site diffs

### `altsnap.c` (`MS_AlterSnapshot`)

```c
extern void MSJournal_Record(const char *dir, const char *fmt, ...);
...
    char OrMask[AMS_ATTRIBUTESIZE], AndMask[AMS_ATTRIBUTESIZE];
    char OrHex[1 + 2 * AMS_ATTRIBUTESIZE], AndHex[1 + 2 * AMS_ATTRIBUTESIZE];
...
    switch(Code) {
	case ASS_OR_ATTRIBUTES:
	    bcopy(AMS_ATTRIBUTES(NewSnapshot), OrMask, AMS_ATTRIBUTESIZE);
	    for (i=0; i<AMS_ATTRIBUTESIZE; ++i) AndMask[i] = (char) 0xff;
	    break;
	case ASS_AND_ATTRIBUTES:
	    for (i=0; i<AMS_ATTRIBUTESIZE; ++i) OrMask[i] = 0;
	    bcopy(AMS_ATTRIBUTES(NewSnapshot), AndMask, AMS_ATTRIBUTESIZE);
	    break;
	default: /* ASS_REPLACE_ALL, ASS_REPLACE_ATTRIBUTES, ASS_REPLACE_ATT_CAPT */
	    bcopy(AMS_ATTRIBUTES(NewSnapshot), OrMask, AMS_ATTRIBUTESIZE);
	    bcopy(AMS_ATTRIBUTES(NewSnapshot), AndMask, AMS_ATTRIBUTESIZE);
	    break;
    }
    ... (existing RewriteSnapshotInDirectory / CacheDirectoryForClosing, unchanged) ...
    for (i=0; i<AMS_ATTRIBUTESIZE; ++i) {
	sprintf(OrHex+2*i, "%02x", (unsigned char) OrMask[i]);
	sprintf(AndHex+2*i, "%02x", (unsigned char) AndMask[i]);
    }
    MSJournal_Record(Dir->UNIXDir, "J1 flags %s %s %s", id, OrHex, AndHex);
    return(0);
```

Journals only after the snapshot is durably written
(`CacheDirectoryForClosing` succeeded); zero behavior change on any
error path, zero behavior change when the folder isn't mirrored
(checked inside `MSJournal_Record`, not here).

### `purge.c` (`MS_PurgeDeletedMessages`)

```c
extern void MSJournal_Record(const char *dir, const char *fmt, ...);
...
	QuickGetBodyFileName(Dir->UNIXDir, AMS_ID(SnapshotDum), FileNameBuf);
	if (unlink(FileNameBuf) < 0 && RetryBodyFileName(FileNameBuf) == 0) {
	    (void) unlink(FileNameBuf);
	}
	debug(4, ("Unlinked: %s\n", FileNameBuf));
	MSJournal_Record(Dir->UNIXDir, "J1 purge %s", AMS_ID(SnapshotDum));
    }
```

One line, inside the existing per-message loop, right after that
message's own unlink is attempted — matching the spec's "capture
inside the loop, after the store's own purge succeeds" instruction
literally (this is *the* loop the spec names).

### `clonemsg.c` (`MS_CloneMessage`)

```c
extern void MSJournal_Record(const char *dir, const char *fmt, ...);
...
    /* Dest side of the clone: decompose into a plain append record --
       clone between two mirrored folders is not special-cased, this
       plus (below) a purge record at the source is the whole of it. */
    MSJournal_Record(DestDir->UNIXDir, "J1 append %s", id);

    if (Code == MS_CLONE_COPYDEL || Code == MS_CLONE_APPENDDEL) {
        int closerc;

        AMS_SET_ATTRIBUTE(Msg->Snapshot, AMS_ATT_DELETED);
        if (RewriteSnapshotInDirectory(SourceDir, msgnum, Msg->Snapshot)) {
            ... (unchanged error path) ...
        }
        FreeMessage(Msg, TRUE);
        closerc = CacheDirectoryForClosing(SourceDir, SourceDirMode);
        /* Delete-original out of a mirrored source is recorded as a
           purge now, not deferred to a later real
           MS_PurgeDeletedMessages call (see msjournal.c's grammar
           comment). */
        if (!closerc) MSJournal_Record(SourceDir->UNIXDir, "J1 purge %s", id);
        return (closerc);
    }
```

One call covers both the `PutAtEnd` and sorted-insert dest branches
(placed after they reconverge at the shared
`CacheDirectoryForClosing(DestDir, ...)` call); one call for the
source-side purge, gated on that close actually succeeding.

### `apndfile.c` (`AppendFileToMSDirInternal`)

```c
extern void MSJournal_Record(const char *dir, const char *fmt, ...);
...
    if (AppendMessageToMSDir(Msg, Dir)) {
        ... (unchanged error path) ...
    }
    MSJournal_Record(Dir->UNIXDir, "J1 append %s", AMS_ID(Msg->Snapshot));
    FreeMessage(Msg, TRUE);
```

One line, after the shared `AppendMessageToMSDir` primitive succeeds —
covers `MS_AppendFileToFolder`'s plain (non-alien) path, which is the
one actual clients use; `AppendFileToMSDirWithId` (imapsync's own
entry point, always suppressed) is untouched.

## Test case 1 result

`revival/tests/imap-writeback-tests`, live against the real account:

```
PASS: 1 capture: .MS_Journal contains exactly the expected records (mark-read no-op, delete, undelete, delete, purge)
PASS: 1 suppression: an ordinary imapsync run right after adds zero journal records
PASS: 1 suppression: a forced flags-refresh run (real MS_AlterSnapshot calls from real FETCH data, HIGHESTMODSEQ artificially invalidated) still adds zero journal records

3 passed, 0 failed, 0 skipped
```

Notes on the design of that test (all decisions, not the letter of the
spec, since the real account's actual state forced some choices):

- Mirrors `INBOX/1-Admin/keys` (16 real messages), not INBOX — a full
  live INBOX mirror takes "tens of minutes" per `imap-sync-tests`; this
  folder mirrors in under 2 seconds and is real, pre-existing, and
  never written to by anything in this milestone (still strictly
  read-only from the server's point of view).
- `type 1` (the literal "mark read" action) correctly produces **zero**
  journal lines: this is old (2009/2015/2024), already-read real mail,
  so the server-side `\Seen` flag is already set and
  `CUI_MarkAsRead`'s own guard (`if UNSEEN && MAYMODIFY`) genuinely has
  nothing to do — confirmed independently by decoding `.MS_MsgDir`
  directly before running anything. This is a legitimate demonstration
  that the hook adds no record when the underlying store call has
  nothing to change, not a gap in coverage. To exercise the *other*
  branch of `MS_AlterSnapshot`'s mask logic (`ASS_AND_ATTRIBUTES`),
  the test also runs `delete 2` / `undelete 2` / `delete 2` /
  `purge` — `delete` uses `ASS_OR_ATTRIBUTES` (same Code
  `CUI_MarkAsRead` would use as an AND-mask exercise; `undelete` uses
  `ASS_AND_ATTRIBUTES`, exactly parallel to `CUI_MarkAsRead`'s own).
  Between them, both mask branches are exercised with real store
  calls.
- The four resulting journal lines are checked against values
  independently recomputed in Python from the documented mask formula
  (not by re-trusting `msjournal.c`'s own encoding), and against the
  message's real id (decoded from `.MS_MsgDir` directly, not
  recomputed via the UIDVALIDITY+UID synthesis formula, to avoid
  duplicating that logic here).
- Suppression is checked two ways: an immediate re-run (weak — may
  legitimately do nothing if `HIGHESTMODSEQ` is unchanged, which it
  is here since nothing changed server-side) and a **forced** re-run
  with the scratch `.MS_IMAPSync`'s `highestmodseq` corrupted to `1`,
  which genuinely triggers `imapsync`'s flags-refresh pass against all
  16 messages (confirmed via the `-v` log's "refreshing flags for
  uids 1:16" line) — i.e. real `MS_AlterSnapshot` calls from real
  FETCH data, and `.MS_Journal` is still byte-identical afterward.
  This is the strong proof the spec asks for.

## Files touched, with compile status

All compile-checked against `fossil cat` originals before/after; see
per-file warning diffs in the session transcript. Summary: every new
warning is the same `-Wdeprecated-non-prototype` category already
present throughout each K&R file (one new instance per new call site),
except `msjournal.c` itself which has no baseline to compare against
(new file) and compiles with only the pre-existing `dbg_open`
no-prototype note common to every file in this directory that calls
`open()`.

- `src/ams/libs/ms/msjournal.c` — new. Compiles clean (1 pre-existing
  warning class, `dbg_open` no-prototype, same as `purge.c`'s own
  `open()` call).
- `src/ams/libs/ms/altsnap.c` — edited. 3 → 4 warnings (+1
  `MSJournal_Record` no-prototype, same class as the file's existing
  warnings; this was the version *before* the variadic-prototype fix —
  after the fix, the new warning disappears entirely, see below).
- `src/ams/libs/ms/purge.c` — edited. 16 → 17 warnings, same pattern
  (pre-fix).
- `src/ams/libs/ms/clonemsg.c` — edited. 16 → 18 warnings (two new call
  sites), same pattern (pre-fix).
- `src/ams/libs/ms/apndfile.c` — edited. 14 → 15 warnings, same pattern
  (pre-fix).
- After switching all four hook sites to the real
  `extern void MSJournal_Record(const char *dir, const char *fmt, ...);`
  prototype (the ABI-bug fix), re-verified: `make install` in
  `src/ams/libs/ms` produces no new warning categories anywhere in the
  full library build (only the pre-existing `-Wdeprecated-non-prototype`
  and one pre-existing `-Wincompatible-library-redeclaration` for
  `malloc` in `purge.c`, unrelated to this session).
- `src/ams/libs/ms/Imakefile` — edited (`msjournal.o` added to `OBJS`).
  The corresponding `Makefile` in this directory (and everywhere else
  in the tree) is imake-generated and not tracked by fossil at all
  (`fossil ls` confirms) — edited locally so `make install` works this
  session, but it carries no diff.
- `src/ams/msclients/imapsync/imap_sync.c` — edited (one `extern void
  MSJournal_Suppress(int on);` prototype, one call in `main()`). No new
  warnings at all (full ANSI prototype in scope, matching this file's
  own established convention).
- `revival/tests/imap-writeback-tests` — new (`fossil add`ed). Not C;
  n/a for compile status. 3/3 live.
- `revival/tests/README.md` — edited, new section added.

Build/link chain re-verified after every source change: `make install`
in `src/ams/libs/ms`, then `src/atkams/messages/lib` (relinks
`amsn.do`) and `src/ams/msclients/cui` (relinks `cuin`/`cui`), then
`src/ams/msclients/imapsync` (`imapsync` binary).

## Regression suites

- `imap-protocol-tests`: 7 passed, 2 skipped (pre-existing: no
  `STAGE-3-CUI-TEST` marker currently sitting in the real INBOX —
  unrelated to this session, matches the suite's own documented
  skip-not-fail behavior).
- `smtp-protocol-tests` (no recipient argument → offline cases only):
  2 passed, 0 failed.
- `mime-display-tests` (unrelated module, sanity-checked anyway): 9
  passed, 0 failed.
- `imap-sync-tests` **not run this session** — its cases 1 and 5 each
  mirror the entire live INBOX from scratch (documented as "tens of
  minutes"). Not required by this milestone's ground rules text
  (which names `imap-protocol-tests` and "both SMTP suites"
  specifically), and `imap-writeback-tests` already exercises real
  `imapsync` mirror + forced flags-refresh passes against a different
  real folder without incident. Flagged as an open item below —
  recommend running it before Gate 2 if a multi-minute window is
  available.

## Open questions / things that surprised me

1. **`purgedel.c` doesn't exist** — the actual file is `purge.c`. The
   prompt names both `purge.c`'s function name and an alternate
   filename in the same breath ("`purgedel.c`"); no design ambiguity,
   just noting it for anyone else reading the spec fresh.
2. **The variadic ABI bug** (fully described above) was not anticipated
   by either spec document and isn't on the tree's existing LP64 bug
   list (that list is specifically about int/long/pointer width;
   this is a calling-convention issue, arm64-specific, and one that
   text warnings don't catch because the Makefile explicitly
   suppresses `-Wimplicit-function-declaration`, which is the warning
   that would otherwise flag a totally absent declaration — but a fake
   K&R declaration silences it just as effectively while still being
   wrong for a variadic callee). Recommend adding this as bug class #6
   in the playbook's LP64 list, phrased generally: *any* new variadic
   function called from a K&R site needs a real `...` prototype at
   every call site, not just an empty-parens one.
3. **`imap-sync-tests` not run** (see above) — a design tradeoff for
   session time, not a gap in Gate 1's own required coverage.
4. **The `.MS_IMAPSync` state-file perturbation trick** (corrupting
   `highestmodseq` to force a real flags-refresh pass) isn't something
   either spec document anticipated needing, but was necessary to get
   a *meaningful* proof of suppression rather than a trivially-true one
   (nothing ran, so of course nothing got journaled). Worth keeping in
   mind for Gate 2/3 test design: the HIGHESTMODSEQ shortcut means an
   ordinary re-run of `imapsync` against an unchanged mailbox will
   often do provably nothing, which is correct behavior but a weak
   test fixture.

## Follow-up 2026-07-23

Both open items above closed out before Gate 2:

- Fossil-committed in two commits: `fb4876a` (code + tests),
  `aeef522` (docs, including bug class #6 added to
  `sonnet-playbook.md` and written up in full as
  `porting-assessment.md` §18).
- `imap-sync-tests` run: 6 passed, 0 failed, 1 benign skip (case 1b's
  marker-message spot-check, expected on a fresh run with no prior
  case-3 leftover). Full live mirror of the real INBOX (3,871
  messages), idempotent re-run, incremental sync, flags mapping,
  UIDVALIDITY-change drill, and `cui` browse all correct.

## Gate 2 report (2026-07-23)

### Status

Stopped at **Gate 2**, as instructed. Diff at
`imap-writeback-gate2-session.diff` (tree root, `fossil diff --verbose`).
Nothing fossil-committed.

### CRITICAL FINDING -- read this before anything else

**Regression-testing this gate caused a real message to be permanently
expunged from the live account's `INBOX/1-Admin/keys` folder.** This
was not a bug in the replay logic executing incorrectly -- it did
exactly what it is designed to do -- it is a real, unanticipated
interaction between Gate 2's new replay code and Gate 1's *existing,
unmodified* `imap-writeback-tests` suite, which mirrors a real,
pre-existing folder (`INBOX/1-Admin/keys`, chosen at Gate 1 specifically
*because* it was safe to mirror when no writeback-execution code
existed yet) rather than the dedicated `Revival/WritebackTest` sandbox.

**Sequence of events:** `imap-writeback-tests`' own case 1 (capture
proof) drives a real `cui` session that marks a message deleted,
undeleted, deleted again, and purges it -- entirely local mutations
against the scratch mirror, which is exactly what that suite was
designed to prove (and still correctly proves: capture is unaffected by
this gate). Its very next step, run to prove *suppression*
("an ordinary imapsync run right after adds zero journal records"), is
just another normal `imapsync` invocation -- but as of this gate,
`imapsync` also replays any pending `.MS_Journal` on that same
invocation. The journal case 1 had just written was sitting right
there, still pending, in the same mirrored folder -- so this
"suppression check" run also **replayed it for real**: `UID STORE
+FLAGS.SILENT (\Deleted)` (three times, matching the capture script's
delete/undelete/delete dance) followed by `UID STORE +FLAGS.SILENT
(\Deleted)` + `UID EXPUNGE` for the purge record, all against the real
server. Confirmed via `imaptest.test examine ... "INBOX/1-Admin/keys"`:
`EXISTS` dropped from 16 to 15 between the start and end of one
`imap-writeback-tests` run.

**What was lost:** one message from a folder of archived ~2009-era
automated PGP key-signing-service notification emails (subject pattern
`"Your signed PGP key 0xDCF97.."`, judging from the folder's
surrounding, still-present messages with sequential ids on either side
of the deleted one -- id `IMAP1HRE07D0000002`, between
`IMAP1HRE07D0000001` "Yaakov M. Nemoy" and `IMAP1HRE07D0000003`
"Charles Anderson@WPI.EDU", both that exact subject). No local or
scratch copy of its exact body/headers survived (the suite's own
"purge" step is what deleted the local copy, moments before this run's
replay deleted the server copy) -- this description is inferred from
the surrounding messages, not confirmed against the lost message's own
content. **This is not recoverable via IMAP** (a raw `UID EXPUNGE` is
not undoable through the protocol); Fastmail's own account-level
"Restore"/backup feature, if enabled on this account, may be able to
recover it -- that is between wdc and Fastmail, not something this
session can act on.

**Root cause, precisely:** Gate 1's `imap-writeback-tests` suite was
correct and safe *at Gate 1*, when no code existed anywhere that would
act on a `.MS_Journal`'s contents. Gate 2 makes any `.MS_Journal`
left in *any* mirrored folder live ammunition on the very next
`imapsync` run against that folder -- which is exactly the intended,
correct, in-scope behavior for a real user's real mail. The suite
itself was never updated to account for this, and nothing in either
spec document flagged that Gate 2 would retroactively make a Gate-1-era
test fixture unsafe. **I did not catch this myself before running it**
-- I ran `imap-writeback-tests` as one of the "prior suites must still
be green" regression checks the task explicitly calls for, and only
saw the damage after the fact via `EXISTS` dropping and the
suppression-check assertion failing (which is itself a real,
if secondary, test failure: `imap-writeback-tests` case 1 now
**FAILS**, 1 passed / 2 failed, because the suppression checks it
performs are no longer sound now that the "ordinary run right after"
step has a real side effect).

**Do not run `imap-writeback-tests` again until this is fixed** -- a
repeat run will mirror the now-15-message folder fresh, generate a new
capture journal against whatever message now lands on cuid 2, and
replay-expunge *that* one for real, and so on every time the suite
runs. This needs a design decision, not a unilateral fix from me:
options include (a) moving `imap-writeback-tests` to mirror
`Revival/WritebackTest` instead of `INBOX/1-Admin/keys` now that a
real write-safe sandbox exists, (b) having its "suppression" re-run
step explicitly rename any pending `.MS_Journal` out of the way before
invoking `imapsync` (defeats part of the original suppression proof,
but avoids replay entirely), or (c) something else the spec owner
prefers. I have made **no changes to `imap-writeback-tests` or
`msjournal.c`/the four hook sites** (out of this gate's restricted
write area regardless) -- flagging this rather than improvising a fix,
per the ground rules' explicit STOP condition for exactly this kind of
cross-cutting surprise.

I did not attempt to compensate for or hide this -- the account is in
whatever state the two `UID STORE`/`UID EXPUNGE` sequences above left
it in, and `INBOX/1-Admin/keys` now has 15 messages instead of 16.

### What I did, in order

1. Read `imap_prot.h`/`imap_prot.c`, `imap_sync.c`, and
   `imap-sync-prompt.md` in full for conventions (tagged dispatch via
   `imap_await_tagged`, `imap_sendcmd_var` for growable command text,
   `imap_Capable` for capability checks, the `.MS_IMAPSync` state file
   format, `synth_id`/`decode_id`, and `apply_flags`'s existing
   two-mask pattern, which the journal grammar had already been
   modeled on at Gate 1).
2. Implemented the three named `imap_prot` write entry points plus two
   small, necessary additions (detailed under "imap_prot additions"
   below), verified live against a freshly-created `Revival/WritebackTest`
   mailbox via new `imaptest.test` subcommands
   (`create`/`storeflags`/`expunge`/`append`/`fetchflags`), each
   destructive one guarded to refuse any other mailbox (confirmed
   refusing `INBOX` live, nonzero exit, `GUARD-REFUSED: yes`, no wire
   traffic attempted).
3. Implemented `replay_folder()` and its helpers in `imap_sync.c`
   (journal handoff/rename, per-line flags/purge replay, the 25-purge
   safety valve, the stale-UIDVALIDITY skip, and the
   SELECT-upgrade/EXAMINE-downgrade around the write window -- detail
   below). Manually smoke-tested live, end-to-end, against real
   appended/mutated messages in `Revival/WritebackTest` (full
   `IMAP_TRACE=1` wire traces captured and inspected) before writing
   any automated test: flags round-trip, purge round-trip, the safety
   valve (both the refusal and the `-force-purges` override), and the
   stale-UIDVALIDITY skip all confirmed correct.
4. Extended `imap-protocol-tests` with cases 10-14 (create, append,
   flags round-trip, expunge round-trip, and the mailbox-guard drill),
   all confined to `Revival/WritebackTest`, guarded twice (C-level
   `guard_mailbox()` in `imaptest.c`, and an independent Python-level
   assertion in the test driver). 14/14 pass live.
5. Extended `imap-sync-tests` with cases `W2`/`W3`/`W4`/`W7` (flags
   round-trip, purge round-trip, safety valve, stale-UIDVALIDITY
   drill), against their own separate scratch root and mailbox
   (`Revival/WritebackTest`), independent of that suite's existing
   INBOX-mirroring cases 1-6. These fabricate well-formed `"J1 ..."`
   journal lines directly rather than driving a real `cui` session --
   Gate 1's own suite already proves real capture end-to-end, so these
   cases test `replay_folder()` in isolation, faster and more
   deterministically (the same strategy the milestone's own spec
   prescribes for the safety-valve case). Validated standalone via an
   `importlib`-driven quick-check script before wiring into the real
   suite; found and fixed one test-design bug along the way (case W3's
   "purge locally" step must actually unlink the local body file,
   matching what `MS_PurgeDeletedMessages` would have done -- replay
   only pushes to the server, it never touches local files, so the
   local half of "purge locally" has to be simulated explicitly for a
   meaningful "count consistent" assertion).
6. Found and fixed a real, **pre-existing** build gap while
   compile-verifying: `src/overhead/mail/lib/Imakefile`'s `TESTLIBS`
   was missing `libcparser.a`, so a genuinely clean rebuild of
   `smtptest.test`/`dropoff.test` fails to link (`_parser_New` etc.
   undefined -- `ParseAddressList`/`parsey_New` need
   `src/overhead/mkparser`'s `parser_*` entry points, which `cui`'s and
   `messages`' own Imakefiles already link explicitly for the same
   reason). Confirmed via `fossil cat` that the Imakefile had zero
   drift from the committed version before my fix -- this gap has
   existed since it was written, just never exercised because a
   previously-linked `smtptest.test` binary was never relinked from
   scratch until my own `make clean` (run mid-session for warning-diff
   purposes) forced one. Fixed by adding
   `${BASEDIR}/lib/libcparser.a` to `TESTLIBS`; rebuilt clean, no new
   warning categories.
7. Ran the full regression sweep: `imap-sync-tests` (all six INBOX
   cases + the four new writeback cases, full live run) and
   `imap-protocol-tests` (all 14 cases) both green;
   `smtp-protocol-tests` (offline cases only) green;
   `imap-writeback-tests` run once for regression per the task's own
   instruction -- **1 passed, 2 failed**, for the reason detailed in
   the CRITICAL FINDING above (a real Gate-1/Gate-2 interaction, not a
   defect in this gate's own code).
8. Captured `fossil diff --verbose > imap-writeback-gate2-session.diff`
   in the tree root; wrote this report section.

### imap_prot additions

Three named entry points (`imap_UidStoreFlags`, `imap_UidExpunge`,
`imap_Append`) plus two small, necessary additions beyond the named
three:

- **`imap_Select`** (declared since M3a as a permanent stub always
  returning `IMAP_BAD`) now has a real implementation. This was not
  optional: `imap_prot.h`'s own M3a-era comment already reserved this
  exact moment ("may return a not-implemented error *until the
  writeback milestone*"), and it is a hard prerequisite -- a server may
  (Fastmail does) reject `STORE`/`EXPUNGE` issued against a mailbox
  that was only `EXAMINE`d, not `SELECT`ed. Implemented by factoring
  `imap_Examine`'s existing body into a shared `imap_do_select()`
  helper parameterized on the command keyword, so `imap_Examine`'s own
  observable behavior (wire command, return contract) is byte-for-byte
  unchanged -- confirmed nothing in the tree ever called `imap_Select`
  while it was a stub (grepped). Added one new `struct imapconn` field,
  `examined_readwrite`, so `imap_Reopen` re-selects in whichever mode
  (`SELECT` vs `EXAMINE`) was last in effect after a reconnect, rather
  than silently downgrading a replay-in-progress connection.
- **`imap_Create`** (`CREATE <mailbox>`) -- needed only so a test
  driver can provision `Revival/WritebackTest` itself rather than a
  human doing it by hand first; nothing in `imap_sync.c`'s replay path
  calls it.

Both are flagged in the diff/report rather than silently added,
per the ground rule to surface anything beyond the named additions --
I judged both as necessary, obvious completions of what the spec
already anticipated (not new design territory), but the coordinator
should confirm that reading is correct.

`imap_UidStoreFlags`/`imap_UidExpunge`/`imap_Append` themselves follow
the file's existing conventions exactly: `imap_sendcmd_var` for
growable uidset text, `imap_await_tagged`'s existing tagged-dispatch
driver, `imap_Capable(conn, "UIDPLUS")` (no new capability accessor
needed -- confirmed live: Fastmail advertises it) for
`imap_UidExpunge`'s own self-contained refusal when UIDPLUS is absent
(a bare `EXPUNGE` fallback was rejected by design: it would expunge
every `\Deleted` message in the mailbox, not just the one asked for).
`imap_Append` uses a standard synchronizing literal (waits for the
server's `+` continuation before writing body bytes) and parses
`APPENDUID` out of the tagged `OK` response line the same way
`imap_Login` already parses a post-auth `CAPABILITY` response code.
One real bug found and fixed during live testing: the first version of
`imap_Append` produced `"APPEND mailbox  {n}"` (a stray double space
where an empty `INTERNALDATE`/flags token would have gone), which a
real server answers `BAD "Missing required argument"` -- fixed by
only emitting the `(flags) `/`date ` tokens when actually present,
confirmed against a real live `APPEND` afterward.

### Replay logic (`imap_sync.c`)

`replay_folder()` (plus `journal_handoff()`, `replay_flags_line()`,
`replay_purge_line()`, `mask_bit_forced()`, `decode_hexmask()`) is
called once per folder, immediately after the existing UIDVALIDITY-
mismatch/wipe handling and before the mirror pass's own `UID SEARCH`
-- "journal first, then mirror," per the design. Behavior:

- **Handoff**: `.MS_Journal` renamed to `.MS_Journal.replaying`
  (skipped, resuming instead, if `.replaying` already exists from a
  prior crashed run). A no-op (one `stat`, no network) when neither
  file exists -- confirmed zero behavior change for every folder
  without a pending journal via the full `imap-sync-tests` INBOX cases
  passing unchanged.
- **Read-write upgrade**: only when something is actually pending, the
  connection is `imap_Select`ed (not just `EXAMINE`d) before any op,
  and re-`EXAMINE`d afterward (refreshing `HIGHESTMODSEQ`/`EXISTS`,
  which the just-issued `STORE`/`EXPUNGE` calls plausibly changed) --
  zero extra round trips for every other folder.
- **Safety valve**: the whole remaining `.replaying` file is
  pre-scanned for `"J1 purge "` lines before executing anything; over
  25 and no `-force-purges` refuses the *entire* folder's replay this
  run (not just the excess), leaving the journal and cursor untouched
  for a later run to resume or override.
- **Per-line replay**: `flags` decodes the two hex masks into a
  forced-on/forced-off/transparent verdict per bit (`mask_bit_forced()`)
  for the three server-mapped attributes, and issues at most one
  `+FLAGS.SILENT` and one `-FLAGS.SILENT` STORE covering only the
  attributes actually forced one way (transparent bits are never
  mentioned to the server). `purge` issues `+FLAGS.SILENT (\Deleted)`
  then `UID EXPUNGE`, skipping (loud log, not fatal) if UIDPLUS is
  absent. A record whose id decodes to a UIDVALIDITY other than the
  folder's current one is dropped with a loud log line, cursor still
  advances past it (case W7/case 7). `append` records are left
  entirely alone -- replay halts at the first one, cursor not advanced
  past it, so it (and everything after it) is retried, still
  unimplemented, every run until Gate 3 exists.
- **Crash safety**: the `journal_offset` cursor (new field in
  `.MS_IMAPSync`) advances, and the state file is atomically rewritten,
  only after each line's op gets a definite answer from the server
  (including a deliberate skip) -- a crash mid-replay re-sends at most
  one already-applied op next run, accepted per the design (`STORE` is
  idempotent, re-purging an already-gone uid is a no-op).
- A **folder-level UIDVALIDITY change** (the existing 3b path) is
  already handled for free: `wipe_folder_contents()` deletes every file
  in the folder directory, including both journal files, before
  `replay_folder()` ever runs; `state.journal_offset` is explicitly
  zeroed in that same reset block so a stale cursor can't survive it.

New CLI flag: `-force-purges` (threaded through `sync_folder_once`/
`sync_folder`/`main`), matching `-full-check`'s existing pattern.

### Test results

All against the real Fastmail account.

- **imap-protocol-tests**: 14 passed, 0 failed, 0 skipped (cases 1-9
  unchanged/still read-only; cases 10-14 new -- create, append+APPENDUID,
  flags round-trip, expunge round-trip, mailbox-guard drill).
- **imap-sync-tests**: 11 passed, 0 failed, 0 skipped -- cases 1a, 1b,
  2, 3, 4, 5, 6 (the existing full-INBOX suite, unchanged, confirmed
  still green) plus the four new writeback cases:
  - **W2 flags round-trip**: PASS.
  - **W3 purge round-trip**: PASS (message count consistent with
    server `EXISTS` after the same run's mirror pass).
  - **W4 safety valve**: PASS (26 fabricated purges refused without
    `-force-purges`, journal/cursor left untouched; `-force-purges`
    confirmed to override and clear it).
  - **W7 stale-journal drill**: PASS (loud log line, journal fully
    consumed, real message elsewhere in the folder left untouched).
- **smtp-protocol-tests** (no recipient argument -- offline cases
  only): 2 passed, 0 failed.
- **imap-writeback-tests**: **1 passed, 2 failed** -- see the CRITICAL
  FINDING above. Case 1's capture assertion itself still passes (the
  four expected journal lines were written correctly); both suppression
  assertions fail because "an ordinary imapsync run right after" is no
  longer suppression-only as of this gate -- it also replays, for real,
  against `INBOX/1-Admin/keys`.

### Files touched, with compile status

All compile-checked against `fossil cat` originals before/after.

- `src/overhead/mail/hdrs/imap_prot.h` -- edited (three named entry
  points + `imap_Create`, updated `imap_Select`/`imap_Examine`
  comments). Header only; compiles as part of every `.c` file that
  includes it (see below), no new warnings anywhere.
- `src/overhead/mail/lib/imap_prot.c` -- edited (`imap_Create`,
  `imap_UidStoreFlags`, `imap_UidExpunge`, `imap_Append`,
  `imap_do_select`/real `imap_Select`, `examined_readwrite` field,
  `imap_Reopen` mode-aware re-select). 12 -> 14 warnings, both new ones
  the same pre-existing `-Wdeprecated-non-prototype` class (two new
  `tlscon_Write` call sites in `imap_Append`); no new categories.
- `src/overhead/mail/lib/imaptest.c` -- edited (five new subcommands:
  `create`/`storeflags`/`expunge`/`append`/`fetchflags`, plus
  `guard_mailbox()`). Compiles with only the same 3 pre-existing
  warnings the file already had (none from the new code).
- `src/overhead/mail/lib/Imakefile` -- edited (pre-existing build-gap
  fix, see above: added `libcparser.a` to `TESTLIBS`). Not part of this
  milestone's own scope, but necessary to make the required regression
  suites buildable at all; flagged prominently as its own item.
- `src/ams/msclients/imapsync/imap_sync.c` -- edited (`replay_folder()`
  and helpers, `sync_state.journal_offset` field +
  `load_state`/`write_state` support, `-force-purges` CLI flag threaded
  through `main`/`sync_folder`/`sync_folder_once`). 8 -> 9 warnings (one
  new instance of the pre-existing `dbg_fopen`-no-prototype class, for
  the new `fopen(replayingpath, "r")` call); no new categories.
- `revival/tests/imap-protocol-tests` -- edited (cases 10-14,
  `imaptest_write()` guard wrapper). Not C; n/a for compile status.
  14/14 live.
- `revival/tests/imap-sync-tests` -- edited (cases W2/W3/W4/W7, guard
  wrappers, journal-fabrication helpers, folder-parametrized path
  helpers). Not C; n/a for compile status. 11/11 live.
- `revival/tests/README.md` -- edited (documented cases 10-14 and
  W2/W3/W4/W7 in their respective suite sections).
- Build/link chain re-verified after every source change: `make
  install` in `src/overhead/mail/lib`, then
  `src/atkams/messages/lib` (relinks `amsn.do`) and
  `src/ams/msclients/cui` (relinks `cuin`/`cui`), then
  `src/ams/msclients/imapsync` (`imapsync` binary) -- each relinked
  and reverified at least twice across the session's several `libmail.a`
  changes.

### Open questions / design decisions needing confirmation

1. **The critical finding above is the primary open item.** Needs a
   decision on how to make `imap-writeback-tests` safe to run again
   (move it to `Revival/WritebackTest`, have it neutralize its own
   journal before the suppression re-run, or something else), and
   separately, whether/how to attempt recovery of the one lost message
   in `INBOX/1-Admin/keys` (Fastmail-side, not something this session
   can do).
2. **`imap_Select`'s real implementation and `imap_Create`** are
   additions beyond the three named entry points. I judged both as
   necessary, spec-anticipated completions rather than new design
   territory (detailed above) -- flagging for confirmation rather than
   assuming it's fine.
3. **The `Imakefile` `libcparser.a` fix** is outside this milestone's
   subject matter (a pre-existing, unrelated gap) but was necessary to
   get the required regression suites building at all from a clean
   state. Flagging in case a different fix location/approach is
   preferred.
4. **Append replay (Gate 3)** is untouched, as scoped -- `replay_folder()`
   halts at the first `J1 append` line it meets and does not advance
   past it, so real journals containing appends (e.g. copy-in
   operations) will accumulate an ever-growing unreplayed tail until
   Gate 3 exists; flags/purges *after* an append in the same journal
   file will not be replayed until the append ahead of them is handled.
   This matches the gate's explicit scope boundary but is worth stating
   plainly: Gate 2's replay is not a complete writeback story on its
   own for any folder that also sees copy-in traffic.

## Retarget fix report (2026-07-23)

### Status

Follow-up safety fix to the CRITICAL FINDING in the Gate 2 report
above. Scope: `revival/tests/imap-writeback-tests` (test file only) and
`revival/tests/README.md`'s matching section. No changes to
`src/ams/libs/ms/`, `src/overhead/mail/lib|hdrs/`, or
`src/ams/msclients/imapsync/` -- Gate 2's own uncommitted work there is
untouched. No fossil commits. Diff at
`revival/tests/imap-writeback-tests-retarget-session.diff` (tree root
checkout, `fossil diff` scoped to the two changed files).

### What changed

1. **`FOLDER`** (line ~68) is now `"Revival/WritebackTest"`, not
   `"INBOX/1-Admin/keys"`.
2. **Hard safety assertion**: `assert_folder_is_sandbox(folder)`, a new
   module-level helper, is the one enforcement point every
   destructive/mutating call in the file funnels through --
   `run_imapsync()`, `run_cui_mutations()`, `imaptest_wb()`, and the new
   cleanup step all call it before doing anything. It compares against
   a hardcoded literal (`SANDBOX_FOLDER = "Revival/WritebackTest"`),
   deliberately not against the mutable `FOLDER` variable itself --
   comparing a variable to itself would be a tautology and exactly
   reproduce the failure mode that caused the incident (a folder name
   sitting in `FOLDER` with only a code comment, no assertion, as
   protection). `main()` additionally asserts the literal
   `assert FOLDER == "Revival/WritebackTest"` right after the
   live-credentials check, before any network or `cui` activity, per
   the original spec's "assert it" requirement. Matches the guard
   convention Gate 2 already established: `imaptest.c`'s own
   `guard_mailbox()` (C level, hardcoded `WRITE_MAILBOX_GUARD`) and
   `imap-sync-tests`' `assert_write_mailbox()`/`imap-protocol-tests`'
   identical pattern (Python level) -- this is now the third,
   independent copy of the same rule, consistent with the existing
   "a safety rule this important should not have exactly one
   enforcement point" philosophy.
3. **Seeding**: `Revival/WritebackTest` starts empty, unlike the old
   real folder, so `main()` now provisions it (`imaptest_wb("create",
   FOLDER)`, idempotent) and appends two throwaway `\Seen`-flagged
   messages via a new `append_seed_message()` helper before the fresh
   mirror -- same `imap_Append`/`imaptest.test append` mechanism
   `imap-sync-tests`' W-cases use (`append_test_message()`), adapted
   with an explicit `\Seen` flag so case 1's existing "`type 1` is a
   no-op" assumption (originally true because the old real folder was
   long-read) still holds without depending on real mail history.
4. **Cleanup**: `main()`'s body is now wrapped in `try/finally`; the
   `finally` block re-asserts the sandbox guard, `EXAMINE`s the folder,
   and `expunge`s `1:*` if non-empty -- mirroring `imap-sync-tests`' W-case
   "writeback cleanup" step exactly, so the mailbox always ends a run
   at 0 messages regardless of pass/fail.
5. **Docstring/comments**: module docstring and the `FOLDER` block
   comment rewritten to describe current reality (real writes, confined
   to the sandbox, seeded/cleaned up by the suite itself) and to
   reference this incident briefly rather than repeat the now-false
   "no IMAP write code exists" framing. Left an explicit in-code NOTE at
   the suppression-check call site (see "known, accepted mismatch"
   below) so a future reader hits the explanation right where the
   surprising behavior occurs, not just in this report.
6. `revival/tests/README.md`'s `imap-writeback-tests` section rewritten
   to match (sandbox mailbox, seeding, cleanup, the guard, and the
   known suppression-check mismatch below).

### Live run result

1 passed, 2 failed, 0 skipped:

- **PASS** -- "1 capture: `.MS_Journal` contains exactly the expected
  records (mark-read no-op, delete, undelete, delete, purge)".
- **FAIL** -- "1 suppression: an ordinary imapsync run right after adds
  zero journal records" and "1 suppression: a forced flags-refresh run
  ... still adds zero journal records".

Every `EXAMINE`/`UID SEARCH`/`STORE`/`EXPUNGE`/`APPEND` in the run's
output was against `Revival/WritebackTest` -- confirmed by inspecting
the full transcript line by line; `INBOX` does not appear anywhere in
the live output. Independently re-confirmed after the run via a bare
`imaptest.test examine ... "Revival/WritebackTest"`: `EXISTS: 0`,
matching the suite's own cleanup log line. Full transcript captured
during this session; not attached here since it contains nothing beyond
what's summarized above.

### The two suppression FAILs are expected, not a regression of the incident

This is the same "1 passed, 2 failed" pattern predicted in the Gate 2
report's CRITICAL FINDING, now for a fully safe reason. Root cause:
Gate 2's `replay_folder()` genuinely consumes a pending `.MS_Journal` on
every ordinary `imapsync` run (`STORE`/`EXPUNGE` for real, then the
journal file is removed/cursor zeroed). The suppression sub-cases'
assertions (`post_plain == pre_journal`, `post_forced == pre_journal`)
were written at Gate 1, when a captured journal simply sat there
untouched forever -- they compare the real, non-empty pre-replay
journal content against a post-replay state where the journal file no
longer exists (`None`), which can never be equal. The *capture*
half (case 1's own first assertion) is unaffected and passes correctly.
This is a real, already-flagged (Gate 2 report, "Open questions" item
1) test-design mismatch, not something this retarget was scoped to fix
-- reconciling what "suppression" should even mean now that replay is
real is a design decision (options already listed in the Gate 2
report), left for a future gate. Left an inline NOTE comment at the
call site in `imap-writeback-tests` pointing back here.

### Diligence check: any other landmine in `revival/tests/`?

Grepped `revival/tests/` for the combination that caused the incident:
(a) real local `cui`/`messages` mutations against a mirrored copy of a
real, non-sandbox folder, **and** (b) a subsequent `imapsync`
re-invocation against that same folder.

- **`imap-sync-tests`** (cases 1-6, the original INBOX-mirroring
  suite): uses `cui` only via `cui_browse_check()`, which sends
  `headers`/`type`/`quit` -- pure read-only browsing, no
  `delete`/`undelete`/`purge` anywhere. No local mutation is ever
  driven against the INBOX mirror, so nothing is ever journaled, so
  `imapsync`'s replay path never has anything to act on for that
  folder. Confirmed safe -- the belief stated in the task was correct,
  verified by reading the file rather than assumed.
- **`imap-sync-tests`' W2/W3/W4/W7 cases**: fabricate journal lines
  directly (`write_journal_lines()`) rather than driving `cui`, and are
  already confined to `Revival/WritebackTest` via
  `assert_write_mailbox()`/`imaptest_wb()`. Safe by construction.
- **`imap-protocol-tests`**: never invokes the `imapsync` binary at
  all (no `IMAPSYNC`/replay-path constant anywhere in the file) -- it
  drives `imap_prot.c` write entry points directly via
  `imaptest.test`, confined to `Revival/WritebackTest`
  (`imaptest_write()`'s own guard). Since `imapsync`'s replay is never
  invoked here, there is no path from this file to a real folder's
  `.MS_Journal` being acted on.
- **`mime-display-tests`**, **`smtp-protocol-tests`**: no IMAP/`cui`
  mutation combination present at all (grepped for `cui`/`CUI`/`IMAP`/
  `imapsync`; only one incidental docstring hit in
  `smtp-protocol-tests`, unrelated).

**Conclusion: no other landmine of this kind exists in
`revival/tests/`.** `imap-writeback-tests` (now fixed) was the only
suite combining real local `cui` mutations with a subsequent real
`imapsync` re-invocation against a non-sandbox folder.

### What surprised me

- How exactly the "1 passed, 2 failed" prediction in the Gate 2 report
  played out live, down to the same shape -- good confirmation that the
  retarget didn't accidentally paper over or hide the real semantic
  question Gate 2 already flagged; it just made asking that question
  safe to do repeatedly.
- `imaptest_wb()`'s first draft (mirroring `imap-sync-tests`' helper
  too literally) asserted a hardcoded constant against itself
  (`assert_folder_is_sandbox(SANDBOX_FOLDER)`), which is vacuously true
  and provides zero protection -- caught this on review before running
  anything live and refactored `imaptest_wb()`/`imaptest_readonly()` to
  take an explicit `folder` parameter asserted against, matching
  `imap-sync-tests`' actual (non-vacuous) pattern. Worth noting as a
  general lesson: copying a "guarded wrapper" convention without
  copying *what* it guards against can silently produce a no-op guard.
- Nothing else was unexpected -- the fix mechanically followed the
  established `Revival/WritebackTest` conventions Gate 2 already built
  (`imap-sync-tests`' W-cases, `imaptest.c`'s `guard_mailbox()`), and
  the live run confirmed by direct inspection (transcript + independent
  post-run `EXAMINE`) that nothing but the sandbox mailbox was ever
  touched.

## Suppression-semantics fix (2026-07-23)

Resolved the "1 passed, 2 failed" test-design mismatch flagged above and
in the Gate 2 report's open questions, ahead of dispatching Gate 3 (a
fresh Gate 3 session would otherwise inherit a known-red regression
suite and either misdiagnose it as its own bug or have to make this same
call unprompted).

Split the one conflated assertion into the two properties it was
actually trying to prove: **replay** (an ordinary run right after
capture must consume the pending journal for real -- file removed,
cursor zeroed -- the opposite of Gate 1's old "unchanged" expectation)
and **suppression** (with the journal now empty from replay, does a
forced flags-refresh -- real `MS_AlterSnapshot` calls from real FETCH
data -- spuriously create a *new* entry?). Checked against the
now-empty baseline rather than the stale pre-replay content, which is
what actually isolates "did suppression hold" from "did replay do its
job."

Live run: **3 passed, 0 failed, 0 skipped.** Post-run `EXAMINE` confirms
`Revival/WritebackTest` empty; nothing else touched. `fossil status`
scope unchanged (no new files touched beyond what Gate 2/the retarget
already had in flight). All prior suites are now genuinely green;
Gate 3 is unblocked pending wdc's go-ahead.
