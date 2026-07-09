# M1 rollout runbook: enabling `-pi` typed dispatch per directory

Procedure for executing one "M1 rollout point" from `roadmap.md`
(Medium-term → ANSI C conversion → M1 rollout points). Written after
pilots on `atk/eq`, `atk/figure`, and `atk/raster/lib` (points 2–4);
the fallout taxonomy below is empirical, from those directories.
Background and rationale: `porting-assessment.md` §14.

One directory per session. Never run two builds at once.

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
   `make eqparse.c` in atk/eq). Then `make install`.

4. **Fix fallout** (see taxonomy below).

5. **Gate.** From `src/`: `make Clean && make dependInstall`, output
   overwriting `~/src/AUIS/andrew-6.4/dependInstall.log`. This is the
   definition of done — it rebuilds every external consumer against
   the newly typed installed headers. It legitimately takes a long
   time; do not kill it. Before blaming any process for a hang, check
   provenance (`ps -axo pid,stat,etime,ppid,comm` — filter out
   Google/crashpad noise); its demo step emits alarming-but-nonfatal
   `cui`/`ms` messages. Success = exit 0 and zero real `error:` lines
   in the log. Known false positive: the string
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
  method whose callers pass integers.
- **Rock idiom** — `long rock` parameters. Judge each by its
  callers: if ALL callers pass pointers, change to `void *rock`
  (K&R impls keep `long` internally; that round-trips on LP64 and is
  cleaned up at M3). If callers pass integers, leave it alone.
- **Signature drift** — wrong types, stray `*`, or transposed
  parameters vs the implementations (`eq.ch DoScript`,
  `figobj.ch Build`). Fix the `.ch` to match impls + callers.
- **Benign, ignore:** `-Wincompatible-pointer-types` passing a
  subclass pointer where the cast names the *defining* superclass
  (`struct traced *` etc.) — prefix-layout subtyping, expected
  tree-wide. Also the pervasive `-Wdeprecated-non-prototype` noise.
- **Zero fallout is a real outcome** (raster/lib) — but only accept
  it after verifying the installed `.ih` actually contains typed
  casts and `_a1`-style positional args (guard against a vacuous
  rebuild, see step 2/3).

## Hard stops — report instead of proceeding

- Implementations disagree with dispatch callers (not drift — a live
  bug; e.g. two callers pass different types for one parameter).
- A fix appears to require editing any `.c` file, or
  `overhead/class/pp` (classpp itself).
- Errors mentioning enum/incomplete types you didn't touch — possible
  macro-parameter capture, a classpp bug class (see §14 Pilot A).
- The gate fails in a way not attributable to this directory's flag.
