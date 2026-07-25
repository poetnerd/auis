# M2 rollout point 4e (bucket 4, session 5): `atk/table`

## 1. Status

Stopped at the (only) gate — subtree-local `make clean && make depend
&& make -k install`, zero real errors, confirmed twice more for
determinism, per the runbook's "Gate scope" ruling (tree-wide gate NOT
required for this directory). No commits made. Working tree: 10 files
edited (`Imakefile` + 9 `.c` files — `print.c` needed no fix, see
§3). Session diff at tree root: `m2-table-session.diff` (372 lines).
Full gate log at the canonical path
`~/src/AUIS/andrew-6.4/dependInstall.log` (overwritten across this
session's several build passes; the two determinism passes are
captured separately at `/tmp/table-det1-install.log`/
`/tmp/table-det2-install.log`, both 0 `error:` lines).

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (including the
   "Command style" section's `cd`-persistence correction and the
   "Logging" section's now-unconditional malloc/realloc/free/calloc
   sweep mandate), `m2-rollout-runbook.md`, `m2-table-prompt.md`, and
   all eight prior rollout reports (`m2-pilot-eq-REPORT.md`,
   `m2-batch2-REPORT.md`, `m2-batch3a-REPORT.md`,
   `m2-batch3b-REPORT.md`, `m2-utillib-REPORT.md`,
   `m2-metamail-REPORT.md`, `m2-text-REPORT.md`,
   `m2-rofftext-REPORT.md`) in full before starting.
2. **Checked command-style/cwd-persistence early**: a throwaway `cd
   .../src/atk/table` call followed by a bare `pwd` in the next tool
   call showed the cwd had reverted to the tree root — cwd does **not**
   persist across Bash calls in this session (same as the `atk/text`/
   `atk/rofftext` sessions). Used `make -C <absolute-path> <target>`
   for every build step for the rest of the session, one command per
   call, never chained with `&&`/`;`.
3. Read `src/atk/table/Imakefile` directly and confirmed no
   `Parser()`/`LexFile` — no generated-source gap (as already
   established by the prompt). Confirmed the build shape: two
   `DynamicMultiObject` targets (`spread.do` from `spread.o print.o
   menu.o update.o hit.o keyboard.o`; `table.do` from `table.o eval.o
   tabio.o funs.o`), no `LibraryTarget`, an `InstallLink(runapp,
   .../bin/table)`.
4. Flagged the Imakefile with the runbook's exact four-flag
   `COMPILERFLAGS` override, verified via `make -C ... Makefile` +
   `grep -n COMPILERFLAGS Makefile` (override at line 294 wins over
   `system.mcr`'s default at line 177 — re-verified after both later
   `depend`-triggered Makefile regenerations too).
5. Fix-surfacing pass: `make -C ... clean`, `make -C ... depend`,
   `make -C ... -k install CDEBUGFLAGS="-ferror-limit=0 -g -O0"`
   (raised limit used from the start, given this is the largest
   bucket-4 directory by stale estimate so far besides metamail) —
   each a separate Bash call, logged to the canonical path. Result:
   **154 real errors**, already past the 113 stale estimate before
   any malloc-family sweep — see §3.
6. Investigated every project-local function's real definition via
   `Read`/`Bash grep` before writing any declaration — see §4 for the
   full per-function table.
7. Fixed all 154 census-visible instances across 9 files (`spread.c`,
   `menu.c`, `update.c`, `hit.c`, `keyboard.c`, `table.c`, `tabio.c`,
   `eval.c`, `funs.c` — `print.c` needed no fix, see §3). Rebuilt —
   exit 0, 0 errors, first attempt (no correction round needed).
8. **Unconditionally swept all 10 `.c` files** for bare
   `malloc(`/`free(`/`realloc(`/`calloc(` calls, per the prompt's
   mandate, including both the no-space and space-before-paren call
   forms (`malloc(` and `malloc (` both appear in this codebase — see
   §5 for a methodology finding this produced). Found the blind spot
   in 6 of 10 files, 32 call sites — see §5.
9. Fixed all 32 additional instances (`<stdlib.h>` added to 6 files:
   `eval.c`, `hit.c`, `keyboard.c`, `spread.c`, `tabio.c`, `table.c`).
   Rebuilt — 0 errors, exit 0.
