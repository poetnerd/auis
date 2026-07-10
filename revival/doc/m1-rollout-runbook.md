# M1 rollout runbook: enabling `-pi` typed dispatch per directory

Procedure for executing one "M1 rollout point" from `roadmap.md`
(Medium-term → ANSI C conversion → M1 rollout points). Written after
pilots on `atk/eq`, `atk/figure`, and `atk/raster/lib` (points 2–4);
the fallout taxonomy below is empirical, from those directories.
Background and rationale: `porting-assessment.md` §14.

One directory per session. Never run two builds at once.
(Point-10 amendment, ruled 2026-07-09: for the breadth sweep, a batch of
related directories may share one session and one gate — census-first
per directory still applies, and builds remain strictly serial.)

## What the flag does

`classpp` (`class`) generates `.ih`/`.eh` headers from `.ch` class
definitions. By default the dispatch macros cast method pointers to
typeless `(void (*)())`. With `-pi`, casts carry the full ANSI
signature from the `.ch`, so every caller of every method in the
directory's classes is type-checked at compile time — which is the
point: `.ch` declarations have drifted from reality for ~35 years,
and this rollout finds and fixes the drift directory by directory.

Rollout state lives ONLY in committed Imakefiles. classpp's
compiled-in defaults do not change until rollout point 11.

## Procedure

1. **Flag the Imakefile.** Add near the top of
   `src/<DIR>/Imakefile`, preserving `$(CLASSINCLUDES)`:

   ```
   /* M1 ANSI rollout: typed dispatch casts (revival/doc/roadmap.md, M1 rollout points) */
   CLASSFLAGS = $(CLASSINCLUDES) -pi
   ```

2. **Regenerate and VERIFY the Makefile.** `make Makefile`, then
   `grep -n CLASSFLAGS Makefile`: the `-pi` line must appear *after*
   the default `CLASSFLAGS = $(CLASSINCLUDES)` line (last assignment
   wins). Do not trust "Makefile is up to date", and do not trust a
   quoted grep for the `$(...)` string — shell quoting has produced
   false "missing" results. Grep for `pi` or `CLASSFLAGS`.

3. **Force-rebuild the directory.** `.o` files do NOT depend on the
   local `.ih`/`.eh` in generated Makefiles, so a bare `make`
   validates stale objects. Run `make clean` in the directory first.
   Caveat: `make clean` also deletes generated parser sources; if the
   directory has yacc/lex targets, build those first (e.g.
   `make eqparse.c` in atk/eq). Then `make -k install`: with `-k`,
   independent per-file fallout lands in one log pass instead of one
   file per rebuild cycle (point 10 batch 1: suite took four cycles
   without it). `-k` stays banned for routine builds where the tree is
   expected clean and it would mask breakage; here errors are the
   point. Success is still zero `error:` lines on the final rebuild,
   never the exit code — see step 5.

   **Logging:** ALL build output — local rebuilds here and the gate in
   step 5 — goes to the one canonical file
   `~/src/AUIS/andrew-6.4/dependInstall.log`, overwritten each build.
   Never invent per-step log names: the fixed path is already in the
   user's permission allow-list, so reusing it keeps an unattended run
   moving, while a new path stops work to ask permission. It also
   gives the user one known place to watch progress.

4. **Fix fallout** (see taxonomy below).

5. **Gate.** From `src/`: `make Clean && make dependInstall`, output
   overwriting `~/src/AUIS/andrew-6.4/dependInstall.log` (same single
   log file as step 3). This is the
   definition of done — it rebuilds every external consumer against
   the newly typed installed headers. It legitimately takes a long
   time; do not kill it. Before blaming any process for a hang, check
   provenance (`ps -axo pid,stat,etime,ppid,comm` — filter out
   Google/crashpad noise); its demo step emits alarming-but-nonfatal
   `cui`/`ms` messages. Success = ZERO real `error:` lines in the log;
   the exit code is untrustworthy — dependInstall keeps walking
   directories after one fails and exited 0 over 20 real errors in
   point 10 batch 1 (this also means the gate is already `-k`-like
   across directories: one gate pass yields the full consumer error
   list). Known false positive: the string
   `Internal error: unknown recognizer type` inside a
   `-Wdeprecated-non-prototype` warning.

