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
