# M3 Wave 2 batch B1: `atk/basics/common` — opens Wave 2, alone

Read `sonnet-playbook.md`, `rollout-procedure.md` (in full — the
"Command style" section matters for an unattended session),
`m3-rollout-runbook.md` (including its full "Findings from real
sessions" section — O1 through O4 are all there now), `m3-batches.md`,
and `porting-assessment.md` §14 and §17 (the DRIFT false-positive
write-up — you will hit exactly this pattern, see below) before
starting. Also skim `m3-o4-overhead-grabbag-REPORT.md`
in particular — it's the most recent session and the first to actually
exercise the `-pe`/`.eh` mechanic for real; you're about to do that at
much larger scale (41 classes here vs. O4's 2), so its lessons matter
directly, not just as background.

This is Wave 2's first batch, and it's a single directory run alone —
`m3-batches.md` calls it out specifically: "M1's own former
largest-blast-radius directory (41 classes, 2,351 external `.ih`
includes at M1 time); treat with the same caution even though M3's
`.eh` mechanism is directory-local." **One clarification on that
figure**: the 2,351-`.ih`-include number is about M1's *Import*-side
(`-pi`) blast radius, which is tree-wide by construction (`.ih` is
installed to `build/include`) — that work is already done and default
tree-wide, not something this session repeats. What this session does
is M3's *Export*-side (`-pe`) rollout, which `m3-rollout-runbook.md`'s
"Gate scope" section argues stays directory-local (`.eh` is never
installed, only same-directory-quote-included) — so don't read the
2,351 figure as this session's blast radius. The real reason for
caution here is simpler: 41 real classes converting in one session is
far more surface area for the kind of per-class fallout O4 just found
(twice) than any batch so far, not a blast-radius argument.

## Directory-specific findings (already done, don't redo)

Signature DB freshly rebuilt (2026-07-26, `build/desc`, same 565/566
baseline, the one failure being the known pre-existing
`contrib/atkbook/console/disk1.ch` case, not your concern). A real
dry-run (`ansify --dry-run --dir src/atk/basics/common`, 2026-07-26)
found **48 files, 0 compile failures (dry-run never compiles), 1 DRIFT,
5 skipped** — every one of the 6 already root-caused below, so there is
no open triage left for you on the census itself. (0 `-pe`-invisible
findings either — see the empty-parens check below.)

### The DRIFT and 3 of the 5 skips are all the same known false positive (`porting-assessment.md` §17)

`init__InitializeObject` (DRIFT: ".c has 2 params, .ch has 0+1"),
`keystate__InitializeObject` (skip: "no signature in DB"), and
`event__InitializeObject` (skip: "no signature in DB") are three more
instances of §17's already-documented classpp special-casing:
`InitializeObject`/`FinalizeObject` always take 2 implicit params
(`classID`, `self`) by classpp's own hardcoded convention, regardless
of what the `.ch` says (or doesn't say — all three of this directory's
`.ch` files simply don't restate `InitializeObject` at all, which is
why the DB lookup fails outright for two of them rather than
DRIFT-ing). All three `.c` definitions are **already fully typed
K&R** (split-declaration style, real types on every param) — confirmed
directly:

```c
// init.c:127
boolean init__InitializeObject(classID, init)
    struct classheader *classID;
    struct init *init;

// keystate.c:202
boolean keystate__InitializeObject(classID, self)
    struct classheader *classID;
    struct keystate *self;

// event.c:56
boolean event__InitializeObject(classID, self)
    struct classheader *classID;
    struct event *self;
```

Hand-fold each to ANSI directly (same as O4's `testobj.c`/`testobj2.c`
precedent for this exact pattern) — do this **before** running
`ansify --dir`, since `ansify`'s own method-conversion path will just
leave all three K&R (DRIFT-blocked or DB-lookup-failed):

```c
boolean init__InitializeObject(struct classheader *classID, struct init *init)
boolean keystate__InitializeObject(struct classheader *classID, struct keystate *self)
boolean event__InitializeObject(struct classheader *classID, struct event *self)
```

Verify each with a direct `make init.o`/`make keystate.o`/`make
event.o` before proceeding to the real `ansify --dir` run.

### 2 of the 5 skips are dead/orphaned methods — leave K&R, no action

`im__PlayActions` (`im.c:2719`) and `view__InsertGraphic` (`view.c:457`)
are both `skipped: no signature in DB` for a different reason than the
three above: neither is declared in its own `.ch` at all, **and neither
is ever called anywhere in the tree** (confirmed via `grep -rn
"view_InsertGraphic\b\|view__InsertGraphic\b"` and equivalent for
`PlayActions`, both empty outside their own definition). `im__PlayActions`
even carries its own `/* REMOVE THIS BEFORE FINAL CHECK-IN */` comment
directly above dead commented-out code inside it — the original 1990s
authors already knew this one was cruft. Both are already fully typed
K&R (real types on every param) — safe to convert if you want the
consistency, but there is no real requirement either way since nothing
calls them; if you do convert them, a direct hand-fold (same pattern as
above) is the only path, since `ansify`'s DB-lookup method path will
never pick them up. Not a finding worth spending more time on.

### 1 skip is a new parser-bailout shape — hand-fold it too

`gif.c:425`'s `gifin_load_cmap` (`skipped: unparseable K&R
declarations`) is a genuinely new shape for the taxonomy: a K&R
parameter declared as a **multi-dimensional array**
(`BYTE cmap[3][256];`), which the strict helper-declaration parser
doesn't recognize (every prior array-shaped or pointer-shaped gap found
so far — O2's postfix-const, O3's embedded-qualifier `char *const *`,
O4's function-pointer-returning-pointer — was about qualifier
placement or pointer nesting, never a trailing array-bounds
declarator). Already fully typed, self-healing, safe hand-fold:

```c
static int gifin_load_cmap(BYTE cmap[3][256], int ncolors)
```

Verify with `make gif.o` before proceeding.

### `-pe`-invisible empty-parens lifecycle methods: checked, none found

O4 found that a class lifecycle method (`InitializeClass`/
`InitializeObject`/`FinalizeObject`) written with **truly empty
parens** (no implicit `classID`/`self` placeholder at all) is invisible
to `ansify`'s own candidate detector — it won't show up as DRIFT or a
skip in the dry-run census at all, only as a compile failure once `-pe`
regenerates the `.eh` with a typed prototype the empty-parens
definition then conflicts with. Already checked this directory for it
(`grep -rEn "__(InitializeClass|InitializeObject|FinalizeObject)\(\)"
src/atk/basics/common/*.c`) — **zero matches**, so this specific O4
finding shouldn't recur here. Still worth re-checking yourself before
trusting a clean `-pe` regen, in case something was missed (41 files is
a lot to have checked with one grep pattern) — and stay alert generally
for `-pe`/`.eh`-regen-side fallout that doesn't show up in `ansify
--dir`'s own report, the way O4's finding didn't.

### New standing task (added after O4): grep for single-underscore macro-name typos

O4 found `overhead/cmenu/cmenu.h` had guarded a typed-declaration branch
behind `#ifdef _STDC_` (missing the second underscore — the real macro
is `__STDC__`), leaving that branch permanently dead for ~35 years and,
once fixed, exposing a second independent bug that had never been
checked because the branch had never compiled. This is now a permanent
per-batch task (`m3-rollout-runbook.md`'s O4 findings, item 3): grep this
directory for `_STDC_`, `_cplusplus`, `_FILE_`, and similar
one-underscore-short misspellings of compiler-predefined macros, and fix
any found on sight, same as ordinary compile-gate fallout — already
checked once (`grep -rn "_STDC_\b\|_cplusplus\b"
src/atk/basics/common/*.c *.h *.ch` — zero matches as of 2026-07-26),
but re-run it yourself as part of your own session in case anything
changed, and treat this as standard procedure for every directory
in every batch from here forward, not just this one.

## The `-pe`/`.eh` rollout itself

This directory has 41 `.ch` files (`find src/atk/basics/common -maxdepth
1 -name '*.ch' | wc -l`). No `CLASSFLAGS` line currently exists in
`src/atk/basics/common/Imakefile` (inherits the tree default,
`CLASSFLAGS = $(CLASSINCLUDES)`, same starting state O4's
`class/testing` had). Procedure, per `m3-rollout-runbook.md`'s
"Mechanics" section, same steps O4 validated for real:

1. Add `CLASSFLAGS = $(CLASSINCLUDES) -pe` to the Imakefile. Regenerate
   the Makefile and confirm via `grep -n CLASSFLAGS Makefile` that the
   flag landed in the `.ch.eh:` rule line (the tree-default line from
   `imake.tmpl` will still appear earlier in the file — the Imakefile's
   restatement wins, since make uses the last definition; this is the
   exact same shape O4 confirmed for `class/testing`, just now with 41
   `.ch` files instead of 2).
2. Force `.eh` regeneration for all 41 (remove the existing `.eh` files,
   or touch all `.ch` files, before the next build).
3. Apply the 6 pre-diagnosed hand-fixes above first (the 3
   `InitializeObject` folds, the `gifin_load_cmap` array-parameter
   fold, and optionally the 2 dead-code folds), verifying each with a
   direct `make <base>.o`.
4. Then run `ansify --dir src/atk/basics/common` for real. Expect
   numbers close to the dry-run baseline (48 files; 0 DRIFT/skip
   remaining, since all 6 are already hand-resolved above) — if a real
   run surfaces anything genuinely new (DRIFT, skip, or a `-pe`-regen
   conflict on a class not covered above), **stop and report before
   doing anything else**, don't try to resolve a real new finding
   yourself, same standing rule as every prior M3 batch.
5. Fix ordinary `COMPILE FAILED — restoring original` fallout as it
   comes up — yours to resolve, not escalate, unless it looks like a
   genuine semantic disagreement rather than a missing include/narrow-
   type/non-idempotency issue. Stay alert for all the previously-
   documented `ansify` limitations (O1's non-idempotent
   `fix-missing-static-decl`, O2's `(void)`/`DECLARE<N>` misparses,
   O3's embedded-qualifier-const shape, O4's function-pointer-
   returning-pointer shape and the new `-pe`-regen-side empty-parens
   shape) — all are documented workarounds in `m3-rollout-runbook.md`,
   not things to re-derive.

## Task

1. Confirm the 41 `.ch` count and the 6 pre-diagnosed findings above
   (should match exactly; flag and stop if not).
2. Grep for the O4 empty-parens lifecycle pattern and the `_STDC_`-style
   macro typo pattern yourself (both already checked clean above, but
   re-verify).
3. Apply the 6 hand-fixes, verifying each individually.
4. Do the `-pe`/`.eh` rollout (flag, regen, verify).
5. Run `ansify --dir` for real; triage any genuinely new finding per
   the standing DRIFT/skip escalation rule.
6. Fix ordinary compile-gate fallout as it arises.
7. Subtree-local gate: `make -C /Users/wdc/src/AUIS/andrew-6.4/src/atk/
   basics/common clean`, then `depend`, then `-k install` — separate
   calls, absolute paths, not chained. Rebuild clean twice to confirm
   determinism. This directory has no generated-source rule
   (`FlexOrLexFileRule`/`Parser()` — checked, neither present), so no
   extra `depend` caution needed beyond the standard sequence.
8. Do not run the tree-wide gate — `atk/basics/common` is not
   `atkams/messages/lib` or `contrib/zip/lib`, the two directories
   `m3-rollout-runbook.md` singles out for that. (A tree-wide gate at
   the *end of Wave 2* is a separate, top-level decision — not yours to
   trigger.)

## Gate

**Do NOT commit. Do NOT run any AUIS GUI/terminal binary interactively.**
This directory is linked into essentially every ATK-based binary in the
tree (it's the basics/support layer `ez`, `messages`, and everything
else builds on) — identify the real consumer set via `nm -g`/Imakefile
`LIBS` lines the same as every prior session, and give wdc the exact
runtime-check command(s) in your report. Stop after the gate is green
(twice, for determinism) and say you have stopped.

## Report

Write `m3-b1-basics-common-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance counts vs. the dry-run baseline above, and confirmation
  all 6 pre-diagnosed findings resolved as expected (or a clear
  description of any discrepancy).
- Full detail on the `-pe`/`.eh` rollout: Imakefile diff, confirmation
  the flag landed in the generated Makefile, the regen, and whether any
  new `-pe`-regen-side fallout (beyond the 6 already handled) turned up
  across the other 38 classes.
- Each hand-fix and each ordinary compile-gate fallout fix: file:line,
  what was wrong, the fix, compile confirmation.
- Exact runtime-check command(s) for wdc, with consumers identified.
- `fossil status` output confirming exactly which files changed (expect
  files only inside `atk/basics/common`, plus its own `Imakefile`), no
  commit made.
- Any new `ansify`/`-pe` fallout pattern not already described in
  `porting-assessment.md` §14/§17 or `m3-rollout-runbook.md`'s O1-O4
  findings — at this scale (41 classes), a genuinely new pattern here
  is useful signal for the rest of Wave 2 (`B2`/`B3` both have real
  class directories too).
