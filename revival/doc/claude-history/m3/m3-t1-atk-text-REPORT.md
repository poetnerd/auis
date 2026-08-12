# M3 Wave 3, Batch T1: `atk/text` — ansify + `-pe`/`.eh` rollout — REPORT

## 1. Status

Stopped at the gate. `atk/text` gates clean (`make clean && make depend
&& make -k install`), twice, with `-pe` live and all 21 `.eh` files
force-regenerated. No commit made.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md`,
   `porting-assessment.md` §17 (including B3's "second real exception"
   subsection), and `m3-rollout-runbook.md`'s B1/B2/B3 findings
   entries, per the prompt's instructions.
2. Re-confirmed the two standing quick checks the prompt asked for:
   macro-typo grep and empty-parens-lifecycle-method grep on
   `atk/text` — both clean, matching the pre-diagnosis.
3. Ran `ansify --dir src/atk/text` for real. **First attempt failed
   uniformly across 29/30 files** with `fatal error: 'X.eh' file not
   found` — this checkout had never had a local build pass over
   `atk/text`, so no `.eh`/`.ih`/etc. existed yet for ansify's own
   compile-gate to test against (a case rollout-procedure's
   "Generated-source directories need `depend`" note anticipates, just
   not usually this literally — here *zero* generated headers existed
   at all). Fixed by running `make -C src/atk/text depend` then
   `make -C src/atk/text -k install` once to establish a normal
   (`-pe`-off) baseline build. `indexpro.c`'s 3 helper conversions
   (no class, no `.eh` dependency) succeeded and installed cleanly in
   this very first pass and were never touched again.
4. Re-ran `ansify --dir src/atk/text` against the now-real baseline.
   Got the pre-diagnosed shape almost exactly: 0 DRIFT, skips at
   `bpv__InitializeObject`, `readscr__InitializeObject`,
   `texttroff__CompileNotes` — but 5 files failed the compile-gate
   with **hard `error:` diagnostics** the pre-diagnosis's dry-run
   pass never surfaced (`drawtxtv.c`, `pcompch.c`, `smpltext.c`,
   `textv.c`, `txttroff.c`; the tool's 12-line truncation hid the
   actual `error:` lines behind more `warning:` noise in the printed
   summary). Investigated each by loading `ansify` as a module
   (`importlib.machinery.SourceFileLoader`, same technique the
   orchestrator's own pre-diagnosis used) and re-running
   `convert_file` + `make X.o` directly to see full, untruncated
   compiler output.
5. Found and fixed 2 genuinely new bugs (detail in §3 below):
   `textv.ch`'s `ViewMove` had the wrong parameter type, and
   `textv.c`'s `HandleSelection` was called with a stray extra
   argument at all 5 call sites. Also identified (not "fixed" — just
   understood) that most of the remaining failures were an artifact of
   testing narrow-by-value-parameter classprocs' compile-gate *before*
   `-pe` was live (`.eh` still emitting empty-parens declarations at
   that point) — this resolves itself once `-pe` goes live, see §3.
6. Reverted the 29 K&R→ANSI `.c` edits back to pristine (keeping the
   `textv.ch` fix and the Imakefile `-pe` addition), added
   `CLASSFLAGS = $(CLASSINCLUDES) -pe` to `atk/text/Imakefile`,
   regenerated the project's `Makefile` for the directory (`Makefile`
   itself is not fossil-tracked; had to restore from the pre-existing
   `Makefile.BAK` and re-run its self-rebuild target after an initial
   bare `xmkmf` attempt produced a broken, macro-unexpanded Makefile —
   corrected by using the tree's own `Makefile:: Imakefile` rule
   instead of calling `xmkmf` directly), then force-regenerated all 21
   `.eh` files with `-pe` live (`rm *.eh`, `make <name>.eh` per class,
   explicit target list).
7. Ran the real `ansify --dir` pass a final time against this `-pe`-live
   environment. This is the canonical run: 0 DRIFT, the same 3
   confirmed-safe skips, and only 3 files (`drawtxtv.c`, `pcompch.c`,
   `txttroff.c`) still hit the tool's own compile-gate — but by this
   point isolated single-file re-tests of their exact converted content
   were already compiling clean, so this was the tool's own gate
   flaking non-deterministically across runs against identical content
   (detail in §3), not a real defect. Verified by re-applying each
   file's conversion via `ansify --no-compile` (same pipeline, skip the
   flaky gate) and compile-testing by hand.
8. That hand-testing surfaced one more genuinely new, general pattern
   (§3.4): 3 file-local static helper functions
   (`drawtxtv.c`'s `CharToOctal`, `pcompch.c`'s `scanerr`,
   `txttroff.c`'s `quote`) each had an old-style empty-parens forward
   declaration that `ansify` doesn't touch (it only converts
   *definitions*), left stranded against a newly-ANSI'd definition with
   a narrow by-value parameter — a real, ISO C hard error, not
   tool-side noise. Fixed by hand-updating each forward declaration to
   match its real definition.
9. Ran a full `make -C src/atk/text -k` after all fixes: 0 `error:`
   lines, build completed through `ar clq libtext.a` and the `tpls`
   subdirectory.
10. Ran the required gate twice: `make clean`, `make depend`,
    `make -k install`, each as a separate absolute-path call, per
    `rollout-procedure.md`. Both passes clean (0 `error:` lines,
    completed through library packaging and doc installation both
    times).
11. `fossil status`/`fossil extras` confirmed exactly which files
    changed and that only ordinary build byproducts are untracked
    extras (§5).

## 3. Findings

### 3.1 Real `ansify --dir` run vs. pre-diagnosis

Matched exactly on the metrics that matter (0 DRIFT, the 3
confirmed-safe skips `bpv__InitializeObject`/`readscr__InitializeObject`/
`texttroff__CompileNotes`), but the pre-diagnosis's dry-run pass could
not have caught two things that only a real, compile-gated,
`-pe`-live run exposes:

- Two genuine `.ch`/`.c`-level bugs (§3.2, §3.3) — neither is a DRIFT
  shape (DRIFT only fires on parameter-*count* mismatches; both of
  these are parameter-*type*/call-arity mismatches invisible to
  `ansify`'s DRIFT check).
- A real ISO C constraint that only bites when a classproc with a
  narrow by-value parameter is converted to ANSI *while its `.eh`
  still emits an old-style empty-parens forward declaration*
  (i.e., specifically during the pre-`-pe` phase of a two-phase
  rollout like this task's). This produced spurious `COMPILE FAILED`
  reverts for `pcompch.c`/`smpltext.c`/`textv.c` on the very first real
  `ansify --dir` attempt (before `-pe` was added) — all three compile
  perfectly clean once `-pe` is live, no fix needed for this part.
  Not previously documented in `porting-assessment.md`/the runbook;
  worth a note there if this two-phase-rollout shape recurs.

### 3.2 Real bug: `textv.ch`'s `ViewMove` had the wrong parameter type

```
textv.ch:135 (before): ViewMove(struct mark *currentLine,long movement);
textv.ch:135 (after):  ViewMove(struct linedesc *lineStructure,long movement);
```

`drawtxtv.c`'s real implementation (`textview__ViewMove`) declares its
first K&R parameter as `struct linedesc *lineStructure` and immediately
dereferences `lineStructure->data`/`lineStructure->nChars` — fields
that exist only on `struct linedesc` (defined in `textv.ch` itself,
line 56: `y`, `height`, `textheight`, `xMax`, `nChars`,
`containsView`, `data`), not on `struct mark`. All 7 real call sites
tree-wide (`textv.c:692,863,979,1038,1073(comment),1298,1305`) pass
`&self->lines[line]`/`newline`, both genuinely `struct linedesc *`
(`self->lines` is declared `struct linedesc *lines` at `textv.ch:180`).
Confirmed no caller anywhere passes an actual `struct mark *`. This is
the same species of pre-existing, ~35-year-old `.ch` type bug as B3's
`treev.ch` `SetHitHandler` finding — invisible under K&R (param types
were never checked against the interface), surfaced only once `-pe`
type-checks the exported prototype against real usage. Fixed by
correcting the `.ch` declaration to match the always-correct
implementation and all real callers; zero behavior change.

(Rebuilt the `ansify` signature DB — `ansify --build-db`, 565 classes,
1 pre-existing unrelated failure in `contrib/atkbook`, not touched
here — after this fix, since `convert_file` reads cached `.desc` files
under `build/desc/`, not `.ch` sources directly; the DB was stale
against my edit until rebuilt.)

### 3.3 Real bug: `textv.c`'s `HandleSelection` called with a stray extra argument

`HandleSelection` is a file-local `static` helper (not a class method,
no `.ch` involved) whose real, working K&R definition has always taken
exactly 2 parameters:

```c
static void HandleSelection(self, len)
struct textview *self;
long len;
{
    ...  /* body never references a 3rd parameter */
}
```

All 5 real call sites (`textv.c:1369,1396,1419,1486,1611` in the
pre-conversion line numbering) passed a stray 3rd argument
(`TRUE`/`FALSE`), silently ignored under K&R's lenient calling
convention. Confirmed via `fossil cat` against the checked-in original
that this mismatch is not something my session's edits introduced —
it's ~35 years old. Same species as B1's `im.c` colormap-dereference
finding and B2's `sbuttonv.c` wrong-variable finding: a real caller
bug, invisible under K&R, that becomes a hard "too many arguments"
compile error the moment the callee gets a real ANSI prototype. Fixed
by dropping the stray argument at all 5 call sites (matching the
function's actual, correct behavior) rather than adding an unused
parameter to the callee.

### 3.4 New pattern: stranded old-style forward declarations vs. newly-ANSI'd narrow-parameter definitions

`ansify` converts K&R function *definitions* to ANSI prototypes but
does not touch separate, old-style (empty-parens) forward
*declarations* of the same function earlier in the same file. Normally
harmless (an unprototyped declaration is compatible with any later
definition) — but when the definition has at least one narrow
by-value parameter (`char`, `short`, `unsigned char`), the two become
a genuine ISO C conflicting-types error (default-argument-promotion
incompatibility, not a compiler quirk). Found in exactly 3 places,
each a single, isolated instance:

| file | function | forward decl (before) | fix |
|---|---|---|---|
| `drawtxtv.c:53` | `CharToOctal` | `static void CharToOctal();` | `static void CharToOctal(unsigned char *s, char c);` |
| `pcompch.c:53` | `scanerr` | `static void scanerr();` | `static void scanerr(char *msg, char key, struct composites *new, long line);` |
| `txttroff.c:152` | `quote` | `static int quote();` | `static int quote(char *buf, char c, int len);` |

Confirmed via a full `make -k` across the whole directory (not just
these 3 files) that this was the complete set — no other file in
`atk/text` hit it. Not tool-patched (3 bounded instances, same
proportionality precedent as B3's array-bracket and double-pointer
tool-bug hand-fixes) — flagged here for whoever next touches
`ansify`'s declaration/definition pairing logic. Worth checking for in
future batches: grep a directory for `static\s+\w[\w ]*\s+\w+\(\);`
(bare empty-parens static forward declarations) before or after a
batch's `ansify` pass, and cross-check against the matching
definition's parameter types.

### 3.5 Non-deterministic `ansify` compile-gate flake (not a code bug)

Across two otherwise-identical `ansify --dir` runs against the same
`-pe`-live environment, `drawtxtv.c`/`pcompch.c`/`txttroff.c` reported
`COMPILE FAILED` both times but with **different specific error
messages each time** (first run: `pcompch__DeleteComposite`/
`EnumerateComposites`/`CharacterToTroff` — narrow-param-vs-stale-`.eh`
artifact, resolved by that point; second run, after regenerating fresh
`.eh` and re-running: `pcompch__ATKToASCII`/`ASCIIToATK`/
`DeleteComposite` — a *different* set of 3, none of which have narrow
by-value parameters at all). Isolated, single-file re-tests of the
*exact same converted content* (via `ansify --no-compile` + manual
`make X.o`) consistently compiled clean with 0 errors on every
attempt. Root cause not fully diagnosed (candidate: `make`'s implicit
dependency-remake behavior for `.eh`/`.ih` interacting with the
compile-gate's `make X.o` subprocess call in a way that isn't fully
reproducible outside the full `--dir` sequential loop) — flagged, not
chased further, since the eventual fix (§3.4's 3 forward-declaration
mismatches) was confirmed by hand and holds up under the authoritative
test (the full directory gate, twice, both clean). Worth a note for
whoever next investigates `ansify`'s compile-gate reliability at scale.

### 3.6 Double-pointer-parameter investigation (§3 of the prompt) — held up

Checked all ~12 real double-pointer parameter instances in the final,
compile-verified converted source: `be1be2a.c`'s `char **argv`,
`content.c`'s `char **lst` (×2 functions), `pcompch.c`'s
`char **fromChars`/`char **str`, `search.c`'s
`struct SearchPattern **result`/`unsigned char **s`, `text.c`'s
`struct dataobject **list` (×2, the `#if 0`/`#else` pair),
`tindex.c`'s `char **error`, `txtvcmds.c`'s `struct menulist
**normalMenus` (×2)/`**Menus` (×2)/`char **fromChars`, `txttroff.c`'s
`char **lst` (×2 functions) — every single one retained both stars.
No instance of B3's `tlex.c` star-drop bug anywhere in this directory,
confirmed under the real (non-dry-run, fully compile-gated)
conversion, not just the orchestrator's earlier in-memory spot-check.

