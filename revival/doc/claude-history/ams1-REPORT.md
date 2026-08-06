# M4 Wave 6, Batch AMS1: `ams/libs/ms` STRICT_COMPILERFLAGS — REPORT

## 1. Status

**Stopped at Gate 1** (the task's only/final gate). `ams/libs/ms` builds
clean under `$(STRICT_COMPILERFLAGS)` — zero real `error:` lines,
confirmed on three separate `clean && depend && install` cycles (one
with `-ferror-limit=0 -g -O0` for full-census fixing, two more at the
directory's normal flags for determinism). `atkams/messages/lib`
(`amsn.do`) and `ams/msclients/cui` (`cuin`) both rebuilt/relinked
clean against the new `libmssrv.a`. **No fossil commit made.**
`fossil diff > ams1-session.diff` (tree root, 1161 lines) has the full
diff. 24 files touched (`Imakefile` + `prsdate.gra` + 22 `.c` files).

## 2. What I did, in order

1. Read `revival/doc/sonnet-playbook.md`, `revival/doc/m4-rollout-runbook.md`
   in full, `revival/doc/m4-batches.md`'s Wave 5/Wave 6 sections, and
   the three required directory-history briefings in full:
   `claude-history/m2/m2-amsms-REPORT.md`, `claude-history/m3/m3-ams1-REPORT.md`,
   and `grep -n "ams/libs/ms" revival/doc/porting-changelog.md` plus
   the corresponding `porting-assessment.md` §19 (LP64 variant #6,
   "wrapper vs. real-impl width drift") entries it points to.
2. Confirmed Batch 0's prerequisite was already applied and committed:
   `src/config/darwin/system.mcr` already defines
   `STRICT_COMPILERFLAGS = -std=gnu89 -Wno-return-type -Werror=implicit-int
   -Werror=int-conversion -Werror=incompatible-function-pointer-types
   -Werror=implicit-function-declaration -Werror=format` (line 26); the
   tree's `fossil status` was clean before I started (last commit
   `bd430e1d6112`, Wave 5 A1's close-out).
3. Replaced `ams/libs/ms/Imakefile`'s M2-era override
   (`COMPILERFLAGS = -std=gnu89 -Wno-implicit-int
   -Werror=implicit-function-declaration -Wno-incompatible-function-pointer-types
   -Wno-return-type`) with `COMPILERFLAGS = $(STRICT_COMPILERFLAGS)` —
   replaced, not stacked, per the runbook's mechanical note.
4. `make Makefile` to regenerate, confirmed the override at line 293
   wins over `system.mcr`'s suppressed default at line 177.
5. Fix-surfacing build: `make clean`, `make depend` (regenerates
   `prsdate.c`/`.h` from the hand-maintained `prsdate.gra` via bison +
   `mkparser`), `make -k install CDEBUGFLAGS="-ferror-limit=0 -g -O0"`.
   **First real count: 123 errors** (worse than the 74 the batch map
   listed — the census predates `-Werror=format`'s promotion, a gap
   `m4-batches.md` itself flags as the "usual pre-`-Werror=format` gap"
   also seen in Wave 5 A1, 39% over there). Breakdown: 52
   `implicit-int`, 42 `-Wformat` type-mismatch, 21
   `incompatible-function-pointer-types`, 3 `-Wformat-security`, 2
   `-Wformat` (aka-annotated), 2 `-Wformat` UB-flag, 1
   `-Wformat-extra-args`.
6. Fixed all 52 `implicit-int` sites first (§4 below), rebuilt,
   caught and fixed one self-inflicted miss (a `time_def` definition
   in `prsdate.gra` I forgot in the first pass, causing a stray
   "conflicting types" error against my own already-fixed forward
   declaration — found immediately on rebuild, fixed, confirmed on
   next build).
7. Fixed all 21 `incompatible-function-pointer-types` sites (§5) — 10
   `qsort` comparator retypings to the true `int(const void*, const
   void*)` contract (rule 2, not casts) across 5 files, plus 11
   `signal()`-handler mismatches in `init.c`, all against the single
   function `DieYouHeathenSwine` (rule 2: retyped its own definition
   from implicit-int to `void`, its true signature — see §5).
8. Fixed all remaining format errors (§6): 30 sites needing a `%d`→
   `%ld`/`%lu`/`%lld` width correction, 3 `-Wformat-security`
   non-literal-format-string sites (`"%s"` template fix), 2
   `%0.Ns`-flag UB sites (dropped the meaningless `0` flag), and one
   **genuine pre-existing bug** in `subs.c:264` (§7) — an extra,
   duplicated argument that had silently misaligned every subsequent
   format conversion.
9. Rebuilt: **0 errors, exit 0** on the very next attempt after all of
   the above (build 5 of the session).
10. Ran two more full `clean && depend && install` cycles at the
    directory's normal (non-`-O0`) flags for determinism — both clean,
    0 errors.
11. `make install` in `atkams/messages/lib` — clean, `amsn.do`
    relinked and reinstalled (confirmed via the build log's own link
    line referencing the fresh `libmssrv.a` and the `install -c -m
    0444 amsn.do .../build/dlib/atk/amsn.do` line; file timestamp
    matches this session).
12. `make install` in `ams/msclients/cui` — clean, `cuin` relinked and
    reinstalled. **Note**: this directory's own `Imakefile` already
    carries `COMPILERFLAGS = $(STRICT_COMPILERFLAGS)` — it was already
    flipped and committed in Wave 5's A1 batch (`m4-batches.md`,
    fossil `bd430e1d6112`), not something this session touched. Its
    clean rebuild here is a genuine double-confirmation: both
    directories are independently strict-clean and link against each
    other without incident.
13. `fossil status` confirms exactly 24 `EDITED` files, zero
    `ADDED`/`DELETED`, no commit. `fossil diff > ams1-session.diff` at
    the tree root.

## 3. Required directory-history briefing — what I carried in

- **M2's `ams/libs/ms` session** (`m2-amsms-REPORT.md`) fixed 1569
  implicit-function-declaration-class instances (missing prototypes,
  the `fdplumb.h` `dbg_*` wrapper gap, 5 genuine LP64 width findings
  including `conv64tolong`/`KRHash`). None of that population
  resurfaces here — M4's flag set doesn't touch
  `implicit-function-declaration` behavior this directory hadn't
  already seen (it's still in `STRICT_COMPILERFLAGS` but M2 already
  closed this directory against it).
- **M3's `ams/libs/ms` session** (`m3-ams1-REPORT.md`) ran the real
  `ansify` K&R-to-ANSI pass, found and fixed a genuinely new parser
  gap (16 silently-unconverted multi-line/multi-statement K&R
  functions), 12 stranded-declaration width conflicts (`Boolean`
  narrow-type item-8 fixes), and one genuine pre-existing bug
  (`unscrib.c`'s `UnformatMessage` calling `FreeMessageContents` with
  1 arg instead of 2, since fixed and confirmed still applied). All of
  that population is about parameter *lists*; M4's `implicit-int`
  errors this session fixed are a structurally different, previously
  invisible population — return-type-only implicit-int, which neither
  M2 nor M3's flag sets ever checked. I did not re-run `ansify` or
  redo any M3 finding; `FreeMessageContents(Msg, FALSE)` at
  `unscrib.c:163` is confirmed still in place, untouched.
- **`porting-changelog.md`/`porting-assessment.md` §19, LP64 variant
  #6 ("wrapper vs. real-impl width drift")**: the 5 known instances
  (`MS_GetConfigurationParameters`, `MS_ParseDate`, `MS_GetDirInfo`,
  `MS_GetNewMessageCount`, `MS_GetSubscriptionEntry`,
  `MS_NameChangedMapFile`) are all already-fixed, pre-existing-in-tree
  state — I did not touch any of them, and none of this session's 52
  `implicit-int` fixes overlapped with that list. I did, however, find
  **one new candidate of the same bug class** while assigning return
  types to `MS_FastUpdateState`/`MS_UpdateState` — flagged, not fixed,
  in §8 below (out of directory scope: the disagreement is with a
  `.ch` file in `atkams/messages/lib`, a different M4 batch).
- **The static-K&R-forward-declaration-missing-`static` bug class**
  (`porting-changelog.md` line 220, ~90 instances tree-wide, "most of
  `ams/libs/ms/*.c`") is a *different* shape from anything I fixed
  this session (that class is about a `static` function used before
  any forward declaration existing at all, becoming implicitly
  *external*-linkage at first use). Every `implicit-int` site I fixed
  already had a `static`-correct forward declaration in scope (only
  its *return type* was missing) — I did not encounter a new instance
  of the missing-`static` class this session; not something this
  round of flags exercises.
- **`rmvdir.c`'s already-fixed `strcat(NewPref, ':')` typo bug**: not
  re-touched, not re-broken; `rmvdir.c` was not among this session's
  edited files at all (it had zero fallout under M4's flags).
- **The M4 runbook's Phase-0 pre-flip audit** ("silent parser gap —
  CLOSED", "`FreeMessageContents` — FIXED, ruling CONFIRMED") — both
  reconfirmed still true; nothing to redo.

## 4. `implicit-int`: 52 sites, all fixed by giving the *true*
   signature, sourced from an existing same-file/cross-file
   declaration wherever one existed

None of these were cast; every one is a rule-2 "prefer fixing the
true signature" fix. Method: for every bare `Name() {` or
`static Name();` site, searched for (a) any other declaration of the
same name already in the tree (predates this session — the width/type
that decl already commits to), and (b) the function's own body for
return statements, to pick `int` vs `void` vs (in one case) the
already-established `int` for a would-be-pointer-typed function.

**19 same-file/cross-file callers already agreed on `int`** — added
`int` to the definition (and, for 4 files, to a redundant duplicate
`static Name();` forward declaration that itself lacked a type):
`disambig.c` (`ReadSubsFile` ×2), `init.c` (`ReportPermanentMallocWaste`,
`MS_ReInitialize`, `InitializeDeathSignals`), `msdir.c`
(`InitializeDirCacheState`), `mungenew.c`
(`ReallyTruly_ProcessNewMessages` ×2), `rawdb.c` (`FreeCheckLists`,
`FreeCheckList` ×2 each — one duplicate forward decl, one definition),
`rebldmap.c` (`MS_RebuildSubscriptionMaps`), `safeexit.c` (`freepag`),
`submsg.c` (`AddNamesToVector`), `subs.c` (10: `WriteSubs`,
`RefreshSubs`, `ReadSubs`, `CheckPathChanges`, `CheckSubsDuplication`,
`LockProfile`, `UnlockProfile`, `MakeSubsListInPathOrder`,
`InitializeSubsPriorities`, `CheckGlobalSubscriptions` — all 10
already had a matching `extern int Name();` forward declaration
earlier in the same file), `update.c` (`MS_Die`, `MS_FastUpdateState`,
`MS_UpdateState` — see §8 for a width caveat on the latter two),
`util.c` (`InitializeSearchPaths`).

**3 sites had zero callers anywhere in the tree** (`stack.c`'s
`Stack_MapHashes`/`Stack_MapPluses`/`Stack_MapNoChars` — confirmed via
a tree-wide grep, genuinely dead code, no `.ch`/no external
reference) — given their true signature, `void`, since none of the
three ever produces a return value and nothing could regress from the
change.

**20 sites in `prsdate.gra`** (the hand-maintained bison grammar
source — `prsdate.c` is generated by `make depend` via
`bison`/`mkparser` and is *not* fossil-tracked; edited the `.gra`,
never the generated `.c`, and re-ran `make depend` after every `.gra`
edit to regenerate before rebuilding, same discipline
`m2-amsms-REPORT.md`/`m3-ams1-REPORT.md` both used):
`parsedate` and `yearsize` got `int` (both already had a matching
`extern int` elsewhere in the tree — `msparse.c`'s
`extern int parsedate();`/`return parsedate(...)`, and `yearsize`'s
own `extern int yearsize();` a few lines above its definition, added
by M3's session). The other 18 (`peeper`, `check`, `constrain`,
`time_def`, `setrep`, `incr`, `incryear`, `incrmonth`, and
`prsdate_ErrorGuts`/`days_date`, which had no prior forward
declaration at all) got `void` — confirmed by reading every call site
tree-wide inside `prsdate.gra`: every one of these is invoked only as
a bare statement, never assigned or compared, and every one of their
bodies either has no `return` statement or only bare `return;` with
no value. `prsdate_ErrorGuts` specifically calls `longjmp(errbuf, 1)`
unconditionally — a true non-returning function, `void` is exactly
its contract.

## 5. `incompatible-function-pointer-types`: 21 sites, all fixed by
   retyping to the real signature (rule 2) — zero casts, zero
   escalations

**10 `qsort` comparator sites, 5 files.** All 10 were declared with
concrete element-pointer parameter types (`struct FileInfo *`,
`struct msglistentry *`, `char **`, etc.) instead of the real
`int(const void *, const void *)` `qsort` contract. Retyped each
comparator's parameters to `const void *`, added a local
`const TYPE *name = p;` (or, for the two double-pointer element
cases, `TYPE * const *name = p;`) inside the function body, left every
other line of logic untouched: `epoch.c`'s `dirCmp` (`char **`
elements), `rawdb.c`'s `CompareTakenUpdates`/`CompareStrings`/
`CompareTimes` (struct-pointer elements), `recon.c`'s
`MsgListEntry_CompareAMSIDs`/`MsgListEntry_CompareMIDs`/
`MsgListEntry_CompareTimes`/`SnapshotListEntry_CompareAMSIDs`
(struct-pointer elements), `subs.c`'s `CompareSubsPtrInternalPriority`
(double-pointer element, array of `struct SubscriptionProfile *`).
Checked every one for other callers/forward declarations before
touching: all are `static` (or, `CompareSubsPtrInternalPriority`,
non-static but with zero external callers found tree-wide) and every
existing forward declaration in scope was already an empty-parens
`static int Name();` unspecified-args form — compatible with any
retyping, no follow-on edits needed.

**11 `signal()` handler sites, 1 file, 1 function.** `init.c`'s
`InitializeDeathSignals`/`DieYouHeathenSwine`'s signal-installation
calls (`signal(SIGHUP, DieYouHeathenSwine)` etc., 11 call sites across
2 functions) all passed a function whose implicit-int-turned-explicit
type was `int DieYouHeathenSwine(int)`, but `signal()`'s real
prototype wants `void (*)(int)`. Checked `DieYouHeathenSwine`'s own
body: no `return` statement anywhere carries a value (two bare
`return;` mid-function, falls off the end otherwise — already
tolerated by `-Wno-return-type`), and grepped the whole tree (this
directory and `rdemo/ms` which has its own independent copy) —
`DieYouHeathenSwine` is *only* ever passed to `signal()`, never called
directly, so nothing depends on any return value. This is a genuine
~30-to-38-year-old interface bug of exactly the shape M2/M3's
experience predicts (K&R's no-signature-checking let a mistyped
signal handler compile and link for decades) — retyped the
*definition* from `int DieYouHeathenSwine(int signum)` to
`void DieYouHeathenSwine(int signum)`, the true signature. No cast
used, no separate forward declaration existed anywhere to update.

**Zero casts anywhere in this batch.** All 21 sites were genuine
signature bugs, not polymorphic dispatch-table slots — nothing here
met the bar for rule 3's cast allowance, so nothing to list for that
section.

## 6. `-Wformat`/`-Wformat-security`/`-Wformat-extra-args`: 50 sites,
   mostly mechanical `%d`→`%ld`/`%lu`/`%lld` width fixes

**~13 sites: the `AMS_ERRNO`/`AMS_ERRCAUSE`/`AMS_ERRVIA` macro family
printed via `%d`.** These macros expand to `(mserrcode >> N) & 0xFF`
expressions — `mserrcode` is `extern long`, so the whole expression's
*type* is `long` even though every value is byte-masked and always
fits in `int`'s range. Same recurring pattern Wave 5 A1's report
already named as a known-recurring shape. Fixed by changing `%d`→`%ld`
at each site (`direx.c`, `epoch.c` ×2, `msdir.c` ×2, `submsg.c`,
`subs.c`, `scavenge.c` ×2) — no cast needed, no data-loss risk (the
macros are already range-limited by construction), matches the house
style already used at dozens of other sites across this same
directory from M2/M3's own passes.

**~14 sites: an actual `long`/`unsigned long`/`off_t` variable printed
via `%d`.** `msdir.c`'s `mTime` (`unsigned long int`, ×5 across 2
distinct message texts), `rawdb.c`'s `TakenUpdates[].modtime` (`long`,
×3 identical-text sites) and a `strlen()` result (`size_t`/`unsigned
long`, ×1), `rebldmuf.c`'s `DescribeTimeInterval`'s own `interval`
parameter (`long`, ×5, one function), `util.c`'s `AMS_ERRNO` (covered
above), `logging.c`'s `statbuf.st_size - size` (`off_t`, cast to
`(long)` and `%ld` — the arithmetic already promotes to `off_t` via
usual conversions regardless of `size`'s `int` type), `rawfil.c`'s
`statbuf.st_size` (`off_t`, cast to `(long long)` and `%lld` since
`off_t` is `long long` on this platform, not `long`). All fixed by
matching the format specifier (or a minimal explicit cast +
specifier, for the two `off_t` cases) to the variable's real declared
type — verified each variable's declaration directly, not inferred
from the call site.

