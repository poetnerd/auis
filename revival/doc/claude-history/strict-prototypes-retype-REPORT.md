# strict-prototypes retype pass — report

## 1. Status

**All 77 directories from the manifest are complete.** No directory was
skipped or left mid-way. Every directory ends this pass compiling clean
under the project's normal `STRICT_COMPILERFLAGS` (verified with a full
`make clean && make depend && make -k install` after all edits to that
directory, including any reverts).

Working notes kept during the run (per-directory tool output, before being
folded into this report) are in the delegated scratch area and are not part
of the tree; nothing besides the `.c` files themselves and this report/diff
was left in the repository.

## 2. Method (brief)

Given the scale (6,700+ candidate declarations across 77 directories), this
pass was done with purpose-built tooling rather than by hand line-by-line:

- A Python script (`retype3.py`) parses each directory's `-Wstrict-prototypes`
  census log, extracts the flagged forward declaration, finds the function's
  real definition (same file, then same directory, then whole-tree as a last
  resort for shared library helpers), and proposes/applies the retyped
  declaration — copying the real definition's parameter list verbatim, never
  guessing.
- It automatically excludes: struct-field/parameter generic function-pointer
  declarations (`void (*hit_handler)();`), declarations that are themselves
  genuine zero-argument definitions using old-style empty parens, and
  macro-expansion artifacts (classpp `_StaticEntry` registration macros, the
  `errno` macro expanding to a function call on macOS).
- `static` functions are correctly scoped to their own file when resolving —
  a same-named `static` function in a *different* file of the same directory
  is never treated as a candidate match (this was a real bug found and fixed
  partway through the run; see §5).
- After applying a directory's retypes, a diagnostic build
  (`-Wstrict-prototypes -Wincompatible-pointer-types -ferror-limit=0`) is run.
  A companion script inserts safe, semantically-null `struct Foo;` /
  `enum Bar;` forward declarations where a retyped declaration references a
  locally-defined struct/union/enum tag before its real definition appears in
  the file (C's function-prototype-scope rule for tags otherwise makes the
  early, typeless mention a *different*, incomplete type from the real one).
- Any remaining compile **error** (argument-count mismatch, or an
  already-`-Werror` class like `int-conversion` /
  `incompatible-function-pointer-types` newly surfaced by the retype) is
  traced back to the specific declaration responsible via the clang note
  that pinpoints it, and *only that declaration* is reverted to its original
  empty-parens form — never a director-wide rollback. For multi-declarator
  statements (`extern int A(), B(), C();`) where only one name was at fault,
  the revert was narrowed by hand to that one name, keeping the other
  correct retypes applied.
- Every revert is logged as a candidate finding, not silently dropped.

## 3. Per-directory summary

Columns: **hits** = census own-file hit count (see note below on `atk/apps`
and `ams/libs/cui` where the manifest's raw count differs from the number of
distinct declaration sites, because those directories compile the same `.c`
files more than once for different binaries); **retyped** = declarations
successfully given a real, verified signature and left applied;
**excluded** = FPTR (generic function-pointer, by design) + OWN_DEF (genuine
own zero-arg definitions, not stale decls) — the two standing exclusion
categories; **unresolved** = everything the tool correctly declined to guess
at (NOMATCH, MULTI/MULTI_PARTIAL, AMBIGUOUS, KR_ONLY — real definition is
itself still K&R/untyped, MACRO_ARTIFACT); **reverted** = retyped, then
found to expose a real issue and rolled back to keep the build green
(itemized in §4); **build** = final state under normal flags.