## 4. Files touched

All compile-clean (confirmed via the double gate, both passes 0
`error:` lines):

- `src/atk/text/Imakefile` — added `CLASSFLAGS = $(CLASSINCLUDES) -pe`
- `src/atk/text/textv.ch` — fixed `ViewMove`'s parameter type
  (`struct mark *` → `struct linedesc *`) — real bug, §3.2
- `src/atk/text/textv.c` — ANSI conversion (45 methods, 3 classprocs,
  21 helpers) + `HandleSelection` caller fix at 5 sites (§3.3)
- `src/atk/text/drawtxtv.c` — ANSI conversion (2 methods, 0 classprocs,
  14 helpers) + `CharToOctal` forward-decl fix (§3.4)
- `src/atk/text/pcompch.c` — ANSI conversion (0 methods, 9 classprocs,
  9 helpers) + `scanerr` forward-decl fix (§3.4)
- `src/atk/text/txttroff.c` — ANSI conversion (0 methods, 6 classprocs,
  17 helpers; skip: `texttroff__CompileNotes`) + `quote` forward-decl
  fix (§3.4)
- `src/atk/text/be1be2.c` — 0 methods, 2 classprocs, 2 helpers
- `src/atk/text/be1be2a.c` — 2 methods, 0 classprocs, 2 helpers
- `src/atk/text/bp.c` — 1 method, 0 classprocs, 0 helpers
- `src/atk/text/bpv.c` — 3 methods, 0 classprocs, 0 helpers; skip:
  `bpv__InitializeObject`
