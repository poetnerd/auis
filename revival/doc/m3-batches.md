# M3 rollout: batch plan

Companion to `m3-rollout-runbook.md`. Exhaustive directory-to-batch
map for the M3 sweep, built 2026-07-25 from the last full tree-wide
gate log (`dependInstall.log`, the `ams/libs/ms` completion gate,
238,642 lines) rather than guessing at the tree — every directory
below is confirmed ACTIVE (`grep -c "building (dependInstall)"` shows
151 directories descend; 90 of those contain at least one `.c` file at
their own level, which is what's batched here).

**Sizing proxy, not a real census.** No `ansify` instance count exists
yet (that's the runbook's "first concrete step," not yet run) — the
number after each directory is its own-level `.c` file count, the same
kind of coarse proxy M1 point 10 used (external `.ih`/`.ch` counts)
before a real fixing pass ever ran. Treat batch boundaries as a
starting point: split a batch further if its real DRIFT/conversion
volume turns out large, same as M2's bucket-4 treatment of its three
biggest directories.

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

Total: 90 directories, ~924 own-level `.c` files (excludes
`class/pp`), 24 batches across 7 waves. Compare M2's 29 directories/13
sessions — roughly 3x the directory count and 4x the batch count, but
each M3 session should be cheaper per file than M2's (tool-driven
conversion + auto-restore vs. M2's hand grep-and-declare), per the
runbook's reasoning.

## Wave 1 — overhead (28 directories, 305 files, 8 batches)

- [ ] **O1**: `overhead/util/lib` (83) — alone. Same directory M2 knew
      as the `fdplumb` wrapper-family home; expect DRIFT/complexity
      here again, not a routine batch.
- [ ] **O2**: `overhead/image/jpeg` (46), `overhead/image/tiff` (32) —
      78 files. Vendored codec libraries, natural pairing.
- [ ] **O3**: `overhead/mail/lib` (33), `overhead/mail/cmd` (3),
      `overhead/mail/testing` (1) — 37 files.
- [ ] **O4**: `overhead/eli/lib` (25), `overhead/eli/bglisp` (1) — 26
      files.
- [ ] **O5**: `overhead/bison` (23) — alone. Vendored parser generator,
      unusual code shape; don't fold into a routine batch.
- [ ] **O6**: `overhead/cmenu` (8), `overhead/mail/metamail/metamail`
      (7), `overhead/mail/metamail/richmail` (6), `overhead/malloc`
      (4), `overhead/index` (4) — 29 files.
- [ ] **O7**: `overhead/class/lib` (4), `overhead/class/testing` (3),
      `overhead/class/cmd` (3), `overhead/class/machdep/darwin` (2) —
      12 files. (Class-support family — NOT `class/pp` itself, see
      exclusion above.)
- [ ] **O8**: `overhead/rxp` (3), `overhead/fonts/cmd` (3), `overhead/
      util/cmd` (2), `overhead/sys` (2), `overhead/mkparser` (2),
      `overhead/util/hdrs` (1), `overhead/errors` (1), `overhead/
      addalias` (1), `ossupport` (1), `inst` (1) — 17 files, small/leaf
      grab-bag.

## Wave 2 — atk/basics+support (19 directories, 197 files, 5 batches)

- [ ] **B1**: `atk/basics/common` (48) — alone. M1's own former
      largest-blast-radius directory (41 classes, 2,351 external `.ih`
      includes at M1 time); treat with the same caution even though
      M3's `.eh` mechanism is directory-local (see runbook).
- [ ] **B2**: `atk/value` (27), `atk/support` (20) — 47 files.
- [ ] **B3**: `atk/supportviews` (17), `atk/adew` (13), `atk/basics/x`
      (11) — 41 files.
- [ ] **B4**: `atk/extensions` (10), `atk/syntax/tlex` (8), `atk/
      textobjects` (7, judgment call — core text-embedded-object
      infra, not a content-type inset), `atk/apt/suite` (6) — 31
      files.
- [ ] **B5**: `atk/lookz` (5), `atk/frame` (5), `atk/syntax/parse` (4),
      `atk/apps` (4, judgment call — this is the `runapp`/app-loader
      infra: `runapp.c`, `pathopen.c`, `statload.c`, `genstatl.c`, not
      an application itself), `atk/utils` (3), `atk/apt/apt` (3),
      `atk/textaux` (2), `atk/syntax/sym` (2), `atk/apt/tree` (2) — 30
      files, small/leaf grab-bag.

## Wave 3 — atk/text (1 directory, 30 files, 1 batch)

- [ ] **T1**: `atk/text` (30) — alone, its own wave per the plan's
      dependency order (highest-consumed single directory in the
      insets/apps waves that follow).

## Wave 4 — insets (16 directories, 133 files, 5 batches)

Note: `atk/eq` is deliberately NOT re-listed here as a batch — it's
already the runbook's designated rollout-point-1 pilot (re-running the
2026-07-08 dry-run validation for real, see `m3-rollout-runbook.md`),
so it runs first and separately, not folded into I4.