**1 genuine LP64 pointer-truncation bug (pattern #1), found while
diagnosing `epoch.c:405`'s `%s`-expects-`char*`-got-`int` error**:
`epoch.c`'s own local `extern int DescribeTimeInterval();` (line 46)
disagreed with the real function's actual signature,
`char * DescribeTimeInterval(long interval)` (defined in
`rebldmuf.c:227`, confirmed the one and only definition tree-wide).
This is the exact `sonnet-playbook.md` LP64 pattern #1 (undeclared/
mistyped pointer-returning function → return value truncated to
`int`, silently, no warning under K&R) — invisible until this
session's `-Wformat` check on the `%s` call site surfaced the
underlying mistyped declaration. Fixed by widening the local
declaration to `extern char *DescribeTimeInterval();`, matching the
real signature. This is a live, reachable bug: `RealEpoch` (epoch
processing on old mail directories, `Epoch on %s: [...] `) calls the
`%s`-consuming `sprintf` path whenever a folder has exactly one
message old enough to be flagged for possible deletion — on a real
LP64 build, `DescribeTimeInterval`'s returned `char *` would have been
truncated to the low 32 bits and then re-widened as a garbage pointer
by the mistyped `int`-returning declaration, a probable crash or
garbled string in that specific epoch-processing message path.