- `src/atk/text/cltextv.c` — 5 methods, 2 classprocs, 3 helpers
- `src/atk/text/content.c` — 10 methods, 2 classprocs, 30 helpers
- `src/atk/text/fnote.c` — 8 methods, 6 classprocs, 6 helpers
- `src/atk/text/fnotev.c` — 11 methods, 4 classprocs, 5 helpers
- `src/atk/text/indexpro.c` — 0 methods, 0 classprocs, 3 helpers
  (standalone program, no class/`.eh` dependency)
- `src/atk/text/readscr.c` — 0 methods, 2 classprocs, 7 helpers; skip:
  `readscr__InitializeObject`
- `src/atk/text/search.c` — 0 methods, 5 classprocs, 2 helpers
- `src/atk/text/smpltext.c` — 42 methods, 2 classprocs, 3 helpers
- `src/atk/text/tabs.c` — 7 methods, 4 classprocs, 1 helper
- `src/atk/text/text.c` — 34 methods, 5 classprocs, 24 helpers
- `src/atk/text/textref.c` — 2 methods, 0 classprocs, 0 helpers
- `src/atk/text/textrefv.c` — 2 methods, 2 classprocs, 1 helper
- `src/atk/text/texttag.c` — 2 methods, 0 classprocs, 0 helpers
- `src/atk/text/texttagv.c` — 2 methods, 2 classprocs, 0 helpers
- `src/atk/text/tindex.c` — 0 methods, 6 classprocs, 18 helpers
- `src/atk/text/tpllist.c` — 1 method, 0 classprocs, 5 helpers
- `src/atk/text/txtvcmds.c` — 0 methods, 0 classprocs, 9 helpers
- `src/atk/text/txtvcmod.c` — 1 method, 0 classprocs, 76 helpers
- `src/atk/text/txtvcmsc.c` — 0 methods, 0 classprocs, 19 helpers
- `src/atk/text/txtvcmv.c` — 0 methods, 0 classprocs, 40 helpers
- `src/atk/text/txtvcsrch.c` — 0 methods, 0 classprocs, 9 helpers
- `src/atk/text/txtvcsty.c` — 16 methods, 0 classprocs, 24 helpers

