# M4 rollout: batch plan

Companion to `m4-rollout-runbook.md`. Built 2026-08-01 from **real,
measured Phase 1 census data** (`dependInstall.log`, the corrected
full-tree run with `CDEBUGFLAGS=-ferror-limit=0` after both `system.mcr`
and the 29 M2-era per-directory overrides were fixed) — unlike
`m3-batches.md`'s first draft, this is not a proxy count built before
checking real scope.

**Numbers**: 1,778 real errors tree-wide (`implicit-int`: 1,079;
`incompatible-function-pointer-types`: 515;
`implicit-function-declaration`: 183; `int-conversion`: 0, clean) across
83 of 91 directories with `.c` files. One of those 83,
`atk/basics/lib` (35 errors), is **not a separate fix target** — its
Imakefile recompiles `atk/basics/x`'s own source files
(`../x/xim.c` etc.) into a combined library; its errors are exact
duplicates of `atk/basics/x`'s own and will close automatically once
`atk/basics/x` is fixed. Excluding that duplicate: **1,743 real errors
across 82 directories**, matching the wave totals below exactly.

**9 directories are already clean**, no batch needed: `atkams/
messages/cmd` (dead source, never compiled — see `m4-rollout-runbook.md`
Phase 0), `contrib/wpedit`/`overhead/malloc`/`inst` (confirmed inert,
same as M3), `ossupport` (1 file, compiles clean), and
`overhead/class/lib`/`overhead/class/pp`/`overhead/class/machdep/darwin`/
`overhead/class/testing` (the classpp prerequisite fix already applied
and verified during Phase 1 — see the runbook).

**Sizing is real this time**, so batches are set by actual volume, not
guessed: the eight largest single directories
(`contrib/zip/lib` 214, `atk/raster/cmd` 112, `atk/apt/tree` 87,
`overhead/mail/metamail/richmail` 81, `atk/table` 81, `ams/libs/ms` 74,
`contrib/zip/utility` 67, `atkams/messages/lib` 65) account for 781 of
the 1,743 total (~45%) and each gets its own session, matching M2/M3's
own practice for outliers. Everything else batches aggressively
(5–10 directories/session), matching M2/M3's later, high-confidence
batch sizes.

Total: **24 batches across 7 waves** (dependency order, same as M3:
overhead → atk/basics+support → atk/text → insets → apps → atkams/ams
→ contrib). No separate pilot needed this time — Phase 0's classpp fix
already serves as the proof-of-mechanics step M3's `atk/eq` pilot
provided.

## Wave 1 — overhead (22 directories, 266 errors, 5 batches)

- [ ] **O1**: `overhead/mail/metamail/richmail` (81) — alone, largest
      in this wave.
- [ ] **O2**: `overhead/eli/lib` (53), `overhead/eli/bglisp` (10) — 63.
- [ ] **O3**: `overhead/mail/metamail/metamail` (32), `overhead/mail/lib`
      (11), `overhead/mail/cmd` (9), `overhead/mail/testing` (5) — 57.
- [ ] **O4**: `overhead/bison` (17), `overhead/util/lib` (11),
      `overhead/index` (7), `overhead/mkparser` (4), `overhead/addalias`
      (4) — 43.
- [ ] **O5**: `overhead/util/cmd` (3), `overhead/sys` (3), `overhead/rxp`
      (3), `overhead/class/cmd` (3), `overhead/image/tiff` (2),
      `overhead/fonts/cmd` (2), `overhead/errors` (2), `overhead/cmenu`
      (2), `overhead/util/hdrs` (1), `overhead/image/jpeg` (1) — 22,
      small/leaf grab-bag, 10 directories.

## Wave 2 — atk/basics+support (19 directories, 381 errors, 5 batches)

- [ ] **B1**: `atk/apt/tree` (87) — alone. Biggest surprise in this
      wave — M3's own `.c`-file-count proxy estimated this directory
      as small (2 files); real M4 fallout is the third-largest single
      directory in the whole census. Worth checking why before
      assuming it's ordinary.
- [ ] **B2**: `atk/adew` (51), `atk/basics/common` (37) — 88. `atk/
      basics/common` carries M1's own former largest-blast-radius
      history — same caution as M3's B1.
- [ ] **B3**: `atk/value` (30), `atk/apt/apt` (23), `atk/textaux` (22),
      `atk/supportviews` (21) — 96.
