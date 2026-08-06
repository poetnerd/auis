# M4 Wave 4, batch I4: 12 small/leaf inset directories — report

## 1. Status

All 12 directories reach a clean gate. For each:
`make clean && make depend && make -k install` exits 0, and
`grep -c "error:"` on the full install log is **0** (confirmed twice
per directory: once with the plain gate command, once more with
`CDEBUGFLAGS="-O -ferror-limit=0"` to rule out truncated diagnostics).
**No fossil commit was made.** `fossil status` shows exactly 59
`EDITED` files across the 12 directories (12 `Imakefile`s + 47 source
files, listed in full in §4), all touched by this session. No
pre-existing unrelated edits were found in the working tree at session
start (`fossil status` was clean before beginning).

Directories: `atk/image`, `atk/fad`, `atk/srctext`, `atk/org`,
`atk/bush`, `atk/rofftext`, `atk/raster/lib`, `atk/hyplink`, `atk/eq`,
`atk/layout`, `atk/raster/scan`, `atk/raster/convert`.

**4 genuine datastream/memory-corruption bugs found and fixed** (see
§3c) — smaller in count than I1's or I3's risk-batch findings, but one
(`atk/raster/lib/rasterio.c`) is a real scanf-direction LP64
memory-corruption bug on the shared raster datastream reader, and one
(`atk/raster/convert/convrast.c`) is the same bug class on a
standalone tool's command-line parser.

## 2. What was done, in order

1. Read `revival/doc/sonnet-playbook.md` and `revival/doc/
   m4-rollout-runbook.md` in full (including "Execution mechanism
   corrected", the `-Wformat` scope ruling, and the function-pointer
   fix policy's 5 rules) before starting. Read `i3-REPORT.md` and
   skimmed `i2-REPORT.md` as precedent/model, per the task prompt.
2. `fossil status` — clean starting tree, confirmed.
3. Confirmed `STRICT_COMPILERFLAGS` already defined in
   `src/config/darwin/system.mcr` (Batch 0, untouched this session).
4. For each of the 12 directories: added or replaced the
   `COMPILERFLAGS = $(STRICT_COMPILERFLAGS)` override in its
   `Imakefile`. Two directories (`atk/rofftext`, `atk/eq`) already
   carried the M2-era narrow override
   (`-Werror=implicit-function-declaration` only) — that line was
   **replaced**, not stacked, per the runbook's explicit warning about
   two assignments silently letting the last one win. The other 10
   directories had no prior override; the line was added next to each
   directory's existing `CLASSFLAGS` line (or near the top of the
   Imakefile for the two program-only directories,
   `atk/raster/scan`/`atk/raster/convert`, which have no `CLASSFLAGS`
   line at all).
5. Regenerated each directory's Makefile (`make Makefile`) and ran the
   Gate-0 dry-run census one directory at a time, never concurrently:
   `make clean`, `make depend`, then
   `make -k install CDEBUGFLAGS="-O -ferror-limit=0" > <dir>.log 2>&1`.
   Real per-directory error counts (`-ferror-limit=0`, before any
   fixes), vs. the batch map's stale pre-`-Werror=format` census:

   | Directory | Census | Real | Δ |
   |---|---|---|---|
   | `atk/image` | 31 | 38 | +23% |
   | `atk/fad` | 29 | 29 | 0% |
   | `atk/srctext` | 22 | 28 | +27% |
   | `atk/org` | 19 | 27 | +42% |
   | `atk/bush` | 16 | 21 | +31% |
   | `atk/rofftext` | 15 | 17 | +13% |
   | `atk/raster/lib` | 9 | 24 | +167% |
   | `atk/hyplink` | 8 | 12 | +50% |
   | `atk/eq` | 8 | 8 | 0% |
   | `atk/layout` | 7 | 64 | +814% |
   | `atk/raster/scan` | 2 | 2 | 0% |
   | `atk/raster/convert` | 2 | 11 | +450% |
   | **Total** | **168** | **281** | **+67%** |

   Gap fully explained the same way as every prior M4 batch: the
   original per-directory census predates the `-Werror=format`
   ruling. `atk/layout`'s outsized gap (7→64) is because `layoutv.c`,
   `boxview.c`, `layout.c`, and `box.c` each carry a near-identical
   family of debug-trace `printf`/`fprintf` calls (component
   coordinates, pointer identities) that were never counted — see §3a.