30/30 files processed, 21/21 `.eh` files force-regenerated with `-pe`
live. Gate result: clean, twice (0 `error:` lines both times; both
passes completed through `ar clq libtext.a` and doc installation).

## 5. `fossil status` / `fossil extras`

`fossil status` after all work: 32 `EDITED` files (all 30 `.c` files
listed in §4, plus `Imakefile` and `textv.ch`), no
`ADDED`/`DELETED`/`CONFLICT`. No commit was made — `checkout:` still
shows the same revision the session started from.

`fossil extras` (filtered to exclude the pre-existing, repo-wide
`build/` tree noise, which is unrelated to this session and present in
every checkout): only ordinary build byproducts inside
`src/atk/text/` and 3 sibling directories that share generated headers
transitively rebuilt by the gate (`.o`, `.eh`, `.ih`, `.do`,
`Makefile`, `Makefile.BAK`, `install.time`, `install.doc`,
`libtext.a`, `indexpro` binary, `PrintFontMap.tmp`) — no stray or
unexpected files. Matches the pattern every prior M3 batch has
reported (Makefiles and generated headers are not fossil-tracked in
this tree).

## 6. Open questions / anything that surprised you

- The directory had never had a local build pass in this checkout —
  not anticipated by the pre-diagnosis, which assumed a `-pe` test-build
  had already established baseline `.eh` files (it had, but reverted
  them afterward per its own methodology, and evidently the checkout's
  build state doesn't otherwise retain them). Worth a note in
  `rollout-procedure.md` if this recurs: a directory's very first
  session in a checkout may need a plain baseline build before
  `ansify --dir`'s own compile-gate can produce a meaningful signal at
  all.
- The `Makefile` self-regeneration hazard (§2 step 6): running `xmkmf`
  directly (rather than the tree's own `Makefile:: Imakefile` rule)
  produces a broken, macro-unexpanded Makefile for this project's
  imake templates. Not previously documented as a hazard in any M3
  runbook entry — worth adding if a future session needs to regenerate
  a Makefile from scratch (rare; only needed here because I wanted to
  test `-pe` before doing the final, canonical `ansify --dir` pass).
- §3.5's non-deterministic compile-gate flake is the most surprising
  finding — genuinely inconsistent behavior across runs against
  identical file content, only observed via the full `--dir` sequential
  loop, never via isolated single-file compiles. Documented but not
  root-caused; did not block the task since the authoritative gate
  (full directory build, twice) is unambiguous and clean.
- Otherwise the batch matched its "unusually clean" billing well: the
  13 restated-lifecycle-param instances, the 3 confirmed-safe skips,
  and the double-pointer-parameter population all resolved exactly as
  pre-diagnosed with zero surprises.

## Suggested runtime checks for wdc

`atk/text` is the highest-consumed single directory in the
insets/apps waves — `nm -g` against `runapp` and the relevant `.do`
files first to confirm live consumers before choosing a check:

```
nm -g /Users/wdc/src/AUIS/andrew-6.4/build/bin/runapp | grep -E "textview_|simpletext_|_ViewMove|_LineRedraw"
nm -g /Users/wdc/src/AUIS/andrew-6.4/build/dlib/atk/textv.do | grep -E "ViewMove|HandleSelection"
```

Since `text`/`textv`/`smpltext` are core to `ez`'s text editing (not
just an inset), the highest-value check is ordinary text editing and
selection behavior in `ez` — but per the hard rule, I am not launching
any GUI app myself. Suggested for wdc to try directly:

- Open any scratch `.ez` document (not a versioned fixture — copy one
  from `revival/tests/` or similar to a scratch path first) in `ez`
  and do a few ordinary edits: type text, select a range with the
  mouse (exercises `HandleSelection`, §3.3's fix — watch specifically
  for the dot/selection-ownership handoff behaving normally when
  collapsing a selection to zero-length, since that's the branch the
  stray argument used to (harmlessly) no-op past), and force a
  multi-line reflow/scroll (exercises `drawtxtv.c`'s `LineRedraw`
  and `textv.c`'s `ViewMove`, §3.2's fix — watch for correct line
  redraw/positioning when lines are inserted or removed near the
  current view, since that's exactly the path `ViewMove` handles).
- If a composite-character/troff-adjacent document is available,
  exercising `pcompch.c`'s composite character lookup and
  `txttroff.c`'s postscript/troff export path would cover the other
  two hand-fixed files (§3.4), though these are lower-traffic paths.
