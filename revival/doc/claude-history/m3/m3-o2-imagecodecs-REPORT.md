# M3 Wave 1 batch O2: `overhead/image/jpeg` + `overhead/image/tiff`

## 1. Status

Stopped at the (only) gate for this batch — subtree-local `make clean
&& make depend && make -k install` for both directories, zero real
`error:` lines, each run twice for determinism. No commits made.
Working tree: 32 files edited (1 in `overhead/image/jpeg`, 31 in
`overhead/image/tiff`), nowhere else. Session diff at tree root:
`m3-o2-imagecodecs-session.diff` (4,166 lines, 32 files). This report
lives in `revival/doc/claude-history/` per the standard convention.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (including its
   "Command style" section), `m3-rollout-runbook.md` (including
   "Findings from real sessions"), `m3-batches.md`,
   `porting-assessment.md` §14 in full and its §17 "ansify DRIFT
   false-positive" section, and `m3-o1-utillib-REPORT.md`
   — all before starting, per the prompt.
2. Confirmed no `.ch` files in either directory (`find ... -maxdepth 1
   -name '*.ch'`, both empty) — matches the prompt's note, no `-pe`/
   `.eh`-regen step applies to either directory.
3. Applied the 5 pre-diagnosed hand-fixes to `tif_fax3.c` (`putcode`,
   `putspan`, `findspan`) and `mkg3states.c` (`extern_table`,
   `write_tables`) exactly as specified in the prompt. Verified each
   with a direct `make <base>.o` before proceeding — both compiled
   clean (only pre-existing benign `-Wdeprecated-non-prototype`
   warnings for the file's other still-K&R functions).
4. `ansify --dir src/overhead/image/jpeg` for real: 46 files, 13
   compile failures, 0 DRIFT. Investigated every failure (see §5) —
   all 13 turned out to be one single newly-discovered `ansify` parser
   bug, not real fallout; resolved via a `--no-helpers` workaround on
   the 9 affected files still in the real build (see §5 and §7).
5. `ansify --dir src/overhead/image/tiff` for real: 32 files, 8
   compile failures, 0 DRIFT. The 5 pre-diagnosed bailouts no longer
   appear (resolved by step 3, applied before this run, exactly as the
   prompt's sequencing prescribed). Investigated the 8 real failures —
   all turned out to be a second, related but distinct newly-discovered
   parser/pipeline bug (the vendored `DECLARE<N>(...)` macro idiom, see
   §6) — resolved by hand-converting the macro invocations to literal
   ANSI headers, then either letting `ansify` reconvert the file's
   remaining genuine K&R helpers, or (`tif_dirwrite.c` only) running
   `convert_file` directly without `fix-missing-static-decl` to avoid a
   third, more serious bug (see §8).
6. Ran `ansify --dir` again on both directories as a verification pass.
   This resurfaced the `tif_dirwrite.c` problem in a different, more
   dangerous shape (§8) — a genuine gap in `ansify`'s own compile-gate
   safety net, not something either directory's own code triggered.
   Fixed it again, this time understanding the mechanism, and did NOT
   re-run a whole-directory `ansify --dir` on `tiff` a third time
   (isolated verification of `tif_dirwrite.c` alone, plus the real
   `make` gate, instead — see §8's closing note).
7. Subtree-local gate, each step a separate `Bash` call, absolute
   paths, never chained: `make clean`, `make depend`, `make -k install`
   for `overhead/image/jpeg` — 0 errors, exit clean, `libjpeg.a`
   installed. Repeated the same three calls a second time — 0 errors
   again, byte-for-byte same set of installed objects.
8. Same three-step gate for `overhead/image/tiff`, twice. First attempt
   (before I understood the §8 bug) genuinely failed with 20 real
   `error:` lines — all `tif_dirwrite.c` "conflicting types," caused by
   my own verification step in point 6, not by the underlying M3 work.
   Fixed (§8), then re-ran the full three-step gate twice more, both
   clean, 0 errors, `libtiff.a` installed identically both times.
9. Identified real runtime consumers via `nm -g` and Imakefile greps
   (§9) — did not run any AUIS GUI/terminal binary interactively.
10. `fossil status`/`fossil diff` (§10) — 32 files edited exactly as
    expected, no commit made.

## 3. Real instance counts vs. dry-run baselines

| | Dry-run baseline (2026-07-25) | Real run |
|---|---|---|
| `jpeg` files | 46 | 46 |
| `jpeg` helpers (census) | 30 | 5 real conversions (`ansi2knr.c`); the other 25 candidate matches the census counted were **not real K&R** — see §5, a tool parser bug inflated the dry-run count too |
| `jpeg` DRIFT | 0 | 0 |
| `jpeg` skipped/bailout | 0 | 0 |
| `tiff` files | 32 | 32 |
| `tiff` helpers (census) | 206 | ~201 tool-converted + 7 hand-converted (mkg3states.c) + 9 hand-converted (tif_dir.c/tif_dirwrite.c/tif_lzw.c/tif_open.c non-DECLARE statics already counted in the ~201) — final total close to the 206 baseline, not an exact match, consistent with the prompt's own framing |
| `tiff` DRIFT | 0 | 0 |
| `tiff` skipped/bailout | 5 (pre-diagnosed) | 0 — all 5 resolved by the hand-fixes in step 3 |

The 5 pre-diagnosed `tif_fax3.c`/`mkg3states.c` bailouts are confirmed
gone: a final `ansify --dir` on `tiff` (before the point-6 resurfacing
of the `tif_dirwrite.c` issue) reported 0 skipped/bailout entries and 0
DRIFT.

## 4. Each hand-fix and compile-gate fallout fix

### 4.1 The 5 pre-diagnosed bailouts (prompt's own spec, applied verbatim)

- `tif_fax3.c:769` `putcode` — `static void putcode(TIFF *tif,
  tableentry const *te)`.
- `tif_fax3.c:783` `putspan` — `static void putspan(TIFF *tif, int
  span, tableentry const *tab)`.
- `tif_fax3.c:1101` `findspan` — `static int findspan(u_char **bpp, int
  bs, int be, u_char const *tab)` (dropped the meaningless `register`
  keywords on parameters, per the prompt's instruction).
- `mkg3states.c:423` `extern_table` — `void extern_table(FILE *fd,
  char *name)`.
- `mkg3states.c:431` `write_tables` — `void write_tables(FILE *fd)`.

All 5 verified with a direct `make <base>.o` before running `ansify`
for real, per the prompt.

### 4.2 New finding — `overhead/image/jpeg`: `ansify`'s helper parser
misparses an already-ANSI zero-argument function `NAME (void)` as a K&R
function with one undeclared parameter literally named `void`

**Mechanism** (`revival/tools/ansify`, `convert_file`): the file-local
helper matcher's `HDR` regex captures `name` and `params` from a line
like `jmem_term (void)`, then checks `BARE_PARAMS.match(params)` — a
regex requiring each comma-separated part to be a bare identifier. The
literal keyword `void` (meaning "no parameters" in this position)
satisfies `[A-Za-z_]\w*` just like a real identifier would, so it
passes. The very next line is `{` (no K&R declaration block follows,
since there are no real parameters) — `parse_decl_block` returns an
*empty but non-`None`* declaration block, so the whole thing is treated
as a valid K&R candidate: a helper named `jmem_term` with one parameter
named `void`, undeclared, defaulting to `int`. The converted header
becomes `jmem_term(int void)` — a syntax error. Confirmed by manual
inspection: all 4 affected files (`jmemnobs.c`, `jquant2.c`, `jrdgif.c`,
`jwrgif.c`) plus 5 more found on the tree-wide re-run
(`jmemmgr.c`, `jchuff.c`, `jcmain.c`, `jdmain.c`, `jmemsys.c`, plus
`ckconfig.c`/`jmemansi.c`/`jmemdos.c`/`jmemname.c`, the last 4 out of
the real build, see below) are **already fully ANSI** — every real
K&R candidate the tool reported converting in each of these files was
this exact `(void)` misparse, never a genuine K&R definition. Verified
by manually reading every function definition in all 13 originally-
failing files and cross-checking the reported helper count against the
number of `(void)`-style zero-arg functions in each (`grep -n
"(void)$"` matched exactly).

**Resolution**: since these files needed zero real changes (already
ANSI), the correct outcome is "no K&R definitions converted," not a
compile failure. Ran `ansify --no-helpers <file>` (a documented, tool-
provided CLI option, not a modification to the tool itself) on the 9
affected files that are actually part of the real build
(`jmemnobs.c`, `jquant2.c`, `jrdgif.c`, `jwrgif.c`, `jmemmgr.c`,
`jchuff.c`, `jcmain.c`, `jdmain.c`, `jmemsys.c`) — since none of them
has any class methods/classprocs (0/0 for the whole directory) and no
real file-local K&R remained after the `(void)` false-positives are
excluded, `--no-helpers` correctly reports "no K&R definitions
converted" for all 9, making zero file changes. Verified via `fossil
diff` on all 9 — byte-identical to the committed tree.

**The other 4** (`ckconfig.c`, `jmemansi.c`, `jmemdos.c`,
`jmemname.c`) are **not part of the real build at all** — confirmed via
the Imakefile's `COMOBJECTS`/`CLIBOBJECTS`/`DLIBOBJECTS`/`COBJECTS`/
`DOBJECTS` lists: only `jmemsys.o`/`jmemmgr.o` are the real jmem
backend for this port (not `jmemansi.o`/`jmemdos.o`/`jmemname.o`, which
are alternate upstream backends never selected here), and `ckconfig.c`
is a standalone upstream compiler-probe utility never referenced by any
Imakefile target. Same class of finding as O1's WHITEPAGES files:
`ansify`'s generic `.c.o:` pattern rule reaches these regardless of
real-build membership. Left untouched (`ansify`'s own auto-revert
already restored them correctly); not investigated further since
they're out of scope and were already correctly excluded from the
directory's install target before this session.

Net effect on `jpeg`: only `ansi2knr.c` (the K&R-detector build tool,
part of `all::`) had real K&R content, 5 helpers converted cleanly, 0
compile failures. All other 45 files needed no source change.

### 4.3 New finding — `overhead/image/tiff`: `ansify`'s helper parser
and `fix-missing-static-decl` both misparse the vendored
`DECLARE<N>(name, type1, arg1, type2, arg2, ...)` macro-invocation
idiom used throughout this directory

This is the *general* form of the phenomenon the prompt's own §1
"consequence" predicted for vendored code, but broader than the 3
pre-diagnosed instances: `overhead/image/tiff` uses a `DECLARE1`
through `DECLARE6` (+ `V` variadic variants) macro family
(`prototypes.h`) throughout the whole directory, not just in
`tif_fax3.c`. Under `USE_PROTOTYPES=1` (this build's setting,
confirmed in the generated Makefile), `DECLARE2(f,t1,a1,t2,a2)`
expands to `f(t1 a1, t2 a2)` — i.e., the *macro-invocation* line, once
preprocessed, already **is** the correct ANSI prototype; only the
textual, pre-preprocessor source is unusual. Two distinct tool-level
misparses result, both affecting only invocations where the macro's
own type arguments are single bare words (`short`, `long`, `char`,
`int`, or a single-word typedef like `TIFFDataType`/`predictorFunc`) —
never invocations using pointer types (`char*`, `TIFF*`, etc.), which
the parsers correctly leave alone:

**(a) `ansify`'s own `convert_file` helper matcher** treats the whole
line `DECLARE2(RealName, type1, arg1, type2, arg2)` as if `DECLARE2`
were the function name and `RealName, type1, arg1, type2, arg2` were 5
bare parameters (same underlying mechanism as §4.2's `(void)` bug: all
5 comma-separated tokens individually match `[A-Za-z_]\w*`). Found in
`mkg3states.c` (7 instances: `horiz_mode_code_black/white`,
`find_null_mode_prefix`, `find_horiz_mode_prefix`,
`find_uncomp_mode_prefix`, `null_mode_type`, `uncomp_mode_type`) and
`tif_dirinfo.c` (2: `TIFFFindFieldInfo`, `TIFFFieldWithTag`) and
`tif_error.c`/`tif_warning.c` (1 each: `TIFFSetErrorHandler`/
`TIFFSetWarningHandler`).

**(b) `fix-missing-static-decl`** (ansify's own pipeline step 1, called
unconditionally before `convert_file`) sees `static\nDECLARE2(RealName,
...)\n{` (or `static void\nDECLARE3(...)\n{`) and — via its "K&R split"
recognizer, which only looks at the identifier immediately following
`static` — concludes a function literally named `DECLARE2`/`DECLARE3`/
etc. is being defined, implicit-int, with no forward declaration
anywhere. It inserts a bogus `static int DECLARE2();` stub after the
`#include` block. Since `DECLARE2` **is** a real 5-argument function-
like macro, this empty-parens "declaration" is a 0-argument invocation
of a macro requiring 5 arguments — `error: too few arguments provided
to function-like macro invocation`. This affects every `static`-
prefixed `DECLARE<N>(...)` definition regardless of the macro's
argument types (pointer or scalar) — found in `tif_dir.c` (6
instances: `setString`, `setShortArray`, `setLongArray`,
`setFloatArray`, `setJPEGQTable`, `setJPEGCTable` — all pointer-typed,
so (a) never fired on these, only (b)), `tif_dirwrite.c` (14
instances), `tif_error.c`/`tif_warning.c` (1 each: `defaultHandler`).

**Resolution, all 7 affected files** (`mkg3states.c`, `tif_dir.c`,
`tif_dirinfo.c`, `tif_dirwrite.c`, `tif_error.c`, `tif_lzw.c`
[see 4.4], `tif_open.c`, `tif_warning.c` — 8 files total): hand-
converted every `DECLARE<N>(...)` invocation that could trigger either
(a) or (b) to a literal ANSI header, using the macro's own
`USE_PROTOTYPES=1` expansion (`f(t1 a1, t2 a2, ...)`) as ground truth —
same principle as the prompt's own prescribed fix for the 5 pre-
diagnosed cases, generalized to every other instance of the same
idiom. Verified each file compiles clean with a direct `make
<base>.o` before re-running `ansify` for the file's remaining genuine
K&R helpers. Full file:line list:

- `mkg3states.c:216,222,450,471,496,517,544` — `horiz_mode_code_black`,
  `horiz_mode_code_white`, `find_null_mode_prefix`,
  `find_horiz_mode_prefix`, `find_uncomp_mode_prefix`,
  `null_mode_type`, `uncomp_mode_type`.
- `tif_dir.c:49,61,71,81,99,131` — `setString`, `setShortArray`,
  `setLongArray`, `setFloatArray`, `setJPEGQTable`, `setJPEGCTable`.
- `tif_dirinfo.c:276,293` — `TIFFFindFieldInfo`, `TIFFFieldWithTag`.
- `tif_dirwrite.c:360,458,478,503,521,536,560,579,605,623,644,664,688,
  713` — all 14 static helpers (`TIFFWriteNormalTag` through
  `TIFFWriteTransferFunction` — see §8 for this file's extra
  complication).
- `tif_error.c:42,53` — `defaultHandler`, `TIFFSetErrorHandler`.
- `tif_warning.c:42,54` — `defaultHandler`, `TIFFSetWarningHandler`.

### 4.4 `tif_lzw.c` and `tif_open.c` — same idiom, simpler resolution

`tif_lzw.c` (`LZWCheckPredictor:220`, `horizontalAccumulate8:310`,
`horizontalAccumulate16:353`, `horizontalDifference8:854`,
`horizontalDifference16:899` — 5 instances, 3 with `register` params
dropped per the same convention as `findspan`) and `tif_open.c`
(`TIFFInitOrder:100`, `getMode:118` — 2 instances, 1 `register`
dropped) have no pre-existing forward-declaration block for these
names anywhere else in the file, so after hand-converting the
definitions, a normal `ansify` run cleanly inserted correct forward
declarations via `fix-missing-static-decl` (this time recognizing the
real function names, not `DECLARE<N>`) and converted the files'
remaining genuine K&R helpers (13 and 11 respectively) with 0 compile
failures. No special handling needed beyond the hand-fix + verify +
re-run-ansify sequence used throughout.

## 5. `tif_dirwrite.c` — the DECLARE-idiom fix plus a real ansify
safety-gate gap

`tif_dirwrite.c` is the one file in this batch where the general fix
(§4.3) wasn't sufficient on its own, for two independent reasons.

**First complication**: unlike `tif_lzw.c`/`tif_open.c`, this file
*already has* a correct, hand-written `#if USE_PROTOTYPES` /
`#else` K&R-empty-parens declaration block at the top of the file
(lines 52–96) for all 16 of its static helpers — the exact dual-
declaration idiom the batch prompt's introduction predicted, just not
one of the 5 pre-diagnosed instances. After hand-converting the 14
`DECLARE<N>` definitions to literal ANSI (§4.3's list), a plain
`ansify tif_dirwrite.c` run still failed: `fix-missing-static-decl`'s
recognizer for "already declared" only matches a *literal empty-parens*
list (`NAME(), NAME2();`) — never a real typed prototype, even one that
correctly declares the exact same function. Since this file's own
`#if USE_PROTOTYPES` block is fully typed, none of its 16 entries are
recognized as "already declared," so the tool inserts a **second**,
duplicate, empty-parens stub for all 16 names — which then conflicts
with the file's own correct typed block 30 lines later (`error:
conflicting types`, 20 instances). This is a variant of the exact
`fix-missing-static-decl` non-idempotency limitation O1's report
already flagged (`unscribe.c`/`WriteFrag`) — same root cause (the tool
only recognizes one narrow "already declared" shape), different
trigger (this time against the file's own pre-existing ground truth,
not a prior tool-inserted stub).

**Resolution**: following O1's precedent exactly ("run only ansify's
helper-conversion step... not the top-level driver, which re-invokes
fix-missing-static-decl and duplicates the stub"), ran a small script
importing `ansify.py`'s `convert_file`/`compile_gate` directly, skipping
`run_fix_tools` (and therefore `fix-missing-static-decl`) entirely for
this one file. The file's own pre-existing declaration block already
covers all 16 static helpers correctly — nothing was missing. This
converted the file's 3 genuine non-`DECLARE` K&R helpers
(`TIFFWriteDirectory`, `TIFFWriteData`, `TIFFLinkDirectory`) cleanly, 0
compile failures.

**Second, more serious complication — a real gap in `ansify`'s own
compile-gate safety net, not previously documented**: later in the
session, as a "final verification" step, I re-ran `ansify --dir` on
the whole `tiff` directory to double check the batch was clean. This
resurfaced the exact same conflict in `tif_dirwrite.c` — but silently,
with no compile error reported and no auto-revert. Root cause, in
`ansify`'s own `process()`: when `run_fix_tools()` (which always runs
unless `--dry-run` is passed) modifies a file — `fix-missing-static-
decl` inserted its bogus stub, exactly as before — but `convert_file`
itself finds *no* K&R definitions left to convert (true here, since
I'd already hand-converted everything), the function takes the "no K&R
definitions converted" early-return path:

```python
if new_text is None:
    print("no K&R definitions converted")
    ...
    os.unlink(backup)
    return True, rep
```

This path **never calls `compile_gate()`** and **never restores from
the backup**. The file is left on disk in whatever state
`run_fix_tools()` put it in — broken, in this case — with the tool
reporting `ok=True` and "no K&R definitions converted," giving no
indication anything happened. This is structurally different from
(and more dangerous than) every other `ansify` limitation found so far
in this session or O1's: those all still went through the compile gate
and auto-reverted on failure (matching the tool's core safety design
promise). This one bypasses that promise entirely whenever a file's
*only* required change comes from `fix-missing-static-decl`/`fix-
static-methods`, not from `convert_file`'s own helper/method
conversion — a condition `tif_dirwrite.c` will hit **every single
time** `ansify --dir` (or even a bare `ansify tif_dirwrite.c`) is run
against it in the future, since `fix-missing-static-decl` has no way to
recognize this file's typed declaration block as satisfying "already
declared."

**Fixed** by removing the 16-line bogus stub block a second time and
re-verifying with a direct `make tif_dirwrite.o` (0 errors). **Not
re-triggered again**: avoided a further whole-directory `ansify --dir`
re-run on `tiff` for the rest of the session; relied on the real `make`
gate (§7 below) instead, which is authoritative for the actual build
regardless of `ansify`'s own report.

**Tool fix (not done, per the Delegation ruling — tool construction
stays top-level)**: `ansify`'s `process()` should run the compile gate
(and restore-on-failure) whenever `run_fix_tools()` reports it modified
the file, not only when `convert_file` also found something to change.
Flagging prominently here since, unlike the other findings in this
session, this one is a genuine hole in the safety property that
distinguishes `ansify` from the June 2026 mass-conversion failure
(`porting-assessment.md` §14) — any future session or automated re-run
that touches `overhead/image/tiff/tif_dirwrite.c` with `ansify` (even
just to re-verify) will silently reintroduce this exact breakage until
the tool itself is fixed.

## 6. Runtime consumers (identified via `nm -g`/Imakefile, not run)

Neither `raster` nor `convertraster` consumes these libraries — that
guess in the prompt's framing was checked and is incorrect. The real,
and only, tree-wide consumers (confirmed via `grep -rl "TIFFLIB\|
JPEGLIB\|libtiff\|libjpeg" --include=Imakefile src/`, which found
exactly these two Imakefiles outside the codec directories
themselves):

- **`atk/basics/common`** builds `jpeg.do` (`DynamicMultiObject(jpeg.do,
  jpeg.o imageaux.o, $(JPEGLIB), )`), the JPEG-format filter for the
  general `image`/`igraphic` inset family. Confirmed via `nm -g
  build/dlib/atk/jpeg.do`: `_jget_small` (from `jmemsys.c`, one of the
  files touched this session) is defined (`T`) directly in `jpeg.do`'s
  own symbol table, alongside the `jpeg__*` class-dispatch entry
  points.
- **`atk/image`** builds `tif.do` (`DynamicObject(tif, $(TIFFLIB),
  $(MATHLIB) $(BSDLIB))`), the TIFF-format filter for the same `image`
  inset family. Confirmed via `nm -g build/dlib/atk/tif.do`:
  `_TIFFOpen`, `_TIFFReadDirectory`, `_TIFFWriteDirectory` all defined
  (`T`) directly in `tif.do`'s own symbol table — `_TIFFWriteDirectory`
  specifically lives in `tif_dirwrite.c`, this session's most heavily
  fallout-affected file, making `tif.do` the load-bearing check for
  §5's fixes.

Both `.do` files are dynamically loaded by `ez`/`runapp` (and any other
`atk` app using the general image inset) when a document contains an
image inset backed by a `.jpg`/`.jpeg` or `.tif`/`.tiff` file.

### Exact runtime-check commands for wdc

Native Terminal.app, not an IDE terminal; `DISPLAY=:0;` as its own
statement first; use a **fresh** `ez` process (`.do` files cache for
the life of a process).

1. **JPEG path** (exercises `jpeg.do`, all of `overhead/image/jpeg`'s
   real build, including the `jmemsys.c`/`jmemmgr.c`/`jchuff.c`/
   `jcmain.c`/`jdmain.c` files whose `ansify` false-positive was
   resolved this session):
   ```
   DISPLAY=:0; ez
   ```
   Insert an Image inset (Insert menu or `<ESC><TAB>image`) pointing at
   a real `.jpg`/`.jpeg` file. Confirm it decodes and displays without
   a crash or garbled output.
2. **TIFF path — the load-bearing check for this session's most
   significant fixes** (§5, `tif_dirwrite.c`'s directory-write path;
   also exercises `tif_dir.c`/`tif_dirinfo.c`/`tif_lzw.c`/`tif_open.c`,
   all touched this session):
   ```
   DISPLAY=:0; ez
   ```
   Insert an Image inset pointing at a real `.tif`/`.tiff` file
   (a plain uncompressed or LZW-compressed TIFF exercises
   `tif_lzw.c`/`tif_open.c`'s conversions; `tif_dirwrite.c`'s functions
   are the *write*-directory path — if there's an AUIS action that
   saves/re-exports the document with the TIFF inset embedded, that
   would additionally exercise `TIFFWriteDirectory` and siblings, but I
   did not find a dedicated "re-save as TIFF" UI action to point at
   specifically; reading/displaying an existing TIFF is the safe,
   available check).
3. `tif_fax3.c`/`mkg3states.c`'s Group 3/4 fax-compression codec path
   (the prompt's 5 originally-diagnosed functions) is exercised by any
   CCITT Group 3 or Group 4 compressed TIFF specifically — a plain
   uncompressed or LZW TIFF does NOT reach this code. If a test TIFF
   with `Compression=3` or `Compression=4` (in the TIFF tag sense) is
   available, that's the specific file to use for #2's check to also
   cover this path; otherwise it remains covered only by the direct
   `make tif_fax3.o`/`mkg3states.o` compiles already confirmed in §4.1.

## 7. `fossil status` — 32 files changed, nowhere else

```
$ fossil status | grep -E "overhead/image/(jpeg|tiff)" | wc -l
32
```

- `overhead/image/jpeg`: 1 file — `ansi2knr.c` (the only file with
  real K&R content; converted cleanly, 5 helpers).
- `overhead/image/tiff`: 31 files (all but `tif_version.c`, which
  reported "no K&R definitions converted" — already ANSI, untouched).

No commit made. `m3-o2-imagecodecs-session.diff` (4,166 lines, 32
files) written to the tree root. `fossil extras` not separately
checked this session (no scratch/investigation scripts were run inside
the tree — the `convert_only.py` helper script used for `tif_dirwrite.c`
lived entirely in the scratchpad directory, never in the checkout).

## 8. New `ansify` fallout patterns found this session (summary)

Three, all new, none previously in `porting-assessment.md` §14/§17 or
`m3-rollout-runbook.md`'s findings section — all specific to vendored
third-party code's own K&R/ANSI dual-declaration conventions, matching
the prompt's own prediction that this batch would surface shapes
AUIS-native code doesn't:

1. **`(void)`-as-bare-parameter misparse** (§4.2) — an already-ANSI
   zero-argument function definition `NAME (void)` is misread by
   `convert_file`'s helper matcher as a K&R function with one
   undeclared parameter literally named `void`, producing `NAME(int
   void)`. Caught and auto-reverted by the compile gate every time (no
   corruption risk on its own), but inflates both the dry-run census
   and the real-run "compile failure" count with false positives.
2. **`DECLARE<N>(name, type, arg, ...)` macro-invocation idiom
   misparse** (§4.3) — both `convert_file` (when the macro's type
   arguments are all bare single-word tokens) and `fix-missing-static-
   decl` (whenever `static` precedes the invocation, regardless of
   argument types) misidentify the macro name itself (`DECLARE2`,
   `DECLARE3`, etc.) as a real, single K&R function name shared across
   every use of that arity in the file. Also caught by the compile gate
   in the common case — **except**:
3. **Compile gate is skipped entirely when only `fix-missing-static-
   decl`/`fix-static-methods` modify a file and `convert_file` finds
   nothing to convert** (§5, closing sub-section) — a real gap in
   `ansify`'s own safety design, not just a false-positive parser
   quirk. This is the one finding in this session that rises above
   "annoying but self-healing" to "can silently leave broken code on
   disk with a success-looking report." Recommend this be the first
   `ansify` tool fix taken up, ahead of the two parser-recognition
   improvements, given the difference in blast radius.

## 9. Open questions / anything that surprised you

- The gate's first real attempt on `tiff` genuinely failed (20 errors)
  — caused by my own "let me double-check with ansify --dir again"
  verification step re-triggering finding 3 above, not by any gap in
  the actual fallout-fixing work. Worth a general lesson for future M3
  sessions: re-running `ansify --dir` on an already-completed directory
  as a "sanity check" is not free and can itself introduce fallout via
  finding 3 — the real gate (`make clean && make depend && make -k
  install`) is the authoritative check, not `ansify`'s own report.
- I did not run any AUIS GUI or terminal binary interactively, and made
  no commits, per the prompt.
- The batch prompt's guess that `raster`/`convertraster` might consume
  these libraries was checked and found incorrect — the only two
  consumers tree-wide are `atk/basics/common`'s `jpeg.do` and
  `atk/image`'s `tif.do`, both general `image`-inset format filters.