6. Fixed every real error, directory by directory, largest first
   within a directory when there were natural groupings; smaller
   directories fixed file-by-file. Each fix was checked against the
   real declared type in the relevant `.ch`/`.h` file before landing,
   per the task's instructions — this is what surfaced the datastream
   bugs in §3c.
7. Ran the mandatory malloc/free/realloc/calloc sweep (word-boundary
   grep, `grep -nE "\bmalloc *\(|\bfree *\(|\brealloc *\(|\bcalloc *\("`
   ) across every `.c` file in each of the 12 directories individually
   — see §3e. No missing declarations found anywhere (all builds were
   already clean at the point the sweep was run in each directory,
   confirming `<stdlib.h>`/`<andrewos.h>` reaches every allocation
   site transitively); no wrapper-name collisions.
8. Gate, each directory: `make clean && make depend && make -k
   install` (plain, no `-ferror-limit=0`) → exit 0, 0 errors, full
   install completed to the end (log tail confirms `.do`s/binaries,
   `.ch`/`.ih` headers, help/docs — not just "stopped before the
   failing file"). All 12 directories confirmed this way.
9. `fossil diff > i4-session.diff` in the tree root (2923 lines).

## 3. Findings

### 3a. Error-mix summary across all 12 directories (281 total)

Dominated by `-Wformat` (roughly 60% of the total, consistent with
every prior I-wave batch) and `-Wimplicit-int` (the classic
"K&R definition missing an explicit return type" shape, found in
nearly every directory). `-Wincompatible-function-pointer-types` was
a smaller share than I1/I2 (no directory here crossed the "~30-35%
typical" escalation flag) and resolved entirely to precedented shapes
— see §3b. `-Wimplicit-function-declaration` was almost entirely two
recurring causes: a missing `#include <andrewos.h>` for the
`strcmp`/`strcpy`/`strlen`/`strncmp`/`index` family (7 files), and the
tree's well-known `AndrewDir()` missing-declaration pattern (3 files,
§3d).

`atk/layout`'s 64 errors were almost entirely one shape, repeated
across `layoutv.c`/`boxview.c`/`layout.c`/`box.c`: parallel debug/print
traces for the layout and box insets (`layoutview_InitializeClass`,
`_FullUpdate`, `_Print`, `_WantUpdate`, `_Read`, `_GetModified`, etc.)
printing `long` coordinate/id fields via `%d` and pointer identities
via `%x`. All mechanical width fixes (`%d`→`%ld`, `%x`→`%lx` with an
explicit `(long)` cast on the pointer argument, matching the
`%lx`-with-cast idiom already used for debug pointer prints
elsewhere in the tree) — see §3d for the two genuine non-mechanical
findings in this same set (`layout.c`'s missing-arg format string and
copy-paste debug-trace bug).

### 3b. Function-pointer casts landed, organized by shape

**Shape 1 — `proctable_DefineProc`/`im_EnqueueEvent` `procedure`
registration (23 sites total).** The established I1/I2/I3 idiom
(`procedure`, aka `int (*)()`) — `srctextv.c` (2:
`forceupperoff`/`forceupperon`), `bushv.c` (9: `ToggleDebug`,
`PerformPop`, `SwitchDirectory`, `PerformRescan`, `PerformDestroy`,
`EntriesPageUp`, `EntriesPageDown`, and 2 `im_EnqueueEvent(Checkpoint,
...)` call sites), `pshbttnv.c` (4: `LabelProc`, `FontProc`,
`StyleProc`, `ColorProc`), `linkv.c` (4: `TargetProc`, `InsertProc`,
`AutolinkProc`, `LinkProc`), `eqvcmds.c` (1: `eqview_Default`),
`fillerv.c` (1: `fillerview__SetDataObjectByName`). All handler
functions are genuinely `void`-returning K&R functions registered
once at class-init time — textbook rule-3 case, no judgment call.

**Shape 2 — `bind_Description.proc` `(void (*)())` dispatch table
(17 sites).** The I2/I3-established idiom for `bind.ch`'s deliberately
untyped `void (*proc)()` keybinding slot — `cpptextv.c` (1: `slash`),
`ctextv.c` (1: `startPreproc`), `m3textv.c` (2: `asterisk`,
`m3pragma`), `modtextv.c` (1: `startPreproc`), `mtextv.c` (1:
`asterisk`), `srctextv.c` (8: `selfinsert`, `selfinsertreindent`,
`paren`, `startComment`, `endComment`, `startLineComment`,
`styleLabel`, `styleString`), `eqvcmds.c` (4: `eqview_Special` and 3×
`eqview_Close` for close-paren/bracket/brace). All handlers are
genuinely `void (struct X *, char)` — matches the field's own
deliberately generic type exactly.

**Shape 3 — `tree_Apply`'s `long (*proc)()` generic traversal
callback, full-prototype cast (1 site, `org.c`).**
`tree.ch:110`'s `Apply(struct tree_node *node, long (*proc)(), void
*anchor, void *datum)` is a deliberately empty-parens, genuinely
polymorphic slot — confirmed by reading its only other real-code call
site in the tree, `atk/apt/tree/treev.c:673`, which already casts a
different real callback there. `org.c`'s `Free_Elements` (`int
(struct org *, struct tree *, tree_type_node, int)`) is cast to the
**full stated prototype** `(long (*)(struct org *, struct tree *,
tree_type_node, int))Free_Elements` at its one call site
(`org.c:120`, `org__FinalizeObject`) — not a bare cast, matches rule 3
exactly.

No bare/argument-untyped casts landed anywhere in this batch; nothing
escalated under rule 4.

### 3c. Genuine bugs found — memory-corruption / datastream paths (4 sites)

1. **`atk/raster/lib/rasterio.c`, `rasterio__ReadImage`
   (`fscanf(file, " %u %ld %ld %ld %ld %ld %ld", &options, ...)`)** —
   `options` is declared `long options;` (`rasterio.c:348`) but read
   with `%u` (into a 4-byte `unsigned int*` write target on an 8-byte
   `long`). This is LP64 bug class #4 from the playbook: writes the
   low 32 bits and leaves the upper 32 bits of `options` as stack
   garbage. Genuine memory corruption on read of any `.raster`
   datastream (or embedded raster inset) with a version ≥2 header —
   not merely a display truncation. Fixed to `%ld`, matching the
   variable's real type and the other six fields already correctly
   read with `%ld` on the same line. The corresponding writer
   (`rasterio__WriteImage`) writes the paired field as a plain `0`
   int literal, which was itself flagged separately by `-Werror=format`
   (see below) — both sides of this datastream field are now
   consistent and LP64-correct.
2. **`atk/raster/lib/rasterio.c`, `rasterio__WriteImage`** — the
   writer's four `fprintf(file, "%ld %ld %ld %ld ", RASTERVERSION, 0,
   DEFAULTSCALE, DEFAULTSCALE)`/`(0, 0, width, height)`/`(id, width,
   height)` calls pass plain `int`-typed literals/macros (`0`,
   `RASTERVERSION`=2, `DEFAULTSCALE`=`1<<16`, `id`) against a `%ld`
   format that's already correct for the `long`-typed `width`/`height`
   siblings on the same lines. Fixed with `(long)` casts on the 7
   literal/int arguments rather than changing the format string, since
   the format string already correctly documents each field's on-disk
   type (matching the reader's `%ld` parses, including finding #1
   above).
3. **`atk/hyplink/link.c`, `link__Write`** — `fprintf(fp, "%d\n",
   link_GetPos(self))` and the paired `link_GetLen(self)` call, both
   `long` fields (`link.ch:50-51`), written into the link's own
   `\begindata{...}` datastream block with `%d`. The paired reader
   (`link.c:362,369`) already uses `atol()` (long-safe). Real
   truncation-on-write bug for any link position/length beyond 32-bit
   range (large documents). Fixed to `%ld`.
4. **`atk/raster/convert/convrast.c`, crop-option parser (`case 'c':`
   in `main`)** — `sscanf(arg+1, "(%d,%d,%d,%d)", &left, &top, &width,
   &height)` where all four locals are declared `long`
   (`convrast.c:322`). Same LP64 scanf-into-`long*` memory-corruption
   class as finding #1, this time on the standalone `convertraster`
   command-line tool's `-c(left,top,width,height)` crop option — any
   invocation of `convertraster -c...` writes garbage into the upper
   32 bits of all four crop rectangle fields before use. Fixed to
   `%ld` at all four positions.

None of these are new information to a user in the sense of visible
corruption under typical small values (raster/crop dimensions rarely
exceed 32-bit range in practice) — but #1 and #4 are genuine
undefined-behavior memory-corruption sites regardless of the values
actually seen, matching wdc's own framing for why the `-Wformat`
ruling exists.

### 3d. Other findings (not on a datastream/corruption path)

- **Duplicate stale K&R forward-declaration blocks** (runbook pattern
  b), found and removed in 4 files: `atk/image/tif.c` (4-line
  duplicate: `gtTileContig`/`gtTileSeparate`/`gtStripContig`/
  `gtStripSeparate`, already correctly typed earlier in the same
  file), `atk/org/org.c` (3-line duplicate: `Read_Body`/`Write_Body`/
  `Strip`), `atk/org/orgv.c` (5-line duplicate:
  `Alter_Control_Button`/`Passivate`/`Activate`/`FullUpdate_Tree`/
  `Prepare_Description`), `atk/bush/bushv.c` (2-line duplicate:
  `DoPrint`/`DoExecute`). In every case the earlier, correctly-typed
  declaration was kept and the stale duplicate deleted, rather than
  independently re-typing the duplicate — matching the runbook's
  named pattern exactly.
- **`atk/raster/scan/ezscan.c` — not the masking-bug pattern
  (checked explicitly).** The entire real Ricoh IS410/3117-series
  scanner driver (lines 62-756) is guarded by `#ifdef
  SUPPORTTED_PLATFORM` [sic], which Darwin does not define; only the
  small "not supported on this machine" stub `main()` at the bottom
  of the file actually compiles. This is the **opposite** of the
  known dead-branch-masking pattern (a K&R branch made "the one
  always compiled") — here the *intentionally inert* stub is what's
  live, by 1990s original design (this drove physical Sun/HP-attached
  page scanners, never going to run on Darwin arm64 regardless). Fix
  was mechanical: add `int` to the stub `main()` and `#include
  <stdlib.h>` for `exit()`. The real scanner code (with its own
  malloc call, per §3e) was never compiled or type-checked by this
  build and remains untouched — flagged here only so a future reader
  doesn't mistake the small diff for an incomplete fix.
- **`atk/image/fbm.c` — a real, if minor, ~30-year-old logic bug.**
  `fbmin_img_aspect` was declared `int` and populated via `atoi()`
  from `phdr.aspect`, a text field (`fbm.h:67`, `char aspect[12]`)
  documented as "ratio of Y to X of one pixel" — i.e., meant to hold
  fractional values. It was then validated against `< 0.01` and `>
  100.0`, a comparison that can never fire as intended for an `int`
  (any nonzero fractional-but-sub-1 aspect ratio truncates to `0` via
  `atoi` before the check ever sees it). The `-Werror=format` error
  was on the `%lg`-formatted diagnostic print of this same variable.
  Fixed end-to-end (the only place this field is used, single file,
  zero other call sites): retyped to `double`, `atoi`→`atof`.
- **`atk/image/imagev.c` — missing declaration for a real
  cross-file, non-static function.** `writePS` is defined in the same
  directory's `ps.c` (`void writePS(struct imagev *self, FILE *fp, int
  *wpts, int *hpts, int toplevel)`) but has no declaration reachable
  from `imagev.c`, which calls it twice. Added a matching `extern`
  prototype to `imagev.c`'s existing static-forward-declaration block.
- **`atk/image/xwd.c` — same shape, `flipBits`.** Defined in
  `atk/basics/common/imageaux.c` (`void flipBits(unsigned char *p,
  unsigned int len)`) with no header of its own; added a local
  `extern` prototype matching the real definition.
- **`AndrewDir()` missing-declaration pattern (3 files) —** the
  tree's well-known LP64 pointer-truncation class (memory:
  "LP64 Missing Prototypes"), fixed via the same tree-wide
  `extern char *AndrewDir();` K&R idiom used at ~18 other sites
  already in the tree (`atk/image/ppm2atkimage.c`,
  `atk/image/xwd2atkimage.c`, `atk/raster/convert/convrast.c`).
- **`atk/bush/bush.c`'s `NodeFilter` — real external libc contract,
  retyped end-to-end (rule 2).** `scandir()`'s filter parameter is
  `int (*)(const struct dirent *)` on Darwin/BSD; `NodeFilter` was
  declared `DIRENT_TYPE *dir` (non-const). Retyped to `const
  DIRENT_TYPE *dir` (read-only use, confirmed) rather than casting,
  matching the real fixed libc interface — same reasoning as I3's
  qsort-comparator fix.
- **`atk/bush/bushv.c`'s sort-handler family — real established
  in-tree contract, retyped end-to-end (rule 2), no casts.**
  `suite.ch:278` declares `sort_handler` as `long (*)()`
  (confirmed genuinely used this way at its only other real call
  site, `atk/apt/suite/suite.c:1818`'s `SortHandler`). `bushv.c`'s
  5 comparator functions (`SortByName`/`SortBySize`/`SortBySuffix`/
  `SortByType`/`SortByDate`, all `int`-returning, none used outside
  this file), the `DetermineSortHandler` helper that selects among
  them, and its 3 local `sorter` variables were all retyped
  `int`→`long` end-to-end. Every comparator's real return values are
  small literals (`-1`, `0`, `1`), so this is a pure, zero-risk type
  widening — no cast needed anywhere, matching rule 2's stated
  preference over rule 3's cast fallback.
- **`atk/bush/bushv.c`'s `ResetChildDirPaths` — same `tree_Apply`
  slot as org.c's Shape 3, but retyped instead of cast.** Single
  private call site (unlike `org.c`'s `Free_Elements`, which had no
  other reasonable option); retyped `int`→`long` end-to-end rather
  than adding a cast, since nothing else depends on its signature.
- **`atk/layout/layout.c:245` — genuine stale debug-trace bug,
  `-Wformat-insufficient-args`.** `"Got <%ld,%ld,%ld,%ld,%c%c>
  havechild=%d\n"` has one more `%c` conversion than there are
  arguments (6 args for 7 conversions: 4×`%ld` + 2×`%c` + 1×`%d`, but
  only one char-typed argument — `(cVaries(c) ? 'V' : 'F')` — is ever
  passed). Fixed by removing the extra, unmatched `%c`.
