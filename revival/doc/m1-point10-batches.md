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
- [x] **Batch 5** (2026-07-10): atk/chart (12/4), atk/org (3/3),
      atk/bush (3/3), atk/fad (2/0), atk/layout (6/3), atk/table
      (2/7). Gate green first pass. chart carried the suite-identical
      variadic-by-macro attribute family (ruling already in hand,
      applied without re-escalating) plus a signature-drift typo
      (`struct char *` → `struct chart *`) and typeless declarations
      (WhichItem/SetChartOptions/HitChart/ObserveChart, the latter
      repeated across 5 subclasses); ~45 dispatch call sites
      mechanically rewritten, all local to atk/chart. Two dual-use
      attribute-value casts missing at `PrintString` call sites,
      caught by chart's local rebuild. org.ch's typeless
      `NodeName(node)` had zero callers — zero-risk retype. bush,
      fad, layout, table were zero-fallout. Runtime: chart verified
      interactively; fad+table verified via Sherman.Alloc. Two
      pre-existing bugs surfaced by first-ever runtime tests (not
      regressions, logged under roadmap Little Annoyances): org
      crashes loading a file (same overlapping-strcpy-under-fortify
      class as bush's already-logged crash); Sherman.Alloc's complex
      layout inset has excess whitespace margin (zero atk/layout
      files touched). Checkins b73958943b (bug fixes), 79a7de90d1
      (rollout).
- [x] **Batch 6** (2026-07-10): atk/textobjects (7/9), atk/textaux (2/0),
      atk/rofftext (4/13), atk/srctext (19/0), atk/typescript (5/7),
      atk/lookz (3/5). Gate green first pass, no attribute-pair macros
      anywhere. Three genuine drift fixes: rofftxta.ch's two typeless
      ParseArgs/InitializeObject declarations; hlptext.ch and
      rawtextv.ch each had InitializeObject/FinalizeObject typed to
      the wrong sibling struct; dired.ch's Enumerate rock (long→void*,
      all callers pointers); chlist.ch's AddItemAtIndex had a
      ~35-year transposed-parameter drift; srctext.ch's Lookup was
      missing `**` on its hash-table param (sibling classprocs had it
      right). textaux, typescript zero-fallout. Runtime: lookz via
      PAPERS/atk/Hansen.Algebra, contentv (Table of Contents) via
      PAPERS/atk/Cattey.Writing, help app (panel) no regression;
      srctext + dired/chlist/unknown accepted gate-only (no fixture
      exists for srctext anywhere in ia-archive/PAPERS/NEWSLETTERS;
      dired/chlist/unknown have no live consumer in the default
      build). typescript crashes on launch — new pre-existing bug
      (Create doesn't NULL-check New() after GetPtyandName fails),
      zero atk/typescript files touched so not a regression; logged
      under roadmap Little Annoyances. Checkins 135deef87d (bug
      fixes), 1bf4c6384e (rollout).
- [x] **Batch 7** (2026-07-10, live subset only): atk/ez, atk/utils,
      atk/help/src, atk/extensions. Gate green first pass. 8 of the
      12 originally-planned directories (ezprint, preview, toez,
      datacat, launchapp, createinset/null, music, prefed) turned out
      to be currently inert — MK_BASIC_UTILS/MK_AUTHORING/MK_AUX_UTILS
      all off, no per-app override defined, none in SUBDIRS, no
      generated Makefile — deferred to **Batch 7b** (below) rather
      than flipping the macros on. ez2ascii/ez2ps in build/bin turned
      out to be contrib/mit/util (batch 11) + CONTRIB_ENV leftovers,
      not built from atk/ezprint at all, so the planned CLI byte-diff
      battery had no live target either. Six drift fixes on the live
      4: dialog.ch/dialogv.ch InitializeObject/FinalizeObject wrong
      sibling struct (struct sbutton*); help/src/hlptextv.ch same
      pattern (struct srctextview* — coincidentally similar name to
      batch 6's srctext/hlptext.ch, different file); help.ch/helpdb.ch
      InitializeClass bogus extra self param; ezdiff.ch FinalizeObject
      missing its self param entirely; dialogv.ch PostInput choicerock
      rock idiom (long->void*, sole caller frame.c:1746 passes a
      pointer). Confirmed struct thisobject *self (pervasive idiom,
      dozens of files) resolves to void* even under -pi — not a bug.
      Runtime: ez launch + Extensions command, Quit-with-unsaved-changes
      dialog (exercises the dialog/dialogv fix + PostInput rock
      directly), help launch with hlptextview-rendered topic + working
      hyperlinks — all user-verified, no regressions. Checkins
      105b96414a (bug fixes), 165e3862b6 (rollout).
- [x] **Batch 7b** — closed 2026-07-10 by the obsolete-inert-flagging
      ruling (below): atk/ezprint, atk/preview, atk/toez, atk/datacat,
      atk/launchapp, atk/createinset/null, atk/music, atk/prefed —
      all inert (MK_BASIC_UTILS/MK_AUTHORING/MK_AUX_UTILS off). No
      flagging needed ever; runbook census happens if/when their
      MK_* macros are turned on.
- [x] **Batch 8** (2026-07-10, live subset: atk/syntax/parse,
      atk/syntax/tlex, atk/syntax/sym; ness/{objects,type} dropped —
      inert, bison blocker, obsolete-inert-flagging ruling below).
      First Sonnet-delegated batch (background agent, runbook-driven;
      stopped at the pre-commit line as instructed). Gate green first
      pass, zero real errors. Six drift fixes, all known taxonomy:
      lexan.ch + tlex.ch InitializeObject/FinalizeObject wrong-sibling
      struct (`struct parse *`); tlex.ch RecentPosition/RecentIndent/
      Repeat `int`→`long index`; sym.ch five classprocs
      `unsigned char *`→`char *name`; sym.ch FindAll rock `long`→
      `long *` (impl truth); parse.ch Create error handler
      `int (*)()`→`void (*)()`. All tlex/sym callers live in inert
      atk/ness — zero live call sites. HARD STOP correctly escalated
      and ruled: lexan.c ParseNumber passed `&len` (`long`) for
      TransEscape's `int *plen` — live LP64 bug (upper-half garbage
      into `buf[len-1]`); fixed with an `int` temporary, separate
      live-bug commit per clockv/htmlview precedent. Runtime:
      user-verified ctext syntax coloring + indentation on a scratch
      `.c` copy in ez. Checkins: .ch fixes d3386126d5, lexan.c fix
      7ad519b869, rollout 6b1564ec89.
- [x] **Batch 9**: atk/examples/ex1–ex19, rdemo/hide (7/9),
      rdemo/rdemosh (1/0), overhead/class/testing (2/0). Done
      2026-07-10, live subset only: pre-flag census found all 19
      examples inert (`MK_EXAMPLES` off, no per-app override) and
      both rdemo dirs entirely outside the build tree (`rdemo` isn't
      referenced anywhere in `src/Imakefile`'s `SUBDIRS` — a
      standalone package with its own `config.csh`, never touched by
      `make dependInstall`). Only `overhead/class/testing` is live.
      User decision: split, same as batch 7 — flag/verify the live
      dir now, defer the rest as Batch 9b. Both `.ch` files
      (testobj.ch, testobj2.ch) were already clean (no
      InitializeObject/FinalizeObject, no pair macros, no typeless
      params) — zero fixes needed, purely a flag-and-gate. Gate green
      first pass; confirmed real typed casts in the local `.ih`
      files (this dir has no InstallClassFiles, so they're never
      copied to build/include). Runtime check skipped by user choice
      — the only artifact, `testmain`, is a class-loader self-test
      that ends in `while(1);` and isn't part of the normal
      install/dependInstall path, matching the batch's own
      "gate is the whole verification" guidance. Checkin:
      rollout-only (no bug-fix commit needed) — see roadmap.md.
- [x] **Batch 9b** — closed 2026-07-10 by the obsolete-inert-flagging
      ruling (below): atk/examples/ex1–ex19, rdemo/hide,
      rdemo/rdemosh. No flagging needed ever; runbook census happens
      if `MK_EXAMPLES` is turned on (rdemo would additionally need
      wiring into `src/Imakefile`'s `SUBDIRS` and a `config.csh` run
      to generate `config.h` — it has never been part of this
      checkout's build at all).
- [ ] ~~**Batch 10**: contrib/zip/lib, contrib/zip/utility.~~
      **OBSOLETE (2026-07-10 census):** zip is entirely inert —
      `contrib/Imakefile` gates it on `MK_ZIP`, which nothing
      defines; it has never been in this checkout's build (this is
      the root cause of the runtime "zip unsupported" message). The
      batch-1 edits to `ltv.c`/`schedv.c` remain compile-unverified
      — no gate has ever compiled them, contrary to this batch's
      original prompt. Superseded by roadmap → Insets to Repair →
      zip: enabling `MK_ZIP` + first build + runbook census is one
      task there, and after point 11 no Imakefile flag is needed.
- [ ] **Batch 11** (recensused 2026-07-10 from the gate log; the
      old "mit/util … CONTRIB_ENV off" note was a mis-census —
      CONTRIB_ENV is ON and contrib builds): live subset is exactly
      8 directories — contrib/mit/annot, contrib/mit/util,
      contrib/srctext/{html,ptext,ltext}, contrib/time,
      contrib/wpedit, contrib/demos/circlepi. Note
      srctext/html's htmlview.c already had its DisplayString
      transposition fixed 2026-07-09; expect similar 1990s-era drift
      density here. Inert remainder (atkbook ×18, mit/neos, calc,
      champ, gestures/gtext, tm, bdffont, alink) is dropped per the
      ruling below. Beware stale Makefiles: atkbook/tm/bdffont have
      generated Makefiles left over from before they were
      conditionalized out — liveness comes from the gate log, not
      Makefile presence. Sonnet-delegable.

**Inert flagging is obsolete (ruled 2026-07-10):** once roadmap
point 11 flips the classpp default and deletes the per-directory
flags, no Imakefile flag is ever needed again — any inert subtree
enabled later (zip, ness, examples, ezprint, ...) gets typed casts
automatically, and the runbook census/fix work happens at enable
time as part of turning the gate on. Batches 7b and 9b and the
inert parts of 8/10/11 are closed on that basis; their directory
lists stay recorded in their entries above for whoever enables the
gates. M1's finish line is therefore: batch 8 (live) → batch 11
(live) → point 11.

Then roadmap point 11 (classpp default flip + delete per-directory
flags + full gate + ez/help/messages regression battery) as its own
final session — top-level, not delegable (touches
overhead/class/pp, a runbook hard-stop area). **Point 11 closes M1.**

## Delegation guide (2026-07-10)

- Batches 8 and 11 (live subsets): Sonnet-class sessions using the
  preamble + per-batch prompts below. All fallout patterns they can
  hit are documented in the runbook taxonomy; hard stops go to the
  user as usual.
- Static censuses inside a batch (pair-macro sweep, call-site
  classification for a rock retype): Haiku-class, given exact
  file:line + expected text + skip-and-report-on-mismatch, per the
  runbook's methodology note.
- Point 11 and any `.ch`-vs-impl disagreement rulings: top-level
  (operator) only.

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

**Batch 8** (syntax/parse, syntax/tlex, syntax/sym — live subset only):

> ness/{objects,type} are OUT of this batch (inert, bison blocker;
> see the obsolete-inert-flagging ruling) — note in the report that
> celv's only callers live in ness/objects and batch 1's nevent.c
> edit stays compile-unverified until ness is enabled. syntax/* is
> consumed by srctext — the gate covers consumers. Runtime: no
> srctext document exists in the archives (batch 6 finding); open a
> SCRATCH COPY of a small `.c` file in ez (filetype maps `.c` to
> ctext, which drives syntax parse/tlex/sym) and confirm syntax
> coloring and indentation — never point ez at a file under src/.

**Batch 9** (examples ex1–ex19, rdemo, overhead/class/testing):

> Zero-consumer mechanical sweep — 19 Imakefile flags in one pass is
> fine, but still grep each .ch set for pair macros and typeless
> declarations. Gate is the whole verification; runtime check is
> optional (ask the user whether to run one example app as a
> smoke test).

**Batch 10** — OBSOLETE, do not run (see batch list): zip has never
been in the build (`MK_ZIP` undefined), so there is nothing to flag
or gate. The old claim that "the gate re-verifies" batch 1's
ltv.c/schedv.c edits was wrong — they have never been compiled.
Superseded by roadmap → Insets to Repair → zip (enable `MK_ZIP` +
first build + runbook census as one task).

**Batch 11** (contrib live subset: mit/annot, mit/util,
srctext/{html,ptext,ltext}, time, wpedit, demos/circlepi):

> Membership is already censused (2026-07-10, from the gate log) —
> flag ONLY the 8 live directories listed; the inert remainder
> (atkbook etc.) is closed by the obsolete-inert-flagging ruling, do
> not flag it. popts.c (mit/util) was edited in batch 1 and has been
> compiling in every gate since — already verified. Runtime/CLI
> checks: `ez2ascii`/`ez2ps` (mit/util) are CLI — capture a
> before/after byte-diff battery over 2-3 ia-archive documents
> BEFORE flagging (raster-pi baseline pattern); htmlview rides an
> HTML file opened in ez (its DisplayString fix of 2026-07-09 gives
> a visible status-message check); time/wpedit/annot/circlepi are
> user-driven GUI checks — ask the user which they care to exercise,
> gate-only with sign-off for the rest.