6. **Runtime check.** Give the user the exact command(s) from the
   rollout point's "runtime check" note and STOP until they confirm
   visually. NEVER launch AUIS GUI/terminal apps from the harness —
   they can hang unkillable (`UE` state) under sandboxed shells.
   Where the directory has CLI consumers, prefer a byte-diff
   before/after battery (pattern: `~/src/AUIS/test-baselines/
   raster-pi/run-battery.sh` — capture baseline BEFORE flagging, run
   twice to prove determinism, diff after). Log any newly noticed
   pre-existing bug in `roadmap.md` Little Annoyances BEFORE flipping
   the flag, so it can't be mistaken for a regression.
   Two more caveats for how the check is worded/scoped (found point
   8, 2026-07-09):
   - If directing the user to a spot in a test document via a search
     string, the string must be in the surrounding *text*, not inside
     an inset — `ez`'s search command does not descend into insets
     (each inset type has its own, different search semantics, if any).
   - Never direct the user to run a save/write action (e.g. "Save
     All") against `ia-archive`/`PAPERS`/etc. test documents — they
     are not under source control, and a save silently overwrites the
     fixture on disk. Confirmed the hard way (point 8): required a
     Time Machine restore. If a save path genuinely needs runtime
     coverage, point at a scratch copy, never the shared archive.

7. **Commit** (after user confirmation), two commits, terse one-line
   messages matching the fossil timeline style:
   1. src: Imakefile + any `.ch` fixes
   2. docs: tick the rollout point in `roadmap.md`; add findings to
      `porting-assessment.md` §14 only if a NEW pattern appeared
   `fossil status` before and after each commit.

## Fallout taxonomy and fix rules

Order of trust: **implementations and dispatch callers are the
runtime truth; the `.ch` declaration is what drifts.** Fix the `.ch`
ONLY. Never edit a `.c` to match a `.ch`.

- **Typeless `.ch` declaration** — e.g. `MoveHandle(x, y, ptref);`
  (no types at all). classpp maps the unknown tokens to `void *` in
  the cast; callers passing integers then fail. Type the declaration
  from the implementations. Telltale: an all-`void *` cast for a
  method whose callers pass integers. classpp does NOT comma-share
  types (`Foo(int a, b, c)` → `(int, void *, void *)`; verified
  point 10 batch 3) — every parameter needs its own type.
- **Unknown type token — a typed cast that lies** (point 10 batch 3,
  `CUI_Initialize(proc TimerFunction, ...)`, `sendmsg.ch Boolean`):
  classpp copies an unrecognized type NAME verbatim into the cast,
  where gnu89 parses the bare identifier as a parameter with
  implicit `int` — 32-bit, silently truncating pointers on LP64
  unless a caller happens to error visibly. Every type name in a
  `.ch` must resolve in every consumer's translation unit. Use
  `procedure` (the class.h typedef) for function pointers — `proc`
  is not a type — and the underlying public type for file-private
  typedefs. Census duty: sweep flagged `.ch`s for type tokens
  outside C keywords / `struct|enum|union` / `FILE` / `boolean` /
  `procedure`.
- **Rock idiom** — `long rock` parameters. Judge each by its
  callers: if ALL callers pass pointers, change to `void *rock`
  (K&R impls keep `long` internally; that round-trips on LP64 and is
  cleaned up at M3). If callers pass integers, leave it alone.
  Callers passing `(long)&x` count as passing pointers — the cast is
  laundering, not meaning (atk/frame `Enumerate`: four call sites
  cast, a fifth passed the pointer bare for ~35 years). After the
  `void *` retype, deleting such now-redundant `(long)`-style casts
  on the rock argument at call sites IS part of the interface fix
  and is pre-authorized, narrow exception to the `.c` hard stop
  below (decided 2026-07-09). Touch nothing else in the caller.
  **Mirror case — dual-use rock** (atk/support `list.ch Enumerate`,
  decided 2026-07-09): rock declared as a pointer type (`char *`)
  with a minority of callers smuggling genuine integers through it
  (values used numerically, not addresses). Same resolution: retype
  the rock `void *`; pointer callers pass bare (drop launderings);
  the integer call sites get an explicit `(void *)` cast on the
  argument — also pre-authorized. Callbacks never need edits: they
  are invoked through typeless `procedure` pointers, outside `-pi`
  checking, and receive the same bits either way. Do NOT split the
  method or redesign callers to pass addresses — semantics must be
  preserved bit-for-bit; deeper cleanup belongs to M3.
  **Integer-majority rock** (point 9, `keymap.ch BindToKey`, decided
  2026-07-09): when genuine-integer callers dominate (161 int/zero vs
  1 pointer there), the rock STAYS `long` and the minority pointer
  site gets an explicit `(long)` cast on the argument — the mirror
  image of the dual-use resolution, same pre-authorization.
  **Unsigned rock** (point 10, `suite.ch` `Apply`/`Create`, ruled
  2026-07-09): a rock declared `unsigned` in BOTH `.ch` and the K&R
  impl, with pointer-passing callers. Unlike a `long` rock, `unsigned`
  is 32-bit on LP64 — the callee itself truncates, so the `.ch`-only
  retype would leave a typed interface that lies. Fix is `.ch` →
  `void *` PLUS editing the impl's K&R param declaration `unsigned` →
  `long` (semantics-restoring; goes in a separate live-bug commit, per
  the clockv/htmlview precedents). The same authorization covers a
  K&R definition MISSING a param declaration entirely (point 10,
  `entrstrv.c`/`entrintv.c` `LinkTree`: `parent` undeclared → implicit
  `int` truncating a view pointer): add the declaration the `.ch`
  specifies.
- **Dual-use attribute values** (point 9): a `long`-returning
  attribute getter (`suite_ItemAttribute`, `chart_ItemAttribute`)
  whose result feeds a correctly-pointer-typed parameter
  (`char *string`, `char *Text`). The getter's class is out of
  scope; the call site gets an explicit pointer cast on the
  argument. Same class of fix as the dual-use casts above.
  Sibling case: magic integer constants passed for a struct-pointer
  parameter by convention (`graphic_BLACK`=0xFF as a `FillRect*`
  Tile) — cast at the call site, convention preserved bit-for-bit.
- **Missing prototype exposed by typing** (point 9, `clockv.c`
  `NewString`): an undeclared pointer-returning function used inline
  in a now-typed argument errors as int-to-pointer. This is the M2
  bug class arriving early; fix by adding the `#include`/`extern`
  declaration the sibling files already use. Semantics-restoring
  (the pointer was truncating on LP64), so it is a fix, not drift.
- **Signature drift** — wrong types, stray `*`, or transposed
  parameters vs the implementations (`eq.ch DoScript`,
  `figobj.ch Build`). Fix the `.ch` to match impls + callers.
  Transposed args at a CALL site (impl+.ch agree, one caller wrong:
  point 9 `htmlview.c` `DisplayString(self, "msg", 0)`) are a live
  bug — hard stop; ruled 2026-07-09: fix the caller, as a separate
  commit from the rollout commit.
- **Variadic-by-macro-convention** (point 10, suite attribute family,
  ruled 2026-07-09): methods whose callers pass a `(code, value)`
  attribute-pair macro (`suite_ItemCaption(x)` → `suite_itemcaption,
  (long) (x)`) through ONE macro argument, so the function call carries
  more C arguments than the dispatch macro has parameters. Worked for
  35 years only because the casts were unprototyped; incompatible with
  `-pi` no matter what the `.ch` says (true arity breaks macro-argument
  counting at pair-style call sites; historical arity breaks the
  prototyped call). A variadic cast is NOT a fix — Darwin arm64 puts
  variadic args on the stack while K&R impls read registers.
  Resolution: declare the true arity in the `.ch` (setters gain
  `long value`) and mechanically expand every dispatch call site —
  setters pass `code, (long) (value)` as real arguments, getters pass
  the bare code and DROP their never-read dummy pair. Pair macros stay,
  fenced by comment, for `*_Specification` initializer tables, where
  they remain exactly right. Ruling explicitly extends to
  `atk/chart`'s identical attribute family.
- **Benign, ignore:** `-Wincompatible-pointer-types` passing a
  subclass pointer where the cast names the *defining* superclass
  (`struct traced *` etc.) — prefix-layout subtyping, expected
  tree-wide. Also the pervasive `-Wdeprecated-non-prototype` noise.
- **Zero fallout is a real outcome** (raster/lib) — but only accept
  it after verifying the installed `.ih` actually contains typed
  casts and `_a1`-style positional args (guard against a vacuous
  rebuild, see step 2/3).

## Methodology notes (point 9, large directories)

- **Liveness check (added 2026-07-10):** a directory is in the
  active tree iff the gate log contains
  `building (dependInstall) (.../src/<dir>)` — grep
  `~/src/AUIS/andrew-6.4/dependInstall.log` after any full gate.
  Makefile presence is NOT evidence: stale Makefiles from before a
  subtree was conditionalized out survive indefinitely (atkbook,
  tm, bdffont, prefed all have them), and site.h/allsys.h must be
  read together (site.h overrides — CONTRIB_ENV is ON there while
  allsys.h shows it commented out; a mis-census of exactly this
  produced the batch-7 "CONTRIB_ENV off" error). Census every
  batch's directories against the gate log BEFORE flagging.

- **The gate log under-reports.** A directory's build stops at its
  first failing file, so later files' fallout is invisible until the
  blocker is fixed — expect new errors on each re-gate. For any
  method with fallout, run a static tree-wide call-site census
  (grep + read each site) BEFORE retyping; classify every site as
  bare-pointer / laundered `(long)` cast / genuine integer / zero.
  Remember: laundered casts and integers compile silently against a
  `long` rock — the gate only ever shows the bare-pointer sites, so
  the census, not the error list, is the basis for the retype
  decision.
- **Census and mechanical edits delegate well** (Sonnet-class):
  give exact file:line + expected argument text + replacement;
  require skip-and-report on any mismatch. Retype rulings and
  hard-stop calls stay with the operator.
- **`-pi` emits `void *` for struct-pointer method args** (self is
  fully typed; basic C types like `char *`/`long` are fully typed).
  Consequence: wrong-struct-pointer mistakes are NOT caught, but
  int-vs-pointer mistakes — the LP64 killers — all are. Also means
  a typed macro can reach into unflagged directories via
  macromethods that expand to a flagged class's dispatch
  (`chartobj.ih` → `graphic_DrawString`).
- **A flagged subclass types its ANCESTORS' casts too** (point 10
  batch 3, fldtreev : orgv). A subclass `.ih` regenerates macros for
  every inherited/overridden method using the DEFINING class's
  declaration, so a malformed decl in an unflagged parent directory
  surfaces under the child's flag; the `.ch` fix belongs in the
  parent. classpp resolves parent `.ch`s from the INSTALLED include
  tree — the fix takes effect only after `make install` in the
  parent's directory, not on editing the source copy.

## Hard stops — report instead of proceeding

- Implementations disagree with dispatch callers (not drift — a live
  bug; e.g. two callers pass different types for one parameter).
- A fix appears to require editing any `.c` file, or
  `overhead/class/pp` (classpp itself).
- Errors mentioning enum/incomplete types you didn't touch — possible
  macro-parameter capture, a classpp bug class (see §14 Pilot A).
- The gate fails in a way not attributable to this directory's flag.