- **`atk/layout/layout.c:422` — genuine copy-paste debug-trace bug,
  found incidentally while fixing the required `%d`→`%ld` conversions
  on the same line.** `layout__SetComponentSize`'s trace printed
  `classname(cData(c)), x, y, w, y` — the fourth positional value
  (documented by the format string as the 4th of `x, y, w, h`) was
  `y` again instead of the function's own `h` parameter. Fixed
  alongside the required format-width fix; debug-output-only, no
  correctness impact on the actual size-setting logic below it (which
  correctly uses `h`).

### 3e. Malloc/free/realloc/calloc sweep (`rollout-procedure.md`'s mandatory check)

Ran the word-boundary-anchored grep individually (never chained)
against every `.c` file in all 12 directories. No missing
`<stdlib.h>`/`<andrewos.h>` reachability and no wrapper-name
collisions found anywhere — every directory's gate was already clean
at the point the sweep ran, confirming this directly rather than by
inference. Notable non-issues checked explicitly:
- `atk/raster/scan/ezscan.c:194`'s one `malloc` call sits inside the
  `#ifdef SUPPORTTED_PLATFORM` dead branch discussed in §3d — never
  compiled on this platform, so it was never at risk regardless of
  the sweep's outcome.
- `atk/rofftext/num.c` and `atk/eq/eqparse.c` (bison-generated, not
  hand-written) both declare their own `malloc`/`free` prototypes
  internally (`YYSIZE_T`-typed, standard bison boilerplate) — no
  action needed, not part of the hand-maintained source surface.
