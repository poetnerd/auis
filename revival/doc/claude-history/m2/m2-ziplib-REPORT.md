# M2 rollout point 4h (bucket 4, session 8): `contrib/zip/lib`

## 1. Status

Stopped at the (only) gate. **Both gates required for this directory**
per the explicit exception in `m2-rollout-runbook.md`: subtree-local
(`make clean && make depend && make -k install`, twice for
determinism) AND the full tree-wide gate (`make Clean && make
dependInstall`) — this is the tree's known highest-defect-density
directory (`porting-assessment.md` §17's Xft bug history). No commits
made. Working tree: 24 files edited (`Imakefile` + `zipedit.h` + 22
`.c` files). Session diff at tree root: `m2-ziplib-session.diff` (498
lines). Full gate log at the canonical path
`~/src/AUIS/andrew-6.4/dependInstall.log` (233,776 lines, the
tree-wide gate's final state).

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (Command style
   and Logging sections, including the word-boundary-anchored
   malloc-family sweep mandate), `m2-rollout-runbook.md`, and all
   eleven prior rollout reports in full before starting.
2. **Checked command-style/cwd-persistence early**: a throwaway `cd
   .../src/contrib/zip/lib` call followed by a bare `pwd` in the next
   tool call showed the cwd had reverted to the tree root — cwd does
   **not** persist across Bash calls in this session (same as every
   recent bucket-4 session). Used `make -C <absolute-path> <target>`
   for every build step, one command per call, never chained.
3. Read `src/contrib/zip/lib/Imakefile` directly: confirmed no
   `Parser()`/`LexFile` (no generated-source gap) and confirmed the
   all-dynamic build shape — 20 separate `DynamicObject`/
   `DynamicMultiObject` targets (`zip.do` 8 files, `zipv.do` 9 files,
   `zipedit.do` 5 files, `zipprint.do` 2 files, 16 single-file
   `DynamicObject`s), no `LibraryTarget` at all, matching the prompt's
   structural note exactly.
4. Flagged the Imakefile with the runbook's exact four-flag
   `COMPILERFLAGS` override, verified via `make -C ... Makefile` +
   `grep -n COMPILERFLAGS Makefile` (override at line 291 wins over
   `system.mcr`'s default at line 177).
5. Fix-surfacing pass: `make -C ... clean`, `make -C ... depend`,
   `make -C ... -k install CDEBUGFLAGS="-ferror-limit=0 -g -O0"` —
   each a separate Bash call. Result: **145 real errors**, matching
   the stale estimate of 141 closely (103%) — see §3.
6. Investigated every project-local function's real definition via
   `Bash grep`/`Read` before writing any declaration — see §4 for the
   full per-family breakdown.
7. Fixed all 145 census-visible instances across 17 files. Rebuilt —
   exit 0, 0 errors, first attempt (no correction round needed).
8. **Unconditionally swept all 41 `.c` files** (word-boundary-anchored,
   space-tolerant pattern `grep -nE "\bmalloc *\(|\bfree *\(|\brealloc
   *\(|\bcalloc *\("`) for bare malloc-family calls. Found the blind
   spot in 19 of 41 files, 111 real call sites — see §5.
9. Fixed all 111 additional instances (`#include <stdlib.h>` added to
   19 files). Rebuilt — 0 errors, exit 0, confirmed 0 `conflicting
   types for` errors (no stale wrong-typed externs existed anywhere in
   this directory to begin with — checked explicitly).
10. Ran the subtree-local gate twice more for determinism (`make -C
    ... clean`, `make -C ... depend`, `make -C ... -k install`, plain
    recipe — i.e. the directory's normal, unmodified `-O` optimization
    level, not the `-O0` fix-surfacing override — each its own call) —
    both passes exit 0, 0 errors. No anomaly of any kind tied to `-O`
    vs `-O0` observed in either build pass (see §11 for the explicit
    check against the known pre-existing rendering bug).
11. Identified runtime consumers via `nm -g build/bin/runapp` (zero
    symbol leakage — fully dynamic, confirmed structurally) and the
    class-load index / `.ezinit` file-type wiring — see §9.
12. Checked `ps aux` for other `make`/`bison` processes — none — then
    ran the required tree-wide gate: `make -C .../src Clean` (exit 0),
    then `make -C .../src dependInstall`, launched via the Bash tool's
    own `run_in_background` mechanism directly (no shell-level
    `nohup`/`&`, per the explicit caution about the immediately prior
    session's mistake), logged to the fixed
    `~/src/AUIS/andrew-6.4/dependInstall.log` path. See §8.
13. `fossil status` (§10) confirms exactly the 24 files touched. No
    commit made. `fossil diff > m2-ziplib-session.diff` written to the
    tree root.

## 3. Real instance count: 145 census-visible + 111 malloc-blind-spot = 256 total fallout instances (vs. the stale table's 141)

The census-visible count (145) landed within 3% of the stale estimate
(141) — the closest match of any bucket-4 directory except
`overhead/util/lib`'s exact match. **This proximity is not evidence
the malloc-family blind spot was absent** (per the runbook's own
standing warning, reinforced yet again here): the mandatory sweep
found 111 more instances across 19 of 41 files, none visible to the
`-k`/`-ferror-limit=0` census at all. Real total fallout is **256
instances across 24 files**, 82% over the stale estimate once the
blind spot is counted — squarely in line with every prior bucket-4
directory's pattern that the stale table is a floor, not a target.

Every one of the 111 malloc-blind-spot sites had **zero declaration of
any kind** reaching it before this session — not even a stale,
wrong-typed `extern char *malloc();` to clean up (checked explicitly:
`grep -n "extern.*malloc\|extern.*realloc\|extern.*free\b\|extern.*calloc"
*.c` returned nothing anywhere in the directory). This is the "total
absence" shape (`atk/text`, `atk/rofftext`, `atkams/messages/lib`), not
the "stale wrong-typed extern becomes a conflict" shape
(`overhead/util/lib`, metamail, `overhead/mail/lib`).

19 of the 41 `.c` files needed **zero** fixes of any kind (neither
census-visible nor malloc-blind-spot): `zipoarc.c`, `zipobj.c`,
`zipocirc.c`, `zipoelli.c`, `zipoline.c`, `zipoplin.c`, `ziporang.c`,
`ziporect.c`, `zipotrap.c`, `zipstat.c`, `zipve01.c`, `zipvf00.c`,
`zipvi00.c`, `zipvs00.c`, `zipvp01.c`, `zipvp02.c`, `zipdf01.c`,
`zipds02.c`, `zipv000.c` (the last had one `malloc`-family-looking
string, but it sits entirely inside a `/* ... */` comment block —
confirmed by reading the surrounding lines before counting it, not a
real call site, correctly excluded).

## 4. Per-family taxonomy: the 145 census-visible errors

145 = 26 standard-library + 22 same-file forward reference + 97
missing-in-tree/no-header-anywhere (cross-file). No new taxonomy
category or sub-case (see §7 for the explicit `AUXMODULE` check the
prompt asked for — it does not recur here).

### Missing standard-library header (26 of 145)

| File | Missing decls | Header added |
|---|---|---|
| `zipd000.c:421,423,864` | `strlen`, `strcpy`, `strcmp` | `<string.h>` |
| `zipdf00.c:296,306,310` | `strcmp`, `strlen`, `strcpy` | `<string.h>` |
| `zipdi00.c:498,508,512` | `strcmp`, `strlen`, `strcpy` | `<string.h>` |
| `zipds01.c:568,763` | `strncmp`, `strlen` | `<string.h>` |
| `zipedit.c:898` | `strcmp` | `<string.h>` |
| `zipocapt.c:166,167` | `strcpy`, `strlen` | `<string.h>` |
| `zipofcap.c:224,225,460` | `strcpy`, `strlen`, `strcat` | `<string.h>` |
| `zipvr00.c:220,221,222,534` | `strlen`, `strcpy`, `strcat`, `strcmp` | `<string.h>` |
| `zipv.c:1637,1644,1711` | `strlen`, `strncpy`, `strcmp` | `<string.h>` |
| `zipvp00.c:242,246` | `strcpy`, `strlen` | `<string.h>` |

### Same-file forward reference (22 of 145)

| File:line(s) | Function | Real definition |
|---|---|---|
| `zipd000.c:846,861,901` | `strhash` | `zipd000.c:982`, same file |
| `zipds00.c:126,192,214` | `zip_Open_Stream_File` | `zipds00.c:749`, same file |
| `zipds00.c:129,174,195,216` | `zip_Close_Stream_File` | `zipds00.c:861`, same file |
| `zipds00.c:998` | `zip_Set_Stream_File_Name` | `zipds00.c:1009`, same file |
| `zipprint.c:235,254` | `apt_MM_Compare` | `zipprint.c:568`, same file |
| `zipvp00.c:970,1014,1490,1516` | `zipview_Compute_Pane_Stretch_Factors` | `zipvp00.c:1520`, same file |
| `zipvp03.c:175,588` | `zipview_Preserve_Overlay` | `zipvp03.c:606`, same file |
| `zipvp03.c:251` | `zipview_Draw_Pane_Border` | `zipvp03.c:453`, same file |
| `zipvp03.c:554` | `zipview_Restore_Overlay` | `zipvp03.c:634`, same file |
| `zipve00.c:652` | `Highlight_Inferior_Image_Points` | `zipve00.c:661`, same file (recursive) |

Fix: plain (non-`static`) forward declarations near the top of each
file, since every one of these functions is also called cross-file
elsewhere in the same `.do` family except `strhash` and
`Highlight_Inferior_Image_Points` (confirmed by grep before deciding
non-`static`).

### Missing in-tree/project header — no header anywhere in the tree, cross-file (97 of 145)

Every one of these is "no header anywhere" sub-case 3 — confirmed by
`grep -ln` against every `.h`/`.ih` in the directory before writing any
declaration, zero hits in all cases. Two shapes, split by which
sub-case-2 rule applied:

**Extended `zipedit.h` directly (56 instances, the `zipedit.do` family
— `zipedit.c` 16, `zipve02.c` 17, `zipve03.c` 23)**: `Show_Enclosure`,
`zipedit_Enclose_Figure`, `zipedit_Reset_Editing_Selection`,
`zipedit_Cancel_Enclosure`, `zipedit_Display_Background_Pane`,
`zipedit_Hide_Selection_Menu`, `zipedit_Redisplay_Edit_Pane`,
`zipedit_Prepare_Editing_Control`, `zipedit_Reset_Editing_Control`,
`zipedit_Expose_Figure_Palette`, `zipedit_Expose_Font_Palette`,
`zipedit_Expose_Shade_Palette`, `zipedit_Hide_Font_Palette`,
`zipedit_Hide_Figure_Palette`, `zipedit_Hide_Shade_Palette`,
`zipedit_Handle_Font_Family_Selection`,
`zipedit_Handle_Font_Height_Selection`,
`zipedit_Handle_Font_Italic_Selection`,
`zipedit_Handle_Font_Bold_Selection`,
`zipedit_Handle_Font_Sample_Selection`,
`zipedit_Handle_Figure_Palette_Hit`, `zipedit_Handle_Shade_Palette_Hit`,
`zipedit_Expose_Selection_Menu`. **Justification (taxonomy sub-case 2,
second shape)**: `zipedit.h` lives *in* the flagged directory, is
small, and — despite its own file banner calling itself "Internal
Macros" — already carries exactly one pre-existing cross-file function
declaration in the identical bare style
(`zip_type_figure zipedit_Next_Selected_Figure();`, confirmed defined
in `zipve03.c`, called from `zipedit.c`/`zipve02.c` — real,
already-working precedent, not something I introduced). No competing
local-extern habit exists anywhere in this family's files (checked:
zero `extern` lines in `zipedit.c`/`zipve00.c`/`zipve01.c`/
`zipve02.c`/`zipve03.c` before this session). All 22 new declarations
added in the identical `TYPE name();` style, all resolve to `int`
(confirmed from each real definition — several are written `int\nname(...)`
K&R-split-line style, the rest bare implicit-`int`, none pointer- or
`long`-returning).

**Local `extern` per consuming file (41 instances, the rest — `zip.c`
2, `zipd000.c` 1, `zipdf00.c` 6, `zipdi00.c` 5, `zipds00.c` 7,
`zipds01.c` 4, `zipprint.c` 6, `zipvp00.c` 2, `zipvp03.c` 8)**:
`zip_Enparse_Stream`, `apt_MM_Compare`, `symtab_delete`,
`symtab_find`, `symtab_add`, `symtab_create`, `symtab_destroy`,
`zip_Open_Stream_File`, `zip_Close_Stream_File`, `zip_Deparse_Stream`,
`zip_Set_Stream_File_Name`, `zipprint_Write_Print_Datastream_Header`,
`zipprint_Write_Print_Datastream_Trailer`, `zipview_Restore_Overlay`,
`zipview_Compute_Pane_Stretch_Factors`, `zipview_Mark_Pane_Exposed`,
`zipview_Recoordinate_Panes`, `zipview_Mark_Pane_Hidden`. No
`zip.h`/`zippane.h`/other in-directory header extended for these — no
existing header there is a dedicated function-declaring API for these
families (`zippane.h` is purely struct-layout, no function
declarations at all; no `zipv.h` exists in `INCFILES`) and no
competing local-extern habit existed to match, so plain local `extern`
next to each file's own include block, per the default rule.

**LP64-relevant finding, sourced from real definitions, not
inferred**: `zip_Enparse_Stream` (`zipds02.c:274`) and
`zip_Deparse_Stream` (`zipds01.c:128`) are both **`long`-returning** at
their real definitions, not the implicit `int` every other function in
this directory's fallout uses — confirmed by reading each definition
directly (`long\nzip_Enparse_Stream(...)`, `long\nzip_Deparse_Stream(...)`)
before writing `extern long ...();` for both, per the taxonomy's
standing caution against introducing a fresh wrong-width declaration
while fixing an M2 warning. A good concrete instance to keep citing
alongside `overhead/mail/lib`'s `AppendHosts` and
`atkams/messages/lib`'s `CUI_*`/`MS_*` mixed-width findings.

**"Possible genuine bug/typo" category: still empty.** Every
unfamiliar-looking name across all 256 instances resolved to a real,
correctly-spelled, `grep`-findable definition once checked. No
candidate for the hard-stop category turned up.

## 5. Malloc-family blind-spot sweep: 111 instances across 19 of 41 files

Per the prompt's unconditional mandate, swept all 41 `.c` files with
the corrected pattern `grep -nE "\bmalloc *\(|\bfree *\(|\brealloc
*\(|\bcalloc *\("` (word-boundary-anchored, space-before-paren
tolerant — confirmed no `*alloc`-suffixed project-local wrapper exists
anywhere in this directory that could false-positive the pattern, and
confirmed the space-variant call style, e.g. `zipd000.c:605`'s
`malloc (sizeof(pool_type)+expected_size)`, is both present and
correctly caught):

| File | Call sites | Census-visible errors in this file? |
|---|---|---|
| `zipds00.c` | 19 | Yes (15) |
| `zipd000.c` | 13 | Yes (4) |
| `ziposym.c` | 13 | **No — zero census-visible errors in this file** |
| `zipdf00.c` | 11 | Yes (6) |
| `zipvp00.c` | 9 | Yes (6) |
| `zipdi00.c` | 8 | Yes (5) |
| `zipve02.c` | 7 | Yes (17) |
| `zipds01.c` | 5 | Yes (4) |
| `zipve03.c` | 5 | Yes (23) |
| `zipopath.c` | 3 | **No — zero census-visible errors in this file** |
| `zipopoly.c` | 3 | **No — zero census-visible errors in this file** |
| `zipve00.c` | 3 | Yes (1) |
| `zipvr00.c` | 4 | Yes (4, std-lib only) |
| `zip.c` | 1 | Yes (2) |
| `zipedit.c` | 2 | Yes (17, std-lib+cross-file) |
| `zipprint.c` | 2 | Yes (8) |
| `zipoarrw.c` | 1 | **No — zero census-visible errors in this file** |
| `zipoimbd.c` | 1 | **No — zero census-visible errors in this file** |
| `zipv.c` | 1 | Yes (3, std-lib only) |
| **Total** | **111** | |

5 of the 19 files (`ziposym.c`, `zipopath.c`, `zipopoly.c`,
`zipoarrw.c`, `zipoimbd.c`) had **zero** census-visible errors of their
own yet real, previously-invisible malloc-family fallout — a fourth
data point (after `atk/text`, `atk/rofftext`, `atkams/messages/lib`)
that per-file `-Werror=implicit-function-declaration` cleanliness says
nothing about this category's presence. All five are single-file
`DynamicObject`s (`ziposym.do`, `zipopath.do`, `zipopoly.do`,
`zipoarrw.do`, `zipoimbd.do`) — the malloc-family blind spot recurring
in this directory's *smallest*, structurally simplest targets, not
just its large multi-file `.do`s. `ziposym.c` (13 sites) is the single
largest blind-spot population in a file with zero other symptom found
in this session. Fix: `#include <stdlib.h>` added to all 19 files.

## 6. Final clean builds (twice, for determinism)

```
$ make -C .../contrib/zip/lib clean && make -C .../contrib/zip/lib depend && make -C .../contrib/zip/lib -k install   [pass 1]
EXIT=0, 0 "error:" lines
$ make -C .../contrib/zip/lib clean && make -C .../contrib/zip/lib depend && make -C .../contrib/zip/lib -k install   [pass 2]
EXIT=0, 0 "error:" lines
```

All 20 `.do` targets installed successfully to `build/dlib/atk/` both
times (confirmed via `ls -la build/dlib/atk/zip*.do` — 20 files, fresh
timestamps). `COMPILERFLAGS` re-verified after both passes (`grep -n
COMPILERFLAGS Makefile` — override line still wins at line 291 over
`system.mcr`'s default at 177). Both determinism passes ran at the
directory's **normal, unmodified `-O` optimization level**
(`CDEBUGFLAGS = -O`, confirmed via `grep -n "^CDEBUGFLAGS"
src/config/darwin/system.mcr` — the fix-surfacing pass's
`-ferror-limit=0 -g -O0` override was not used here), per the prompt's
explicit instruction to confirm the final determinism builds don't
newly trip over the pre-existing `-O`-only rendering bug. **No anomaly
of any kind observed** — both passes exit 0 with 0 errors, byte-for-byte
identical result, no crash, hang, or unusual diagnostic tied to
optimization level. This is a compile-time-only confirmation (the
known bug is a runtime rendering issue, not a build-time one) — see
§11 for the explicit reminder to wdc not to conflate the two.

## 7. `AUXMODULE` double-underscore sub-case check (prompt's explicit ask) — does NOT recur here

Checked explicitly, not assumed, given this directory's many small
classes and 4 multi-file `.do`s (`zip.do`, `zipv.do`, `zipedit.do`,
`zipprint.do` — the only targets structurally capable of hitting this
sub-case, since it requires more than one file sharing a `.do` and
using an `AUXMODULE`-guarded `.eh`):

```
$ grep -l "AUXMODULE" *.c
(no output)
$ grep -n "#define AUXMODULE" *.c
(no output)
```

**Zero files in this directory use `#define AUXMODULE` at all** —
structurally different from `atk/table`, where the sub-case's
recurrence depended on some (but not all) files in a `.do` family
defining `AUXMODULE` before including their own `.eh`. Here, no file
ever does, so every file compiled into a multi-file `.do` sees its
`.eh`'s full content unconditionally — there is no walled-off block
for a class-internal double-underscore declaration to hide behind.
Confirmed independently by checking every one of the 145 census
instances' real names: none use the `class__Method` double-underscore
naming convention (all are plain single-underscore project-local C
functions — `zipedit_Foo`, `zip_Foo`, `apt_MM_Compare`, `symtab_*`,
`zipview_*`), none appear in any `.ch`/`.eh` file at all. **Confirmed:
no recurrence, and structurally cannot recur in this directory as
currently written.**

## 8. Full tree-wide gate results

Checked `ps aux` first — no other `make`/`bison` processes running.
Ran as directed by the runbook's explicit exception for this
directory:

```
$ make -C /Users/wdc/src/AUIS/andrew-6.4/src Clean
EXIT=0
```

```
$ make -C /Users/wdc/src/AUIS/andrew-6.4/src dependInstall > ~/src/AUIS/andrew-6.4/dependInstall.log 2>&1
```

Launched via the Bash tool's own `run_in_background: true` mechanism
directly, with **no** shell-level `nohup`/`&` wrapper — the explicit
lesson from the immediately prior (`atkams/messages/lib`) session's
self-inflicted false-completion mistake. The coordinator independently
confirmed real completion (no `make`/`bison` processes running,
`dependInstall.log` ending with normal install-completion markers) and
relayed it; I then independently re-verified against the log content
myself rather than taking that on trust alone:

```
$ wc -l ~/src/AUIS/andrew-6.4/dependInstall.log
233776
$ grep -c "error:" ~/src/AUIS/andrew-6.4/dependInstall.log
4
$ grep -n "error:" ~/src/AUIS/andrew-6.4/dependInstall.log
112087:  200 |                 ErrorA(FATAL, "Internal error: unknown recognizer type", buff);
177948:clang: error: linker command failed with exit code 1 (use -v to see invocation)
229895:ltapp.c:115:27: error: incompatible integer to pointer conversion passing 'boolean' (aka 'int') to parameter of type 'void *' [-Wint-conversion]
229907:ltapp.c:123:32: error: incompatible integer to pointer conversion passing 'boolean' (aka 'int') to parameter of type 'void *' [-Wint-conversion]
```

**Exactly the 4 known pre-existing baseline errors every prior M2
session has documented**, each confirmed by citation/context rather
than assumed:

1. **Recognizer-type false positive** (line 112087) — the literal
   source text of an `ErrorA(FATAL, "Internal error: unknown
   recognizer type", buff)` call, not a real diagnostic (this string
   lives in `atk/syntax/tlex/defaults.c`, per `m2-batch3a-REPORT.md`).
2. **`ams/msclients/nns`'s pre-existing SSLLIB link failure** (line
   177948) — confirmed by context: `grep -n "^building (dependInstall)"
   ... | awk -F: '$1<177948' | tail -1` returns line 176335,
   `building (dependInstall) (.../src/ams/msclients/nns)`, with no
   other directory's build in between. Unrelated to any M2 work
   (per `m2-batch3b-REPORT.md` §6's independent confirmation).
3–4. **`contrib/zip/utility/ltapp.c`'s two pre-existing int-conversion
   errors** (lines 229895, 229907) — confirmed by context: `grep -n
   "^building (dependInstall)" ... | awk -F: '$1<229895' | tail -1`
   returns line 228745, `building (dependInstall)
   (.../src/contrib/zip/utility)` — a **different directory** from
   this session's `contrib/zip/lib`, exactly the distinction the
   prompt warned not to confuse. Not touched, not fixed, not mine to
   fix this session.

**Confirmed `contrib/zip/lib`'s own build span is clean**, not just
absence of errors tree-wide:

```
$ grep -n "building (dependInstall) (.*contrib/zip/lib)" dependInstall.log
218448:building (dependInstall) (/Users/wdc/src/AUIS/andrew-6.4/src/contrib/zip/lib)
$ grep -n "^building (dependInstall)" dependInstall.log | awk -F: '$1>218448' | head -1
228718:building (dependInstall) (/Users/wdc/src/AUIS/andrew-6.4/src/contrib/zip/symbols)
```

This directory's entire build span is lines 218448–228718 — none of
the 4 baseline `error:` lines (112087, 177948, 229895, 229907) fall
inside that range (the nearest, 229895/229907, are both *after* this
span, inside `contrib/zip/utility`'s own span starting at 228745,
confirmed a different, later-building directory). `build/dlib/atk/`'s
20 `zip*.do` files all show fresh timestamps matching this gate run.
**Gate is green. Zero new `error:` lines anywhere in the tree.**

## 9. Runtime consumers and structural confirmation

`contrib/zip/lib` builds no `LibraryTarget` — confirmed structurally
via the Imakefile (§2) and empirically via `nm -g build/bin/runapp`
(issued as a single unpiped, tree-root-relative call, per the
established carve-out — an absolute-path attempt was denied outright
first, matching every prior bucket-4 session's finding): **zero**
symbols matching `zip_`/`zipedit_`/`zipview_`/`zipprint_`/
`apt_MM_Compare`/`symtab_` appear anywhere in `runapp`'s symbol table.
This directory is **100% dynamically loaded**, structurally not just
empirically — every one of its 20 `.do` targets is confirmed present
in `build/dlib/atk/` with fresh timestamps, and the class-load index
(`build/dlib/atk/index`) lists all 20 classes directly:

```
zip        zip        297f30  zip.do
zipedit    zipedit    292ef8  zipedit.do
zipoarc    zipoarc    96180   zipoarc.do
...
zipview    zipv       29a860  zipv.do
```

`build/lib/global.ezinit` wires `addfiletype .zip zip` (files with a
`.zip` extension auto-load as a `zip` inset). No dedicated **Media >**
menu entry for inserting a fresh zip inset was found in
`global.ezinit`/`specials.ezinit` (checked, not assumed) — insertion is
via the generic `<ESC><TAB>zip` command, the same pattern used for
several other insets before/without a dedicated menu binding. The
Imakefile's own `InstallLink(runapp, $(DESTDIR)/bin/zip)` means
`build/bin/zip` is also a directly-launchable `runapp` symlink
(confirmed present), same pattern as `ez`/`messages`/`table`/`rofftext`.

## 10. Files touched (compile status)

All 24 edited files compile clean (0 errors of any kind) in their
final state, confirmed by two full `make -C ... clean && make -C ...
depend && make -C ... -k install` passes back-to-back after the
fix-surfacing and malloc-sweep passes, plus the tree-wide gate.

- `Imakefile` — `COMPILERFLAGS` override added.
- `zipedit.h` — extended with 22 new function declarations for the
  `zipedit.do` family (§4), matching the pre-existing
  `zipedit_Next_Selected_Figure` declaration's style.
- `zip.c` — `<stdlib.h>` (blind spot) added; local extern for
  `zip_Enparse_Stream` (`long`) added.
- `zipd000.c` — `<string.h>` + `<stdlib.h>` added; same-file forward
  declaration for `strhash`; local extern for `apt_MM_Compare` added.
- `zipdf00.c` — `<string.h>` + `<stdlib.h>` added; local externs for
  `symtab_delete`/`symtab_find`/`symtab_add` added.
- `zipdi00.c` — `<string.h>` + `<stdlib.h>` added; local externs for
  `symtab_delete`/`symtab_find`/`symtab_add` added.
- `zipds00.c` — `<stdlib.h>` added; same-file forward declarations for
  `zip_Open_Stream_File`/`zip_Close_Stream_File`/
  `zip_Set_Stream_File_Name`; local externs for `zip_Deparse_Stream`
  (`long`)/`zip_Enparse_Stream` (`long`)/`apt_MM_Compare`/
  `symtab_create`/`symtab_destroy` added.
- `zipds01.c` — `<string.h>` + `<stdlib.h>` added; local externs for
  `zip_Close_Stream_File`/`zip_Set_Stream_File_Name`/
  `zip_Open_Stream_File`/`apt_MM_Compare` added.
- `zipedit.c` — `<string.h>` + `<stdlib.h>` added (all its 16
  project-local fixes resolved via the `zipedit.h` extension above,
  already `#include`d).
- `zipoarrw.c` — `<stdlib.h>` added (blind spot only; zero
  census-visible errors).
- `zipocapt.c` — `<string.h>` added.
- `zipofcap.c` — `<string.h>` added.
- `zipoimbd.c` — `<stdlib.h>` added (blind spot only).
- `zipopath.c` — `<stdlib.h>` added (blind spot only).
- `zipopoly.c` — `<stdlib.h>` added (blind spot only).
- `ziposym.c` — `<stdlib.h>` added (blind spot only, largest
  single-file blind-spot population, 13 sites).
- `zipprint.c` — `<stdlib.h>` added; same-file forward declaration for
  `apt_MM_Compare`; local externs for
  `zipprint_Write_Print_Datastream_Header`/`...Trailer` added.
- `zipv.c` — `<string.h>` + `<stdlib.h>` added.
- `zipve00.c` — `<stdlib.h>` added; same-file forward declaration for
  `Highlight_Inferior_Image_Points` added.
- `zipve02.c` — `<stdlib.h>` added (all 17 project-local fixes
  resolved via the `zipedit.h` extension).
- `zipve03.c` — `<stdlib.h>` added (all 23 project-local fixes
  resolved via the `zipedit.h` extension).
- `zipvp00.c` — `<string.h>` + `<stdlib.h>` added; same-file forward
  declaration for `zipview_Compute_Pane_Stretch_Factors`; local
  externs for `zipview_Restore_Overlay`/`apt_MM_Compare` added.
- `zipvp03.c` — same-file forward declarations for
  `zipview_Preserve_Overlay`/`zipview_Draw_Pane_Border`/
  `zipview_Restore_Overlay`; local externs for
  `zipview_Compute_Pane_Stretch_Factors`/`zipview_Mark_Pane_Exposed`/
  `zipview_Recoordinate_Panes`/`zipview_Mark_Pane_Hidden` added (no
  `<stdlib.h>` needed — not in the blind-spot list).
- `zipvr00.c` — `<string.h>` + `<stdlib.h>` added.

Regenerated `Makefile` is not fossil-tracked, not a source edit.

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
config-db:    /Users/wdc/.fossil
checkout:     cdf8824f6b4ff244cb36a08b9ff95a3bd7cf99e5 2026-07-25 14:10:47 UTC
parent:       ac8a93e4a42b74d5c1be4d82f55dd4f8b8e6482f 2026-07-25 14:01:03 UTC
tags:         andrew-6.4
comment:      docs: M2 rollout 4h prompt (contrib/zip/lib) (user: wdc)
EDITED     src/contrib/zip/lib/Imakefile
EDITED     src/contrib/zip/lib/zip.c
EDITED     src/contrib/zip/lib/zipd000.c
EDITED     src/contrib/zip/lib/zipdf00.c
EDITED     src/contrib/zip/lib/zipdi00.c
EDITED     src/contrib/zip/lib/zipds00.c
EDITED     src/contrib/zip/lib/zipds01.c
EDITED     src/contrib/zip/lib/zipedit.c
EDITED     src/contrib/zip/lib/zipedit.h
EDITED     src/contrib/zip/lib/zipoarrw.c
EDITED     src/contrib/zip/lib/zipocapt.c
EDITED     src/contrib/zip/lib/zipofcap.c
EDITED     src/contrib/zip/lib/zipoimbd.c
EDITED     src/contrib/zip/lib/zipopath.c
EDITED     src/contrib/zip/lib/zipopoly.c
EDITED     src/contrib/zip/lib/ziposym.c
EDITED     src/contrib/zip/lib/zipprint.c
EDITED     src/contrib/zip/lib/zipv.c
EDITED     src/contrib/zip/lib/zipve00.c
EDITED     src/contrib/zip/lib/zipve02.c
EDITED     src/contrib/zip/lib/zipve03.c
EDITED     src/contrib/zip/lib/zipvp00.c
EDITED     src/contrib/zip/lib/zipvp03.c
EDITED     src/contrib/zip/lib/zipvr00.c
```

Exactly 24 files `EDITED`, no `ADDED`/`DELETED`/`MISSING` entries, no
commit made at any point in this session. `m2-ziplib-session.diff`
(498 lines) is the exact, complete record of every change.

## 11. Exact runtime-check commands for wdc

**Load-bearing**: nothing from this session should be committed until
these are confirmed. Run from native Terminal.app (not an IDE
terminal), `DISPLAY=:0;` set as its own statement first, per
`sonnet-playbook.md`'s X11 convention. `.do` files are cached for the
life of a process — use a **fresh** `ez`/`zip` process for each check,
not one already running before this session's rebuild.

**Important, read before testing**: this directory has a **known,
unresolved, pre-existing rendering bug** — a solid-black render at `-O`
optimization only (the build's normal, unmodified level, confirmed
still in effect for these binaries per §6), found during prior
figure/inset rendering work, root cause not found after extensive
`lldb` tracing. **This bug is unrelated to this M2 session's
declaration-only fixes and was not investigated or touched here.** If
you see a zip inset render solid black, that is very likely this
pre-existing bug resurfacing, not a regression from today's work —
please flag it back either way so it doesn't get silently
double-counted, but don't treat it as evidence something here broke.

1. **Primary check — insert a fresh zip inset and draw a few
   primitive shapes** (exercises the largest fix populations directly:
   the `zipedit.do` family's enclosure/palette/selection-menu commands,
   §4, and `zip.do`'s figure/image symbol-table plumbing, §4):
   ```
   DISPLAY=:0; ez
   ```
   `<ESC><TAB>` and type `zip` to insert a fresh zip drawing inset
   (no dedicated menu path was found this session, §9 — this is the
   confirmed insertion mechanism). In the drawing pane, try:
   - Drawing a rectangle (`ziporect` — needed zero fixes itself, but
     confirms the pane/palette machinery this session's `zipedit.do`
     fixes control still works around it) and an ellipse (`zipoelli`,
     also zero fixes) via the shape palette.
   - Adding a text/caption element (`zipocapt.do` — touched this
     session, `<string.h>` fix) and typing a short label.
   - Selecting a shape and dragging an "enclosure" box around it —
     this is the single most heavily-touched code path in the session
     (`Show_Enclosure`, `zipedit_Enclose_Figure`,
     `zipedit_Cancel_Enclosure`, `zipedit_Reset_Editing_Selection`, all
     part of the 56-instance `zipedit.h` extension, §4) — confirm the
     enclosure box appears, tracks the drag, and can be cancelled
     (click elsewhere) as well as completed.
   - Opening the font-selection controls if the palette exposes one
     (exercises `zipedit_Handle_Font_Family_Selection`/
     `..._Height_Selection`/`..._Italic_Selection`/`..._Bold_Selection`/
     `..._Sample_Selection`, all in `zipve03.c`).
   Confirm no crash on any of the above, and that shapes/text render
   with actual content rather than a blank or solid-black pane (see
   the caution above about the known unrelated bug).

2. **Save/reload round-trip** (exercises `zip.do`'s stream/symbol-table
   read-write path directly — `zip_Enparse_Stream`/`zip_Deparse_Stream`
   (both `long`-returning, §4's LP64 finding), `symtab_create`/
   `symtab_find`/`symtab_add`/`symtab_destroy`, `zip_Open_Stream_File`/
   `zip_Close_Stream_File`):
   With a drawing in place from step 1, use **File > Write File** to
   save to a new scratch path such as `/tmp/m2-ziplib-save-test.ez`,
   then open that scratch file fresh:
   ```
   DISPLAY=:0; ez /tmp/m2-ziplib-save-test.ez
   ```
   Confirm the drawing round-trips correctly — shapes, text, and any
   applied enclosure/grouping all present and in the right place, no
   crash on either the save or the reload.

3. **Direct binary launch** (exercises the `InstallLink(runapp,
   .../bin/zip)` symlink directly, §9):
   ```
   DISPLAY=:0; zip
   ```
   Confirm a fresh window opens ready for drawing, same behavior as
   inserting via `<ESC><TAB>zip`.

4. **`.zip` file-type auto-load** (exercises `global.ezinit`'s
   `addfiletype .zip zip` wiring, §9): save a scratch drawing from
   step 2 above but with a `.zip` extension (e.g.
   `/tmp/m2-ziplib-filetype-test.zip`), then open it with a bare `ez`
   (not `zip`):
   ```
   DISPLAY=:0; ez /tmp/m2-ziplib-filetype-test.zip
   ```
   Confirm it opens directly as a zip drawing inset (not as plain
   text or a generic file), confirming the extension-to-class
   auto-association still works.

5. **Printing/print-datastream path** (exercises
   `zipprint_Write_Print_Datastream_Header`/`...Trailer`, `zipprint.do`,
   the only touched family not otherwise exercised by steps 1–4):
   if the zip inset offers a print/preview command (check its own
   pop-up menu or `zip.help`'s "Previewing and Printing" section),
   trigger it once against the step-1 drawing and confirm it completes
   without error — not required to inspect actual print output in
   detail, just confirm the datastream-header/trailer write path
   doesn't crash or hang.

## 12. Command-style discipline — how it went this session

Followed the established `atk/text`/`atk/rofftext`/`atk/table`/
`overhead/mail/lib`/`atkams/messages/lib` adaptation exactly, having
checked early per the prompt's instruction: this session's Bash tool
does **not** persist working directory across calls (confirmed via a
throwaway `cd`+`pwd` pair before any real build step). Used `make -C
<absolute-path> <target>` for every build step (roughly 20 build-step
calls across the fix-surfacing pass, the malloc-sweep-triggered
rebuild, two full subtree determinism cycles, and the tree-wide
`Clean`/`dependInstall` pair), each its own unchained call.

**Background-build mechanism**: launched the tree-wide
`dependInstall` via the Bash tool's own `run_in_background: true`
parameter directly, with no shell-level `nohup ... &` wrapper — the
explicit fix for the immediately prior (`atkams/messages/lib`)
session's mistake. This worked as intended: the tool call itself did
not report a premature "completed" status, and the genuine completion
arrived later as a background-task notification (relayed by the
coordinator, who had independently verified via `ps aux` and the log's
own completion markers before passing it along) — no manual PID-polling
correction was needed this time, unlike the prior session.

**Permission-prompt/denial pattern, and the auto-mode question**: zero
prompts and zero denials on every individually-allow-listed command
this session (`make -C ...`, `grep -n`/`grep -c`/`grep -l`, `ls`,
`fossil status`/`diff`, `Edit`, `Read`) — consistent with every prior
bucket-4 session's experience. The **one** friction point matched the
now-standing pattern exactly: a first `nm -g` attempt using an
absolute path (`nm -g /Users/wdc/.../runapp`) was denied outright with
no prompt; reissuing as the established carve-out shape (`nm -g
build/bin/runapp`, relative path, no pipe, no redirect, no `cd` prefix
needed since this session's default cwd already resolves to the tree
root for bare relative paths) went through immediately, with its large
output automatically persisted to a tool-result file, grepped
separately in a following call. **On whether auto mode's effect was
detectable this session**: same honest answer as the
`atkams/messages/lib` session's — I have no way to distinguish "no
prompts occurred because auto mode suppressed them" from "no prompts
occurred because every command I issued matched an allow-list entry or
the known carve-out shape" from inside a single delegated session; I
can only report what I observed (zero prompts, one clean self-corrected
denial), which is indistinguishable from every pre-auto-mode bucket-4
session's own reported pattern. If auto mode's benefit is specifically
"the human isn't interrupted for calls that were already going to be
auto-approved," that's real but invisible to me from in here.

## 13. Open questions / anything that surprised you

- The `zipedit.h` extension (§4) is the most structurally interesting
  finding this session: a header whose own file banner calls it
  "Internal Macros" turned out to already be quietly doubling as a
  (nearly empty, one-declaration) function-declaring API for its
  `.do` family — recognizing that single pre-existing declaration as
  real, working precedent (rather than dismissing it as noise because
  the file is "really" a macro header) is what made the taxonomy's
  sub-case-2 "extend the header" rule apply cleanly to 56 of this
  session's 97 cross-file instances in one edit, rather than needing
  56 individual local externs scattered across 3 files.
- `ziposym.c`'s 13-site malloc-family population with zero
  census-visible errors (§5) is the largest single-file blind-spot
  count found with zero other symptom in any bucket-4 session so far
  — worth citing alongside `atk/text`'s 106-across-18-files finding as
  evidence the blind spot's scale genuinely doesn't correlate with a
  directory's other characteristics.
- The two `long`-returning functions (`zip_Enparse_Stream`,
  `zip_Deparse_Stream`, §4) are a clean, concrete "M2 exists for
  exactly this" citation, sourced from real definitions both times
  rather than guessed from call-site usage.
- The `AUXMODULE` check (§7) came back negative, cleanly and
  structurally (not just empirically) — this directory's many small
  classes turned out to be a false lead for that specific sub-case,
  though it was worth checking explicitly rather than assuming either
  way, per the prompt's instruction.
- No hard stops triggered: no called-but-undefined function anywhere
  (every name resolved to a real, correctly-spelled definition), no
  header-vs-usage type disagreement requiring escalation (the two
  `long`-returning functions were resolved by sourcing the real
  definitions, not a disagreement needing a ruling), and every fix
  stayed inside this directory's own `.c`/`.h` files — no header
  outside `contrib/zip/lib` was ever edited or needed to be.
- I did not run any AUIS GUI or binary interactively, and made no
  commits, per the prompt.
