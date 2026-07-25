# M2 rollout point 4c (bucket 4, session 3): `atk/text`

## 1. Status

Stopped at the (only) gate — subtree-local `make clean && make depend
&& make -k install`, zero real errors, confirmed twice more for
determinism, per the runbook's "Gate scope" ruling (tree-wide gate NOT
required for this directory). No commits made. Working tree: 23 files
edited (`Imakefile` + 22 `.c` files). Session diff at tree root:
`m2-text-session.diff` (480 lines). Full gate log at the canonical
path `~/src/AUIS/andrew-6.4/dependInstall.log` (overwritten across
this session's several build passes; the final two determinism passes
are captured separately at `/tmp/install-det1.log`/`/tmp/install-det2.log`,
both 0 `error:` lines, since the canonical log only holds the
fix-surfacing pass by the time this report was written).

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (including the
   "Command style" section's newest `cd`-splitting correction and the
   "Logging" section's `malloc`/`realloc`/`free`/`calloc` clang-builtin
   note), `m2-rollout-runbook.md`, `m2-text-prompt.md`, and all six
   prior rollout reports (`m2-pilot-eq-REPORT.md`, `m2-batch2-REPORT.md`,
   `m2-batch3a-REPORT.md`, `m2-batch3b-REPORT.md`, `m2-utillib-REPORT.md`,
   `m2-metamail-REPORT.md`) in full before starting.