**3 `-Wformat-security` (non-literal format string) sites**: `msprint.c`'s
and `openpipe.c`'s (×2) `sprintf(buf, AndrewDir(...))` calls used
`AndrewDir()`'s return value directly as the format string (no actual
`%` conversions intended, just a path/command copy). Fixed with the
established `"%s"`-template pattern (same fix Wave 5 A1's report
already used for 3 analogous sites in `atk/help/src`).

**2 `%0.Ns`-flag UB sites**: `mswp.c`'s two `%0.700s` format
specifiers (a meaningless, UB `0` flag on an `s` conversion — same
already-known pattern `m3-ams1-REPORT.md`'s Wave-adjacent sibling
found in `helpdb.c`'s `%0.230s`). Dropped the `0` flag, leaving
`%.700s`. One of the two sites also had an independent `%d`-vs-
`strlen()`(`size_t`) mismatch, fixed alongside (`%lu`).

**1 `-Wformat-extra-args` site**: see §7, the genuine bug.

## 7. Genuine pre-existing bug found and fixed: `subs.c:264`'s
   subscription-file writer had a duplicated argument, misaligning
   every format conversion after it

```c
fprintf(fp, "mail %s %d %s %d\n", "mail", ErrTxt,
        SubsInUserOrder[i].status, SubsInUserOrder[i].time64,
        SubsInUserOrder[i].filedate);
```

Five arguments for four format conversions. Compared directly against
the structurally-identical, *correct* sibling four lines below
(`subs.c:270`, same fields, no bug):

```c
fprintf(fp, "%s %s %d %s %d\n", SubsInUserOrder[i].sname,
        SubsInUserOrder[i].key, SubsInUserOrder[i].status,
        SubsInUserOrder[i].time64, SubsInUserOrder[i].filedate);
```

and against `ReadSubs()`'s own parser (`subs.c:437-454`), which reads
exactly 5 space-separated fields per line in this exact order —
`sname key status time64 filedate`. The buggy line's format string
already spells `sname` as the literal `"mail"` (this is the
"recovered a message whose subscription name was empty, treat it as
folder `mail`" code path — reached when `home/MS_TREEROOT/mail`
exactly matches the entry's key), so the leading `"mail"` *argument*
is an erroneous duplicate of what the format string already commits
to. As written, every argument after the first was shifted one
position out of alignment: `ErrTxt` (a `char *`) landed in the `%d`
slot, `status` (an `int`) landed in the following `%s` slot (which
`printf` would try to dereference as a pointer — a near-certain
crash), `time64` (a `char[]`, decaying to `char *`) landed in the
final `%d` slot, and `filedate` (the true 5th field) was silently
dropped — this is precisely how clang's four separate diagnostics on
this one line (`int` vs `char*` ×2 directions, `char*` vs `int`, and
"data argument not used") add up. Confirmed this is real, live,
reachable code (not dead/vestigial): it's the `WriteSubs()` path that
serializes `~/.subscriptions` back to disk after silently repairing a
malformed entry — a state every long-lived user profile could
plausibly hit. Fixed by dropping the redundant `"mail"` argument
(the format string's own literal `"mail "` already supplies that
field) and correcting the trailing `%d`→`%ld` for `filedate` (a
`long` field, the same width issue as everywhere else in this
report — confirmed via `subs.c:270`'s otherwise-identical line, which
had the *identical* `%d`-vs-`long` mismatch and is fixed the same way
in §6):

```c
fprintf(fp, "mail %s %d %s %ld\n", ErrTxt,
        SubsInUserOrder[i].status, SubsInUserOrder[i].time64,
        SubsInUserOrder[i].filedate);
```

This now produces exactly 5 space-separated fields (`mail`, `ErrTxt`,
`status`, `time64`, `filedate`), matching `ReadSubs()`'s parser
exactly, and matching the sibling line's now-fixed field order and
types. **Not independently runtime-verified this session** (would
require a live `.subscriptions` file with an empty-`sname` entry
matching the treeroot's `mail` path, and this session made no writes
against any real mailbox per the playbook's IMAP-read-only rule) —
flagged in §10 for wdc.

## 8. Flagged, not fixed (out of this directory's scope): a new
   candidate for LP64 variant #6 ("wrapper vs. real-impl width
   drift") in `MS_FastUpdateState`/`MS_UpdateState`

Assigning `update.c:76`/`80`'s `MS_FastUpdateState`/`MS_UpdateState`
their first-ever explicit return type required checking every
existing claim about their width. Findings:
- The real implementation chain is unambiguous: both directly
  `return(UpdateState(...))`, and `UpdateState` itself is
  already explicitly `int UpdateState(Boolean DoEverything)`
  (typed by an M3-era item-8 fix, not something this session touched).
  `UpdateState`'s own body assigns `retcode = mserrcode` (a `long`)
  into an `int` local, but `mserrcode`'s value at that point is always
  the byte-masked `AMS_RETURN_ERRCODE` construction (same range-limited
  shape as §6's macro family) — no data loss in practice.
- Every same-directory and cross-directory caller already agrees on
  `int` or implicit-int-as-int: `direx.c`'s
  `extern int MS_FastUpdateState();`, `fatalerr.c`'s
  `extern int MS_UpdateState();`, `ams/libs/cui/andmchs.c`'s
  `extern int MS_FastUpdateState();`, `ams/msclients/cui/cui.c`'s
  `extern int MS_FastUpdateState(void);`, `ams/msclients/cui/cuifns.c`'s
  and `unixmach.c`'s same, `ams/msclients/imapsync/imap_sync.c`'s
  `extern int MS_UpdateState(void);`, `ams/msclients/nns/nns.c`'s
  implicit-int declaration.
- **But** `atkams/messages/lib`'s three `.ch` files
  (`ams.ch`/`amsn.ch`/`amss.ch`) all declare
  `MS_FastUpdateState() returns long;`/`MS_UpdateState() returns long;`,
  and their generated wrapper methods (`amsn__MS_UpdateState`,
  `amss__MS_UpdateState`, `ams__MS_UpdateState` — all real, hand-written
  `long`-returning C functions, not part of this directory) call the
  bare `MS_UpdateState()`/`MS_FastUpdateState()` C functions with **no
  prototype in scope of their own** (K&R implicit call, since that
  directory hasn't been strict-flagged yet). This is the same
  wrapper-vs-real-impl shape as the 6 already-known LP64 variant-#6
  instances in `porting-assessment.md` §19, just not yet exploited
  into a visible bug: because the caller-side `.c` files (`amsn.c`
  etc.) never got a real prototype either, their compiler currently
  assumes an `int`-shaped call (matching what I gave the real
  function) and correctly reads only the low 32 bits before widening
  to `long` for their own return — so declaring `int` here is *not*
  worse than the pre-M4 status quo, and is factually correct given the
  computation never needs more than `int` range.

**Decision**: declared both `int` (§4), matching the real
implementation and every live caller I could find, and did **not**
touch the three `.ch` files (`atkams/messages/lib` is a different M4
batch — AMS2 — not yet flagged, and `.ch` edits are out of this
directory's scope). Flagging this for whoever runs AMS2: when
`atkams/messages/lib` gets its own `STRICT_COMPILERFLAGS` pass, its
own `amsn.c`/`amss.c`/`ams.c` will need a real prototype for
`MS_UpdateState`/`MS_FastUpdateState` in scope (currently reached via
implicit-function-declaration, itself already `-Werror` tree-wide once
that directory flips) — at that point, whoever fixes it should use
`int`, matching this directory's now-real, now-int-typed
implementation, and should narrow the three `.ch` files' `returns
long` to `returns int` to match (the same "narrow `.ch` back to match
the real, unanimous-elsewhere width" resolution `porting-assessment.md`
§19 instances 3–4 already used for `MS_GetDirInfo` et al.) — **not**
widen this directory's functions to `long`, since nothing here or in
any live caller actually needs more than `int`.

## 9. Files touched (compile status)

All 24 files compile clean (0 errors, 0 new warnings beyond the
pre-existing tree-wide `-Wdeprecated-non-prototype` informational
warnings on old K&R-style declarations, confirmed present before this
session too), verified across 3 full `clean && depend && install`
cycles (1 fix-surfacing, 2 determinism):

| File | What changed |
|---|---|
| `Imakefile` | `COMPILERFLAGS` replaced with `$(STRICT_COMPILERFLAGS)` |
| `prsdate.gra` | 20 `implicit-int` fixes (18 `void`, 2 `int`) — hand-maintained bison source, not the generated `.c` |
| `direx.c` | 1 format width fix (`AMS_ERR*` macro, `%ld`) |
| `disambig.c` | 2 `implicit-int` fixes (`int`, matching existing forward decl) |
| `epoch.c` | 1 format width fix + **1 genuine LP64 pointer-truncation bug fix** (`DescribeTimeInterval`) + 1 `qsort` comparator retype |
| `init.c` | 3 `implicit-int` fixes (`int`) + **1 genuine 30-year signal-handler-type bug fix** (`DieYouHeathenSwine` → `void`) |
| `logging.c` | 1 format width fix (`off_t` cast + `%ld`) |
| `msdir.c` | 1 `implicit-int` fix (`int`) + 5 format width fixes (`%lu`) |
| `msprint.c` | 1 `-Wformat-security` fix (`"%s"` template) |
| `mswp.c` | 2 `%0.Ns`-flag UB fixes + 1 format width fix (`%lu`) |
| `mungenew.c` | 2 `implicit-int` fixes (`int`, matching existing forward decl) |
| `openpipe.c` | 2 `-Wformat-security` fixes (`"%s"` template) |
| `rawdb.c` | 4 `implicit-int` fixes (`int`) + 3 `qsort` comparator retypes + 5 format width fixes (`%lu`/`%ld` ×2 identical-text sites collapsed via `replace_all`) |
| `rawfil.c` | 1 format width fix (`off_t` cast + `%lld`) |
| `rebldmap.c` | 1 `implicit-int` fix (`int`) |
| `rebldmuf.c` | 5 format width fixes (`%ld`, one function, `DescribeTimeInterval`) |
| `recon.c` | 4 `qsort` comparator retypes |
| `safeexit.c` | 1 `implicit-int` fix (`int`) |
| `scavenge.c` | 2 format width fixes (`AMS_ERR*` macro, `%ld`) |
| `stack.c` | 3 `implicit-int` fixes (`void`, dead code, zero callers tree-wide) |
| `submsg.c` | 1 `implicit-int` fix (`int`) + 1 format width fix (`%ld`) |
| `subs.c` | 10 `implicit-int` fixes (`int`) + 1 `qsort` comparator retype + **1 genuine misaligned-`fprintf`-argument bug fix** (§7) + 2 more format width fixes (`%ld`) |
| `update.c` | 3 `implicit-int` fixes (`int`) — see §8 for the flagged-not-fixed width caveat on 2 of them |
| `util.c` | 1 `implicit-int` fix (`int`) + 1 format width fix (`%ld`) |

## 10. Downstream linkage confirmation

Per `sonnet-playbook.md`'s linkage rule (after changing `ams/libs/ms`):

- **`atkams/messages/lib`**: `make install` — clean, 0 errors.
  `amsn.do` relinked (confirmed via the build log's link-invocation
  line referencing the fresh `.../build/lib/libmssrv.a` and the
  `install -c -m 0444 amsn.do .../build/dlib/atk/amsn.do` line) and
  reindexed (`doindex` ran clean over all 16 `.do` files in that
  directory). This directory itself is **not** yet strict-flagged
  (a future M4 batch, AMS2) — I did not fix or touch any of its own
  fallout, only confirmed it still builds/links against my changed
  `ams/libs/ms` objects under its *current* (non-strict) flags.
- **`ams/msclients/cui`**: `make install` — clean, 0 errors. `cuin`
  relinked (confirmed via the build log's link line, referencing the
  fresh `libmssrv.a`) and reinstalled. **This directory is already
  strict-flagged** (Wave 5 A1, fossil `bd430e1d6112`) — its clean
  rebuild here is a genuine double confirmation that both directories
  independently satisfy `STRICT_COMPILERFLAGS` and link cleanly
  against each other, not just a "did it still build" check.

No tree-wide gate run — not required for this batch per the runbook
(only `C1`/`AMS2` are flagged for a mandatory tree-wide gate; the
Wave 6 checkpoint belongs after the wave's other 2 batches, AMS2/AMS3,
close).

## 11. Open questions / anything that surprised me

- **§8's `MS_FastUpdateState`/`MS_UpdateState` width note** is the
  main thing worth carrying forward — not a bug today, but a decision
  point for whoever runs AMS2 (`atkams/messages/lib`).
- **§7's `subs.c:264` bug is the most consequential finding this
  session** — a genuinely broken write path (a mistyped/misaligned
  `fprintf` that would have printed garbage or crashed on
  dereferencing an `int` as a `char *`) that has apparently gone
  unexercised or unnoticed for decades, reached only via the narrow
  "empty subscription name that happens to match the treeroot's
  literal `mail` path" recovery branch. Recommend the runtime check in
  §12 specifically targets this.
- **The census undercount (74 → 123, 66% over) is the second-largest
  jump seen in the M4 rollout so far** (Wave 5 A1 was 39% over) —
  entirely attributable to `-Werror=format` not being in the original
  per-directory census numbers, a gap `m4-batches.md` already
  documents as expected and structural, not specific to this
  directory.
- I did not re-run `ansify`, did not re-touch anything from M2/M3's
  already-closed findings, and made no fossil commits, per the
  prompt.

## 12. Suggested runtime checks for wdc

Per `rollout-procedure.md`'s Runtime check rules: `nm -g` first to
confirm live consumers, never launch GUI apps from this session, no
saves against unversioned fixtures.

- **Highest priority — exercise §7's `subs.c:264` fix**: this requires
  a `.subscriptions` entry whose `sname` is empty and whose
  reconstructed key exactly equals `$HOME/<MS_TREEROOT>/mail` — a
  narrow, hard-to-manufacture-safely condition. Rather than trying to
  reproduce it against a real profile, recommend a **code read-through
  confirmation only** (re-check my field-count/type reasoning in §7
  against the live `ReadSubs()` parser at `subs.c:437-454` before
  trusting the fix) — deliberately not suggesting wdc corrupt/craft a
  real `~/.subscriptions` entry to trigger this, since a mis-timed
  crash mid-write could itself damage the subscriptions file.
- **`messages`** (GUI, native Terminal.app only, never from this
  session): open a folder, read a message, check mail — exercises the
  bulk of `libmssrv.a`'s directory/message-read path through
  `amsn.do`, confirming the `implicit-int` return-type fixes (§4)
  didn't change any observable behavior.
- **Signal-handling exercise for §5's `DieYouHeathenSwine` fix**: from
  a `messages` or `cuin` session, send it `SIGINT` or `SIGTERM` (e.g.
  Ctrl-C in the controlling terminal, or `kill -TERM <pid>` from a
  *different*, non-`-9` signal — never `kill -9` per the playbook) and
  confirm it still checkpoints and exits cleanly rather than crashing
  or hanging — this is the one behavior-relevant fix in this batch
  (the others are type-only, zero behavior change).
- **`cuin`** (CLI): list folders, read a message, run a subscription
  add/remove or path-rebuild command if `cuin` exposes one — exercises
  `subs.c`'s and `epoch.c`'s fixed paths (including §6's
  `DescribeTimeInterval` LP64 fix, reachable via any "single old
  message, consider for deletion" epoch-processing message).
- **`imapsync`**: run with the environment's normal IMAP-sync
  invocation and confirm no crash or new failure — exercises the
  directory/message-store path outside a GUI, same as
  `m2-amsms-REPORT.md`'s own suggested check for this directory.