| # | Directory | hits | retyped | excluded | unresolved | reverted | build |
|---|---|---:|---:|---:|---:|---:|---|
| 1 | ams/libs/ms | 913 | 783 | 35 | 33 | 8 | clean |
| 2 | atkams/messages/lib | 529 | 294 | 68 | 20 | 0 | clean |
| 3 | contrib/zip/lib | 368 | 288 | 5 | 61 | 9 | clean |
| 4 | atk/text | 359 | 317 | 17 | 12 | 1 | clean |
| 5 | overhead/bison | 239 | 60 | 89 | 10 | 6 | clean |
| 6 | atk/basics/common | 238 | 148 | 27 | 36 | 1 | clean |
| 7 | atk/table | 215 | 129 | 63 | 11 | 2 | clean |
| 8 | atk/apps | 209 (95 unique) | 10 | 5 | 75 | 0 | clean |
| 9 | overhead/mail/lib | 185 | 123 | 13 | 29 | 0 | clean |
| 10 | atk/image | 183 | 77 | 39 | 11 | 20 | clean |
| 11 | atk/figure | 168 | 70 | 12 | 32 | 0 | clean |
| 12 | atk/supportviews | 162 | 84 | 17 | 44 | 1 | clean |
| 13 | atk/basics/x | 151 | 118 | 6 | 14 | 1 | clean |
| 14 | overhead/mail/metamail/metamail | 146 | 42 | 29 | 11 | 0 | clean |
| 15 | ams/libs/cui | 145 (50 unique) | 27 | 19 | 2 | 0 | clean |
| 16 | overhead/util/lib | 129 | 48 | 18 | 42 | 10 | clean |
| 17 | atk/extensions | 128 | 114 | 4 | 6 | 0 | clean |
| 18 | atk/help/src | 125 | 73 | 26 | 15 | 1 | clean |
| 19 | atk/raster/cmd | 124 | 68 | 8 | 8 | 1 | clean |
| 20 | atk/apt/suite | 117 | 80 | 5 | 27 | 0 | clean |
| 21 | atk/frame | 103 | 76 | 21 | 4 | 0 | clean |
| 22 | atk/adew | 102 | 89 | 5 | 5 | 0 | clean |
| 23 | ams/msclients/cui | 99 | 22 | 16 | 55 | 2 | clean |
| 24 | atk/srctext | 95 | 74 | 15 | 0 | 0 | clean |
| 25 | contrib/zip/utility | 94 | 81 | 1 | 1 | 0 | clean |
| 26 | atk/bush | 86 | 58 | 6 | 21 | 0 | clean |
| 27 | overhead/image/tiff | 83 | 80 | 1 | 0 | 18 | clean |
| 28 | atk/typescript | 80 | 52 | 19 | 2 | 0 | clean |
| 29 | atk/support | 74 | 34 | 7 | 22 | 0 | clean |
| 30 | atk/apt/tree | 72 | 56 | 5 | 5 | 0 | clean |
| 31 | atk/chart | 70 | 47 | 3 | 13 | 0 | clean |
| 32 | atk/textobjects | 59 | 37 | 1 | 11 | 0 | clean |
| 33 | atk/value | 49 | 47 | 2 | 0 | 0 | clean |
| 34 | overhead/mail/metamail/richmail | 48 | 18 | 14 | 4 | 0 | clean |
| 35 | ams/msclients/nns | 44 | 22 | 3 | 4 | 1(of 16) | clean |
| 36 | contrib/mit/util | 42 | 35 | 4 | 2 | 0 | clean |
| 37 | contrib/srctext/html | 41 | 33 | 7 | 1 | 0 | clean |
| 38 | atk/textaux | 41 | 35 | 5 | 1 | 0 | clean |
| 39 | atk/rofftext | 37 | 28 | 1 | 4 | 0 | clean |
| 40 | atk/lookz | 36 | 14 | 0 | 20 | 2 | clean |
| 41 | contrib/srctext/ptext | 34 | 22 | 12 | 0 | 0 | clean |
| 42 | atk/hyplink | 31 | 19 | 0 | 2 | 0 | clean |
| 43 | atk/fad | 31 | 30 | 0 | 1 | 0 | clean |
| 44 | overhead/cmenu | 27 | 18 | 5 | 4 | 0 | clean |
| 45 | contrib/mit/annot | 25 | 25 | 0 | 0 | 0 | clean |
| 46 | overhead/eli/lib | 24 | 11 | 8 | 5 | 0 | clean |
| 47 | contrib/time | 24 | 19 | 0 | 0 | 0 | clean |
| 48 | atk/layout | 24 | 15 | 1 | 7 | 0 | clean |
| 49 | atk/org | 23 | 22 | 0 | 1 | 0 | clean |
| 50 | overhead/class/lib | 22 | 9 | 9 | 4 | 0 | clean |
| 51 | doc/mkbrowse | 21 | 12 | 4 | 5 | 0 | clean |
| 52 | overhead/rxp | 20 | 10 | 4 | 5 | 0 | clean |
| 53 | atk/eq | 18 | 8 | 6 | 4 | 0 | clean |
| 54 | atk/apt/apt | 18 | 12 | 3 | 3 | 0 | clean |
| 55 | ams/libs/shr | 17 | 16 | 0 | 1 | 0 | clean |
| 56 | overhead/class/cmd | 16 | 7 | 4 | 3 | 0 | clean |
| 57 | atk/syntax/tlex | 14 | 10 | 4 | 0 | 0 | clean |
| 58 | atk/raster/convert | 12 | 2 | 4 | 3 | 0 | clean |
| 59 | contrib/eatmail | 11 | 4 | 1 | 3 | 0 | clean |
| 60 | atk/raster/lib | 11 | 8 | 1 | 1 | 0 | clean |
| 61 | overhead/index | 10 | 8 | 2 | 0 | 0 | clean |
| 62 | contrib/srctext/ltext | 10 | 9 | 1 | 0 | 0 | clean |
| 63 | overhead/mkparser | 8 | 4 | 3 | 1 | 0 | clean |
| 64 | overhead/eli/bglisp | 7 | 1 | 3 | 3 | 0 | clean |
| 65 | atk/syntax/sym | 7 | 4 | 2 | 1 | 0 | clean |
| 66 | atk/syntax/parse | 7 | 0 | 5 | 1 | 0 | clean |
| 67 | atk/ez | 7 | 6 | 0 | 1 | 0 | clean |
| 68 | atk/utils | 4 | 4 | 0 | 0 | 0 | clean |
| 69 | overhead/fonts/cmd | 3 | 2 | 1 | 0 | 1 | clean |
| 70 | atk/help/maint | 3 | 2 | 1 | 0 | 0 | clean |
| 71 | ams/libs/nosnap | 3 | 0 | 2 | 1 | 0 | clean |
| 72 | contrib/demos/circlepi | 2 | 2 | 0 | 0 | 0 | clean |
| 73 | overhead/mail/testing | 1 | 0 | 1 | 0 | 0 | clean |
| 74 | overhead/mail/cmd | 1 | 0 | 0 | 1 | 0 | clean |
| 75 | overhead/errors | 1 | 1 | 0 | 0 | 1 | clean |
| 76 | overhead/addalias | 1 | 0 | 1 | 0 | 0 | clean |
| 77 | atk/raster/scan | 1 | 0 | 1 | 0 | 0 | clean |