10. Ran the subtree-local gate twice more for determinism (`make -C
    ... clean`, `make -C ... depend`, `make -C ... -k install`, plain
    recipe, each its own call) — both passes exit 0, 0 errors, no
    redeclaration-conflict warnings from the new `<stdlib.h>` includes.
11. Confirmed the `.do`-dynamic linkage shape via `nm -g` (one target,
    unpiped) plus the class-load index and `.ezinit` menu wiring — see
    §6.
12. `fossil status` (§8) confirms exactly the 10 files touched. No
    commit made. `fossil diff > m2-table-session.diff` written to the
    tree root.

## 3. Real instance count: 154 census-visible + 32 census-invisible = 186 total fallout instances (vs. the stale table's 113)

65% over the 113 stale estimate from the census-visible count alone,
growing to 65% over even before the mandatory malloc sweep — in line
with every prior bucket-4 directory's pattern of the stale table
being a floor, not a target. `print.c` (which defines `WriteTroff`,
called cross-file from `spread.c`/`menu.c`) needed **no edit at all**:
it contains no implicit-function-declaration error and no
malloc-family call of its own — the only bucket-4 file so far in
either of these directories' 10-`.c`-file rosters that needed zero
fallout fixes.

Per-file breakdown of the 154 census-visible errors:

| File | Census-visible errors |
|---|---|
| `menu.c` | 36 |
| `keyboard.c` | 27 |
| `table.c` | 15 |
| `hit.c` | 14 |
| `eval.c` | 12 |
| `spread.c` | 8 |
| `update.c` | 6 |
| `funs.c` | 35 |
| `tabio.c` | 1 |
| `print.c` | 0 |
| **Total** | **154** |

(`funs.c`'s 35 sits between `table.c`'s 15 and `keyboard.c`'s 27 in
the table above only because of formatting — no significance to the
ordering.)

## 4. Per-instance taxonomy: the 154 census-visible errors