- [ ] **B4**: `atk/support` (20), `atk/lookz` (18), `atk/textobjects`
      (17), `atk/extensions` (16), `atk/frame` (14) — 85.
- [ ] **B5**: `atk/apt/suite` (9), `atk/basics/x` (6) — **fixing this
      one closes `atk/basics/lib`'s 35 duplicate errors too, confirm
      both gates clean** — `atk/utils` (4), `atk/apps` (3), `atk/
      syntax/tlex` (1), `atk/syntax/parse` (1), `atk/syntax/sym` (1) —
      25, small/leaf grab-bag, 7 directories.

## Wave 3 — atk/text (1 directory, 57 errors, 1 batch)

- [ ] **T1**: `atk/text` (57) — alone, same as M3's own treatment.

## Wave 4 — insets (16 directories, 447 errors, 5 batches)

- [ ] **I1**: `atk/raster/cmd` (112) — alone, largest in this wave and
      second-largest overall after `contrib/zip/lib`.
- [ ] **I2**: `atk/table` (81) — alone. Historically the directory
      with the `AUXMODULE` classpp sub-case (M2) — expect similar
      class-internal-dispatch findings here.
- [ ] **I3**: `atk/figure` (48), `atk/chart` (38) — 86. `atk/figure`
      carries known LP64/DRIFT history (M1 Pilot B, M2 batch3a, M3
      i2) — expect findings, not a routine batch.
- [ ] **I4**: `atk/image` (31), `atk/fad` (29), `atk/srctext` (22) —
      82.
- [ ] **I5**: `atk/org` (19), `atk/bush` (16), `atk/rofftext` (15),
      `atk/raster/lib` (9), `atk/hyplink` (8), `atk/eq` (8, the M3
      pilot directory — clean under M3's own flags, but has real M4
      fallout like everywhere else), `atk/layout` (7), `atk/raster/
      scan` (2), `atk/raster/convert` (2) — 86, small/leaf grab-bag, 9
      directories.

## Wave 5 — apps (9 directories, 59 errors, 1 batch)

- [ ] **A1**: `ams/msclients/imapsync` (18), `ams/msclients/cui` (14),
      `atk/typescript` (13), `doc/mkbrowse` (3), `atk/help/src` (6),
      `atk/help/maint` (2), `atk/ez` (2), `ams/msclients/nns` (1) — 59,
      one batch (`atkams/messages/cmd` already clean, not included).

## Wave 6 — atkams/ams (5 directories, 185 errors, 3 batches)

- [ ] **AMS1**: `ams/libs/ms` (74) — alone, own dedicated session,
      same fdplumb-history briefing M2/M3 both required.
- [ ] **AMS2**: `atkams/messages/lib` (65) — alone. Tree-wide gate
      required (the `messages` GUI app's actual backend, same rule
      M2/M3 both applied).
- [ ] **AMS3**: `ams/libs/cui` (35), `ams/libs/nosnap` (10), `ams/libs/
      shr` (1) — 46.

## Wave 7 — contrib (11 directories, 348 errors, 4 batches)

- [ ] **C1**: `contrib/zip/lib` (214) — alone. Largest single
      directory in the entire M4 census by a wide margin; tree-wide
      gate required (tree's known highest-defect-density directory,
      same rule M2/M3 both applied).
- [ ] **C2**: `contrib/zip/utility` (67) — alone. Shares lifecycle-
      method/lt/sched classes with `zip/lib`'s history (M3 C1/C2's
      `FinalizeObject`/`InitializeObject` findings) — do after C1,
      not before, so any shared pattern is already known.
- [ ] **C3**: `contrib/mit/annot` (15), `contrib/srctext/ptext` (15),
      `contrib/eatmail` (11), `contrib/calc` (8), `contrib/time` (8) —
      57.
- [ ] **C4**: `contrib/mit/util` (5), `contrib/srctext/ltext` (3),
      `contrib/srctext/html` (1), `contrib/demos/circlepi` (1) — 10,
      small/leaf grab-bag.

## Session-count summary

24 batches, no separate pilot (Phase 0's classpp fix already proved
the mechanics), across the 7 dependency-order waves. Compare M3's
final 16-batch map for 91 directories: M4 needs more batches (24) for
fewer total directories (82) because the real distribution is far more
uneven — 8 outlier directories each get their own session versus M3's
similar treatment of only 3–4 outliers. Tick batches here as they
complete, same convention as `m1-point10-batches.md`/`m3-batches.md`.
