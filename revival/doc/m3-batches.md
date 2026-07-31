# M3 rollout: batch plan

Companion to `m3-rollout-runbook.md`. Exhaustive directory-to-batch
map for the M3 sweep, built 2026-07-25 from the last full tree-wide
gate log (`dependInstall.log`, the `ams/libs/ms` completion gate,
238,642 lines) rather than guessing at the tree — every directory
below is confirmed ACTIVE (`grep -c "building (dependInstall)"` shows
151 directories descend; 90 of those contain at least one `.c` file at
their own level, which is what's batched here).

**Revised 2026-07-25 (wdc feedback): batches consolidated to M2's own
level of batching aggressiveness.** The first draft of this file split
24 batches — more checkpoints than M2's 13 sessions, which read as a
regression against the runbook's own "fewer subsets" argument. The
scope comparison behind that argument was never actually checked
before the first draft shipped: `grep -rl
"Werror=implicit-function-declaration" --include=Imakefile .` shows M2
only ever touched **28** directories, not the tree at large — its
census found zero violations in the other ~120 active directories, so
they needed no M2 attention at all. M3 has no equivalent narrowing:
K&R-style definitions are the default style of this whole codebase,
not an occasional defect, so all 90 directories with real `.c` content
have genuine conversion work. **The directory count is not the lever
— M2's real batches ran up to 8 unrelated small directories in one
session (M2 rollout point 2) once early sessions proved it safe; this
draft matches that aggressiveness from the start**, per the runbook's
own reasoning that `ansify`'s per-file compile gate does most of the
discovery labor a session would otherwise spend by hand.

**Sizing proxy, not a real census.** No `ansify` instance count exists
yet (that's the runbook's "first concrete step," not yet run) — the
number after each directory is its own-level `.c` file count, the same
kind of coarse proxy M1 point 10 used (external `.ih`/`.ch` counts)
before a real fixing pass ever ran. Treat batch boundaries as a
starting point: split a batch further if its real DRIFT/conversion
volume turns out large, same as M2's bucket-4 treatment of its three
biggest directories — and merge further if a wave's real volume turns
out small.

**Out of scope for now:** directories not in the 151-directory active
list (`MK_*`/site.h-gated inert subtrees — `atk/controllers`, `atk/
prefed`, `atk/console`, `atk/ezprint`, etc.) — same treatment M1 point
10 gave them (flag only if/when their macros turn on). **Excluded
entirely, not batched anywhere:** `overhead/class/pp` (`class.c` —
classpp itself, 1 file). This is the M1 codegen tool, and `ansify`'s
own signature database is built by running `class -D -N` over every
`.ch` — converting classpp with the tool that depends on it is
circular. Kept top-level/manual, same guardrail that already keeps the
M1 codegen change itself off the delegation list
(`porting-assessment.md` §14 "Delegation").

**Wave assignment is a judgment call for a few directories** — ATK's
own directory names don't cleanly encode an insets/apps/support
boundary the way `overhead`/`contrib` are self-evident. Flagged inline
below (`atk/srctext`, `atk/org`, `atk/textobjects`, `atk/apps`); a
session that finds one belongs elsewhere is free to say so and the
batch list gets corrected, not treated as a fixed contract.

**On validation cadence** (asked 2026-07-25): every batch below is one
delegated session, and every session still ends with its own runtime
check and commit — `rollout-procedure.md`'s per-session rhythm doesn't
change. Waves are not checkpoints for your visual confirmation; they
only group batches for ordering and mark where a tree-wide *build*
gate additionally runs (a compile-log check I run myself, not
something needing your hands-on testing). Some batches may turn out
gate-only with no live/GUI-visible consumer — M1 point 10 found this
repeatedly (`atk/srctext`, `dired`/`chlist` had none) — which would
drop that batch's ask to a report, not a live-app check; not
pre-decided here, each session determines it the way M1/M2 did (`nm
-g` against `runapp`/the relevant `.do`).

Total: 90 directories, ~924 own-level `.c` files (excludes
`class/pp`), **16 batches across 7 waves + the `atk/eq` pilot = 17
sessions.** Compare M2's 28 directories/13 sessions: ~3.2x the
directory count for 1.3x the sessions (5.4 dirs/session here vs. M2's
2.2), consistent with the runbook's "M3's per-instance labor is lower"
reasoning once batch size actually reflects it.

**Revised 2026-07-30 (wdc-approved amendments, after B3 closed Wave
2):** the standalone `atk/eq` pilot is retired — O4/B1/B2 proved the
`-pe`/`.eh` mechanic live at up to 81-class scale, fully serving the
pilot's purpose — and `atk/eq` (6 files) joins I2 as an ordinary
directory; I3 is merged into I1 (both leaf-scale; B3 proved a
13-dir/61-file session comfortable). New totals: 91 directories
(the original 90 + `atk/eq`), ~930 files, **15 sessions — 7 complete
(O1–O4, B1–B3), 8 remaining (T1, I1, I2, A1, AMS1, AMS2, C1, C2).**
Routine remaining batches run with a delegate-side Gate 0 replacing
full orchestrator pre-diagnosis; T1/I2/AMS1/C1 keep full
pre-diagnosis — see the runbook's "Session structure going forward"
section.

## Wave 1 — overhead (28 directories, 305 files, 4 batches)

- [x] **O1**: `overhead/util/lib` (83) — alone. Same directory M2 knew
      as the `fdplumb` wrapper-family home; expect DRIFT/complexity
      here again, not a routine batch. **Done 2026-07-25**: 0 DRIFT (no
      `.ch` files in this directory at all), 2 real compile-gate fixes
      (`encode.c`, `unscribe.c`), 14 files correctly left K&R
      (`#ifdef`-gated dead code, not reachable from the real build —
      see `m3-rollout-runbook.md` "Findings from real sessions"),
      fdplumb family itself converted clean. Committed.
- [x] **O2**: `overhead/image/jpeg` (46), `overhead/image/tiff` (32) —
      78 files. Vendored codec libraries. **Done 2026-07-26**: 0 real
      DRIFT; 5 pre-diagnosed parser bailouts plus ~30 more of the same
      vendored `DECLARE<N>(...)`-macro shape hand-fixed; found and
      fixed a real `ansify` safety-gate gap (see
      `m3-rollout-runbook.md` "Findings from real sessions" → O2).
      Runtime check found a real, confirmed-pre-existing (not a
      regression) `image` inset bug — see `roadmap.md` → Insets to
      Repair → "image". Committed.
- [x] **O3**: `overhead/mail/lib` (33), `overhead/mail/cmd` (3),
      `overhead/mail/testing` (1), `overhead/eli/lib` (25), `overhead/
      eli/bglisp` (1), `overhead/bison` (23) — 86 files, 6 dirs.
      Unrelated families bundled in one session, same shape as M2's
      rollout point 2 (8 unrelated small directories, one session).
      **Done 2026-07-26**: 0 real DRIFT; 4 pre-diagnosed `getopt`
      bailouts plus 4 ordinary compile-gate fallout fixes, one reaching
      outside the batch (`overhead/mail/hdrs/mail.h`, small/verified);
      one real ~35-year-old non-`.ch` bug found and, per wdc's ruling,
      fixed (`overhead/eli/lib/prmtives.c`'s `regcomp`/`regexec` typo —
      see `m3-rollout-runbook.md` "Findings from real sessions" → O3
      and `revival.md` "Old bugs never found till now"). Committed.
- [x] **O4**: `overhead/cmenu` (8), `overhead/mail/metamail/metamail`
      (7), `overhead/mail/metamail/richmail` (6), `overhead/malloc`
      (4), `overhead/index` (4), `overhead/class/lib` (4), `overhead/
      class/testing` (3), `overhead/class/cmd` (3), `overhead/class/
      machdep/darwin` (2), `overhead/rxp` (3), `overhead/fonts/cmd`
      (3), `overhead/util/cmd` (2), `overhead/sys` (2), `overhead/
      mkparser` (2), `overhead/util/hdrs` (1), `overhead/errors` (1),
      `overhead/addalias` (1), `ossupport` (1), `inst` (1) — 58 files,
      19 dirs, small/leaf grab-bag (class-support family here is NOT
      `class/pp` itself — see exclusion above). **Done 2026-07-26**: 0
      real DRIFT, 0 skipped tree-wide — cleanest baseline of any Wave 1
      batch. First real `-pe`/`.eh` rollout in the project
      (`overhead/class/testing`, a judgment call — see
      `m3-rollout-runbook.md` "Findings from real sessions" → O4);
      found and fixed a genuine ~35-year-old dead-branch bug
      (`overhead/cmenu/cmenu.h`'s `_STDC_`/`__STDC__` macro typo, see
      `revival.md` "Old bugs never found till now"), which in turn
      required a small fix to one external consumer
      (`atk/basics/x/xim.c`, outside this batch but verified). Two
      directories turned out entirely inert in this build rather than
      "already ANSI" (`overhead/malloc`, `inst`). **Completes Wave 1**
      (O1-O4). Committed.

## Wave 2 — atk/basics+support (19 directories, 197 files, 3 batches)

- [x] **B1**: `atk/basics/common` (48) — alone. M1's own former
      largest-blast-radius directory (41 classes, 2,351 external `.ih`
      includes at M1 time); treat with the same caution even though
      M3's `.eh` mechanism is directory-local (see runbook). **Done
      2026-07-26**: first large-scale `-pe`/`.eh` rollout (41 classes);
      all 6 pre-diagnosed dry-run findings resolved as expected; found
      and fixed a real ~35-year-old caller bug (`im.c`'s stray `*cmap`
      dereference, invisible since M1's typed-`.ih` rollout went live);
      found a new silent/unreported `ansify` parser gap (brace glued to
      last K&R parameter, see runbook) and a new cross-`.ch` rock-type
      disagreement sub-pattern, both bounded/non-silent. The parser gap
      was later fixed in the tool and retroactively cleared tree-wide
      2026-07-30 (see runbook's "ansify brace-glued parser fix" entry) —
      B1's own directory turned out to have ~20+ real methods silently
      left K&R by it, despite the batch's original "proved inert" call.
      Committed.
- [x] **B2**: `atk/value` (27), `atk/support` (20), `atk/supportviews`
      (17), `atk/adew` (13), `atk/basics/x` (11) — 88 files, 5 dirs.
      **Done 2026-07-30**: 81 classes, first M3 batch to edit `.ch`
      files (7 of them); found a real classpp `FinalizeObject`
      codegen bug and a second classpp bug (unnamed-parameter type
      loss), 6 real ~35-year-old `.ch` typos, and confirmed live (for
      the first time) B1's predicted brace-glued parser gap
      (`xgraphic.c`, 32 methods). See `m3-rollout-runbook.md` findings
      → B2 and `revival.md`/`porting-assessment.md` for full detail.
      Committed.
- [x] **B3**: `atk/extensions` (10), `atk/syntax/tlex` (8), `atk/
      textobjects` (7, judgment call — core text-embedded-object
      infra, not a content-type inset), `atk/apt/suite` (6), `atk/
      lookz` (5), `atk/frame` (5), `atk/syntax/parse` (4), `atk/apps`
      (4, judgment call — this is the `runapp`/app-loader infra:
      `runapp.c`, `pathopen.c`, `statload.c`, `genstatl.c`, not an
      application itself), `atk/utils` (3), `atk/apt/apt` (3), `atk/
      textaux` (2), `atk/syntax/sym` (2), `atk/apt/tree` (2) — 61
      files, 13 dirs. **Done 2026-07-30**: heaviest pre-diagnosis of
      any M3 batch so far — orchestrator test-compiled several `.eh`
      files under a temporary `-pe` flag before delegating and found a
      new classpp-interaction bug class (a `.ch` restating
      `InitializeClass`'s/`FinalizeObject`'s implicit param gets
      over-counted by classpp's ordinary, non-hardcoded prototype loop,
      producing a prototype/definition mismatch — 2 `InitializeClass`
      instances, 1 `FinalizeObject` instance, all fixed by simplifying
      the `.ch`). Delegate found 10 further categories of fallout
      beyond the pre-diagnosis, including 2 genuine new `ansify` tool
      bugs (a double-pointer drop, an array-bracket transposition) and
      a much larger, more dangerous variant of the brace-glued gap (32
      instances of a local variable declaration glued to the opening
      brace, silently deleted rather than just skipped) — traced back
      after the fact to a narrow regression in that same morning's
      brace-glued tool fix; confirmed via compilation that the
      corruption can never silently reach committed code (always a
      hard, unsuppressible compile error), so no retrospective recheck
      of prior batches was needed. Follow-up fix completed and
      committed same day (`8b0ce5f9`/`ef2cfa01`). See
      `m3-rollout-runbook.md` findings → B3 and
      `revival.md`/`porting-assessment.md` for full detail. Committed.

## Wave 3 — atk/text (1 directory, 30 files, 1 batch)

- [x] **T1**: `atk/text` (30) — alone, its own wave per the plan's
      dependency order (highest-consumed single directory in the
      insets/apps waves that follow). **Done 2026-07-30**: matched the
      orchestrator's pre-diagnosis exactly on the metrics that mattered
      (0 DRIFT, 3 confirmed-safe skips, all ~12 double-pointer
      parameters intact); found and fixed 2 real ~35-year-old bugs
      (`textv.ch`'s `ViewMove` declared the wrong parameter type,
      `textv.c`'s file-local `HandleSelection` was called with a stray
      extra argument at all 5 sites) and a new tooling-interaction
      pattern (3 stranded old-style forward declarations left
      incompatible by their own newly-ANSI'd narrow-parameter
      definitions — now a standing checklist item). Closes Wave 3;
      retroactive tree-wide `dependInstall` gate run clean except the
      already-known, already-queued `contrib/zip/utility/ltapp.c`
      errors (Wave 7 C2, not T1 fallout). See
      `m3-rollout-runbook.md` findings → T1 and `revival.md` for full
      detail. Committed.

## Wave 4 — insets (17 directories, 139 files, 2 batches)

Note (revised 2026-07-30): `atk/eq` is no longer a separate pilot —
it joins I2 below as an ordinary directory (wdc confirmed O4's call;
the 2026-07-08 dry-run validation plus O4/B1/B2's live `-pe` rollouts
fully served the pilot's purpose). Former I3 is merged into I1.

- [x] **I1**: `atk/image` (22), `atk/srctext` (20, judgment call — the
      roff/source-text viewer; grouped here as a content-display
      inset rather than core text infra), `atk/raster/lib` (7),
      `atk/layout` (6), `atk/hyplink` (4), `atk/org` (3, judgment
      call — outline/tree navigation, grouped as inset-adjacent
      rather than basics), `atk/bush` (3), `atk/raster/scan` (2),
      `atk/fad` (2), `atk/raster/convert` (1) — 70 files, 10 dirs
      (absorbed former I3's small/leaf grab-bag, 2026-07-30). **Done
      2026-07-30**: first batch to run the delegate-side Gate 0
      pattern for real; Gate 0 found one genuinely new item (an
      `ansify` signature-DB case-collision bug), ruled on and fixed
      before Gate 1. Gate 1 hit a Claude Code auto-mode permission
      block on the real `ansify --dir` call in the background
      delegate session (confirmed transient, cleared after a session
      resource-limit reset); the orchestrator completed Gate 1
      directly. Found and fixed 4 `ansify`/`fix-missing-static-decl`
      tool bugs (most consequential: a two-line K&R return-type-plus-
      comment idiom that reverted all 6 files in `atk/layout` at
      once) and 3 real `.ch`-vs-implementation/caller bugs
      (`atk/image/cmapv.ch` wrong restated self type,
      `atk/image/pbm.c` stray call argument, `atk/srctext/
      {asmtextv,srctextv}.ch` covariant `SetDataObject` overrides
      diverging from classpp's actual exported type). All 10
      directories gated clean, twice each; no tree-wide gate this
      batch (deferred to I2's close, per Wave 4's two-batch split).
      See `m3-rollout-runbook.md` findings → I1 and
      `claude-history/m3-i1-insets-batch1-REPORT.md` for full detail.
      Committed.
- [x] **I2**: `atk/eq` (6, formerly the standalone pilot — its
      2026-07-08 dry-run found zero DRIFT), `atk/figure` (17),
      `atk/chart` (13), `atk/table` (10), `atk/rofftext` (9),
      `atk/raster/cmd` (8) — 63 nominal (72 files edited: `eqparse.c`/
      `num.c` excluded as bison-generated, not fossil-tracked), 6
      dirs. `atk/figure` has known LP64/DRIFT history (M1 Pilot B, M2
      batch3a) — got full orchestrator pre-diagnosis, not Gate 0, per
      the flagged-risky-batch amendment. **Done 2026-07-31**: all
      three Pilot B fixes confirmed intact; found one genuinely new
      M3 fallout shape (enum prototype-scope vs. `.eh` include order,
      7 files in `atk/figure`), 8 real `.ch`-vs-implementation bugs,
      2 bounded/single-instance tool quirks (`classpp -D` blank-tag
      bug, `ansify` cast-style-K&R-function-pointer rendering bug —
      both worked around, not tool-fixed), and confirmed a
      pre-existing, out-of-scope gap (`atk/chart` never had the M2
      `COMPILERFLAGS` implicit-declaration guard). All 6 directories
      plus the Wave-4-closing tree-wide gate ran clean twice each,
      independently re-verified by the orchestrator (full from-scratch
      rebuilds of 4 of the 6 directories, spot-checked fixes against
      diff). wdc ran every suggested runtime check, all passed. See
      `m3-rollout-runbook.md` findings → I2 and
      `claude-history/m3-i2-insets-batch2-REPORT.md` for full detail.
      Committed.

## Wave 5 — apps (9 directories, 29 files, 1 batch)

- [x] **A1**: `ams/msclients/nns` (10), `atk/typescript` (5), `atk/
      help/src` (5), `ams/msclients/cui` (4), `atk/help/maint` (1),
      `atkams/messages/cmd` (1), `ams/msclients/imapsync` (1), `atk/ez`
      (1), `doc/mkbrowse` (1) — 29 files, one batch (all small/leaf).
      **Done 2026-07-31**: `-pe`/`.eh` added to the 3 `.ch` directories;
      found and fixed a more severe variant of the `ansify` signature-DB
      collision bug (identical, not just case-differing, classnames —
      a dead demo tree silently overwriting real DB entries with zero
      error); found and fixed a real, live LP64 pointer-truncation bug
      during wdc's runtime testing (`cuin`'s `dirinfo` crashed —
      `cvEng()` called with no declaration anywhere in `ams/msclients/
      cui`); corrected an earlier over-optimistic COMPILERFLAGS scope
      estimate for that same directory (141 undeclared functions, not
      a quick fix, closer to AMS1's scale). All 9 directories plus the
      wave-end tree-wide gate ran clean twice each, independently
      re-verified by the orchestrator. wdc ran every suggested runtime
      check; all passed once the `cvEng` fix landed. Closes Wave 5. See
      `m3-rollout-runbook.md` findings → A1 and
      `claude-history/m3-a1-apps-REPORT.md` for full detail. Committed.

## Wave 6 — atkams/ams (5 directories, 148 files, 2 batches)

- [ ] **AMS1**: `ams/libs/ms` (114) — alone, own dedicated session.
      Mirrors M2's own treatment: largest single directory (~38% of
      M2's total there), `fdplumb` include-order history — read that
      history first (`claude-history/fdplumb-REPORT.md`), same
      briefing the M2 runbook required.

      **Amendment (2026-07-31, wdc-approved): fold in the
      `ams/msclients/cui` COMPILERFLAGS closure as an add-on to this
      batch, not a separate session.** Found at A1: `ams/msclients/cui`
      (already `-pe`'d/ansify'd, committed) has never carried the M2-
      era `COMPILERFLAGS` implicit-declaration guard and has 141
      distinct undeclared functions across its 4 files (essentially the
      entire `MS_*`/`CUI_*` AMS API — neither `ms.h` nor a full `cui.h`
      prototype set is included). One of those 141 (`cvEng`, pointer-
      returning) was a live LP64 truncation bug, caught via a real
      `cuin` crash and already fixed standalone (`c34b1636`); the other
      140 are confirmed safe (`int`/`void`/`boolean` returns, matching
      K&R's implicit-int default, no truncation risk) — so this is now
      hardening, not live-bug-fixing. Lowest-cost path: since AMS1's
      session is already deep in `ms.h`/the real `MS_*` signatures, it
      should determine whether `ms.h`+`cui.h` can just be `#include`d
      into `ams/msclients/cui`'s 4 files without conflict, add the
      COMPILERFLAGS line, and gate — rather than a from-scratch
      standalone investigation. Not required to block AMS1's own
      close-out if it turns out bigger than expected; may be split into
      its own follow-up commit same-day (same pattern as the `atk/chart`
      follow-up) if warranted.
- [ ] **AMS2**: `atkams/messages/lib` (23), `ams/libs/shr` (7), `ams/
      libs/cui` (3), `ams/libs/nosnap` (1) — 34 files. Keep the
      tree-wide gate here too (mirrors M2's rule for `atkams/messages/
      lib` — the `messages` GUI app's actual backend).

## Wave 7 — contrib (12 directories, 81 files, 2 batches)

- [ ] **C1**: `contrib/zip/lib` (41) — alone, tree-wide gate kept
      (tree's known highest-defect-density directory, same as M2's
      rule for it).
- [ ] **C2**: `contrib/mit/annot` (9), `contrib/zip/utility` (6),
      `contrib/time` (6), `contrib/mit/util` (6), `contrib/srctext/
      html` (3), `contrib/srctext/ptext` (2), `contrib/srctext/ltext`
      (2), `contrib/demos/circlepi` (2), `contrib/calc` (2), `contrib/
      wpedit` (1), `contrib/eatmail` (1) — 40 files, 11 dirs.

## Session-count summary

Revised 2026-07-30: 15 sessions total (was 17 — pilot retired into
I2, I3 merged into I1), across the 7 waves in dependency order.
11 complete (O1–O4, B1–B3, T1, I1, I2, A1 — Waves 1-5 now closed), 4
remaining: AMS1, AMS2, C1, C2. Tick batches here as they
complete, same convention as `m1-point10-batches.md`; the runbook's
own summary gets the one-line rollup per wave, not per batch.