Grouped by function (154 individual call sites across 9 files; full
per-line detail available in `m2-table-session.diff`'s hunks). Every
project-local function was investigated via its real definition
before any declaration was written — no signature was guessed.

| Function | Real definition | Instances | Taxonomy category |
|---|---|---|---|
| `k_TellUser` | `keyboard.c:71` (void) | 14 (all `menu.c`) | Cross-file, no header anywhere |
| `SetCurrentCell` | `hit.c:486` | 20 (2 same-file `hit.c` + 6 `menu.c` + 12 `keyboard.c`) | Same-file forward ref + cross-file, no header |
| `CopyChunk` | `hit.c:246` | 15 (1 same-file `hit.c` + 2 `menu.c` + 12 `keyboard.c`) | Same-file forward ref + cross-file, no header |
| `MakeBogus` | `table.c:1169` | 23 (6 same-file `table.c` + 6 `eval.c` + 11 `funs.c`) | Same-file forward ref + cross-file, no header |
| `MakeStandard` | `table.c:1159` | 15 (1 same-file `table.c` + 3 `eval.c` + 11 `funs.c`) | Same-file forward ref + cross-file, no header |
| `k_AskUser` | `keyboard.c:81` | 7 (all `menu.c`) | Cross-file, no header |
| `spread_InvertRectangle` | `update.c:281` | 7 (all `hit.c`) | Cross-file, no header |
| `rcref` | `table.c:908` | 5 (1 `eval.c` + 4 `funs.c`) | Cross-file, no header |
| `rangeLimit` | `table.c:877` | 5 (all `funs.c`) | Cross-file, no header |
| `DestroyCell` | `table.c:701` | 4 (3 same-file `table.c` + 1 `tabio.c`) | Same-file forward ref + cross-file |
| `eval` | `eval.c:142` (void) | 3 (1 `table.c` + 2 `funs.c`) | Cross-file, no header |
| `GetFormula` | `hit.c:385` | 2 (all `keyboard.c`) | Cross-file, no header |
| `WriteASCII` | `tabio.c:280` (void) | 2 (all `table.c`) | Cross-file, no header |
| `WriteTroff` | `print.c:365` | 2 (1 `spread.c` + 1 `menu.c`) | Cross-file, no header |
| `spread_PartialUpdate` | `update.c:238` | 2 (1 cross-file `spread.c` + 1 same-file `update.c`) | Same-file forward ref + cross-file |
| `spread_WantHighlight` | `update.c:115` (int) | 2 (both `spread.c`) | Cross-file, no header |
| `strcmp` | `<string.h>` | 2 (1 `menu.c` + 1 `update.c`) | Missing standard-library header |
| `strcpy` | `<string.h>` | 2 (1 `menu.c` + 1 `update.c`) | Missing standard-library header |
| `CompareChunk` | `hit.c:256` | 1 | Same-file forward ref |
| `extendCurrentCell` | `hit.c:533` | 1 | Same-file forward ref |
| `k_SetMessageState` | `keyboard.c:55` (void) | 1 (`hit.c`) | Cross-file, no header |
| `spread_ClearSelectionBox` | `update.c:345` | 1 (`hit.c`) | Cross-file, no header |
| `TellFormula` | `hit.c:455` | 1 (`keyboard.c`) | Cross-file, no header |
| `k_DefineKeys` | `keyboard.c:516` (void) | 1 (`spread.c`) | Cross-file, no header |
| `DefineMenus` | `menu.c:669` | 1 (`spread.c`) | Cross-file, no header |
| `ResetCurrentCell` | `hit.c:472` | 1 (`spread.c`) | Cross-file, no header |
| `spread_update_FullUpdate` | `update.c:207` | 1 (`spread.c`) | Cross-file, no header |
| `k_WantToDiscard` | `keyboard.c:115` | 1 (`menu.c`) | Cross-file, no header |
| `AddRows` | `keyboard.c:135` | 1 (`menu.c`) | Cross-file, no header |
| `AddCols` | `keyboard.c:161` | 1 (`menu.c`) | Cross-file, no header |
| `spread__WantLimitedHighlighting` | `spread.c:79` (boolean) | 1 (`update.c`) | **New sub-case — see §7** |
| `InitializeGraphic` | `spread.c:124` (void) | 1 (`update.c`) | Cross-file, no header |
| `ComputeRowSizes` | `spread.c:191` | 1 (`update.c`) | Cross-file, no header |
| `WriteCell` | `tabio.c:182` (void) | 1 (`table.c`) | Cross-file, no header |
| `ReadCell` | `tabio.c:592` (void) | 1 (`table.c`) | Cross-file, no header |
| `enterfuns` | `funs.c:630` | 1 (`eval.c`) | Cross-file, no header |
| `idate` | `funs.c:426` (int) | 1 (`eval.c`) | Cross-file, no header |
| `syntaxError` | `eval.c:116` | 1 (`funs.c`) | Cross-file, no header |
| `enterfun` | `eval.c:427` (void) | 1 (`funs.c`) | Cross-file, no header |
| `strncpy` | `<string.h>` | 1 (`menu.c`) | Missing standard-library header |

154/154 accounted for. This directory has **no `.h` file of its own**
(confirmed: `ls *.h` in the directory returns nothing except the
generated `.ch`/`.eh`/`.ih` triad) — every project-local fix is
necessarily a local `extern`, same shape as `overhead/mail/metamail/
metamail`'s and `atk/rofftext`'s directories. All the K&R-untyped
functions above (no explicit return type at their real definition)
were declared with a bare `extern int Name();` (or `extern void
Name();`/`extern boolean Name();` where the definition has an
explicit non-`int` return type, noted in the table above), matching
the directory's own pre-existing precedent: several files already had
`extern struct view *spread_FindSubview();` for a function defined
elsewhere with no header — I placed the new declarations in the same
style, right after each file's `#include <spread.eh>`/`<table.eh>`
line (or, for genuine same-file forward references sharing linkage
with a cross-file caller, as a plain non-`static` prototype alongside
the file's own existing `static ... Foo();` forward-declare block).

**"Possible genuine bug/typo" category: still empty.** Every
unfamiliar-looking name resolved to a real, correctly-spelled
definition somewhere in this directory. No candidate for the hard-stop
category turned up.

## 5. Malloc-family blind-spot sweep: 32 instances across 6 of 10 files, none census-visible

Per the prompt's unconditional mandate, swept all 10 `.c` files
individually (`grep -nE "\bmalloc *\(|\bfree *\(|\brealloc
*\(|\bcalloc *\("`, one call per file):

| File | Call sites | Census-visible errors in this file? |
|---|---|---|
| `table.c` | 13 | Yes (15) |
| `tabio.c` | 9 | Yes (1) |
| `spread.c` | 5 | Yes (8) |
| `keyboard.c` | 3 | Yes (27) |
| `hit.c` | 1 | Yes (14) |
| `eval.c` | 1 | Yes (12) |
| `funs.c` | 0 | Yes (35), but no malloc-family calls |
| `menu.c` | 0 | Yes (36), but no malloc-family calls |
| `print.c` | 0 | No — zero fallout of any kind |
| `update.c` | 0 | Yes (6), but no malloc-family calls |
| **Total** | **32** | |

Every one of the 32 sites had zero declaration of any kind reaching it
before this session — no stale wrong-typed `extern char *malloc();`
to clean up anywhere except one already-dead one (`eval.c:61`'s
`extern char * malloc();`, guarded by `#if !POSIX_ENV`; confirmed
`POSIX_ENV` is `#define`d `1` in this build's `system.h`, so that
branch has never compiled here — left in place, harmless, matching
the established "don't touch dead-guarded code outside this session's
own fallout" precedent). Fix: `#include <stdlib.h>` added to
`eval.c`, `hit.c`, `keyboard.c`, `spread.c`, `tabio.c`, `table.c`. No
redeclaration-conflict warnings appeared after adding it (confirmed by
grepping the final clean build log for `conflicting`/`incompatible`).

**New methodology finding, worth folding into `rollout-procedure.md`'s
malloc-sweep guidance**: this directory's `table.c` defines its own
custom reallocation wrapper, `myrealloc()`, called dozens of times
throughout the file. A naive `grep -n "realloc("` sweep produces a
**false positive** on every one of those calls, because `"realloc("`
is a literal substring of `"myrealloc("` — `grep` matches it whether
or not a word boundary is respected. The corrected sweep pattern used
here, `grep -nE "\bmalloc *\(|\bfree *\(|\brealloc *\(|\bcalloc
*\("` (word-boundary anchored, and tolerant of the space-before-paren
call style also present in this codebase, e.g. `malloc (n)` vs.
`malloc(n)` — both forms coexist even within the same file), correctly
excludes `myrealloc(` while still catching every real `malloc(`/
`free(`/`realloc(`/`calloc(` call, including the space-variant ones
that a plain `malloc(` pattern (no space tolerance) would also have
missed. I caught this only by manually reading `table.c` and noticing
`myrealloc`'s own body (which itself calls real `malloc`/`free`/
`realloc`) wasn't appearing in an early un-anchored sweep's output the
way I expected — **recommend the runbook's sweep instructions
explicitly call out both hazards** (word-boundary matching against a
project's own `*alloc`-suffixed wrapper names, and the space-before-
paren call-style variant) rather than relying on a plain substring
`grep`, since a directory with its own `xrealloc`/`myrealloc`/
`saferealloc`-style wrapper is a plausible recurring shape, not unique
to this one.

## 6. `.do`-dynamic linkage confirmed via `nm -g`, and real runtime consumers

Confirmed, not assumed, per the prompt's explicit ask:

```
$ nm -g build/bin/runapp
```
(single unpiped call — output persisted to a tool-result file, then
grepped in a separate call, per the established pattern from prior
sessions' "avoid piping/chaining `nm`" finding) shows **zero** of this
directory's distinctive function names (`spread_`/`table_`-family
project-locals: `k_AskUser`, `k_TellUser`, `k_SetMessageState`,
`MakeBogus`, `MakeStandard`, `CopyChunk`, `CompareChunk`, `WriteTroff`,
`ResetCurrentCell`, `DestroyCell`, `SetCurrentCell`, `rangeLimit`,
`rcref`, `enterfun`) anywhere in `runapp`'s symbol table — confirming
the prompt's structural note: `atk/table` is fully dynamic, not
statically linked, unlike `atk/text` (linked into `runapp` directly).

```
$ nm -g build/dlib/atk/table.do
$ nm -g build/dlib/atk/spread.do
```
Both `.do`s are self-contained: every fixed function (`DestroyCell`,
`WriteASCII`, `WriteCell`, `ReadCell`, `MakeBogus`, `MakeStandard`,
`rcref`, `rangeLimit`, `eval`, `enterfun`, `enterfuns`, `idate`,
`syntaxError` in `table.do`; `AddCols`, `AddRows`, `CompareChunk`,
`ComputeRowSizes`, `CopyChunk`, `DefineMenus`, `InitializeGraphic`,
`MouseHit`, `ResetCurrentCell`, `SetCurrentCell`, `TellFormula`,
`WriteTroff`, `GetFormula` in `spread.do`) appears as a defined (`T`)
symbol directly in its own `.do`, with `malloc`/`free`/`realloc`/
`strcmp`/etc. correctly showing as undefined (`U`) externs resolved
against `libSystem` — confirms no duplicate-symbol or signature
mismatch was introduced by this session's local `extern` declarations.

**Real consumers, identified via the class-load index and `.ezinit`,
not assumed**:
- `build/dlib/atk/index` (the `class_Load` registry `ez`/`runapp`
  consults when a document or the `<ESC><TAB>` command names a class)
  lists both classes directly:
  ```
  spread	spread	393a00	spread.do
  table	table	292ca0	table.do
  ```
- `build/lib/global.ezinit` wires a real menu path, not just
  `<ESC><TAB>`-typing discoverability:
  ```
  addfiletype .table table
  addmenu textview-insert-inset-here "Media,Spreadsheet~5" textview textview noinherit "table"
  ```
  (files with a `.table` extension auto-load as a `table` inset; the
  **Media > Spreadsheet** menu item inserts one directly.)
- `build/lib/specials.ezinit` adds a second path:
  ```
  addmenu gnucompat-insert-inset "Special~4,Insert table~43" textview gnucompat inherit "table"
  ```
  (**Special > Insert table**.)
- The Imakefile's own `InstallLink(runapp, $(DESTDIR)/bin/table)`
  means `build/bin/table` is also a directly-launchable `runapp`
  symlink, same pattern as `ez`/`messages`/`help`/`rofftext`.

## 7. New taxonomy sub-case: a class-internal (double-underscore) method declared in its own `.eh`, but only outside `AUXMODULE`

`update.c:120` (original error site) calls the macro
`spread_WantLimitedHighlighting()`. That macro is generated by
classpp from `spread.ch`'s `WantLimitedHighlighting() returns
boolean;` declaration, and — unusually for a class method macro —
expands to a **direct** call of the double-underscore implementation
rather than an indirect call through the class's dispatch-table
pointer:
```
#define spread_WantLimitedHighlighting() \
    ((boolean) spread__WantLimitedHighlighting(NULL))
```
`spread.eh` **does** declare `boolean spread__WantLimitedHighlighting();`
(a real, findable declaration, not simply absent) — but that
declaration sits inside a `#ifndef AUXMODULE ... #endif /* AUXMODULE
*/` block (`spread.eh:977-1207`) that only compiles for the file that
owns the class implementation (`spread.c`, which does not `#define
AUXMODULE` before including its own `.eh`). Every other file in this
directory that includes `<spread.eh>` (`menu.c`, `update.c`,
`keyboard.c`, `hit.c`, `print.c`, `tabio.c`, `funs.c`) `#define
AUXMODULE` first, by design, to avoid re-emitting the class's static
method tables in every translation unit that links into the same
`.do` — but that same guard also hides the double-underscore
implementation declaration the macro needs, with no fallback. This is
distinct from every previously-documented "missing in-tree/project
header" sub-case: the declaration isn't missing, incomplete, or
misspelled — it's real, complete, and correctly typed, but reachable
only from files that structurally cannot use it, while the file that
*can* use it (any `AUXMODULE` consumer of the macro) is deliberately
excluded from seeing it by the same generated header. Resolved the
same way as ordinary sub-case 3 (local `extern boolean
spread__WantLimitedHighlighting();` in `update.c`, sourced from the
real definition in `spread.c:79`, with a comment explaining the
`AUXMODULE` mechanics so a future reader doesn't mistake it for a
plain missing-header gap and try to "fix" it by editing
`spread.eh`/`spread.ch`, which are generated, out of scope, and
already structurally correct for their own intended purpose). Worth
folding into the runbook's taxonomy as a new recognizable shape —
flagging for wdc rather than presuming it's a one-off, since any
directory with a class method whose generated macro bypasses indirect
dispatch (checkable by whether the macro body calls the
double-underscore name directly vs. through
`classname_CLASSPROCEDURES->routines[n]`) could hit the same gap.

## 8. Files touched (compile status)

All 9 edited `.c` files plus the `Imakefile` compile clean (0 errors
of any kind) in their final state, confirmed by two full `make -C ...
clean && make -C ... depend && make -C ... -k install` passes
back-to-back after the fix-surfacing pass (three total per file).
`print.c` needed no edit — confirmed clean before and after, no
fallout of any category.

- `Imakefile` — `COMPILERFLAGS` override added.
- `spread.c` — `<stdlib.h>` added (malloc-family blind spot); local
  externs added for `k_DefineKeys`/`DefineMenus`/`ResetCurrentCell`/
  `WriteTroff`/`spread_update_FullUpdate`/`spread_PartialUpdate`/
  `spread_WantHighlight` (§4).
- `menu.c` — `<string.h>` added (`strcmp`/`strncpy`/`strcpy`); local
  externs added for `k_AskUser`/`k_WantToDiscard`/`k_TellUser`/
  `AddRows`/`AddCols`/`SetCurrentCell`/`CopyChunk`/`WriteTroff` (§4).
- `update.c` — `<string.h>` added (`strcmp`/`strcpy`); local extern
  added for `spread__WantLimitedHighlighting` (§7) and
  `InitializeGraphic`/`ComputeRowSizes` (§4); same-file (non-`static`)
  forward declaration added for `spread_PartialUpdate` (§4).
- `hit.c` — `<stdlib.h>` added (malloc-family blind spot); same-file
  (non-`static`) forward declarations added for `CopyChunk`/
  `SetCurrentCell`/`CompareChunk`/`extendCurrentCell`; local externs
  added for `spread_InvertRectangle`/`spread_ClearSelectionBox`/
  `k_SetMessageState` (§4).
- `keyboard.c` — `<stdlib.h>` added (malloc-family blind spot); local
  externs added for `CopyChunk`/`SetCurrentCell`/`TellFormula`/
  `GetFormula` (§4).
- `table.c` — `<stdlib.h>` added (malloc-family blind spot); same-file
  (non-`static`) forward declarations added for `DestroyCell`/
  `MakeBogus`/`MakeStandard`; local externs added for `WriteASCII`/
  `WriteCell`/`ReadCell`/`eval` (§4).
- `tabio.c` — `<stdlib.h>` added (malloc-family blind spot); local
  extern added for `DestroyCell` (§4).
- `eval.c` — `<stdlib.h>` added (malloc-family blind spot); local
  externs added for `MakeBogus`/`MakeStandard`/`rcref`/`enterfuns`/
  `idate` (§4).
- `funs.c` — local externs added for `MakeBogus`/`MakeStandard`/
  `rangeLimit`/`rcref`/`eval`/`syntaxError`/`enterfun` (§4); no
  `<stdlib.h>` needed (not in the blind-spot list, §5).
- `print.c` — untouched, no fallout of any kind.

Regenerated `Makefile` is not fossil-tracked, not a source edit.

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
config-db:    /Users/wdc/.fossil
checkout:     103316f3364168a731d9a9a85e5d8b7d478ef1ae 2026-07-25 02:25:45 UTC
parent:       1093f029adac96ecb01080fd1b5cb42e04e2f57c 2026-07-25 02:18:04 UTC
tags:         andrew-6.4
comment:      docs: M2 rollout 4e prompt (atk/table) (user: wdc)
EDITED     src/atk/table/Imakefile
EDITED     src/atk/table/eval.c
EDITED     src/atk/table/funs.c
EDITED     src/atk/table/hit.c
EDITED     src/atk/table/keyboard.c
EDITED     src/atk/table/menu.c
EDITED     src/atk/table/spread.c
EDITED     src/atk/table/tabio.c
EDITED     src/atk/table/table.c
EDITED     src/atk/table/update.c
```

Exactly the 10 files listed above (`print.c` correctly absent — no
edit made to it). No commit made. `m2-table-session.diff` (372 lines)
written to the tree root.

## 9. Gate scope — subtree-local, no doubt found; confirms the prompt's structural note

Checked via `nm -g`, per the prompt's explicit instruction rather than
citing the note alone: **confirmed** `atk/table` builds no
`LibraryTarget` (only two `DynamicMultiObject`s), and `nm -g
build/bin/runapp` shows zero symbols from either `.do`'s function
population (§6). Structurally the same shape as `atk/rofftext`
(`DynamicMultiObject`-only, no `LibraryTarget`) — cross-directory
fallout is not just empirically absent but structurally impossible,
since neither `.do` is archived into anything else's link line. Did
not run the tree-wide gate, per the runbook's schedule (`atk/table` is
explicitly on the "skip the tree-wide gate" list, and this session's
own `nm -g` check found nothing to cast doubt on that ruling).

## 10. Exact runtime-check commands for wdc

**Load-bearing**: nothing from this session should be committed until
these are confirmed. Run from native Terminal.app (not an IDE
terminal), `DISPLAY=:0;` set as its own statement first. `.do` files
are cached for the life of a process — use a **fresh** `ez`/`table`
process for each check, not one already running before this session's
rebuild.

1. **Primary check — insert a fresh table/spread inset via the real
   menu path** (exercises the largest and riskiest fix population:
   `k_TellUser`/`SetCurrentCell`/`CopyChunk`/`k_AskUser`, all the
   menu-driven cell-editing commands in `menu.c`/`keyboard.c`/`hit.c`):
   ```
   DISPLAY=:0; ez
   ```
   Use **Media > Spreadsheet** (confirmed real menu binding, §6) to
   insert a fresh table inset. Type into a few cells, use Tab/arrow
   keys to move between cells (exercises `SetCurrentCell`,
   `extendCurrentCell` for a multi-cell selection), select a range and
   Cut/Copy/Paste (exercises `CopyChunk`, `CompareChunk`, the
   `k_TellUser`/`k_AskUser` prompt-and-confirm dialogs directly — try
   both confirming and cancelling a destructive prompt if one appears,
   e.g. deleting a non-empty region), and add/remove a row or column
   (exercises `AddRows`/`AddCols`). Confirm no crash, correct redraw,
   and the cell contents behave as expected.
2. **Formula/eval path** (exercises `eval`, `MakeBogus`,
   `MakeStandard`, `rcref`, `rangeLimit`, `enterfun`/`enterfuns`,
   `idate`, `syntaxError` — the entire `eval.c`/`funs.c`/`table.c`
   spreadsheet-calculation engine, the single largest fix population
   in this session by instance count):
   In the same table inset, type a formula into a cell (e.g. `=1+2` or
   a cell reference like `=A1+A2` if the table's formula syntax
   matches — check `table.help`'s "Entering formulas" section via
   **Help > table** if unsure of the exact syntax) and confirm it
   evaluates to the correct value, redraws correctly, and that an
   intentionally malformed formula (e.g. unbalanced parens) triggers a
   graceful "bogus"/error indicator rather than a crash (exercises
   `MakeBogus`/`syntaxError` directly). Try a built-in function (e.g.
   `sum`, `today`, per `funs.c`'s `enterfuns` table — exercises
   `enterfun`/`enterfuns`, `idate`).
3. **Read/write round-trip** (exercises `WriteASCII`/`WriteCell`/
   `ReadCell`/`DestroyCell` in `tabio.c`/`table.c`):
   With a few cells filled in (some plain text, some formulas), use
   **File > Write File** to save to a new scratch path such as
   `/tmp/m2-table-save-test.ez`, then open that scratch file fresh
   (`DISPLAY=:0; ez /tmp/m2-table-save-test.ez`) and confirm the table
   round-trips correctly — cell contents and formulas both present and
   correct, no crash.
4. **Direct binary launch** (exercises the `InstallLink(runapp,
   .../bin/table)` symlink directly, per §6):
   ```
   DISPLAY=:0; table
   ```
   Confirm a fresh window opens with an empty table ready for input,
   same behavior as inserting via the Media menu.
5. **`.table` file-type auto-load** (exercises `global.ezinit`'s
   `addfiletype .table table` wiring, §6):
   Save a scratch table document from step 3 above but with a `.table`
   extension (e.g. `/tmp/m2-table-filetype-test.table`), then open it
   with a bare `ez` (not `table`):
   ```
   DISPLAY=:0; ez /tmp/m2-table-filetype-test.table
   ```
   Confirm it opens directly as a table inset (not as plain text),
   confirming the extension-to-class auto-association still works.

## 11. Command-style discipline — how it went this session

Followed `atk/text`'s and `atk/rofftext`'s adaptation: this session's
Bash tool does **not** persist working directory across calls
(confirmed via the throwaway `cd`+`pwd` pair in §2 step 2, before any
real build step). Used `make -C <absolute-path> <target>` for every
build step (roughly 15 build-step calls across the fix-surfacing pass,
the malloc-sweep rebuild, and two full determinism cycles), each its
own unchained call.

**Zero permission prompts or denials across the entire session** —
notably including the `nm -g build/bin/runapp` call (§6, §9), issued
as a single bare unpiped call (not even a `cd <tree-root> &&` prefix,
since this session's cwd was already the tree root by default) with no
pipe, no redirect, no chain — went through immediately on the first
attempt, with its large output automatically persisted to a separate
tool-result file rather than needing a follow-up `nm ... | grep`
compound. This is the cleanest command-style outcome of any bucket-4
session so far: no self-inflicted piping/chaining slip needed
correcting, unlike the `overhead/util/lib`, metamail, and
`atk/rofftext` sessions, each of which hit at least one denied
piped/chained `nm` attempt before finding the working shape. Nothing
new to add to the `nm`-piping-denial open question beyond what
`atk/text`'s report already flagged (inconsistent across session
types/instances, not a fixed rule) — this session simply never
attempted a chained/piped form in the first place, having read the
prior sessions' cautions before starting.

## 12. Open questions / anything that surprised you

- §5's `myrealloc`-substring false-positive risk in the malloc-family
  sweep pattern is the main new finding worth carrying forward — it's
  a plausible recurring shape (any directory with its own
  `*alloc`-suffixed wrapper name) that a naive substring `grep` for
  this rollout's mandatory sweep would silently misreport, either by
  over-including a project's own already-forward-declared wrapper
  calls as if they were bare libc calls, or (worse, if someone trusted
  a match count without reading context) masking a real bare-libc call
  sitting nearby. Recommend the runbook's sweep instructions be
  updated to specify a word-boundary-anchored pattern.
- §7's `AUXMODULE`-guarded class-internal-method finding is the
  session's other notable discovery: a real, complete, correctly-typed
  declaration existing in the generated `.eh` file, but structurally
  unreachable from the one file that needs it by the same header's own
  design (the `AUXMODULE` split, used specifically to avoid duplicate
  static-table definitions across a multi-file `.do`). Distinct from
  every previously-documented "missing header" sub-case since nothing
  is actually missing or incomplete — the header is simply designed to
  exclude exactly the file that needs this one declaration. Worth
  flagging to wdc as a candidate new taxonomy entry.
- The two-file-family cross-calling pattern here (`spread.c`/`menu.c`/
  `update.c`/`hit.c`/`keyboard.c`/`print.c` all sharing one class
  family via `spread.eh`; `table.c`/`eval.c`/`funs.c`/`tabio.c`
  sharing the other via `table.eh`) produced a dense web of
  cross-file, no-header calls (`CopyChunk`/`SetCurrentCell` alone
  account for 35 of the 154 census-visible instances) — structurally
  similar to `atk/text`'s `textv.do`/`text.do` eight-file family
  (`m2-text-REPORT.md` §6) and `ams/libs/cui`'s `cuilib.c`/`andmchs.c`
  two-way calling (`m2-batch3b-REPORT.md` §7), reinforcing that this
  "several files behind one or two `DynamicMultiObject`s, sharing no
  internal header" shape recurs across the codebase rather than being
  a one-off.
- `print.c` needing zero fixes of any kind (§3, §8) is the first
  bucket-4 file in either `atk/text`'s or `atk/rofftext`'s 10-file-ish
  rosters to come through completely clean — worth noting only because
  every other file in this directory (and every file in the two prior
  bucket-4 sessions) needed at least the malloc-family sweep even when
  census-clean.
- No hard stops triggered: no called-but-undefined function anywhere
  (every name resolved to a real, correctly-spelled, in-directory
  definition — this directory, like `atk/rofftext`, never calls out to
  another directory's project-local function at all), no
  header-vs-usage type disagreement requiring escalation, and every
  fix stayed inside this directory's own `.c` files (no `.h` file
  exists in this directory to edit or extend — every project-local fix
  is necessarily a local `extern`, confirmed via `ls *.h` returning
  nothing).
- I did not run any AUIS GUI or binary interactively, and made no
  commits, per the prompt.