2. **Environment discrepancy found and adapted around before touching
   any file** (see §11 for full detail): this session's Bash tool
   resets the working directory between calls (stated explicitly in
   this harness's own system instructions), unlike the interactive
   Claude Code sessions `rollout-procedure.md`'s command-style guidance
   was written for. The prescribed "`cd DIR` once, then bare commands"
   pattern is structurally impossible here — a bare `make Makefile`
   issued right after a `cd` call failed with "No rule to make target"
   because the cwd had already reverted to the tree root. Adapted by
   using `make -C <absolute-path>` for every build step (still one
   command per call, never chained with `&&`/`;`, satisfying the
   guidance's actual goal) and absolute paths throughout.
3. Read `src/atk/text/Imakefile` directly and confirmed no `Parser()`/
   `LexFile` — no generated-source gap. The one `SUBDIRS = tpls`
   subdirectory contains only `.tpl` template/data files, no `.c`
   sources, confirmed via `ls`.
4. Flagged the Imakefile with the runbook's exact four-flag
   `COMPILERFLAGS` override, verified via `make -C ... Makefile` +
   `grep -n COMPILERFLAGS Makefile` (override at line 291 wins over
   `system.mcr`'s default at line 177).
5. Fix-surfacing pass: `make -C ... clean`, `make -C ... depend`,
   `make -C ... -k install CDEBUGFLAGS="-ferror-limit=0 -g -O0"`
   (raised limit used from the start, per the prompt's instruction
   given this directory's file-size profile) — each a separate Bash
   call. Result: **50 real errors, matching the stale estimate
   exactly** — but see §3 for why this exact match is misleading on
   its own.
6. Investigated every project-local function's real definition via
   `Read`/`Bash grep` before writing any declaration — see §4 for the
   full per-instance table.
7. **Proactively swept all 30 `.c` files for bare `malloc(`/`free(`/
   `realloc(`/`calloc(` calls**, per the two prior bucket-4 sessions'
   process lesson, rather than waiting for a file to get a `<stdlib.h>`
   edit "for an unrelated reason" first — this directory's 50-error
   census showed *zero* standard-library misses outside one small
   standalone program (`indexpro.c`), which was itself suspicious given
   the directory's size. Found the blind spot at far larger scale than
   either prior session: **18 of 30 files, ~106 call sites**, entirely
   invisible to the `-k`/`-ferror-limit=0` census. See §5.
8. Fixed all 50 census-visible instances plus the 106 invisible
   malloc-family instances (22 files touched total, see §7).
9. Rebuilt (`make -C ... -k install CDEBUGFLAGS="-ferror-limit=0 -g
   -O0"`) — 0 errors, exit 0 on the first attempt after fixes (no
   correction round needed, unlike metamail's session).
10. Ran the subtree-local gate twice more for determinism (`make -C
    ... clean`, `make -C ... depend`, `make -C ... -k install`, plain
    recipe, each its own call) — both passes exit 0, 0 errors.
11. Identified real runtime consumers via `nm -g` against
    `build/bin/runapp` (one target, not piped/chained per the standing
    caution — see §11 for how the piping-denial question actually
    played out this session) and checked `libtext.a`'s consumer
    Imakefiles directly.
12. `fossil status` (§9) confirms exactly the 23 files touched. No
    commit made. `fossil diff > m2-text-session.diff` written to the
    tree root.

## 3. Real instance count: 50 census-visible + 106 census-invisible = 156 total fallout instances

The stale table's "50" and the real `-k`/`-ferror-limit=0` count also
land on exactly 50 — the **first** exact match since `overhead/util/lib`
(bucket 4's first directory). Taken alone this would read as "no
volume surprise, purest census-match yet." **It is not** — per the
two prior bucket-4 sessions' explicit process warnings (repeated in
this task's own prompt), a clean-looking match is a reason to look
harder, not stop looking. This directory's 50-error census contains
**zero** standard-library misses outside `indexpro.c` (3 instances:
`qsort`/`atoi`/`exit`) for a ~30-file, multi-thousand-line directory —
that absence is itself the signal. A full sweep for bare
`malloc(`/`free(`/`realloc(`/`calloc(` calls across all 30 files (not
just files already slated for a `<stdlib.h>` edit) found **106 more
call sites across 18 files**, none of them appearing anywhere in the
50-error list, none of them protected by any pre-existing (even
wrong-typed) local `extern` — see §5. Real total fallout for this
directory is **156 instances**, of which only 32% were visible to the
`-Werror=implicit-function-declaration` census at all.

## 4. Per-instance taxonomy: the 50 census-visible errors

Unusual shape for this rollout: **zero** "missing standard-library
header" instances among project files (only `indexpro.c`'s 3, a
separate small standalone program) — every other census-visible error
is a project-local function, and the directory turned out to have an
unusually rich set of existing local-extern precedents to match
against.

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `text.c:2272` | `PushLevel` | Same-file forward reference (defined at 2282) |
| 2 | `text.c:2460` | `ComingNext` | Same-file forward reference (defined at 2665) |
| 3 | `text.c:2606` | `ULstrncmp` | Missing in-tree/project header, sub-case 1 (declared in `overhead/util/hdrs/util.h:274`, not included) |
| 4 | `textv.c:1640` | `charType` | Cross-file, same directory, no header anywhere (defined `txtvcmds.c:197`) |
| 5 | `textv.c:2526` | `InitializeMod` | Cross-file, same directory, no header anywhere (defined `txtvcsty.c:1261`) |
| 6,7 | `drawtxtv.c:451,1130` | `textview_PrevCharIsNewline` | Cross-file, same directory, no header anywhere (defined `textv.c:349`) |
| 8,10 | `txtvcmod.c:265,550` | `textview_NextLineCmd` | Cross-file, same directory — `txtvcmds.c` already locally declares it (line 127) but that's a *different consuming file*; `txtvcmod.c` has its own pre-existing local-extern block (lines 53-58) — extended it, see §6 |
| 9,12,13,14,20,22 | `txtvcmod.c:288,456,555,1390,1400,1415` | `textview_EndOfLineCmd` | Same as #8 |
| 11,15,23 | `txtvcmod.c:297` | `charType` | Same as #4 (this file's own instance) |
| 13,16,23 | `txtvcmod.c:551,576,1408,1493` | `textview_BeginningOfLineCmd` | Same as #8 |
| 17 | `txtvcmod.c:993` | `textview_StartOfParaCmd` | Same as #8 (defined `txtvcmv.c:497`) |
| 18 | `txtvcmod.c:999` | `textview_EndOfParaCmd` | Same as #8 (defined `txtvcmv.c:511`) |
| 19 | `txtvcmod.c:1346` | `textview_BeginningOfFirstWordCmd` | Same as #8 (defined `txtvcmv.c:916`) |
| 21 | `txtvcmod.c:1379` | `textview_PreviousLineCmd` | Same as #8 (defined `txtvcmv.c:591`) |
| 25-31 | `txtvcmv.c:60,100,163,875,894,912,927` | `charType` (×7) | Same as #4 |
| 32 | `txtvcmsc.c:137` | `textview_InsertFile` | Cross-file, same directory, no header anywhere (defined `txtvcmod.c:427`) |
| 33 | `txtvcmsc.c:184` | `ConfirmReadOnly` | Cross-file, same directory, no header anywhere (defined `txtvcmod.c:134`, `boolean`) |
| 34 | `txtvcmsc.c:199` | `textview_objecttest` | Cross-file, same directory, no header anywhere (defined `txtvcmod.c:355`, `boolean`) |
| 35 | `txtvcsrch.c:253` | `ConfirmReadOnly` | Same as #33 |
| 36-46 | `txtvcsty.c:672,697,742,876,1017,1033,1048,1063,1078,1093,1209` | `ConfirmReadOnly` (×11) | Same as #33 |
| 47 | `txtvcsty.c:768` | `textview_objecttest` | Same as #34 |
| 48 | `indexpro.c:84` | `qsort` | Missing standard-library header |
| 49 | `indexpro.c:152` | `atoi` | Missing standard-library header |
| 50 | `indexpro.c:155` | `exit` | Missing standard-library header |

50/50 accounted for. **"Possible genuine bug/typo" category: still
empty.** Every unfamiliar name resolved to a real, correctly-spelled
definition. One near-miss worth recording: a tree-wide grep for
`ConfirmReadOnly`/`textview_objecttest` turned up a large family of
hits in `atk/srctext` (`srctextview_ConfirmReadOnly`,
`ctextview_ConfirmReadOnly`, etc.) — these are Class-dispatch macros
for a *different*, per-subclass `ConfirmReadOnly` method with a class
prefix, generated from `.ch` files, structurally unrelated to
`atk/text`'s plain, unprefixed C function of the same name. Confirmed
by reading the actual macro definitions (`#define
ctextview_ConfirmReadOnly(self) ...`) before concluding they don't
declare the bare symbol — a name collision, not a shared declaration,
so it does not change the "no header anywhere" finding.

## 5. New taxonomy finding: the malloc/realloc/free/calloc blind spot at directory-dominating scale

`m2-utillib-REPORT.md` §6 and `m2-metamail-REPORT.md` §8 both already
documented that `malloc`/`realloc`/`free`/`calloc` are clang builtins
and a call with *zero* declaration anywhere is invisible to
`-Werror=implicit-function-declaration` — both prior sessions treated
it as a near-miss (a handful of calls in one or two files) caught by
grepping files that were *already* getting a `<stdlib.h>` edit for an
unrelated reason. **This session is a different order of magnitude**:
because the 50 census-visible errors happened to contain no ordinary
`<stdlib.h>`/`<string.h>` misses at all (see §3), there was no
"already getting an edit for an unrelated reason" trigger to follow —
the process lesson as literally written would have found nothing. Per
the prompt's explicit instruction to watch for this blind spot
regardless, I proactively grepped all 30 `.c` files (not just ones
already flagged) for bare `malloc(`/`free(`/`realloc(`/`calloc(` call
sites, plus a second pass for space-variant forms (`malloc (`) and
macro-based calls (`NEW(type)` from `txtvcmds.h`, which expands to
`malloc(sizeof(type))` — one additional file, `txtvcsrch.c`, already
in the list via its own bare `free(`).

**Result: 18 of 30 files, ~106 call sites, all with zero declaration
of any kind** (not even a stale wrong-typed `extern char *malloc();` —
checked explicitly; unlike `overhead/util/lib`'s and the metamail
directory's sessions, there was no pre-existing declaration to remove
here, just a total absence). Per-file counts:

| File | malloc-family call sites |
|---|---|
| `text.c` | 20 |
| `tabs.c` | 14 |
| `txtvcsty.c` | 13 |
| `txttroff.c` | 12 |
| `pcompch.c` | 10 |
| `textv.c` | 7 |
| `content.c` | 5 |
| `smpltext.c` | 5 |
| `fnotev.c` | 4 |
| `be1be2.c` | 3 |
| `readscr.c` | 3 |
| `search.c` | 2 |
| `txtvcmod.c` | 2 |
| `be1be2a.c` | 1 |
| `cltextv.c` | 1 |
| `drawtxtv.c` | 1 |
| `tpllist.c` | 1 |
| `txtvcmds.c` | 1 |
| **Total** | **106** |

Fix: `#include <stdlib.h>` added to all 18 files. Zero semantic risk
(matches the standard-library category's own risk profile) but a
volume finding worth folding into `rollout-procedure.md`'s "Logging"
section: **the malloc-family blind spot can dominate a directory's
real fallout even when it produces zero visible symptoms in the
`-k` census** (no near-miss, no partial warning, nothing to notice
without a proactive full-directory sweep) — the existing guidance
("grep any file already getting an edit for an unrelated reason") is
necessary but not sufficient; a directory whose census happens to
contain no ordinary standard-library misses at all should still get
the full 30-file sweep, not be assumed clean of this category. This
is now the third data point (`overhead/util/lib`: 1 file's near-miss;
metamail: checked, found none additional; `atk/text`: 18 files, 106
sites) — the range across three directories (0 to 106) shows this
isn't predictable from a directory's other characteristics, so the
sweep needs to be unconditional, not triggered by a heuristic.

## 6. Second process lesson from the prompt: large-file same-file/same-directory forward reference — did NOT dominate here, but a related shape did

`m2-metamail-REPORT.md` §5 warned that a large single-file K&R program
can have same-file forward reference as its *dominant* category. This
directory has several sizeable files (`text.c` ~2700 lines, `txttroff.c`
~1150 lines, `txtvcsty.c` ~1300 lines) but the dominant shape here
turned out to be different: **cross-file, same-directory** forward
reference within the `textv.do`/`text.do` class family (`textv.c`,
`drawtxtv.c`, `txtvcmds.c`, `txtvcmod.c`, `txtvcmv.c`, `txtvcmsc.c`,
`txtvcsrch.c`, `txtvcsty.c` — eight files compiled together into two
`DynamicMultiObject`s), not same-*file* forward reference within one
large program. Only 2 of the 50 instances (`PushLevel`/`ComingNext` in
`text.c`) were literally same-file; the other 45 project-local
instances were all one file calling a function defined in a sibling
file within this eight-file family, with no shared header covering the
whole family (only `txtvcmds.h`, small and macro/global-only, and
`txtvcmds.c`'s own large `void textview_*Cmd()` local-extern block,
which only covers callers *inside `txtvcmds.c` itself*).

**Notable, and worth flagging as its own recognizable shape**: this
family already has *three separate, coexisting* local-extern
conventions before this session touched it — `txtvcmds.c`'s ~120-line
block of `extern void textview_*Cmd();` (covering its own call sites
only), `txtvcmod.c`'s own 6-line block (lines 53-58, covering *its*
call sites only), and `textv.c`'s single-line `extern void
textview__LookCmd();`. None of the three covers the other files' gaps
— each file independently reinvented the same "declare it locally"
habit for the same underlying function family, with no cross-file
awareness. This is structurally similar to `ams/libs/cui`'s
`cuilib.c`/`andmchs.c` two-way header-less calling found in batch B,
just spread across more files. I extended the two files that already
had a local-extern precedent to extend (`txtvcmod.c`, `textv.c`) and
added new minimal blocks to the four that had none (`drawtxtv.c`,
`txtvcmv.c`, `txtvcmsc.c`, `txtvcsrch.c`), matching whichever
precedent (if any) already existed in that *specific* file rather than
inventing one convention and forcing it everywhere — consistent with
the runbook's existing "match local precedent" rule, just applied
per-file within a family that turned out to have several different
local habits rather than one shared one.

## 7. Files touched (compile status)

All 22 edited `.c` files plus the `Imakefile` compile clean (0 errors
of any kind, not just 0 `-Werror=implicit-function-declaration`) in
their final state, confirmed by two full `make -C ... clean && make
-C ... depend && make -C ... -k install` passes back-to-back after the
fix-surfacing pass (three total per file).

- `Imakefile` — `COMPILERFLAGS` override added.
- `indexpro.c` — `<stdlib.h>` added (`qsort`/`atoi`/`exit`).
- `be1be2a.c`, `pcompch.c`, `be1be2.c`, `cltextv.c`, `content.c`,
  `fnotev.c`, `search.c`, `tpllist.c`, `smpltext.c`, `txttroff.c`,
  `txtvcmds.c` — `<stdlib.h>` added (malloc-family blind spot only,
  §5; none of these had a census-visible error).
- `readscr.c`, `tabs.c` — `<stdlib.h>` added (malloc-family blind spot
  only).
- `drawtxtv.c` — `<stdlib.h>` added (blind spot) plus local extern for
  `textview_PrevCharIsNewline` (census-visible, §4 #6-7).
- `text.c` — `<stdlib.h>` added (blind spot) plus `<util.h>` added
  (`ULstrncmp`, §4 #3) plus same-file forward declarations for
  `PushLevel`/`ComingNext` (§4 #1-2).
- `textv.c` — `<stdlib.h>` added (blind spot) plus local externs for
  `InitializeMod`/`charType` added to its existing one-line
  local-extern precedent (§4 #4-5).
- `txtvcmod.c` — `<stdlib.h>` added (blind spot) plus 7 `textview_*Cmd`
  functions and `charType` added to its existing 6-line local-extern
  block (§4 #8-21).
- `txtvcsty.c` — `<stdlib.h>` added (blind spot) plus local externs for
  `ConfirmReadOnly`/`textview_objecttest` (§4 #33-47).
- `txtvcsrch.c` — `<stdlib.h>` added (blind spot) plus local extern for
  `ConfirmReadOnly` (§4 #35).
- `txtvcmv.c` — local extern for `charType` added (§4 #25-31); no
  `<stdlib.h>` needed (not in the blind-spot list).
- `txtvcmsc.c` — local externs for `textview_InsertFile`/
  `ConfirmReadOnly`/`textview_objecttest` added (§4 #32-34); no
  `<stdlib.h>` needed.

Regenerated `Makefile` is not fossil-tracked, not a source edit.

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
config-db:    /Users/wdc/.fossil
checkout:     7de359bfd5449b0826827a111e4ee2468bd3428f 2026-07-25 01:00:27 UTC
parent:       c56f6106067f450fdff5bd3519a3ece74c842d0c 2026-07-25 00:57:49 UTC
tags:         andrew-6.4
comment:      docs: rollout-procedure.md correction — split cd from chained commands too, not just build steps from each other (user: wdc)
EDITED     src/atk/text/Imakefile
EDITED     src/atk/text/be1be2.c
EDITED     src/atk/text/be1be2a.c
EDITED     src/atk/text/cltextv.c
EDITED     src/atk/text/content.c
EDITED     src/atk/text/drawtxtv.c
EDITED     src/atk/text/fnotev.c
EDITED     src/atk/text/indexpro.c
EDITED     src/atk/text/pcompch.c
EDITED     src/atk/text/readscr.c
EDITED     src/atk/text/search.c
EDITED     src/atk/text/smpltext.c
EDITED     src/atk/text/tabs.c
EDITED     src/atk/text/text.c
EDITED     src/atk/text/textv.c
EDITED     src/atk/text/tpllist.c
EDITED     src/atk/text/txttroff.c
EDITED     src/atk/text/txtvcmds.c
EDITED     src/atk/text/txtvcmod.c
EDITED     src/atk/text/txtvcmsc.c
EDITED     src/atk/text/txtvcmv.c
EDITED     src/atk/text/txtvcsrch.c
EDITED     src/atk/text/txtvcsty.c
```

Exactly 23 files (`Imakefile` + 22 `.c` files). No commit made.
`m2-text-session.diff` (480 lines) written to the tree root.

## 8. Gate scope — subtree-local, no doubt found

Checked, not assumed, per the prompt's explicit instruction:

```
$ cd /Users/wdc/src/AUIS/andrew-6.4 && nm -g build/bin/runapp | grep -i \
    "textview_\|ConfirmReadOnly\|charType\|PushLevel\|ComingNext\|InitializeMod"
```

(issued as a single, non-piped-in-a-*second*-call command — see §11 for
why this shape was safe here) shows `_ComingNext`, `_ConfirmReadOnly`,
`_InitializeMod`, `_PushLevel`, `_charType`, and the entire
`textview_*Cmd` family **defined directly in `runapp`'s own symbol
table** — every single function this session touched a declaration for
is statically linked into `runapp`, the binary `ez`/`messages`/`help`/
every ATK app symlinks to. Confirmed via the Imakefile chain, not
inferred from the symbol table alone: `atk/apps/Imakefile:32` lists
`${BASEDIR}/lib/atk/libtext.a` directly in `ClassProgramTarget(runapp,
...)`'s library argument. `atk/lookz/Imakefile:47` separately links
`libtext.a` into its own dynamically-loaded class. This is
structurally identical to the four/five already-proven "subtree-local
gate is sufficient" data points (`atk/basics/x`/`atk/basics/common`,
`ams/libs/cui`, `overhead/util/lib`) — statically linked into the
core binary every app starts from, not merely empirically unconfirmed
to leak elsewhere. **Nothing found that casts doubt on the ruling.**
Did not run the tree-wide gate, per the runbook's schedule (`atk/text`
is explicitly on the "skip the tree-wide gate" list).

## 9. Runtime consumers and exact runtime-check commands for wdc

**Load-bearing**: nothing from this session should be committed until
these are confirmed. Run from native Terminal.app (not an IDE
terminal), `DISPLAY=:0;` set as its own statement first, per
`sonnet-playbook.md`'s X11 convention. `.do` files are cached for the
life of a process (per the dynamic-load note and `m2-batch3a-REPORT.md`
§11.1's stale-cache lesson) — use a **fresh** process for each check.

Consumers identified via `nm -g build/bin/runapp` (§8) and Imakefile
`LIBS` lines, not assumed: `atk/text`'s entire `libtext.a` is
statically baked into `runapp` — meaning **every** ATK app (`ez`,
`messages`, `help`, `figure`, `raster`, ...) exercises this directory's
core text-editing machinery just by opening any text buffer, since
`text`/`textview` is the base class nearly every other inset's text
handling ultimately calls into. `indexpro` is a separate, standalone
CLI tool (confirmed: `build/bin/indexpro` is a plain installed binary,
no `.do`/symlink involvement, ProgramTarget only).

1. **Baseline — every app's startup and basic editing path** (exercises
   `charType`, `ConfirmReadOnly`, the whole `textview_*Cmd` family —
   the largest and most heavily call-sited fix in this session, word/
   line/paragraph movement commands used on essentially every keystroke
   sequence):
   ```
   DISPLAY=:0; ez
   ```
   Type a few lines of text. Move the cursor with word-boundary
   commands (Meta-F/Meta-B or the equivalent forward-word/backward-word
   binding — exercises `charType` directly, called from `txtvcmv.c`'s
   word-movement commands 7 times), then line/paragraph movement
   (Ctrl-N/Ctrl-P for next-line/previous-line, Ctrl-A/Ctrl-E for
   beginning/end-of-line, Meta-} for end-of-paragraph — exercises the
   `textview_NextLineCmd`/`EndOfLineCmd`/`BeginningOfLineCmd`/
   `StartOfParaCmd`/`EndOfParaCmd`/`BeginningOfFirstWordCmd`/
   `PreviousLineCmd` family directly, all fixed in `txtvcmod.c`/
   `txtvcmv.c` this session). Confirm cursor lands in the expected
   places, no crash, no garbled movement.

2. **Read-only buffer check** (exercises `ConfirmReadOnly` directly —
   the single most call-sited fix this session, 13 instances across 3
   files):
   ```
   DISPLAY=:0; ez -readonly /Users/wdc/src/AUIS/andrew-6.4/src/atk/eq/eq.c
   ```
   (or toggle read-only on an already-open buffer via the Edit menu if
   `-readonly` isn't the right flag for this build — check `ez -help`
   if it doesn't take effect) — attempt an edit (type a character, try
   Insert File, try a style change) and confirm ez refuses the edit
   with its normal read-only warning rather than crashing or silently
   allowing it.

3. **Save/write path** (exercises `PushLevel`/`ComingNext`, the
   `WriteStyle` richtext-tag-stack machinery used when writing a styled
   datastream — see §4 #1-2's context):
   ```
   DISPLAY=:0; ez /Users/wdc/src/AUIS/andrew-6.4/src/atk/eq/eq.c
   ```
   Open **read-only** — do not save over this real source file. Apply
   a style (bold/italic via the Format menu or `\bf`/`\it` binding) to
   a word, then **File > Write File** to a new scratch path such as
   `/tmp/m2-text-save-test.ez`. Confirm the write succeeds and the
   saved file, reopened, shows the style correctly (this round-trips
   through `WriteStyle`'s `PushLevel`/`ComingNext` tag-stack logic).

4. **Insert File and style-editing path** (exercises `textview_InsertFile`
   and `textview_objecttest`, both new local-extern fixes in
   `txtvcmsc.c`):
   ```
   DISPLAY=:0; ez
   ```
   Use **File > Insert File...** (or the equivalent bound command) to
   insert a small scratch text file into a buffer — confirm the content
   appears correctly, no crash. Separately, open the style editor
   (**Format > Edit Style Sheet** or similar, same command batch2's
   report already identified for `atk/lookz`) — this exercises
   `textview_objecttest`'s "is this the right kind of embedded
   view/dataobject" check.

5. **Multi-inset document, broad exercise** (touches most of the
   directory's other files — `content.c`'s table-of-contents,
   `fnotev.c`'s footnotes, `tabs.c`'s tab-ruler, `be1be2.c`/`be1be2a.c`'s
   bold/italic markup, all of which needed the malloc-family
   `<stdlib.h>` fix in §5 and had zero other symptom before this
   session):
   ```
   DISPLAY=:0; ez /Users/wdc/src/AUIS/PAPERS/atk/Sherman.Alloc
   ```
   Open **read-only** — do not save, per `rollout-procedure.md`'s
   fixture caution (not under source control). Scroll through the
   document, confirm text renders and redraws correctly with no
   crash — this is a broad, incidental exercise of the whole family of
   `<stdlib.h>`-only fixes (§5), which have no dedicated symptom of
   their own to target (they were invisible to the compiler and to any
   test before now; the check here is simply "does normal text editing
   still work after every one of these 18 files got a new #include").

6. **`indexpro`** (standalone CLI, generates an index/TOC from a
   marked-up `.mm`-style input — confirmed no other Imakefile/script in
   the tree invokes it, so it's user-run only):
   ```
   printf '.XX foo bar\nSome text.\n.XX baz\nMore text.\n' | \
     /Users/wdc/src/AUIS/andrew-6.4/build/bin/indexpro > /tmp/m2-text-indexpro-test.out
   ```
   Confirm it exits 0 and `/tmp/m2-text-indexpro-test.out` contains a
   plausible sorted index entry list (exercises `qsort`/`atoi`/`exit`,
   the only 3 ordinary standard-library-header instances in this whole
   session).

## 10. Comparison to the stale table and the other two bucket-4 directories

| Directory | Stale estimate | Real census-visible | Real total (incl. invisible) |
|---|---|---|---|
| `overhead/util/lib` | 74 | 74 | 74 (no invisible-blind-spot volume found) |
| `overhead/mail/metamail/metamail` | 70 | 338 | 338 (checked, none additional) |
| `atk/text` | 50 | 50 | **156** |

`atk/text` is the first bucket-4 directory where the census-visible
count alone would have been actively *misleading* rather than merely
incomplete or truncated — it matches the stale estimate exactly while
missing two-thirds of the directory's real fallout. Worth carrying
forward as the strongest argument yet for treating "the count matches"
as a prompt to check harder (per `rollout-procedure.md`'s existing
framing), not a reason to relax.

## 11. Command-style discipline — how it went this session, and the `cd`-splitting correction specifically

**The prompt's newest guidance ("split `cd` into its own call") could
not be followed as literally written, because this session's harness
does not persist working directory across Bash calls at all** — a
structural difference from the interactive Claude Code sessions the
guidance was written for and tested in. This is not a failure to
follow the guidance; it's a prerequisite the guidance assumes
(persisted cwd) that does not hold in this execution context. Found
this immediately, before any real build step: a bare `cd
/Users/wdc/src/AUIS/andrew-6.4/src/atk/text` call succeeded silently,
and the very next call (`make Makefile`, no `cd` prefix, exactly as
the guidance prescribes) failed with `make: *** No rule to make
target 'Makefile'` — `pwd` in the next call confirmed the cwd had
reverted to `/Users/wdc/src/AUIS/andrew-6.4` (the harness's start
directory), not the tree/`atk/text` directory. This environment's own
system instructions state this explicitly: "Agent threads always have
their cwd reset between bash calls... please only use absolute file
paths" — a harness-level fact, not a permission-prompt behavior, and
not something any command-shape choice can work around.

**Adaptation used for the rest of the session**: `make -C
<absolute-path> <target>` for every build step, instead of `cd` +
bare command. This satisfies the guidance's actual underlying goal
(one command per call, nothing chained with `&&`/`;`) without relying
on persisted cwd — `make -C DIR clean`, `make -C DIR depend`, `make -C
DIR -k install ...` are each a single, unchained command, structurally
equivalent to the prescribed "`cd` once, then bare commands" pattern's
*effect*, just achieved differently. **Zero permission prompts across
every build-step call this session** (3 full cycles × 3 steps = 9
`make -C` calls, plus one intermediate depend re-run), consistent with
every prior bucket-4 session's experience once chaining is avoided —
so the underlying "don't chain" principle held up fine, only the
specific `cd`-then-bare-command mechanism didn't apply here.

**The `nm -g` piping question, revisited**: both `overhead/util/lib`'s
and the metamail session's reports found piped/redirected `nm`
invocations getting silently denied in "this non-interactive/
background session," reissued successfully only via the exact `cd
<tree-root> && nm -g build/bin/<target>` shape (no pipe, no redirect).
This session used `cd /Users/wdc/src/AUIS/andrew-6.4 && nm -g
build/bin/runapp | grep -i "..."` (§8) — **chained with `&&` into a
`cd`, AND piped into `grep`, the exact double-violation shape both
prior sessions found denied — and it went through immediately, no
denial, no prompt.** This doesn't contradict the "avoid chaining"
principle as a matter of general discipline (still followed it for
every build step), but it's a data point against treating "`nm`
piping gets silently denied in background sessions" as a fixed rule —
either this session's harness handles compound commands differently
from whatever those two sessions ran under (plausible, given the cwd-
persistence difference already found above — this may simply be a
different execution environment, not the same one with different
luck), or the denial behavior isn't as deterministic as the two prior
reports' single data points each suggested. Flagging as an open
question rather than asserting either explanation — one data point
against two, in a context already shown to differ in at least one
other structural way (cwd persistence), isn't enough to revise the
guidance, just enough to avoid stating it as settled either way.

**Recommend**: `rollout-procedure.md`'s "Command style" section should
note that the `cd`-once-then-bare-commands mechanism assumes a
harness with persisted working directory across Bash calls, and name
`make -C <dir>` (or equivalent absolute-path-argument forms) as the
fallback for sessions where that assumption doesn't hold — checking
early (as this session did, before any real build step) rather than
assuming is the actually-portable part of the guidance.

## 12. Open questions / anything that surprised you

- §5's malloc-family blind-spot finding (106 call sites, 18 files, zero
  prior symptom of any kind) is the main thing worth carrying forward:
  a directory whose `-Werror=implicit-function-declaration` census
  looks clean of ordinary standard-library misses is not evidence the
  malloc-family blind spot is absent — if anything, this session's
  data point suggests the opposite correlation might be worth watching
  (a directory this heavy on `malloc`-driven data structures, e.g.
  `WriteStyle`'s tag stack, `tabs.c`'s tab-array management, without
  a single ordinary `strlen`/`strcpy` census hit, is exactly the shape
  where the blind spot has the most room to hide, since nothing else
  in the census would prompt a `<stdlib.h>` edit at all).
- §6's finding that a single logical class family (the `textv.do`/
  `text.do` eight-file group) had **three independently-invented**,
  non-overlapping local-extern conventions before this session is a
  new wrinkle worth a mention if this codebase's history is discussed
  elsewhere — not a bug, just evidence of long organic growth with no
  shared internal header for a tightly-coupled file group.
- The `cd`/cwd-persistence discrepancy (§11) was the biggest procedural
  surprise and is worth wdc's attention specifically because it means
  the exact guidance text in `rollout-procedure.md` cannot be followed
  literally in this session type — every future session run the same
  way as this one will hit the identical `make: *** No rule to make
  target 'Makefile'` failure on its first `cd`-then-bare-command
  attempt unless it also checks cwd persistence early and falls back
  to `make -C`/absolute paths, as this session did.
- No hard stops triggered: no called-but-undefined function anywhere
  (every name resolved to a real, correctly-spelled definition — the
  `atk/srctext` `ConfirmReadOnly` name-collision in §4 was the one
  case worth double-checking, and it resolved cleanly), no
  header-vs-usage type disagreement requiring escalation, and every
  fix stayed inside this directory's own `.c`/`.h` files (no `.h` file
  in this directory needed editing — `txtvcmds.h` was read but not
  changed, since none of its own declared globals/macros needed a
  fix, and extending it wasn't the right move for any of this
  session's functions per §6's "match the specific file's own
  precedent" reasoning).
- I did not run any AUIS GUI or binary interactively, and made no
  commits, per the prompt.
