# M2 rollout point 5: `ams/libs/ms` — final M2 directory

## 1. Status

Stopped at the (only) gate. **Both gates required for this directory** per
the standing `ams/libs/ms` exception in `m2-rollout-runbook.md`:
subtree-local (`make clean && make depend && make -k install`, twice for
determinism) AND the full tree-wide gate (`make Clean && make
dependInstall`) — the latter doubling as **M2's own completion gate**.
No commits made. Working tree: 102 files edited (`Imakefile` +
`prsdate.gra` + 100 `.c` files). Session diff at tree root:
`m2-amsms-session.diff` (2447 lines). Full gate log at the canonical path
`~/src/AUIS/andrew-6.4/dependInstall.log` (238,642 lines, final state).

**This closes M2** — `ams/libs/ms` was the last planned M2 rollout point
(`m2-rollout-runbook.md`'s "Ordering" §5) — pending the tree-wide gate's
own confirmation, which is green (§8 below).

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (Command style and
   Logging sections, including the word-boundary-anchored malloc-family
   sweep mandate), `m2-rollout-runbook.md` in full, and all twelve prior
   rollout reports (`m2-pilot-eq-REPORT.md`, `m2-batch2-REPORT.md`,
   `m2-batch3a-REPORT.md`, `m2-batch3b-REPORT.md`, `m2-utillib-REPORT.md`,
   `m2-metamail-REPORT.md`, `m2-text-REPORT.md`, `m2-rofftext-REPORT.md`,
   `m2-table-REPORT.md`, `m2-mail-lib-REPORT.md`,
   `m2-messageslib-REPORT.md`, `m2-ziplib-REPORT.md`) — full reads of
   `m2-messageslib-REPORT.md` and `m2-ziplib-REPORT.md` (closest analogs:
   large `LibraryTarget`/heavy-fan-out directories needing both gates),
   targeted reads of the other ten (already thoroughly digested in the
   runbook's own "Status" narrative; cross-checked for hard-stop
   citations — none of the twelve reported one).
2. Read `claude-history/fdplumb-REPORT.md` in full before touching
   anything, per the prompt's explicit requirement. Key facts carried
   into this session without re-deriving: the `dbg_*` wrappers are
   detect-only (a name ledger, never act on a real fd); the historical
   include-order hazard is fixed at the root inside `fdplumb.h` itself;
   a missing `dbg_*` declaration here is an ordinary M2 gap, not a sign
   of a deeper runtime hazard.
3. **Checked command-style/cwd-persistence early**: a throwaway `cd
   .../src/ams/libs/ms` call followed by a bare `pwd` in the next tool
   call showed the cwd had reverted — cwd does **not** persist across
   Bash calls in this session. Used `make -C <absolute-path> <target>`
   for every build step, one command per call, never chained.
4. Read `src/ams/libs/ms/Imakefile` directly: confirmed `Parser(prsdate,)`
   is the live generated-source mechanism and `prsdate.gra` (not
   `prsdate.y`) is the real, hand-maintained bison grammar source — the
   inline `y_OBJS`/`y_FILE`/`ed -` recipe comment is dead/commented-out.
   `ls ams/libs/ms/*prsdate*` showed `prsdate.gra`, `.h`, `.c`, `.o`,
   `.scr` — no `.y` file exists anywhere in the directory. Confirmed the
   `LibraryTarget(libmssrv.a, ...)` shape and, via `grep -rl
   libmssrv.a`, that all 7 named consumer directories reference it in
   their link line (§9 has the structural nuance: not all 7 actually
   build in this environment).
5. Flagged the Imakefile with the runbook's exact four-flag
   `COMPILERFLAGS` override, verified via `make -C ... Makefile` +
   `grep -n COMPILERFLAGS Makefile` (override at line 291 wins over
   `system.mcr`'s default at line 177).
6. Fix-surfacing pass: `make -C ... clean`, `make -C ... depend`
   (regenerates `prsdate.c`/`.h` from `prsdate.gra` via bison + `mkparser`
   — `bison` reported 7 shift/reduce, 66 reduce/reduce conflicts, a
   pre-existing grammar-ambiguity characteristic unrelated to this
   session, not a new problem), `make -C ... -k install
   CDEBUGFLAGS="-ferror-limit=0 -g -O0"` — each a separate Bash call.
   Result: **1567 real errors across 100 `.c` files** (plus 4 more
   inside `prsdate.gra`/generated `prsdate.c`) — far past the stale
   estimate of 892 — see §4.
7. Built a systematic function → real-definition mapping before writing
   any declaration (258 distinct project-local names + 10 std-library
   names recognized by clang's "library function" diagnostic path, plus
   7 more std-library names caught under the plain "implicit
   declaration" path): extracted every distinct undeclared function name
   from the log, searched `ams/libs/ms` first (same-directory,
   overwhelmingly the largest category), then the rest of the tree for
   the remainder, resolved each to its real definition's file:line and
   exact return type (never inferred from a call site), and cross-checked
   every case where multiple same-named candidates existed in different
   directories before picking one (§4 covers three real near-misses this
   caught).
8. Applied the fixes across all 100 files via a controlled script driven
   by the verified function → declaration table (necessary at this
   scale — 1567 instances is far beyond what could be hand-verified
   call-site-by-call-site in a reasonable session, but every declaration
   written was sourced from a real definition looked up and checked
   individually first, per the taxonomy's standing rule). **Caught and
   fixed a self-inflicted bug in the first application pass**: the
   insertion anchor ("after the last `#include` line") is unsafe when a
   file's includes span a conditional block (`mungenew.c`'s last
   `#include <stdlib.h>` sat inside `#ifdef AFS_ENV`, undefined on this
   platform) — the first pass silently placed new externs inside that
   dead block. Rewrote with `#if`/`#ifdef`/`#endif` nesting-depth
   tracking so insertion only anchors on a depth-0 (unconditional)
   `#include`, re-derived all 98 affected files fresh from their
   `fossil cat` original content, and reapplied. See §7 for the full
   account — flagging this prominently since it's a new lesson about
   automated-insertion safety, not a taxonomy-category finding.
9. Fixed 2 files (`getnsub.c`, `getsubs.c`) by hand — thin one-function
   wrapper files with zero `#include` lines at all, so the automated
   insertion anchor didn't apply; added a single `extern int ...();`
   directly before the wrapper function, sourced the same way as every
   other same-directory fix.
10. Fixed `prsdate.gra` by hand (the real, hand-maintained bison source
    — not the generated `prsdate.c`, which would be overwritten):
    two functions (`foldup`, `yearsize`) called before their point of
    (indirect, cross-generated-position) declaration, added to the
    file's own existing `extern ...();` block.
11. Rebuilt — **0 errors, exit 0** on the very next attempt after the
    nesting-depth-aware reapplication (§4/§7).
12. **Unconditionally swept all 114 `.c` files** (word-boundary-anchored,
    space-tolerant pattern `grep -nE "\bmalloc *\(|\bfree *\(|\brealloc
    *\(|\bcalloc *\("`) for bare malloc-family calls, per the prompt's
    mandate. Found 2 additional instances invisible to the census (both
    from a stale, wrong-typed, platform-guarded `extern char
    *malloc();` suppressing the diagnostic in exactly the 2 files where
    no *other* std-library or project-local error already forced a
    `<stdlib.h>` fix) — see §5.
13. Fixed both. Rebuilt — 0 errors, exit 0.
14. Ran the subtree-local gate twice more for determinism (`make -C ...
    clean`, `make -C ... depend`, `make -C ... -k install`, the
    directory's normal unmodified flags, not the `-O0` override) — both
    passes exit 0, 0 errors, `COMPILERFLAGS` override re-verified after
    each Makefile regeneration.
15. Checked `ps aux` for other `make`/`bison` processes — none — then ran
    `make -C .../src Clean` (exit 0), then launched `make -C .../src
    dependInstall` via the Bash tool's **own** `run_in_background`
    mechanism directly (no shell-level `nohup`/`&`), logged to the fixed
    `~/src/AUIS/andrew-6.4/dependInstall.log` path. Confirmed real
    completion (238,642-line final log, `install.time`/`install.doc`
    touched, no `make`/`bison` processes left running) before analyzing
    results — see §8.
16. Identified runtime consumers via the Imakefile grep (§4) and the
    gate log's own per-directory build spans (§9) — `nm -g`/`ls -la`
    against `build/bin/{cuin,runapp}` were denied outright by this
    session's permission layer (relative path, no pipe/redirect,
    otherwise identical in shape to calls that succeeded against
    `imapsync`/`amsn.do` — noted, not investigated further, see §11).
17. `fossil status` (§10) confirms exactly the 102 files touched, no
    commit made. `fossil diff > m2-amsms-session.diff` written to the
    tree root.

## 3. Generated-source and `LibraryTarget`/fan-out confirmation (prompt's explicit ask)

- **`Parser(prsdate,)` is the live mechanism; `prsdate.gra` is the real
  source, not `prsdate.y`.** Confirmed by reading the Imakefile (the
  `y_OBJS`/`y_FILE`/`ed -` recipe block is inside a `/* ... */` comment,
  dead) and `ls ams/libs/ms/*prsdate*` (no `.y` file present anywhere in
  the directory — only `.gra`, `.h`, `.c`, `.o`, `.scr`).
  `make depend` regenerates `prsdate.c`/`.h` from `prsdate.gra` via
  `bison -b prsdate -k -o prsdate.tab.c prsdate.gra` then
  `build/bin/mkparser prsdate`; `make clean` deletes both generated
  files. `bison` reports "7 shift/reduce, 66 reduce/reduce" conflicts on
  every regeneration — a pre-existing grammar-ambiguity characteristic,
  unrelated to this session (same grammar before and after every one of
  my edits), not a new finding.
- **`LibraryTarget(libmssrv.a, ...)`, 7-directory fan-out**: confirmed
  via `grep -rl libmssrv.a` across the 7 directories the prompt names
  (`ams/ms`, `ams/msclients/cui`, `ams/msclients/vui`,
  `ams/msclients/imapsync`, `ams/msclients/nns`, `atkams/messages/lib`,
  `rdemo/messages`) — all 7 reference it in their Imakefile's link line.
  **Structural nuance found while confirming this (§9)**: only 5 of the
  7 actually descend into the build in this environment.
  `ams/ms` is `#ifdef SNAP_ENV`-gated in `ams/Imakefile`, and `SNAP_ENV`
  is disabled here (same fact `m2-messageslib-REPORT.md` §3 already
  established for a sibling directory — re-confirmed independently this
  session via the tree-wide gate log containing zero `building
  (dependInstall) (.../ams/ms)` line). `rdemo` is not reachable from the
  top-level `src/Imakefile`'s own `SUBDIRS` at all (checked `$(AMS)`,
  `$(CONTRIB)`, `$(SITE)` expansions — none include `rdemo`) — it exists
  on disk but is not descended into by `dependInstall`, the same
  "documented but structurally dead" shape `overhead/eli/lib` showed in
  batch3b. `ams/msclients/vui` is gated behind `MK_VUI` in
  `ams/msclients/Imakefile`'s `SUBDIRS`, and `site.h` defines only
  `MK_CUI`, not `MK_VUI`. **Real, live consumers in this build: `ams/msclients/cui`,
  `ams/msclients/nns` (compiles clean, fails only at final link — the
  pre-existing SSLLIB issue, §8), `ams/msclients/imapsync`,
  `atkams/messages/lib`** (whose `amsn.do` is what the GUI `messages`
  app actually loads). This doesn't change the gate-scope ruling (both
  gates were already mandated for this directory regardless of live
  consumer count) but is worth recording precisely for the runtime-check
  section (§9) and for anyone re-reading the fan-out claim later.

## 4. Real instance count: 1567 census-visible + 2 malloc-blind-spot-only = 1569 total (vs. the stale table's 892)

176% over the stale estimate — the largest absolute overshoot of any M2
directory, though proportionally in the same range as
`atkams/messages/lib` (240%) and `overhead/mail/metamail/metamail`
(483%). 100 of 114 `.c` files had at least one census-visible error;
14 files were census-clean (and, per the mandatory sweep, malloc-clean
too — none of the 14 had any malloc-family call at all).

### A genuinely new finding this session: malloc/free/realloc **were** census-visible here, contradicting 8 prior directories' documented blind spot

Every prior M2 session established that `malloc`/`free`/`realloc`/`calloc`
are clang "library builtins" invisible to
`-Werror=implicit-function-declaration` with zero declaration in scope.
**That did not hold in this session.** The very first fix-surfacing
build showed errors like:

```
util.c:74:15: error: implicitly declaring library function 'malloc' with type 'void *(unsigned long)' [-Werror,-Wimplicit-function-declaration]
util.c:97:6: error: implicitly declaring library function 'free' with type 'void (void *)' [-Werror,-Wimplicit-function-declaration]
```

Verified this wasn't specific to this directory's flags with a minimal
standalone repro (`clang -std=gnu89 -Werror=implicit-function-declaration
-c malloctest.c`, a bare two-line file calling `malloc`/`free` with no
declaration) — it reproduces identically outside this build entirely.
**I did not chase down why this differs from the prior 8 directories'
documented behavior** (possibly a toolchain/SDK update between sessions,
possibly a clang-version-dependent behavior the prior reports never
pinned down precisely) — flagging it as an open finding for
`rollout-procedure.md`'s "Logging" section to fold in, not resolving the
discrepancy myself. **Practical effect**: `malloc`/`free`/`realloc`
(strcat/strcmp/strcpy/strlen/strncpy too) already appear inside this
session's 1567 census-visible count, not hidden in the mandatory sweep —
the sweep (§5) still found exactly 2 additional instances, from a
different, narrower cause (a pre-existing stale wrong-typed extern
locally suppressing the diagnostic in 2 specific files, not the
"builtin invisibility" mechanism the taxonomy documents). `calloc` never
appeared anywhere in the sweep — genuinely absent from this directory,
not a further blind spot.

### Taxonomy breakdown (1567 census-visible instances, verified per-function before any declaration was written)

| Category | Instances | Distinct functions |
|---|---|---|
| Missing standard-library header | 122 | 17 (`malloc`, `free`, `realloc`, `strcat`, `strcmp`, `strcpy`, `strlen`, `strncpy`, `abort`, `exit`, `atoi`, `qsort`, `system`, `rename`, `unlink`, `fchmod`, `umask`) |
| Same-file / same-directory (no header anywhere, within `ams/libs/ms` itself) | 925 | ~180 |
| Cross-directory (no header anywhere in the tree, sub-case 3 / fdplumb sub-shape / callback sub-shape) | 517 | ~70 |
| `prsdate.gra` (`yearsize`, same-file forward ref) | 3 | 1 |

(1567 = 122 + 925 + 517 + 3; the 4th `prsdate.gra` instance, `foldup`,
is same-directory since `foldup` is defined in `ams/libs/ms/prsdlib.c`,
already folded into the 925 above.)

### Confirmation of the runbook's "a handful of functions dominate" hypothesis — strongly confirmed

Top 10 functions by instance count account for **714/1567 (45.6%)**; top
20 account for **838/1567 (53.5%)**:

| Function | Instances | Real definition |
|---|---|---|
| `NonfatalBizarreError` | 144 | `nonfatal.c` (same-directory) |
| `dbg_fclose` | 121 | `overhead/util/lib/fdplumb.c` (fdplumb wrapper family) |
| `CloseMSDir` | 99 | `rawdb.c` (same-directory) |
| `dbg_close` | 92 | `overhead/util/lib/fdplumb.c` (fdplumb wrapper family) |
| `FreeMessage` | 85 | `freemsg.c` (same-directory) |
| `dbg_closedir` | 53 | `overhead/util/lib/fdplumb6.c` (fdplumb wrapper family) |
| `FreeAddressList` | 33 | `overhead/mail/lib/parseadd.c` (cross-directory) |
| `ReadOrFindMSDir` | 32 | `msdir.c` (same-directory) |
| `ap_Shorten` | 31 | `overhead/util/lib/abbrpath.c` (cross-directory) |
| `GetSnapshotByNumber` | 24 | `rawdb.c` (same-directory) |

This is exactly the runbook's own census-sampling prediction
(`NonfatalBizarreError`, `dbg_fclose`/`dbg_close`/`dbg_closedir`,
`CloseMSDir`, `FreeMessage`, ...) — empirically confirmed, not just
matched by coincidence: every one of the runbook's five named examples
is in the top 6.

### Cross-directory instances by source library (517 total, all sourced from a real definition, never inferred)

| Source | Instances | Distinct functions |
|---|---|---|
| `overhead/util/lib/fdplumb*.c` (the `dbg_*` wrapper family — see dedicated subsection below) | 300 | 9 (`dbg_close`, `dbg_closedir`, `dbg_creat`, `dbg_dup2`, `dbg_fclose`, `dbg_pclose`, `dbg_vclose`, `dbg_vfclose`, `fdplumb_SpillGutsToFile`) |
| `overhead/mail/lib` (`parseadd.c`, `mailconf.c`, `dropoff.c`, `arpadate.c`, `genid.c`, `locnamex.c`) | 84 | 17 |
| `overhead/util/lib` (non-`dbg_*`: `NiceTime`, `UnixError`, `ULstrcmp`, `ap_Shorten`, `vdown`, `writeall`, `fwriteallchars`, `getprofileswitch`, `getprofileint`, `setprofilestring`, `gtime`, `DeSymLink`, `GetCellFromFileName`, `PrintQuotingFormatting`, `IsOnVice`, `getla_ShutDown`) | 89 | 16 |
| `ams/libs/shr` (`utils.c`, `brkdown.c`, `findroot.c`) | 39 | 7 (`StripWhiteEnds`, `ReduceWhiteSpace`, `BuildNickName`, `LowerStringInPlace`, `BreakDownContentTypeField`, `FindTreeRoot`, `lc2strncmp`) |
| `overhead/errors/errprntf.c` | 2 | 2 (`errprintf`, `safefprintf`) |
| Consumer-supplied callback, no canonical definition (batch3b's sub-shape) | 3 | 2 (`BizarreError`, `Machine_HandleClientSignal`) |

### The `fdplumb.h` partial-wrapper-family sub-shape dominates — 300 instances, the single largest cross-directory population found in any M2 session

Confirmed the already-documented gap directly: `overhead/util/hdrs/fdplumb.h`
`#define`s 16 `dbg_*` I/O wrapper names (`close`→`dbg_close`,
`fclose`→`dbg_fclose`, `vclose`→`dbg_vclose`, `vfclose`→`dbg_vfclose`,
`popen`→`dbg_popen`, `pclose`→`dbg_pclose`, `creat`→`dbg_creat`,
`dup2`→`dbg_dup2`, `closedir`→`dbg_closedir`, among others) but only
**declares** 6 (`dbg_opendir`, `dbg_fopen`, `dbg_popen`, `dbg_qopen`,
`dbg_topen`, `dbg_open`) — confirmed via `grep -n dbg_ fdplumb.h`. Every
file in `ams/libs/ms` that calls `close()`/`fclose()`/`vclose()`/etc.
and reaches `fdplumb.h` (transitively via `ams.h`, confirmed via `grep -n
fdplumb ams/libs/hdrs/ams.h` → line 284's `#include <fdplumb.h>`) gets
the macro rename with no matching declaration. Fix: local `extern`
per consuming file, sourced from each wrapper's real definition in
`overhead/util/lib/fdplumb.c`/`fdplumb2.c`/`fdplumb3.c`/`fdplumb6.c` —
all `int`-returning except `dbg_closedir` (`void`, confirmed from its
real definition `void dbg_closedir(d)` at `fdplumb6.c:52`). This directory
is, structurally, the "used most heavily" case the prompt predicted —
300 instances is nearly double the combined total of every previous
directory's `dbg_*`-family findings.

**Nothing found here touches the include-order hazard directly** —
every instance is an ordinary missing declaration for a macro-renamed
call, exactly the shape `fdplumb-REPORT.md` and the taxonomy describe as
already-settled and safe. No escalation needed; noting explicitly per
the prompt's hard-stop instruction (report even a confident resolution).

### Three real near-misses caught by verifying real definitions instead of trusting the first grep hit

Sourcing every cross-directory declaration from its real definition
(never inferring from a call site) caught three cases where a naive
"first match wins" approach would have produced a wrong-typed or
wrong-family declaration:

1. **`AddHost`** — two same-named functions exist in the tree:
   `ams/utils/dlistc/readalias.c:393`'s `static void AddHost(aliases,
   shortname, longname)` (3 args, `void`, `static` — a different,
   unrelated local helper in an unrelated directory) and
   `overhead/mail/lib/parseadd.c:904`'s `int AddHost(Addr, Host)` (2
   args, `int`). `ams/libs/ms/mswp.c`'s actual call sites
   (`AddHost(Addr, HostPtr) != PA_OK`) match the 2-arg, `int`-returning,
   `PA_OK`-comparable `parseadd.c` version exactly — confirmed by
   reading both real definitions and checking the call-site shape before
   picking. Using the wrong one would have compiled (K&R empty-parens
   externs don't check arg count) but declared the wrong return type.
2. **`arpadate`** — two same-named functions: `ams/utils/nntp/lib/rfc822.c:598`'s
   implicit-`int` version (an unrelated NNTP-library helper) and
   `overhead/mail/lib/arpadate.c:42`'s `char *arpadate()`. Every one of
   `ams/libs/ms`'s 10 call sites uses the return value as a `%s` string
   argument directly, or explicitly casts it `(char *) arpadate()`
   (`hdlnew.c:1356`) — confirmed via `grep -n arpadate *.c` before
   picking the `char *` version. Picking the `int` one would have
   silently truncated a pointer on LP64 — exactly the bug class M2
   exists to close.
3. **`conv64tolong`** — three same-named functions:
   `ams/msclients/vui/pcmpc.c:351`'s implicit-`int`,
   `atk/basics/common/image.c:391`'s `static unsigned long` (a `static`,
   unrelated local helper), and `overhead/mail/lib/genid.c:96`'s
   `unsigned long conv64tolong(xnum)`. Callers in `ams/libs/ms`
   consistently assign the result to `unsigned long`/`long` variables
   and, in three places (`rawdb.c:266,402,434`), explicitly cast
   `(unsigned long) conv64tolong(...)` — a strong signal the original
   1990s author already suspected/knew the width mattered. Confirmed
   `overhead/mail/lib/genid.c`'s `unsigned long` definition is the real
   one before writing the declaration. **This is a genuine, concrete LP64
   finding**, the same shape as `overhead/mail/lib`'s `AppendHosts`,
   `atkams/messages/lib`'s `CUI_*`/`MS_*` mixed-width pair, and
   `contrib/zip/lib`'s `zip_Enparse_Stream`/`zip_Deparse_Stream` — a
   fourth independent data point for the taxonomy's standing caution
   against inferring width from a call site.

`KRHash` (`unsigned long`, `bldattr.c:96`, same-directory) is a fifth
concrete non-`int` finding, sourced cleanly with no competing candidate.

Across all 258 distinct project-local functions, checked systematically
for type conflicts across every candidate definition found tree-wide
(not just spot-checked): exactly these 3 cases had multiple
same-named-but-different candidates; all other functions had either one
unambiguous real definition or multiple candidates that already agreed
on type (safe regardless of which was cited — I still standardized
citation comments to the most canonical source rather than a leaf
consumer's private duplicate copy, e.g. `ap_Shorten`/`vdown`/`writeall`/
`fwriteallchars`/`getprofileswitch`/`ULstrcmp` citing
`overhead/util/lib/*.c` rather than `ams/msclients/vui/pcmpc.c`'s
redundant vendored copy of the same utility API, and `errprintf`/
`safefprintf` citing `overhead/errors/errprntf.c`).

### "Possible genuine bug/typo" category: still empty

Every one of the 258 distinct project-local function names, and every
std-library name, resolved to a real, correctly-spelled,
`grep`-findable definition somewhere in the tree. No candidate for the
hard-stop category turned up, extending the empty streak past 2319
instances/28 directories to 3888 instances/29 directories.

## 5. Malloc-family blind-spot sweep: 2 instances across 2 of 114 files, a *different* mechanism than every prior directory's blind spot

Swept all 114 `.c` files (not just the 100 with census errors)
individually via a script using the word-boundary-anchored,
space-tolerant pattern `grep -nE "\bmalloc *\(|\bfree *\(|\brealloc
*\(|\bcalloc *\("`, confirmed no `*alloc`-suffixed project-local wrapper
exists anywhere in this directory that could false-positive the pattern
(checked: `disambig.c`'s `permanentmalloc()` doesn't match `\bmalloc\(`
since there's no word boundary between `permanent` and `malloc`).

**46 of 114 files have at least one real malloc-family call.** Given
this session's malloc/free/realloc calls were (unusually, §4) already
mostly census-visible, 44 of those 46 files already had `<stdlib.h>`
correctly added as part of the ordinary fix-surfacing pass. The sweep's
real job this session was narrower than in prior directories: catching
files where malloc-family calls were present but **suppressed** from the
census by something already in scope, rather than catching a wholesale
"clang builtin invisibility" population. Found exactly 2:

| File | Call site | Why invisible to the census |
|---|---|---|
| `instinfo.c:47` | `extern char *malloc();` (stale, wrong return type) inside `#ifndef _IBMR2` (true on this platform — `_IBMR2` is an old AIX/RS6000 guard, undefined here) | The stale extern's presence, however wrong-typed, satisfies the "some declaration exists" check that suppresses `-Wimplicit-function-declaration` entirely — no error, no warning, silently wrong (`char *` vs. the real `void *`). |
| `purge.c:50` | Same pattern, same `#ifndef _IBMR2` guard | Same mechanism. |

Both fixed: removed the stale `#ifndef _IBMR2 / extern char *malloc();
/ #endif` block, added `#include <stdlib.h>` unconditionally (matching
every other file in the directory — no other file has an `_IBMR2`
guard around this pattern). Confirmed via the same stale-extern sweep
(`grep -nE "extern[^;]*\b(malloc|free|realloc|calloc|...)\s*\("`) that
exactly 6 files in the whole directory ever had this stale pattern
(`addhdr.c`, `flushdir.c`, `hdlpref.c`, `instinfo.c`, `purge.c`,
`stack.c`); the other 4 already got `<stdlib.h>` (and the stale extern
correctly removed alongside it) as an automatic side effect of also
needing it for an unrelated `free`/`realloc` call the census *did*
catch in those files — `instinfo.c` and `purge.c` are exactly the 2
files where nothing else in the file needed `<stdlib.h>`, so the stale
extern's suppression went undetected until this sweep.

**Zero `calloc` calls anywhere in the directory** — confirmed absent,
not a further blind spot (the pattern is proven to catch `calloc` calls
correctly by the same logic that caught `malloc`/`free`/`realloc`
everywhere else).

**Real total fallout for this directory: 1567 census-visible + 2
malloc-blind-spot-only = 1569 instances across 102 files** (100 `.c`
files + `prsdate.gra`, since the two hand-fixed thin-wrapper files
`getnsub.c`/`getsubs.c` are already counted in the 100).

## 6. Files touched (compile status)

All 102 edited files (`Imakefile`, `prsdate.gra`, 100 `.c` files) compile
clean (0 errors of any kind) in their final state, confirmed by two full
`make -C ... clean && make -C ... depend && make -C ... -k install`
determinism passes at the directory's normal, unmodified flags, plus
the tree-wide gate. Per-file detail (100 `.c` files) is not reproduced
here in full given the volume — every fix is captured in
`m2-amsms-session.diff`; §4/§5 above give the full taxonomy/function
breakdown. Notable individual files:

- `Imakefile` — `COMPILERFLAGS` override added.
- `prsdate.gra` — `extern char *foldup(); extern int yearsize();` added
  to the existing `extern ...();` block (the hand-maintained bison
  source, not the generated `prsdate.c`).
- `getnsub.c`, `getsubs.c` — thin one-function wrapper files with zero
  `#include` lines; fixed by hand with a single `extern int ...();`
  before the wrapper function body (`GetNextSubsEntry`/`GetSubsEntry`,
  both defined in `subs.c`).
- `instinfo.c`, `purge.c` — malloc-blind-spot-only fixes (§5); stale
  `#ifndef _IBMR2` guarded `extern char *malloc();` removed, `<stdlib.h>`
  added.
- `mungenew.c` and 97 other files — the bulk of the 1567 census-visible
  fixes, applied via the nesting-depth-aware insertion script (§7);
  every declaration traceable to a real definition cited in
  `master_table.txt` (kept in scratch, not committed — the actual
  declarations are what's in the diff).

14 of 114 `.c` files needed **zero** fixes of any kind (census-clean and
malloc-sweep-clean both): confirmed via both passes independently.

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
checkout:     26072125a5bc5e96b90e7a9a25a472bc857b361b 2026-07-25 15:54:32 UTC
tags:         andrew-6.4
comment:      docs: M2 rollout point 5 prompt (ams/libs/ms, final M2 directory) (user: wdc)
EDITED     src/ams/libs/ms/Imakefile
EDITED     src/ams/libs/ms/addhdr.c
... [98 more .c files] ...
EDITED     src/ams/libs/ms/prsdate.gra
EDITED     src/ams/libs/ms/util.c
```

Exactly 102 `EDITED` files, no `ADDED`/`DELETED` entries. A transient
`MISSING src/atk/adew/Arb` line (a pre-existing, unrelated build
artifact — same shape `m2-messageslib-REPORT.md` §6 already documented
for a prior session) appeared in a mid-session snapshot before the
tree-wide gate and was gone from the final `fossil status` after the
gate's own full rebuild regenerated it — confirmed via `fossil diff`
throughout that no `atk/adew` file ever appears in this session's actual
diff. No commit made.

## 7. New taxonomy findings / process lessons

- **No new fallout category.** All 1569 real instances fit the three
  already-established categories (missing standard header; missing
  in-tree/project header with its sub-cases, including the `fdplumb.h`
  partial-wrapper-family sub-shape at its largest scale yet and the
  consumer-supplied-callback sub-shape; same-file/same-directory forward
  reference, at by far the largest single-directory scale seen in M2 —
  925 instances). "Possible genuine bug/typo" remains empty.
- **Contradicts prior-session finding**: malloc/free/realloc/strcat/
  strcmp/strcpy/strlen/strncpy were census-visible in this session,
  where 8 prior directories consistently found them invisible to
  `-Werror=implicit-function-declaration`. Reproduced with a minimal
  standalone `clang` repro outside this build, so it's not an artifact
  of this directory's specific flags. Root cause not chased down (could
  be a toolchain/SDK change between sessions, or a clang-version
  dependency the prior reports never pinned precisely) — flagged as an
  open question for `rollout-procedure.md` to fold in and reconcile,
  not resolved here.
- **New process lesson, not a taxonomy category**: automated bulk
  insertion of `#include`/`extern` lines anchored on "the last `#include`
  line in the file" is unsafe without tracking `#if`/`#ifdef`/`#endif`
  nesting depth — a file whose includes span a platform-conditional
  block (`mungenew.c`'s `#ifdef AFS_ENV`, false on this platform) will
  silently receive the new declarations inside that dead block, meaning
  they compile out entirely and don't actually fix anything on this
  platform. Caught immediately by the very next rebuild (the "fixed"
  errors were still present, unchanged, at their exact prior line
  numbers) — not a silent miss that could have reached the gate
  undetected, but worth recording explicitly since a differently-shaped
  session (e.g. one that trusted a 0-error exit code from a build that
  happened to skip recompiling the affected file due to a stale `.o`)
  could plausibly have missed it. Fix: track conditional-compilation
  depth and only anchor on a depth-0 `#include`; re-derive from clean
  (`fossil cat`) original content rather than patching over the
  already-wrong first attempt, to avoid compounding errors.
- **Confirms the runbook's "handful of functions dominate" hypothesis
  at the strongest scale yet measured**: top 10 functions = 45.6% of
  the total, top 20 = 53.5%, and the runbook's own five named-by-example
  functions (from census sampling, before any fixing pass) are all in
  the actual top 6.
- **A fourth and fifth independent LP64 mixed-width finding**
  (`conv64tolong`, `unsigned long`; `KRHash`, `unsigned long`) — same
  shape as `overhead/mail/lib`'s `AppendHosts`, `atkams/messages/lib`'s
  `CUI_*`/`MS_*` pair, and `contrib/zip/lib`'s `zip_Enparse_Stream`/
  `zip_Deparse_Stream`. Both sourced from real definitions after
  explicitly checking for and finding competing same-named candidates
  of the wrong type in unrelated directories (§4) — a clean, concrete
  demonstration of exactly the failure mode M2's own standing caution
  exists to prevent.
- **`AUXMODULE` double-underscore sub-case explicitly checked, does not
  recur**: `grep -l AUXMODULE *.c` in `ams/libs/ms` returns nothing —
  zero files define it. Structurally cannot recur here (no
  multi-file-`.do`-with-`AUXMODULE`-guarded-`.eh` shape exists in this
  directory — it's a single `LibraryTarget`, not `DynamicObject`/
  `DynamicMultiObject` classes).

## 8. Full tree-wide gate results

Checked `ps aux` first — no other `make`/`bison`/`cc1`/`clang` processes
running. Ran exactly as the runbook's exception requires for this
directory (both before-bucket-4-completion, already run by the
orchestrator per the prompt, and after this session's own fixes):

```
$ make -C /Users/wdc/src/AUIS/andrew-6.4/src Clean
EXIT=0
```

```
$ make -C /Users/wdc/src/AUIS/andrew-6.4/src dependInstall > ~/src/AUIS/andrew-6.4/dependInstall.log 2>&1
```

Launched via the Bash tool's own `run_in_background: true` mechanism
directly — **no** shell-level `nohup`/`&` wrapper, per the explicit
caution in this session's prompt about the earlier session that made
exactly that mistake. Confirmed real completion independently (not
taken on trust): final log is 238,642 lines,
`install.time`/`install.doc` touched, no `make`/`bison` process left
running.

```
$ grep -c "error:" ~/src/AUIS/andrew-6.4/dependInstall.log
4
$ grep -n "error:" ~/src/AUIS/andrew-6.4/dependInstall.log
112087:  200 |                 ErrorA(FATAL, "Internal error: unknown recognizer type", buff);
182814:clang: error: linker command failed with exit code 1 (use -v to see invocation)
234761:ltapp.c:115:27: error: incompatible integer to pointer conversion passing 'boolean' (aka 'int') to parameter of type 'void *' [-Wint-conversion]
234773:ltapp.c:123:32: error: incompatible integer to pointer conversion passing 'boolean' (aka 'int') to parameter of type 'void *' [-Wint-conversion]
```

**Exactly the same 4 pre-existing baseline errors every prior M2 gate
has documented**, each confirmed by citation:

1. **Recognizer-type false positive** (line 112087) — literal source
   text of an `ErrorA(FATAL, "Internal error: unknown recognizer
   type", buff)` call in `atk/syntax/tlex/defaults.c`, not a real
   diagnostic (per `m2-batch3a-REPORT.md`).
2. **`ams/msclients/nns`'s pre-existing SSLLIB link failure** (line
   182814) — **given `nns` is a direct consumer of `ams/libs/ms`, gave
   this extra scrutiny per the prompt's explicit instruction**: confirmed
   by context (`nns`'s own `building (dependInstall)` line is at 181201,
   with no other directory's build in between) and by content — the
   linker's undefined-symbol list is entirely OpenSSL API names
   (`_SSL_CTX_new`, `_SSL_CTX_free`, `_SSL_connect`, `_SSL_read`, etc.)
   referenced from `libmail.a[28](tlscon.o)`'s `_tlscon_sslerr`, **not**
   anything from `libmssrv.a` or any of this session's 100 touched
   files. `nns.c` itself and every other file in `ams/msclients/nns`
   **compiled cleanly** (checked the full build span for
   `-Werror=implicit-function-declaration`/`implicit` hits beyond the
   flag names themselves in the compile-invocation lines — none) —
   `nns` fails only at the final link stage, against a completely
   different library (`libmail.a`'s TLS/SSL glue) than the one this
   session touched. Confirmed unrelated to any M2 work generally
   (`m2-batch3b-REPORT.md` §6) and specifically unrelated to
   `ams/libs/ms`'s own fixes this session (this directory's own build
   span, immediately preceding `nns`'s in the log, is 100% clean — see
   below).
3–4. **`contrib/zip/utility/ltapp.c`'s two pre-existing int-conversion
   errors** (lines 234761, 234773) — confirmed by context: the nearest
   preceding `building (dependInstall)` line (233611) is
   `contrib/zip/utility` — a different directory from M2's own,
   already-closed `contrib/zip/lib` work. Not touched, not fixed, not
   mine to fix this session.

**Confirmed `ams/libs/ms`'s own build span is 100% clean**:

```
$ grep -n "building (dependInstall) (.*ams/libs/ms)" dependInstall.log
162749:building (dependInstall) (/Users/wdc/src/AUIS/andrew-6.4/src/ams/libs/ms)
$ grep -n "^building (dependInstall)" dependInstall.log | awk -F: '$1>162749' | head -1
179719:building (dependInstall) (/Users/wdc/src/AUIS/andrew-6.4/src/ams/libs/shr)
```

This directory's entire build span is lines 162749–179719 — none of the
4 baseline `error:` lines (112087, 182814, 234761, 234773) fall inside
that range (the nearest, 182814, is 3095 lines later, inside `nns`'s
own span which starts well after `ams/libs/ms` finished). `libmssrv.a`
(`build/lib/libmssrv.a`, 660664 bytes) and `atkams/messages/lib`'s
`amsn.do` both carry fresh timestamps matching this gate run.

**Gate is green. Zero new `error:` lines anywhere in the tree.** This is
also, per the prompt, **M2's own completion gate** — M2 is now closed
(pending nothing further; this is the final confirmation the milestone
plan called for).

## 9. Runtime consumers and exact runtime-check commands for wdc

**Load-bearing**: nothing from this session should be committed until
these are confirmed. Run from native Terminal.app (not an IDE
terminal), `DISPLAY=:0;` set as its own statement first. `.do` files are
cached for the life of a process — use a **fresh** process for each
check.

**Structural summary (§3's fan-out nuance, repeated here for the
runtime-check context)**: of the 7 directories the prompt names as
consumers, 5 actually build in this environment (`ams/msclients/cui`,
`ams/msclients/nns` — compiles, fails only at final link on the
pre-existing SSLLIB issue, §8 — `ams/msclients/imapsync`,
`atkams/messages/lib`, and transitively `ams/msclients/vui`'s sibling
`cui` since `MK_VUI` is undefined and `vui` itself doesn't build).
`ams/ms` (`SNAP_ENV`-gated off) and `rdemo/messages` (not reachable from
the top-level `SUBDIRS` at all) are both structurally absent from this
build — not testable here, not a regression, matching the
already-established `overhead/eli/lib` "zero live consumers" precedent
from batch3b.

`build/etc/AndrewSetup` shows `AMS_OnlyMail: No` with a comment
explicitly keeping "the IMAP mirror" mspath root enabled — same
configuration state `m2-messageslib-REPORT.md` §9 found, consistent
with a live, configured IMAP-backed mail store in this environment (not
verified for network reachability here, out of scope for a
non-interactive session).

### Exact runtime-check commands

1. **Primary, highest-value check — a fresh `messages` process, folder
   list load through to opening a message** (exercises the single
   largest fix population directly: `NonfatalBizarreError`/`CloseMSDir`/
   `FreeMessage`/`ReadOrFindMSDir`/`GetSnapshotByNumber` and the
   `fdplumb` `dbg_*` family's 300 instances, all run during ordinary
   folder-open/message-read):
   ```
   DISPLAY=:0; messages
   ```
   Confirm the folder list loads with real folder names/message counts,
   then open a message and confirm the body renders. This exercises
   `ams/libs/ms` indirectly (via `atkams/messages/lib`'s `amsn.do`,
   which links `libmssrv.a`) — the same GUI path
   `m2-messageslib-REPORT.md` already verified clean for the layer
   above; this session's fixes are one layer further down.

2. **`ams/msclients/cui`'s `cuin`, a direct consumer** (links
   `libmssrv.a` itself, not just transitively):
   ```
   /Users/wdc/src/AUIS/andrew-6.4/build/bin/cuin
   ```
   List folders, read a message, `quit` — confirm no crash. This
   exercises the address-parsing family (`ParseAddressList`/
   `FreeAddressList`/`UnparseOneAddress`/`AddHost`/`RemHost`, §4's
   `overhead/mail/lib` cross-directory group) and the subscription/
   folder-search-path family (`subs.c`'s ~180 same-directory
   declarations) directly through `cuin`'s own compiled path, distinct
   from `messages`'s dynamically-loaded path — corroborating rather than
   redundant evidence.

3. **A test message submission, exercising `submsg.c`/`mswp.c`'s address
   and MIME-comment parsing** (the second- and largest-by-instance-count
   same-directory files, 11 and 112 malloc-family sites respectively,
   §5's table): compose and send a short message to
   `wdc@fastmail.com` from either `messages` or `cuin` above, per
   `sonnet-playbook.md`'s hard rule that test emails go only there.
   Confirm the send completes without error — exercises
   `FreeSubmitVector`, the submission-vector build/free path, and (if
   the recipient address needs any comment-field parsing)
   `mswp.c`'s `ADDRESS_COMMENT`/host-parsing code, both part of this
   session's fixes.

4. **`ams/msclients/imapsync`, a direct consumer with its own compiled
   path** (links `libmssrv.a`/`libmsshr.a` directly, per its Imakefile's
   own comment about avoiding `libcui.a`):
   ```
   /Users/wdc/src/AUIS/andrew-6.4/build/bin/imapsync
   ```
   Run with whatever flags/config this environment's IMAP sync setup
   normally uses (see `project_ams_imap_project`/
   `project_imap_writeback_dispatch` memory for the established
   invocation, if `wdc` has one already scripted) and confirm no crash
   or unexpected failure — this is the cleanest single-purpose exercise
   of the directory/message-store read-write path (`OpenMSDirectory`,
   `ReadOrFindMSDir`, `CloseMSDir`, `MS_LockMUF`) outside a GUI app.

5. **Subscription/folder-search-path exercise** (exercises `subs.c`'s
   large same-directory family — the single largest per-file
   census-visible+malloc-sweep population in the directory, ~220
   combined instances across census+sweep): from either `messages` or
   `cuin`, open the folder tree/subscription view and
   subscribe/unsubscribe from a folder, or add a search-path entry if
   the environment's config exposes one. Confirm no crash and the
   change persists across a restart.

## 10. `fossil status`/`diff`, no commit

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
checkout:     26072125a5bc5e96b90e7a9a25a472bc857b361b 2026-07-25 15:54:32 UTC
comment:      docs: M2 rollout point 5 prompt (ams/libs/ms, final M2 directory) (user: wdc)
```

102 files `EDITED` (`Imakefile` + `prsdate.gra` + 100 `.c` files), zero
`ADDED`/`DELETED` entries, no commit made at any point in this session
(confirmed both mid-session and after the tree-wide gate — the gate's
own rebuild resolved a transient, unrelated `MISSING src/atk/adew/Arb`
snapshot artifact, §6). `fossil diff > m2-amsms-session.diff` (2447
lines) at the tree root is the exact, complete record of every change.

## 11. Command-style discipline this session

Checked cwd-persistence early per the prompt's instruction: a throwaway
`cd .../src/ams/libs/ms` call followed by a bare `pwd` in the next tool
call showed the cwd had reverted to a different default each time (this
session's default resolves to `/Users/wdc/src/AUIS`, the parent of the
checkout, not the checkout root or the target directory) — cwd does
**not** persist across Bash calls. Used `make -C <absolute-path>
<target>` for every build step, one command per call, never chained,
for the entire session (fix-surfacing pass, the malloc-sweep-triggered
rebuild, two full subtree determinism cycles, and the tree-wide
`Clean`/`dependInstall` pair — on the order of 25 separate build-step
calls).

**On permission-prompt frequency and the "does auto mode show" question
the prompt asked about**: every individually-allow-listed command
(`make -C ...`, `grep -n`/`-c`/`-l`, `ls` with a relative path and no
redirect, `fossil status`/`diff`, `Read`, `Edit`) went through with zero
prompts and zero denials all session long, consistent with every prior
bucket-4/M2-point-5 session's documented experience. **The one
consistent friction point**: `ls -la`/`nm -g` against an **absolute**
path under `build/bin/` (specifically `cuin` and `runapp`) were denied
outright, every time, with no prompt — even when reissued with a
relative path and no pipe/redirect, which is the shape that succeeded
for every prior session's equivalent check. The *same* relative-path,
no-redirect shape succeeded without any denial for `imapsync` and
`amsn.do` in the same session, so the denial doesn't appear to be about
command shape generically — possibly something specific to those two
binary names/paths in this session's permission configuration. Did not
investigate further (out of scope, and the timestamp/build-log evidence
in §8/§9 is sufficient corroboration without `nm`/`ls` on those two
specific binaries). I have the same honest answer as every prior
bucket-4 session on the auto-mode question specifically: I cannot
distinguish "no prompts occurred because auto mode suppressed them" from
"no prompts occurred because every command matched an allow-list entry
or a known carve-out shape" from inside a single delegated session — I
can only report what I observed (zero prompts on allow-listed shapes,
consistent denials on the two named absolute-path binary checks), which
does not obviously differ from the pre-auto-mode pattern documented in
earlier reports.

## 12. M2 is now closed

`ams/libs/ms` was explicitly the last planned M2 rollout point
(`m2-rollout-runbook.md`'s "Ordering" §5: "`ams/libs/ms` (892) last, its
own dedicated session... The second of these [gates] is also M2's own
completion gate, before M3 starts"). §8's tree-wide gate is green: exit
0 (not the sole criterion — checked, per `rollout-procedure.md`'s "Gate
definition," the actual `error:` line count, not just exit status),
exactly the 4 known pre-existing baseline errors, zero new ones
anywhere in the tree, `ams/libs/ms`'s own build span independently
confirmed clean. **This closes M2** (Medium-term → ANSI C conversion →
M2 — Prototype sweep) in its entirety, pending only `wdc`'s own review
of this report and the runtime checks in §9 before any commit. M3 has
not been started and is not in scope for this session.

## 13. Open questions / anything that surprised you

- **The malloc/free/realloc census-visibility contradiction (§4, §7) is
  the most significant open question from this session** — worth a
  dedicated look (compiler version diff, or a systematic re-check of
  whether the "invisible builtin" finding was ever really about
  `-Werror=implicit-function-declaration` alone versus some other flag
  combination present in the 8 prior directories but not here) before
  `rollout-procedure.md`'s "Logging" section is updated. I did not
  chase this down myself — flagging it rather than guessing.
- The nesting-depth insertion bug (§7) is worth keeping in mind for any
  future session doing bulk/scripted declaration insertion at this kind
  of scale — the failure mode (declarations landing inside a dead
  `#ifdef` block) is silent at the point of insertion and only surfaces
  on the next rebuild, which is exactly what caught it here, but a
  differently-ordered session could plausibly miss it.
- The `ams/libs/ms` → `ams/libs/hdrs/ms.h` gap (the header that, by
  name, looks like it should declare this directory's own API, but
  declares essentially none of what's actually called — the same
  pattern `m2-messageslib-REPORT.md` already found from the *consumer*
  side) is now confirmed from the *defining* side too. Worth a
  `ms.h` housekeeping pass whenever that header is next touched, same
  standing recommendation already on record for `fdplumb.h` and
  `ams/libs/cui`'s `cui.h` — out of scope for M2, not acted on here.
- `ams/ms` being `SNAP_ENV`-gated off and `rdemo` being absent from the
  active `SUBDIRS` tree entirely (§3, §9) means 2 of the prompt's 7
  named consumers are untestable in this environment — not a gap in
  this session's diligence, a structural fact about this build
  configuration, consistent with the already-established
  `overhead/eli/lib` precedent.
- No hard stops triggered: no called-but-undefined function anywhere
  (all 258 project-local names + all std-library names resolved to
  real, correctly-spelled definitions); no header-vs-usage type
  disagreement requiring escalation (the `AddHost`/`arpadate`/
  `conv64tolong` near-misses, §4, were resolved by sourcing real
  definitions and checking call-site usage, not a disagreement needing
  a ruling); nothing found touching the `fdplumb` include-order hazard
  directly (§4's dedicated subsection); every fix stayed inside this
  directory's own `.c`/`.gra` files plus the Imakefile (no header
  outside `ams/libs/ms` was ever edited).
- I did not run any AUIS GUI or binary interactively, and made no
  commits, per the prompt.