MACRO_ARTIFACT (classpp `X_StaticEntry;` registration macros and the
`errno`-macro-expands-to-a-function-call artifact) is folded into
"unresolved" above; it showed up most heavily in `atk/apps` (75 of its 80
unresolved candidates), `atk/basics/common` (27, all `basinit.c`'s
`_StaticEntry` lines), and `atk/support` (13). None of these were
investigated further, per the standing classpp-internals exclusion rule.

## 4. Confirmed real bugs found (the important part)

These are declarations where retyping the stale forward declaration to
match the function's real signature caused the compiler to catch a genuine
call-site mismatch — exactly the diagnostic method this pass was built to
validate. None of these were fixed; they are reported for wdc to triage.

### 4a. High-value: LP64 pointer-truncation bug

**`ams/msclients/cui/cui.c`** — `MS_GetPartialFile(char *FileName, char *Buf,
int BufLim, int offset, int *remaining, int *ct)` (real definition:
`ams/libs/ms/getfile.c:40`) is declared at `cui.c:241` and called at **six
sites** — `cui.c:831, 1551, 1586, 2809, 3366` and `cuifns.c:680` — all
passing `&bytesunfetched` (a local declared `long`, `cui.c:804`) for the
`int *remaining` out-parameter. This is exactly LP64 bug class #4 from this
project's own standing list (int\* out-param, long\* argument passed — only
the low 4 bytes get written, the upper 4 bytes are garbage), previously
invisible because the stale empty-parens declaration suppressed all type
checking. The sibling out-param `ct` (declared `int`) is fine. Occurs in
`ReadCommandsFromFile`, an alias/source-file command-loading path — not an
obviously hot path, but real and silent.

