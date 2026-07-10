# M1 rollout point 10: batch plan and session prompts

Companion to `m1-rollout-runbook.md`. Point 10 runs as batches (one
session + one gate per batch, ruled 2026-07-09). This file is the
exhaustive batch list and the paste-ready prompt for each successor
session. Tick batches here as they complete; the roadmap's point-10
entry gets the one-line summary.

Directory sizing is the 2026-07-09 survey (external `.ih` includes /
`.ch` count). "Inert" = not in the default build (flag committed
anyway per the controllers/prefs precedent; edits there are
compile-unverified by the gate).

## Batch list

- [x] **Batch 1** (2026-07-09): atk/value, atk/adew,
      atk/apt/{apt,suite,tree}, atk/controllers (inert).
      Checkins d05f15c8cc (bug fixes), d111e78211 (rollout),
      41f1db2a29 (docs).
- [x] **Batch 2** (2026-07-10): atk/basics/wm (6/35, inert —
      `WM_ENV` off, no generated Makefile), atk/basics/x (7/40).
      Zero fallout; Imakefile-only. Checkin 154c07674a (rollout).
- [x] **Batch 3** (2026-07-10): atkams/messages/lib (17/95) — alone.
      Gate green first pass; all fallout local. Seven `.ch` drift
      fixes + one pre-authorized dual-use cast; new pattern: `proc`
      (and any unknown type token) emits an implicit-`int` cast
      param — fixed to `procedure` in ams/amsn/amss.ch and
      cross-directory in orgv.ch (fldtreev inherits it; classpp
      reads the INSTALLED parent .ch, so atk/org needed
      `make install` to propagate). messages runtime fixture
      verified by user. Checkin a106ca615e (rollout).
- [x] **Batch 4** (2026-07-10): atk/image (19/21), atk/hyplink (4/20),
      atk/console/lib (2/24), atk/console/cmd (2/0),
      atk/raster/cmd (4/24). Gate green first pass. console/lib and
      console/cmd turned out inert (`MK_CONSOLE`/`MK_BASIC_UTILS`
      off, no generated Makefile, no `console` binary) — flagged but
      compile-unverified, same as `atk/controllers`/`atk/basics/wm`.
      Two known-taxonomy fixes (image `sliderv.ch SetCallback` rock;
      hyplink `pshbttn.ch ParseRGB` array-vs-value drift), no new
      patterns. `convertraster` battery run but doesn't verify
      raster/cmd (`convrast.c` never includes its headers) —
      byte-identical regardless. Runtime: hyplink
      (`PAPERS/conf/1995/widgets.ez`) and raster/cmd
      (`NEWSLETTERS/EZ/92Sep.ez`) user-verified; image accepted
      gate-only (no known fixture). Checkins ce9be43c7e (bug fixes),
      d01f1a4958 (rollout).
- [ ] **Batch 5**: atk/chart (12/4), atk/org (3/3), atk/bush (3/3),
      atk/fad (2/0), atk/layout (6/3), atk/table (2/7).
- [ ] **Batch 6**: atk/textobjects (7/9), atk/textaux (2/0),
      atk/rofftext (4/13), atk/srctext (19/0), atk/typescript (5/7),
      atk/lookz (3/5).
- [ ] **Batch 7**: atk/ez (1/2), atk/ezprint (1/0), atk/preview
      (2/1), atk/toez (1/0), atk/utils (3/8), atk/help/src (4/4),
      atk/datacat (1/0), atk/launchapp (1/0), atk/createinset/null
      (3/5), atk/music (1/1), atk/extensions (10/0),
      atk/prefed (21/15, inert).
- [ ] **Batch 8**: atk/syntax/parse (2/13), atk/syntax/tlex (1/7),
      atk/syntax/sym (2/4), atk/ness/objects (7/31, inert),
      atk/ness/type (1/2, inert).
- [ ] **Batch 9**: atk/examples/ex1–ex19, rdemo/hide (7/9),
      rdemo/rdemosh (1/0), overhead/class/testing (2/0).
- [ ] **Batch 10**: contrib/zip/lib (21/24),
      contrib/zip/utility (6/6).