- `atk/raster/lib/xwdio.c` (not itself touched by any compiler error
  this batch) has 8 calloc/free call sites, all already correctly
  declared.

## 4. Files touched (all compile clean under the gate — §3)

**Imakefiles (12, flag-override only, plus 2 flag-override replacements noted):**
`src/atk/image/Imakefile`, `src/atk/fad/Imakefile`,
`src/atk/srctext/Imakefile`, `src/atk/org/Imakefile`,
`src/atk/bush/Imakefile`, `src/atk/rofftext/Imakefile` (replaced
M2-era override), `src/atk/raster/lib/Imakefile`,
`src/atk/hyplink/Imakefile`, `src/atk/eq/Imakefile` (replaced M2-era
override), `src/atk/layout/Imakefile`, `src/atk/raster/scan/Imakefile`,
`src/atk/raster/convert/Imakefile`.

**`src/atk/image`** (8 source files): `imagev.c` (1
implicit-function-declaration fix — `writePS` forward decl),
`sunraster.c` (1 format fix, debug-print), `xwd.c` (1 format fix + 1
implicit-function-declaration fix — `flipBits` forward decl), `fbm.c`
(1 implicit-int fix + real `int`→`double` retype, §3d), `cmuwm.c` (2
format fixes, debug-print), `rle.c` (6 implicit-function-declaration
fixes via forward decls + 1 implicit-int fix on `make_magic`),
`tif.c` (11 implicit-int fixes + 3 implicit-function-declaration
fixes + 1 duplicate-block removal, §3d), `ppm2atkimage.c` (1
`AndrewDir` fix), `xwd2atkimage.c` (1 `AndrewDir` fix + 1 format fix).