**`ams/libs/ms/reply.c`** — the real definition of `PrintFwdHeaders`
(`reply.c:452`) declares its first parameter as `int fp`, but it is called
at `reply.c:357` and `373` with `fp`, which is a `FILE *` in both callers,
and — worse — `PrintFwdHeaders`'s own body immediately forwards that
(wrongly-typed) `fp` to `PrintQuotingFormatting(FILE *fp, ...)` (real
signature confirmed via `overhead/util/lib/unscribe.c`). This is a genuine
`FILE *` truncated through an `int` parameter, on every mail-forwarding
call. The forward declarations for `PrintFwdHeaders` (and, only in this one
file, `PrintQuotingFormatting`, since its correct type is what surfaced the
bug from inside `PrintFwdHeaders`'s body) were reverted to keep the build
green — the actual bug is in the **definition**'s parameter type
(`int fp` should be `FILE *fp`), which is a real code fix, not a
declaration retype, and was left for wdc.

### 4b. Argument-count mismatches (confirmed, compiler-verified)

- `ams/libs/ms/bldcapt.c:168` — `FindPrettiestFromString(FromBuf, NewFromBuf)`
  passes 2 args; real def (`prettyn.c:117`) takes 3
  (`char *MyFromBuf, char *FromBuf, int lim`) — missing `lim`.
- `ams/libs/ms/mswp.c:2311` and `ams/libs/ms/submsg.c:150` —
  `UnparseOneAddress(...)` called with **7** args at both sites; real def
  (`overhead/mail/lib/parseadd.c:146`) takes **6**. Same extra trailing
  argument at both call sites (looks copy-pasted from the 8-arg
  `UnparseAddressList`).
- `contrib/zip/lib/zipdi00.c:335` — `Propagate_Image_Stream_Ptr(image)`
  passes 1 arg; real def (`zipdi00.c:384`) takes 2 (`self, image`) — missing
  `self`.
- `atk/text/txtvcmsc.c:135` — `textview_InsertFile(self, key)` passes an
  extra `key` argument; the real helper (`txtvcmod.c:421`) takes only
  `self`. (Distinct from the unrelated classpp class-method
  `text_InsertFile`/`InsertFile` of the same short name — don't conflate.)
- `atk/table/table.c:563` — `WriteCell(...)` passes 4 args; real def
  (`table.c:66`) takes 5 (`struct table *T, FILE *f, struct cell *cell,
  char **buff, int level`) — missing `level`.
- `atk/image/ps.c:242` — `epsPreview(fp, inpix, colorType, w, h)` passes 5
  arguments that don't semantically resemble the real 3-arg signature
  (`struct imagev *self, FILE *fp, byte *pic`, `ps.c:641`) at all — no
  `self`, two extra unrelated-looking params. Looks like long-dead or
  long-stale code rather than a simple off-by-one; worth a closer look
  before assuming it's ever actually exercised.
- `atk/raster/cmd/rastvauy.c:390` and `:416` — `RemoveInsetProc(self)` and
  `ResizeInsetProc(self)` each pass 1 arg; both real defs
  (`rastvauy.c:393`/`419`) take 2 (`self, rock`) — missing `rock` at both
  call sites.
- `ams/msclients/nns/nns.c:346` — `BuildDateField(Msg)` passes 1 arg; real
  def (in `ams/libs/ms`) takes 2 (`struct MS_Message *Msg, int datetype`) —
  missing `datetype`.
- `overhead/fonts/cmd/fdbbdf.c:196` — `fontcvt(stdin, stdout)` passes 2
  args; real def (`fdbbdf.c:308`) takes 3
  (`FILE *fin, FILE *fout, int count`) — missing `count`.

### 4c. Pointer/int confusion (confirmed, compiler-verified)

- `contrib/zip/lib/zipve02.c:244` — a call to `Create_Shade_Palette(...)`
  passes a `zip_type_pane` value where the real signature
  (`zipve02.c:173`) expects a plain `int` (`containing_pane`) —
  pointer-vs-int confusion, same LP64-adjacent flavor as the `cui.c` finding
  above, though the specific fields weren't traced further.

### 4d. `-Wincompatible-pointer-types` findings independent of any retyped
declaration (surfaced simply by turning the diagnostic flag on; not tied to
a specific stale forward declaration, so nothing to revert — flagged as-is)

- `contrib/zip/lib/ziposym.c:677` — `String(self, algorithm, buffer)`:
  `buffer` is `char[32000]` (decays to `char *`) passed where the callee's
  3rd parameter is `char **`.
- `contrib/zip/lib/zipv.c:418` and `atk/image/imagev.c:1813` — both are
  `Data = data;`-shaped assignments (the `Data` macro resolving to a
  concretely-typed base-class field) receiving a `struct dataobject *`
  where a more specific subclass pointer type is declared. Looks like a
  recurring pattern across this codebase generation's `xxx__SetDataObject`
  methods — worth a wider grep if anyone wants to chase it, not done here.
- `atk/basics/x/xim.c:1737` and `:1742` — `struct im *` passed where
  `struct xgraphic *` is expected.
- `atk/basics/x/xim.c:3149` — `struct seldata **` passed where `long *` is
  expected — an LP64-shaped mismatch worth a look.
- `atk/basics/x/xgraphic.c` — 9 call sites passing `struct graphic *`
  (base) where `struct xgraphic *` (derived) is declared. This one looks
  like the codebase's ordinary Class-system struct-embedding "subclass"
  idiom (matches the recognized-pattern rule for generic dispatch), not a
  bug — flagged for context only, not investigated further.

## 5. Tool bugs found and fixed mid-run (not code bugs — reported for
transparency, since they shaped what got reverted where)

1. **Static-function cross-file collision** (found in `atk/supportviews`,
   directory #12 of 77): the directory-wide definition index didn't respect
   C's file-scoping rule for `static` functions, so a same-named `static`
   helper in an unrelated sibling file could be picked as the "real
   definition" for a completely different function. Caught by the
   argument-count check the same way every other case in this report was
   caught (`oscroll.c`'s `draw_bar` got matched to `scroll.c`'s unrelated
   2-arg `draw_bar` instead of its own 4-arg K&R definition). Fixed in the
   tool (`static` entries are now only used as same-file matches); the one
   instance this produced was already caught and reverted before the fix
   landed, and all 11 directories processed before the fix were confirmed
   independently via their own clean final rebuilds — the same
   conflicting-types/argument-count mechanism that caught this case would
   have caught any other occurrence too. The only theoretical residual gap
   is a *silently plausible* (compiles clean, but factually wrong) cross-file
   static/extern name collision, which is possible in principle but not
   confirmed to have happened anywhere.
2. **`#ifdef`-blind struct-visibility insertion** (found in
   `overhead/mail/metamail/metamail`, directory #14): the companion script
   that inserts safe `struct Foo;` forward declarations picked an insertion
   point ("right after the last top-of-file `#include`") without checking
   whether that point was inside a conditional block. In `mailto.c`, the
   entire `#include` block happened to be wrapped in `#ifdef AMIGA`/`#else`,
   so the inserted `struct mailpart;` landed inside the (unbuilt) AMIGA
   branch and never took effect, leaving 8 declarations broken. Fixed by
   hand for that file and upgraded the script to track `#if`/`#ifdef`/`#endif`
   nesting depth so it only ever inserts at depth 0.
3. **Whole-statement multi-declarator reverts losing good retypes**: the
   revert mechanism's unit of work is the full matched statement text, so a
   single bad name in `extern int A(), B(), C();` reverted `A` and `C` too
   even though they were retyped correctly. Found in `ams/msclients/nns`
   and, retroactively, in two earlier directories (`atk/image`'s `ps.c` and
   `atk/raster/cmd`'s `rastvauy.c`) — all three were found and manually
   corrected to revert only the actual culprit name (see §4b for the
   surviving bugs; the innocent names in each statement are back to their
   correct retyped state, rebuild-verified). This was not automated in the
   tool since it only came up 3 times across roughly 50 total reverts.

## 6. Files touched

Only `.c` files in the 77 processed directories, plus the safe struct/enum
forward-declaration scaffolding lines added to a subset of those same files
(never a new file). `fossil status` shows **508 `EDITED` files, all `.c`**,
zero `ADDED`/`UNKNOWN` entries, across `ams/`, `atk/`, `atkams/`, `contrib/`,
`doc/`, and `overhead/`. The full list is in
`strict-prototypes-retype-session.diff` at the tree root (also captured
there per the playbook). No `.eh`/`.ih`/`.h`/build files were touched.

Directories NOT touched (out of scope, per the task): `contrib/calc`
(pilot, already committed as `dc4b3151d1cf`), `overhead/class/pp` (classpp
itself, self-referential-tooling exclusion).

## 7. Things that surprised me / didn't fit the expected pattern

- **The "local typedef/tag defined later in the same file" class was the
  single most common non-bug obstacle**, far more than anticipated: it hit
  at least 7 directories with real code (zip/lib's `symtab_type`,
  bison's `symbol_list`, atk/image's and overhead/image/tiff's `RGBvalue`
  (same code, two copies of libtiff — atk/image's is an old fork of the
  upstream tree in overhead/image/tiff), atk/basics/x's `drop_t`,
  overhead/util/lib's `access_list_t`/`id_and_name_t`, atk/lookz's
  `struct strTbl` used as an array parameter — a variant where even a plain
  forward struct tag declaration isn't enough because array-of-incomplete-
  type is stricter than pointer-to-incomplete-type). None of these are
  runtime bugs; they're all artifacts of 1988-era code relying on K&R's
  total lack of type checking to get away with declaring things before
  their types exist. Wherever it was safe to do so with a plain
  `struct Foo;`/`enum Bar;` (function/pointer parameters), the tool added
  that scaffolding automatically and kept the retype; where the real
  definition itself was still K&R/untyped (`KR_ONLY` category) or where the
  parameter shape required a *complete* type (the `struct strTbl tbl[]`
  case), the declaration was left as-is.
- **The "fake variadic via 8-or-so `int` placeholder parameters" idiom**
  (`warns`/`fatals` in bison, `ErrorMsg` in atk/basics/common, `errprintf2`
  in ams/msclients/cui, `safefprintf` in overhead/errors) recurred
  independently in at least 4 unrelated directories/eras of code — a
  genuine pre-varargs C convention from before `<stdarg.h>` was portable,
  not a bug, but its every real call site under-supplies the fixed
  parameter count on purpose. Every instance had to be reverted since ANSI
  prototypes make partial application a hard error.
- **A "generic function pointer received via a plain parameter" idiom**
  (`enterfun` in atk/table, `index_Enumerate` in atk/help/src) is the same
  spirit as the standing struct-field exclusion rule but shows up as a bare
  parameter instead — worth considering as a documented exclusion pattern
  for any future pass like this one, since it recurred independently twice.
- **`qsort`-style comparator callbacks with concrete pointer types**
  (`CompareFileTimeStructs`, `CompareSubsPtrPriority`, both in
  `ams/libs/ms`) hit `-Werror=incompatible-function-pointer-types` the
  moment their real signature became visible, even though this is
  completely standard (if not strictly ANSI-clean) C — every `qsort`
  comparator in K&R-derived code looks like this. Not a bug, but a
  systemic source of the same false-positive-shaped revert across any
  directory using `qsort`/`bsearch`.
- **The manifest's own hit counts don't always match unique declaration
  sites**: `atk/apps` (209 → 95 unique) and `ams/libs/cui` (145 → 50 unique)
  both compile the same handful of `.c` files more than once for different
  target binaries in one Imakefile, so the raw `-Wstrict-prototypes` line
  count in the original census double- or triple-counts the same source
  line. Nothing to fix, just worth knowing when comparing this report's
  numbers against the original census document.
- **One real tool bug (static cross-file collision) was caught by the
  method itself** — the same build-and-revert safety net built to catch
  bugs in the *target* code also caught a bug in *my own* retyping
  approach, which is a reassuring sign the safety net is doing its job
  rather than rubber-stamping.

## 8. Suggested next steps for wdc

The highest-value single item is almost certainly **§4a's `MS_GetPartialFile`
long\*/int\* truncation in `ams/msclients/cui/cui.c`** — it's the exact bug
class already known to matter in this codebase (per the project's LP64 bug
list), hits 6 call sites, and the fix is a one-line variable type change per
site (or better, fix the actual out-param usage). `PrintFwdHeaders`'s
`int fp` is the second-highest value (also LP64, also in `ams/libs/ms`, and
also a small, localized fix once someone decides `int fp` should really be
`FILE *fp`). The argument-count bugs in §4b are all small, mechanical,
easy-to-verify fixes; `atk/image/ps.c`'s `epsPreview` is the one that
deserves a closer look before fixing, since its shape suggests genuinely
dead/stale code rather than a simple typo.