- [ ] **I1**: `atk/image` (22), `atk/srctext` (20, judgment call — the
      roff/source-text viewer; grouped here as a content-display
      inset rather than core text infra) — 42 files.
- [ ] **I2**: `atk/figure` (17), `atk/chart` (13) — 30 files. `atk/
      figure` has known LP64/DRIFT history (M1 Pilot B, M2 batch3a) —
      expect findings, not a routine batch.
- [ ] **I3**: `atk/table` (10), `atk/rofftext` (9), `atk/raster/cmd`
      (8) — 27 files. Same three-directory grouping M2's bucket 4
      used.
- [ ] **I4**: `atk/raster/lib` (7), `atk/layout` (6) — 13 files. (`atk/
      eq`, 6 files, runs separately as the pilot — see note above.)
- [ ] **I5**: `atk/hyplink` (4), `atk/org` (3, judgment call — outline/
      tree navigation, grouped as inset-adjacent rather than basics),
      `atk/bush` (3), `atk/raster/scan` (2), `atk/fad` (2), `atk/
      raster/convert` (1) — 15 files, small/leaf grab-bag.

## Wave 5 — apps (9 directories, 29 files, 1 batch)

- [ ] **A1**: `ams/msclients/nns` (10), `atk/typescript` (5), `atk/
      help/src` (5), `ams/msclients/cui` (4), `atk/help/maint` (1),
      `atkams/messages/cmd` (1), `ams/msclients/imapsync` (1), `atk/ez`
      (1), `doc/mkbrowse` (1) — 29 files, one batch (all small/leaf).

## Wave 6 — atkams/ams (5 directories, 148 files, 2 batches)

- [ ] **AMS1**: `ams/libs/ms` (114) — alone, own dedicated session.
      Mirrors M2's own treatment: largest single directory (~38% of
      M2's total there), `fdplumb` include-order history — read that
      history first (`claude-history/fdplumb-REPORT.md`), same
      briefing the M2 runbook required.
- [ ] **AMS2**: `atkams/messages/lib` (23), `ams/libs/shr` (7), `ams/
      libs/cui` (3), `ams/libs/nosnap` (1) — 34 files. Keep the
      tree-wide gate here too (mirrors M2's rule for `atkams/messages/
      lib` — the `messages` GUI app's actual backend).

## Wave 7 — contrib (12 directories, 81 files, 3 batches)

- [ ] **C1**: `contrib/zip/lib` (41) — alone, tree-wide gate kept
      (tree's known highest-defect-density directory, same as M2's
      rule for it).
- [ ] **C2**: `contrib/mit/annot` (9), `contrib/zip/utility` (6),
      `contrib/time` (6), `contrib/mit/util` (6) — 27 files.
- [ ] **C3**: `contrib/srctext/html` (3), `contrib/srctext/ptext` (2),
      `contrib/srctext/ltext` (2), `contrib/demos/circlepi` (2),
      `contrib/calc` (2), `contrib/wpedit` (1), `contrib/eatmail` (1)
      — 13 files, small/leaf grab-bag.

## Session-count summary

Pilot (`atk/eq`, already validated, re-run for real) + 24 batches = 25
sessions total, across the 7 waves in dependency order (Wave 1 →
Wave 6, with the eq pilot opening Wave 4). Tick batches here as they
complete, same convention as `m1-point10-batches.md`; the runbook's
own summary gets the one-line rollup per wave, not per batch.