**`src/atk/fad`** (2 source files): `fad.c` (1 `#include
<andrewos.h>` + 1 implicit-int fix), `fadv.c` (1 forward-decl addition
for `doan` + 21 implicit-int fixes on already-forward-declared static
helpers).

**`src/atk/srctext`** (9 source files): `asmtext.c` (1 `#include
<andrewos.h>`), `compressv.c` (1 implicit-int fix), `cpptextv.c` (1
`(void (*)())` cast), `ctextv.c` (1 cast), `m3textv.c` (2 casts),
`modtextv.c` (1 cast), `mtextv.c` (1 cast), `srctext.c` (2 stray `\0`
removals from format strings + 1 format fix, all cosmetic messages),
`srctextv.c` (8 `(void (*)())` casts + 2 `(procedure)` casts + 1
implicit-int fix + 3 stray `\0` removals).

**`src/atk/org`** (3 source files + 1 `.ch`): `org.c` (1
duplicate-block removal + 4 implicit-int fixes + 1 full-prototype
`tree_Apply` cast, §3b Shape 3 + 2 format fixes + 5 field-width
`(int)` casts, all on the outline-node datastream writer, §3c-adjacent
but not itself a bug — see below), `orgv.c` (1 duplicate-block removal
+ 5 implicit-int fixes + 1 format fix), `orgv.ch` (1 field retype,
`hit_handler` `struct view *(*)()`→`procedure`, §3d).

  (Note on `org.c`'s `Write_Body` field-width fixes: `2 * level` and
  `2 * current_level`, both `long`, used as printf field-width
  arguments — these must be `int` per the C standard regardless of
  the underlying value's real range, so `(int)` casts are the correct
  fix, not a symptom of a wider type bug; the *value* being printed at
  those same call sites — indentation depth, always small — was
  already fine. The adjacent `realSize` fix at the same call site
  (`%d`→`%ld` for the embedded description's byte length) is on this
  file's `.org` outline datastream, and is a genuine asymmetry: the
  paired reader parses via `atoi()`, itself `int`-range-limited, so
  both sides already implicitly agreed to an `int`-range contract
  before this fix and still do after it — flagged for awareness per
  the I3 `figattr.c` precedent, not treated as a live corruption bug.)

**`src/atk/bush`** (2 source files): `bush.c` (1 format
insufficient-args fix + 1 `NodeFilter` retype to `const DIRENT_TYPE
*`, §3d), `bushv.c` (1 duplicate-block removal + 2 implicit-int fixes
+ 9 `(procedure)` casts + 2 format fixes + 5 comparator/sort-handler
retypes int→long, §3d + 1 `ResetChildDirPaths` retype int→long, §3d).

**`src/atk/rofftext`** (3 source files): `rofftext.c` (14
implicit-int fixes on already-forward-declared static helpers),
`roffcmds.c` (1 format fix, a shell-command-string builder — see note
below), `num.gra` (1 implicit-int fix on `EvalString`, the bison
grammar source — regenerates `num.c` at build time).

  (Note on `roffcmds.c:819`: builds a `$tail ... +N|head -M|...`
  shell command string executed via `link_SetLink`/a later shell
  invocation, not a re-parsed datastream — flagged as worth
  distinguishing from the datastream-bug class in §3c even though the
  fix itself is the same mechanical `%d`→`%ld`.)

**`src/atk/raster/lib`** (5 source files): `heximage.c` (2
implicit-int fixes + 5 `-Wformat-security` fixes, non-literal
`fprintf` format arguments iterating a static PostScript-template
array — fixed by inserting `"%s"` at the 5 zero-extra-argument call
sites, leaving the 4 sites that already pass real format arguments
untouched), `paint.c` (2 implicit-int fixes), `plusspc.c` (2
implicit-int fixes), `rasterio.c` (1 implicit-int fix + 1 `#include
<andrewos.h>` + the 2 genuine datastream bugs in §3c, findings #1-2),
`xbm.c` (1 implicit-int fix + 2 format fixes on a generated `.xbm`
C-source-format writer, digit-text-identical for typical
image-dimension values so not a behavior change).

**`src/atk/hyplink`** (4 source files): `pshbttn.c` (1 format fix,
cosmetic font-name string), `pshbttnv.c` (4 `(procedure)` casts + 1
format fix, cosmetic PostScript font-scaling command), `link.c` (2
format fixes — genuine datastream bug, §3c finding #3), `linkv.c` (4
`(procedure)` casts).

**`src/atk/eq`** (3 source files): `eqparse.gra` (2 implicit-int
fixes — `eq__Parse` retyped to match its real `.ch`/`.eh`-declared
`void` return, `print` given an `int` return type — bison grammar
source, regenerates `eqparse.c`), `draweqv.c` (1 implicit-int fix),
`eqvcmds.c` (4 `(void (*)())` casts + 1 `(procedure)` cast).

**`src/atk/layout`** (6 source files): `layoutv.c` (2 format fixes on
`classID`/pointer debug-prints + 1 `#include <andrewos.h>` + 12
further format fixes, mostly `%d`→`%ld` on `long`-typed component
coordinates in debug traces), `layout.c` (1 format-insufficient-args
fix, §3d + 4 format fixes, one bundled with the copy-paste fix, §3d),
`fillerv.c` (1 `#include <andrewos.h>` + 1 `(procedure)` cast),
`boxview.c` (6 format fixes, same coordinate/pointer debug-trace shape
as `layoutv.c`), `box.c` (3 format fixes).

**`src/atk/raster/scan`** (1 source file): `ezscan.c` (1 implicit-int
fix + 1 `#include <stdlib.h>`, on the "unsupported platform" stub
only — see §3d).

**`src/atk/raster/convert`** (1 source file): `convrast.c` (1 format
fix — genuine memory-corruption bug on the crop-option parser, §3c
finding #4 + 1 implicit-int fix + 1 `AndrewDir` fix + 2 further format
fixes).

## 5. Linkage per directory (per task instructions — no `nm -g` cross-checks attempted)

| Directory | Linkage (read from Imakefile) |
|---|---|
| `atk/image` | Dynamic (`DynamicMultiObject(imagev.do,...)` + 6× `DynamicObject`: `tif`, `pbm`, `sunraster`, `rle`, `xwd`, `colorv`); also builds 2 standalone helper programs (`ppm2atkimage`, `xwd2atkimage`) via `ClassProgramTarget`, installed to `${BASEDIR}/bin` |
| `atk/fad` | Dynamic (`DynamicObject(fadv,,)`; `fad.do` installed via `InstallClassFiles`/`NormalATKRule`, no separate `DynamicObject` line needed for it) |
| `atk/srctext` | Dynamic (`DynamicObject(srctextv,...)` + `DynamicMultiObject(hlptext.do,...)` + `DynamicMultiObject(srctext.do,...)`) |
| `atk/org` | Dynamic (`org.do`/`orgv.do` via `DOBJS`/`InstallClassFiles`/`NormalATKRule`, no explicit `DynamicObject` macro but same `.do`-loaded mechanism) |
| `atk/bush` | Dynamic (`DynamicObject(bush,...)` + `DynamicObject(bushv,...)`) |
| `atk/rofftext` | Dynamic (`DynamicMultiObject(rofftext.do, rofftext.o roffcmds.o roffutil.o roffstyl.o num.o roffchrs.o, ...)`) |
| `atk/raster/lib` | **Mixed** — builds a real static archive (`LibraryTarget(libraster.a, $(OBJS))`, statically linked into consumers such as `atk/raster/cmd` (I1) and this same batch's `atk/raster/scan`/`atk/raster/convert`) **and** 2 of its own objects are also separately `.do`-dynamic-loaded (`DynamicObject(heximage,...)`, `DynamicObject(oldrf,...)`); the other 5 objects (`paint`, `plusspc`, `rasterio`, `xwdio`, `xbm`) are `.do`-installed via `DOBJS`/`InstallClassFiles` only, not individually named in a `DynamicObject` line |
| `atk/hyplink` | Dynamic (`DynamicMultiObject(pshbttn.do, pshbttn.o,...)`; `pshbttnv.do`/`link.do`/`linkv.do` via `DOBJS`/`InstallClassFiles`) |
| `atk/eq` | Dynamic (`DynamicMultiObject(eq.do, eq.o symbols.o eqparse.o,...)` + `DynamicMultiObject(eqv.do, eqv.o draweqv.o eqvcmds.o,,)`) |
| `atk/layout` | Dynamic (6× `DynamicObject`: `layoutv`, `layout`, `fillerv`, `filler`, `boxview`, `box`) |
| `atk/raster/scan` | **Standalone program**, not `runapp`-linked and not `.do` — `ProgramTarget(ezscan, ...)`, installed to `${DESTDIR}/bin/ezscan` |
| `atk/raster/convert` | **Standalone program**, not `runapp`-linked and not `.do` — `ClassProgramTarget(convertraster, convrast.o, $(LIBS), $(BSDLIB))` (statically links `libbasics.a`/`liberrors.a`/`UTILLIB`), installed to `${DESTDIR}/bin/convertraster` |

No `nm -g` cross-checks were attempted (known-unreliable per the
task's constraint); no `runapp` relink was attempted.

## 6. Open questions / anything that surprised you

- `atk/layout`'s census-vs-real gap (7→64, +814%, the largest of any
  I-wave batch by a wide margin) is the most surprising single number
  in this batch — worth flagging explicitly since it's an order of
  magnitude beyond even `atk/figure`'s previously-largest +298% gap.
  Root cause is fully understood and mechanical (§3a), not a sign of
  hidden complexity, but the orchestrator may want to sanity-check the
  `m4-batches.md` census methodology if a similarly print-trace-heavy
  directory appears in a future batch.
- `atk/raster/lib`'s mixed linkage (§5) — both a real static library
  *and* two of its own objects independently `.do`-loaded — is
  unusual enough among this wave's directories that it's worth the
  orchestrator double-checking the Imakefile reading is correct before
  relying on it for any runtime-check planning.
- The `org.c`/`bushv.c` `tree_Apply` slot (`long (*)()`) appeared
  twice in this single batch (once resolved by a full-prototype cast,
  once by a zero-cast retype) — worth noting for pattern-recognition
  in any future batch touching `atk/apt/tree` consumers, alongside the
  now-3-instance-total `bind_Description.proc`/`procedure` idioms.
- No dead-branch-masking bug (runbook pattern a) was found this batch
  — `atk/raster/scan/ezscan.c`'s dead branch is the opposite shape
  (intentional stub live, real code inert) and was checked explicitly
  to confirm it isn't the masking pattern in disguise.

## 7. Suggested runtime checks for wdc

Per `rollout-procedure.md`'s Runtime check rules — GUI apps should be
launched by wdc, not this session; commands below are for wdc to run
directly, from native Terminal.app, not an IDE terminal.

- **`atk/hyplink` (real bug, §3c #3):** create or open a document with
  a link-button inset (`\033<TAB>link` or via the Link menu), set a
  target, save, quit, and reopen — confirms `link_GetPos`/`GetLen`
  round-trip correctly through the fixed `%ld` write. Low risk of ever
  having shown symptoms before this fix (link positions rarely exceed
  32-bit range), but worth a basic save/reload pass given it's a real
  datastream fix.
- **`atk/raster/lib`/`atk/raster/cmd` (real bug, §3c #1-2):** insert
  a raster inset in `ez`, draw something, save, reload — exercises
  `rasterio__ReadImage`/`WriteImage`'s fixed `options` field
  round-trip. `DISPLAY=:0; runapp ez` then Insert menu → Raster (or
  similar), or open an existing `.ez` file with a raster inset already
  embedded.
- **`atk/raster/convert` (real bug, §3c #4):** from Terminal.app,
  `build/bin/convertraster -c(10,10,100,100) <some-image> /tmp/out` —
  confirms the crop rectangle survives the fixed `%ld` parse (compare
  against the same command pre-fix if a pre-fix binary is still
  available, or just confirm the output image is actually cropped to
  the requested rectangle rather than a garbage-offset one).
- **`atk/org`:** insert an org (outline) inset in `ez`, add a node
  with a text description longer than a few dozen characters
  (exercises the `[<size>...]` embedded-description write/read path),
  save, reload.
- **`atk/bush`/`atk/layout`/`atk/eq`/`atk/srctext`/`atk/image`/
  `atk/fad`/`atk/rofftext`:** no genuine behavioral bugs were found in
  these 7 directories (all fixes were format-width/declaration/cast
  mechanics with no data-loss or corruption implication) — a basic
  smoke pass (open the relevant inset/app, confirm it displays and
  responds to input) is sufficient if wdc wants one, but not required
  by anything this session found.