- [ ] **Batch 11**: contrib remainder — mit/{neos,annot,util},
      atkbook/* (18 dirs), champ, time, tm,
      srctext/{html,ptext,ltext}, gestures/gtext, calc, bdffont,
      alink, demos/circlepi, wpedit. Check build membership per
      directory FIRST (mit/util is built; most atkbook is not);
      split into two sessions if the built subset is heavy.

Then roadmap point 11 (classpp default flip + delete per-directory
flags) as its own final session.

## Shared prompt preamble (paste first, verbatim, for every batch)

> You are working on the AUIS revival (Fossil checkout
> ~/src/AUIS/andrew-6.4). Execute M1 rollout point 10 batch N —
> directories listed below — by following
> revival/doc/m1-rollout-runbook.md exactly. Before touching
> anything, read: the runbook; roadmap.md "M1 rollout points";
> porting-assessment.md §14 from "Pilot A findings" through "Point 10
> batch 1 findings"; and revival/doc/m1-point10-batches.md (this
> plan). Respect the runbook's hard stops — when one triggers, report
> to the user and stop rather than improvising; the user is available
> for rulings.
>
> Batch procedure reminders (all codified in the runbook, verbatim
> rules win over this summary): census-first per directory before
> flagging, including a sweep of EVERY .ch (and companion .h) in each
> directory for `(code, value)` attribute-pair macros
> (`#define X(x) code, (long)(x)`) — treev.ch hid one behind a
> one-parameter declaration. One gate for the whole batch. Local
> fallout rebuilds use `make -k install`, `make clean` first. Judge
> every build by zero `error:` lines in
> ~/src/AUIS/andrew-6.4/dependInstall.log (the ONE canonical log,
> overwritten each build), never by exit code. Never run two builds
> at once; never launch AUIS GUI apps from the harness — hand the
> user exact commands for the runtime check and stop until they
> confirm. Expect consumer fallout to arrive in rings across gate
> re-runs. Terse one-line fossil commit messages; bug-fix files,
> rollout files, and docs in separate commits (file-granular);
> fossil status before and after each.

## Per-batch prompt additions

**Batch 2** (atk/basics/wm, atk/basics/x):

> These are the window-manager and X11 graphics implementation
> classes under everything; their .ih consumers are tree-wide, so
> expect gate fallout in many directories. Runtime check: full ez +
> help regression pass (window creation, redraw, cursors, fonts,
> scrollbar drag, menus) — any rendering or input regression
> implicates this batch. Read the xim/xgraphic/xfontd notes in
> revival docs before flagging; the Xlib display-lock deadlock
> (roadmap Little Annoyances) is pre-existing — do not chase it.

**Batch 3** (atkams/messages/lib):

> messages is built with AMS_ENV/CONTRIB_ENV on against a local mbox
> store. Runtime check: launch messages per the existing fixture
> (user drives), browse folders, open a message with rich content.
> Do NOT touch ams/ delivery machinery; the flag goes on
> atkams/messages/lib only.

**Batch 4** (image, hyplink, console/lib+cmd, raster/cmd):

> raster/cmd has CLI tools: capture a convertraster before/after
> battery per the raster-pi baseline pattern
> (~/src/AUIS/test-baselines/raster-pi/run-battery.sh), baseline
> BEFORE flagging. The RF read hang is pre-existing (roadmap).
> console is a GUI app — user-driven check. hyplink runtime check
> needs a document with link insets; ask the user for a fixture.

**Batch 5** (chart, org, bush, fad, layout, table):

> chart implements the same attribute-pair convention as suite —
> the expansion ruling is already in hand (runbook signature-drift
> bullet); apply it without re-escalating. bush crashes at startup
> pre-existing (bush__InitTree overlapping strcpy, roadmap Little
> Annoyances) — startup crash is NOT a batch regression, but do not
> fix it in this batch either. There is no chart example document
> yet (roadmap); ask the user whether to author one for the runtime
> check. fad/table verified fixtures exist: Sherman.Alloc renders
> fad and table insets.

**Batch 6** (textobjects, textaux, rofftext, srctext, typescript, lookz):

> Runtime check: ez with a srctext/ctext document if one exists in
> ia-archive (ask the user), typescript via the console/typescript
> path, lookz via its lookzv panel (a lookzv caller was already
> retyped in point 7 — see §14). rofftext/textaux ride the ez check.

**Batch 7** (ez, ezprint, preview, toez, utils, help/src, datacat, launchapp, createinset/null, music, extensions, prefed):

> ezprint/ez2ascii/ez2ps are CLI: capture a before/after byte-diff
> battery over 2-3 ia-archive documents BEFORE flagging (baseline
> pattern as raster-pi). prefed is inert (MK_PREFS off). Runtime:
> ez + help launch, print-to-file battery diff.

**Batch 8** (syntax/parse, syntax/tlex, syntax/sym, ness/objects, ness/type):

> ness is not in the default build (bison extension blocker,
> roadmap) — flags there are inert but celv's only callers live in
> ness/objects (nevent.c already edited in batch 1, unverified);
> state that explicitly in the report. syntax/* is consumed by
> srctext — gate covers it; runtime rides an ez srctext document.

**Batch 9** (examples ex1–ex19, rdemo, overhead/class/testing):

> Zero-consumer mechanical sweep — 19 Imakefile flags in one pass is
> fine, but still grep each .ch set for pair macros and typeless
> declarations. Gate is the whole verification; runtime check is
> optional (ask the user whether to run one example app as a
> smoke test).

**Batch 10** (contrib/zip/lib, contrib/zip/utility):

> zip inset currently reports "not supported" at runtime (roadmap) —
> the flag still types its .ih for consumers. ltv.c and schedv.c
> were already edited in batch 1 (suite pair expansion + Create
> anchor); the gate re-verifies them. Runtime check: whatever zip
> fixture the user designates, else gate-only with user sign-off.

**Batch 11** (contrib remainder):

> FIRST determine per-directory build membership (does the directory
> have a generated Makefile / does the gate descend into it?). Flag
> everything for consistency; report the built/inert split. atkbook
> is a textbook's example code — expect crude .ch files and a high
> typeless-declaration rate. popts.c (mit/util) was already edited
> in batch 1. Gate-only verification plus one user-chosen app if any
> of these are apps the user cares about.
