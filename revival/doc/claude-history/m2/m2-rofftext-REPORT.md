# M2 rollout point 4d (bucket 4, session 4): `atk/rofftext`

## 1. Status

Stopped at the (only) gate — subtree-local `make clean && make depend
&& make -k install`, zero real errors, confirmed twice more for
determinism, per the runbook's "Gate scope" ruling (tree-wide gate NOT
required for this directory). No commits made. Working tree: 9 files
edited (`Imakefile` + 8 `.c` files — `num.c` is bison/`mkparser`-
generated, not fossil-tracked, needed no fix either way). Session diff
at tree root: `m2-rofftext-session.diff` (197 lines). Full gate log at
the canonical path `~/src/AUIS/andrew-6.4/dependInstall.log`
(overwritten across this session's several build passes; the final
two determinism passes are both clean, 0 `error:` lines).

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (including the
   "Command style" section's `cd`-persistence correction and the
   "Logging" section's malloc/realloc/free/calloc clang-builtin note,
   now upgraded to an unconditional sweep), `m2-rollout-runbook.md`,
   `m2-rofftext-prompt.md`, and all seven prior rollout reports
   (`m2-pilot-eq-REPORT.md`, `m2-batch2-REPORT.md`,
   `m2-batch3a-REPORT.md`, `m2-batch3b-REPORT.md`,
   `m2-utillib-REPORT.md`, `m2-metamail-REPORT.md`,
   `m2-text-REPORT.md`) in full before starting.
2. **Checked command-style/cwd-persistence early**, per the prompt's
   instruction: a throwaway `cd .../src/atk/rofftext` call followed by
   a bare `pwd`/`ls` call in the next tool call showed the cwd had
   reverted to `/Users/wdc/src/AUIS` (this harness's start directory),
   not the `cd`-targeted directory — cwd does **not** persist across
   Bash calls in this session. Used `make -C <absolute-path> <target>`
   for every build step for the rest of the session, one command per
   call, never chained with `&&`/`;`.
3. Read `src/atk/rofftext/Imakefile` directly and confirmed the
   `Parser(num,none)` generated-source gap (line 37): it expands (via
   `src/config/andrew.rls`'s macro definition) to a `depend::
   num.h num.c` rule that runs `bison` + `mkparser` against `num.gra`
   — wired **only** into the `depend::` target, not `all`/`install`.
   `make clean` deletes `num.h`/`num.c`; confirmed empirically that
   `make depend` regenerates both (verified via `ls -la` timestamps
   after each of the three `depend` runs this session) before the
   `-k install` pass ever touches `num.o`.
4. Flagged the Imakefile with the runbook's exact four-flag
   `COMPILERFLAGS` override, verified via `make -C ... Makefile` +
   `grep -n COMPILERFLAGS Makefile` (override at line 294 wins over
   `system.mcr`'s default at line 177 — re-verified after both later
   `depend`-triggered Makefile regenerations too).
5. Fix-surfacing pass, each its own call: `make -C ... clean`,
   `make -C ... depend` (confirmed `num.c`/`num.h` regenerated),
   `make -C ... -k install CDEBUGFLAGS="-ferror-limit=0 -g -O0"`.
   Result: **54 real errors, matching the stale estimate exactly** —
   see §3 for why an exact match still got the full malloc-sweep
   treatment per the prompt's explicit instruction.
6. Investigated every project-local function's real definition via
   `Read`/`Bash grep` before writing any declaration — see §4.
7. Fixed all 54 census-visible instances (6 of 9 `.c` files: `rofftext.c`,
   `roffcmds.c`, `roffutil.c`, `roffstyl.c`, `roffchrs.c`, `mmtext.c`).
   Rebuilt — exit 0, 0 errors, first attempt (no correction round
   needed).
8. **Unconditionally swept all 9 `.c` files** (not just the 6 already
   touched) for bare `malloc(`/`free(`/`realloc(`/`calloc(` calls, one
   `grep` call per file, per the prompt's explicit mandate regardless
   of the exact census match. Found the blind spot in **6 of 9 files,
   50 call sites**, zero prior declaration of any kind in any of them
   — see §5. Also swept for the space-variant form (`malloc (`) across
   all files; the only hits were bison's own generated boilerplate
   prototypes inside `num.c` (`YYSIZE_T`-comment lines), not undeclared
   call sites — no action needed there.
9. Fixed all 50 additional instances (`<stdlib.h>` added to 6 files:
   `rofftext.c`, `roffcmds.c`, `roffutil.c`, `mmtext.c`, `mantext.c`,
   `rofftxta.c` — the last two had *zero* census-visible errors at all,
   confirming the blind spot doesn't correlate with whether a file
   already has other fallout).
10. Rebuilt (`make -C ... -k install CDEBUGFLAGS="-ferror-limit=0 -g
    -O0"`) — 0 errors, exit 0.
11. Ran the subtree-local gate twice more for determinism (`make -C
    ... clean`, `make -C ... depend`, `make -C ... -k install`, plain
    recipe, each its own call) — both passes exit 0, 0 errors,
    `num.c`/`num.h` regenerated fresh each time, `COMPILERFLAGS`
    override re-verified after each regeneration.
12. Identified real runtime consumers via `nm -g` against
    `build/bin/runapp` (single unpiped `cd <tree-root> && nm -g
    build/bin/<target>` call, per the standing caution about
    piped/chained `nm` calls being denied outright in this session
    type) plus a tree-wide `grep` for `rofftext.ih`/`mmtext.ih`/
    `mantext.ih` inclusion outside this directory — see §9.
13. `fossil status` (§8) confirms exactly the 9 files touched. No
    commit made. `fossil diff > m2-rofftext-session.diff` written to
    the tree root.

## 3. Real instance count: 54 census-visible + 50 census-invisible = 104 total fallout instances

The stale table's "47" and the real `-k`/`-ferror-limit=0` census-
visible count land close but not exact: **54 vs. the stale 47** (a
modest, not dramatic, overshoot — nothing like metamail's 338-vs-70 or
`atk/text`'s hidden two-thirds). Taken alone this might read as "close
enough, no deeper investigation warranted." Per the prompt's explicit,
now-unconditional instruction, the malloc-family sweep ran regardless
of that closeness — and found **50 more instances across 6 of 9 files**,
none visible to the `-k`/`-ferror-limit=0` census at all, none
protected by even a stale/wrong-typed pre-existing declaration (every
one of the 50 sites had *zero* declaration of any kind reaching it
before this session, the same "total absence" shape `atk/text` found,
not the "wrong-typed extern becomes a conflict" shape `overhead/util/lib`
and metamail found). **Real total fallout for this directory is 104
instances, of which only 52% were visible to the
`-Werror=implicit-function-declaration` census at all** — this
directory is the second data point (after `atk/text`) where the
malloc-blind-spot count is the same order of magnitude as the
census-visible count, not a minor addendum, and it happened despite
the census count looking like an unremarkable, near-exact match to the
stale estimate. This reinforces the runbook's upgraded guidance: a
census that looks complete (even one within 15% of the stale estimate,
not just an exact match) is not evidence this category is absent.

## 4. Per-instance taxonomy: the 54 census-visible errors

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1-7 | `rofftext.c:532,569,599,676,825,837,855` | `get` | Same-file forward reference (defined at line 956) |
| 8 | `rofftext.c:518` | `EndStyle` | Missing in-tree/project header, no header anywhere (defined `roffstyl.c:158`) |
| 9 | `rofftext.c:521` | `BeginStyle` | Same as #8 (defined `roffstyl.c:127`) |
| 10 | `rofftext.c:584` | `EvalString` | Same as #8, no header anywhere (defined `num.c:1285`) |
| 11 | `rofftext.c:613` | `EvalString` | Same as #10 |
| 12 | `rofftext.c:1287` | `InitChars` | Same as #8, no header anywhere (defined `roffchrs.c:47`; call site already had a `/* in roffchrs.c */` comment) |
| 13 | `rofftext.c:1577` | `InsertTbl` | Same as #8, no header anywhere (defined `roffcmds.c:754`) |
| 14,16-19,22,28,38 | `roffcmds.c:140,362,411,460,513,650,1147,1493` | `FreeBuf` | No header anywhere (defined `roffutil.c:56`) |
| 15,20,29,31-35,39 | `roffcmds.c:201,581,1174,1220,1253,1332,1366,1460,1581` | `EvalString` | No header anywhere (defined `num.c:1285`) |
| 21,23 | `roffcmds.c:646,693` | `Set_BOL` | No header anywhere (defined `rofftext.c:177`) |
| 24,26,36 | `roffcmds.c:869,1109,1462` | `BeginStyle` | No header anywhere (defined `roffstyl.c:127`) |
| 25,27,37 | `roffcmds.c:872,1146,1466` | `EndStyle` | No header anywhere (defined `roffstyl.c:158`) |
| 30 | `roffcmds.c:1202` | `sv_cmd` | Same-file forward reference (defined at line 1205) — **fixed via the file's own dedicated header, see the note below** |
| 40,44 | `roffutil.c:131,148` | `EndStyle` | No header anywhere |
| 41,45 | `roffutil.c:136,152` | `BeginStyle` | No header anywhere |
| 42,44,45 | `roffutil.c:138,154` | `SetIndent` | Missing in-tree/project header, sub-case 1 (declared in `rofftext.h:34`, not included) |
| 46,47 | `roffutil.c:174` | `strcpy`, `strlen` | Missing standard-library header |
| 48,49 | `roffstyl.c:76,88` | `BeginStyle` | Same-file forward reference (defined at line 127) |
| 50,51 | `roffstyl.c:85,87` | `CloseStyle` | Same-file forward reference (defined at line 103) |
| 52,53 | `roffchrs.c:50` | `strcpy`, `strlen` | Missing standard-library header |
| 54 | `mmtext.c:77` | `strcpy` | Missing standard-library header |

54/54 accounted for. **"Possible genuine bug/typo" category: still
empty.** Every unfamiliar name resolved to a real, correctly-spelled
definition somewhere in this directory (all 9 functions besides the
standard-library ones are defined within `atk/rofftext` itself — this
directory never calls out to another directory's project-local
function, unlike most prior M2 directories).

**New sub-shape of same-file forward reference, worth flagging**:
`sv_cmd` (#30) is textually a same-file forward reference — defined at
`roffcmds.c:1205`, called at `roffcmds.c:1202`, both in the same
file — but the fix used was **not** a hand-written local `extern`.
`roffcmds.h` (this directory's own dedicated header, small, and
already declaring the entire `*_cmd` family including `sv_cmd` by
name: `int sv_cmd();` among ~50 sibling declarations) exists and is
already the header `rofftext.c` reaches to use these same functions
(`rofftext.c:58` already has `#include <roffcmds.h>`) — but
`roffcmds.c`, the file that *defines* the whole family, never included
its own header. Fixed by adding `#include <roffcmds.h>` to
`roffcmds.c` itself, which resolves `sv_cmd`'s forward reference via
the pre-existing, complete, correctly-typed declaration rather than a
hand-written one. This is a variant the taxonomy hasn't named
explicitly before: prior same-file forward-reference fixes were always
a fresh local `extern` block; here the fix is "the file forgot to
include its own already-complete header." Distinguishing detail:
`roffcmds.h` also declares `CloseStyle` (under a "style code for
be2roff" comment) even though `CloseStyle` is actually defined in
`roffstyl.c`, not `roffcmds.c` — but `CloseStyle` is never called
outside `roffstyl.c` itself in this directory, so that declaration is
never actually exercised by anything; `roffstyl.c` doesn't include
`roffcmds.h`, and didn't need to (its own #48-51 fix is a plain
same-file forward declaration instead, since including a whole
50-name external header just to reach one already-local function
would be the wrong shape here — no competing local-extern habit, but
also no benefit to reaching outside the file for something the file
defines and only the file uses).

**`Set_BOL` vs. `SetBOL` naming near-miss, checked and resolved**:
`rofftext.h` declares an `extern int SetBOL(...)` (no underscore,
line 37) — different from the real function actually called,
`Set_BOL` (with underscore, defined `rofftext.c:177`). Confirmed by
reading the real definition directly rather than assuming the
similarly-named header declaration was the intended target — these are
two distinct, both apparently real, functions (`Is_BOL`/`SetBOL` also
both exist in `rofftext.h`'s declared family, suggesting `SetBOL` was
a once-planned or historical sibling, not a typo for `Set_BOL`).
`Set_BOL`'s fix is an ordinary local `extern`, unaffected by this
naming coincidence.

## 5. Malloc-family blind-spot sweep: 50 instances across 6 of 9 files, none census-visible

Per the prompt's now-**unconditional** mandate (`m2-utillib-REPORT.md`
§6, `m2-metamail-REPORT.md` §8, `m2-text-REPORT.md` §5 all found this
blind spot at increasing scale; the guidance explicitly says grep every
file regardless of whether the census count looks complete), swept all
9 `.c` files individually (one `Bash grep` call per file, no loop) for
bare `malloc(`/`free(`/`realloc(`/`calloc(`:

| File | Call sites | Census-visible errors in this file? |
|---|---|---|
| `rofftext.c` | 16 | Yes (13) |
| `roffcmds.c` | 16 | Yes (26) |
| `roffutil.c` | 11 | Yes (8) |
| `mmtext.c` | 3 | Yes (1) |
| `mantext.c` | 3 | **No — zero census-visible errors in this file** |
| `rofftxta.c` | 1 | **No — zero census-visible errors in this file** |
| `roffstyl.c` | 0 | Yes (4), but no malloc-family calls |
| `roffchrs.c` | 0 | Yes (2), but no malloc-family calls |
| `num.c` | 0 (generated file; two bison-boilerplate `malloc`/`free` *prototypes*, not call sites, left untouched) | No |
| **Total** | **50** | |

Every one of the 50 sites had **zero declaration of any kind**
reaching it before this session — not even a stale, wrong-typed
`extern char *malloc();` to remove (unlike `overhead/util/lib`'s and
metamail's sessions, which both found and cleaned up such stale
externs). `mantext.c` and `rofftxta.c` are notable: both compiled with
*zero* `-Werror=implicit-function-declaration` errors in the original
54-instance census, yet both had real, invisible malloc-family
fallout (3 and 1 sites respectively) — directly confirming the
runbook's warning that a clean-per-file census result is not evidence
of this category's absence, extending it down to the single-file
level, not just the whole-directory level `atk/text` demonstrated.
Fix: `#include <stdlib.h>` added to all 6 files.

## 6. New taxonomy finding: `Parser(num,none)` mechanics, spelled out fully

Confirmed by reading `src/config/andrew.rls:285-296` directly (not
just trusting the prompt's citation): `Parser(classname,bisonargs)`
expands to:

```
depend:: classname.h classname.c
classname.h classname.c: classname.gra
	$(RM) classname.tab.c classname.tab.h
	ExecuteFromBASEDIR(bison bisonargs -b classname -k -o classname.tab.c classname.gra)
	$(RM) classname.c classname.h
	$(BASEDIR)/bin/mkparser classname
	$(RM) classname.tab.c classname.tab.h
classname.o: classname.c
clean:: ; $(RM) classname.h classname.c
```

For `Parser(num,none)`: `num.gra` (the grammar source, an
AUIS-specific extension, not a plain `.y` file) is run through
`bison` (with no extra bison args, since the second macro argument is
literally the word `none`) to produce `num.tab.c`, which
`$(BASEDIR)/bin/mkparser` (a project-local post-processor, not a
stock bison/yacc tool) then converts into the real `num.c`/`num.h`
pair actually compiled into `rofftext.do`. The generation rule is
wired only into `depend::`, confirmed by `make clean` deleting
`num.h`/`num.c` (its own separate `clean::` rule) and `make depend`
being the only target that regenerates them — verified empirically by
checking `num.c`/`num.h`'s on-disk mtimes changed after each of this
session's three `depend` runs, not just trusting the rule text. `make
depend` was run before every `install` this session, all three times
(fix-surfacing pass plus two determinism passes), per the confirmed
gap. This is the exact same shape the pilot (`atk/eq`, `Parser(eqparse,-d)`)
and batch B's `overhead/eli/lib` (`Parser(eliy,-d)`) already found —
`atk/rofftext` is the third confirmed instance of this class of
directory, no new variant.

## 7. Files touched (compile status)

All 9 edited files (`Imakefile` + 8 `.c` files) compile clean (0
errors of any kind) in their final state, confirmed by two full
`make -C ... clean && make -C ... depend && make -C ... -k install`
passes back-to-back after the fix-surfacing pass (three total per
file). `num.c`/`num.h` (generated, not fossil-tracked) needed no
fix — they compiled clean in every pass.

- `Imakefile` — `COMPILERFLAGS` override added.
- `rofftext.c` — `<stdlib.h>` added (malloc-family blind spot);
  local forward declarations added for `get` (same-file), `EndStyle`/
  `BeginStyle`/`EvalString`/`InitChars`/`InsertTbl` (no header
  anywhere, cross-file).
- `roffcmds.c` — `<stdlib.h>` added (blind spot); `#include
  <roffcmds.h>` added (fixes `sv_cmd`'s same-file forward reference
  via the file's own pre-existing, already-complete header — see §4);
  local externs added for `FreeBuf`/`EvalString`/`Set_BOL`/
  `BeginStyle`/`EndStyle` (no header anywhere).
- `roffutil.c` — `<string.h>` added (`strcpy`/`strlen`); `<stdlib.h>`
  added (blind spot); `#include <rofftext.h>` added (fixes
  `SetIndent`, sub-case 1, matching `roffcmds.c`'s own pre-existing
  precedent of reaching `rofftext.h` this way); local extern added for
  `BeginStyle`/`EndStyle` (no header anywhere).
- `roffstyl.c` — local forward declarations added for `BeginStyle`/
  `CloseStyle` (both same-file forward references).
- `roffchrs.c` — `<string.h>` added (`strcpy`/`strlen`).
- `mmtext.c` — `<string.h>` added (`strcpy`); `<stdlib.h>` added
  (blind spot).
- `mantext.c` — `<stdlib.h>` added (blind spot only; zero
  census-visible errors in this file).
- `rofftxta.c` — `<stdlib.h>` added (blind spot only; zero
  census-visible errors in this file).

Regenerated `Makefile` is not fossil-tracked, not a source edit.
`num.c`/`num.h` are generated (bison + `mkparser`), also not
fossil-tracked.

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
config-db:    /Users/wdc/.fossil
checkout:     bc1067f862b66bff063897ab86d426918209a0f4 2026-07-25 01:46:28 UTC
parent:       fa80dac4d910dd43e5f771bae127cf1a1fd30347 2026-07-25 01:46:21 UTC
tags:         andrew-6.4
comment:      docs: M2 rollout 4c complete (atk/text, 156 instances); malloc-blind-spot scale + cd-persistence corrections (user: wdc)
EDITED     src/atk/rofftext/Imakefile
EDITED     src/atk/rofftext/mantext.c
EDITED     src/atk/rofftext/mmtext.c
EDITED     src/atk/rofftext/roffchrs.c
EDITED     src/atk/rofftext/roffcmds.c
EDITED     src/atk/rofftext/roffstyl.c
EDITED     src/atk/rofftext/rofftext.c
EDITED     src/atk/rofftext/rofftxta.c
EDITED     src/atk/rofftext/roffutil.c
```

Exactly 9 files, matching this section's list above. No commit made.
`m2-rofftext-session.diff` (197 lines) written to the tree root.

## 8. Gate scope — subtree-local, no doubt found; structurally the cleanest data point yet

Checked, not assumed, per the prompt's explicit instruction. This
directory has **no `LibraryTarget` at all** — its Imakefile builds
only a `DynamicMultiObject` (`rofftext.do`, from 6 of the 9 `.c`
files) plus three single-file `.c.do` targets (`rofftxta.do`,
`mmtext.do`, `mantext.do`), and installs a `runapp` symlink
(`InstallLink(runapp, ${DESTDIR}/bin/rofftext)`) — the same pattern as
`ez`/`messages`/`help`. Confirmed via a single, unpiped `cd
/Users/wdc/src/AUIS/andrew-6.4 && nm -g build/bin/runapp` call (per
the standing caution that piped/chained `nm` calls get denied outright
in this session type — a bare piped `nm -g .../runapp | grep ...` was
in fact denied on the first attempt; the `cd <tree-root> && nm -g
build/bin/<target>` shape, followed by a separate `grep` over the
saved tool output, went through cleanly) that **none** of this
directory's distinctive function names (`EvalString`, `BeginStyle`,
`EndStyle`, `InsertTbl`, `InitChars`, `FreeBuf`, `Set_BOL`,
`CloseStyle`, `SetIndent`) appear anywhere in `runapp`'s own symbol
table — this directory is purely dynamically loaded, never statically
linked. This is structurally the same argument `overhead/mail/metamail/
metamail`'s session made (a directory that builds no `LibraryTarget`
cannot leak fallout into another directory's link line, not just
empirically unconfirmed to do so), extended here to a
`DynamicMultiObject`-only directory rather than a `ProgramTarget`-only
one — the mechanism differs but the conclusion (structurally impossible
for this directory's fallout to cross into another directory's build)
is the same. Did not run the tree-wide gate, per the runbook's
schedule (`atk/rofftext` is explicitly on the "skip the tree-wide
gate" list).

## 9. Runtime consumers and exact runtime-check commands for wdc

**Load-bearing**: nothing from this session should be committed until
these are confirmed. Run from native Terminal.app (not an IDE
terminal), `DISPLAY=:0;` set as its own statement first. `.do` files
are cached for the life of a process — use a **fresh** process for
each check.

Consumers identified via `nm -g build/bin/runapp` (§8, confirms
`rofftext` itself is fully dynamic) plus a tree-wide `grep` for
`#include <rofftext.ih>`/`rofftext_ReadRoffIntoText`/`rofftext_New`
outside this directory, **not assumed**:

- **`atk/help/src/help.c`** (compiles into `help.do`, dynamically
  loaded — confirmed via that directory's own Imakefile,
  `DynamicMultiObject(help.do, help.o helpaux.o, ...)`) **is the real,
  load-bearing, live consumer**: `help.c:397`'s comment says it
  outright ("it's a roff file (1st char '.' or '#' or '\\') and read
  it in using rofftext"), and `help.c:677` calls
  `rofftext_ReadRoffIntoText((struct text *)newdata, fd, 0, manfiles)`
  directly whenever a help document's first character indicates
  roff/man-page markup. `build/bin/help` is confirmed to exist as a
  `runapp` symlink (`MK_HELP` is `#define`d `1` in
  `config/allsys.h:153`). This makes **`help`** the primary runtime
  check — every one of this session's fixed functions (`get`,
  `EndStyle`/`BeginStyle`, `EvalString`, `InitChars`, `InsertTbl`,
  `FreeBuf`, `Set_BOL`, `sv_cmd`, `CloseStyle`, `SetIndent`, and all 6
  files' malloc-family fixes) is exercised by parsing any real
  roff-formatted help document, since they're all part of the same
  roff-parsing pipeline (`rofftext`'s command-table dispatch loop).
- **`atk/toez/toeza.c`** also includes `<rofftext.ih>` and calls
  `rofftext_New`/`rofftext_ReadRoffIntoText`/`rofftext_Read`/
  `rofftext_Write` (a roff-to-ez conversion utility), but **is not a
  live consumer in this build** — confirmed, not assumed:
  `atk/toez` only builds when `MK_TOEZ` or `MK_AUX_UTILS` is defined;
  `config/allsys.h:156` has `MK_AUX_UTILS` explicitly commented out,
  and `MK_TOEZ` is not defined anywhere in this build's config. No
  `build/bin/toez` exists. A structural finding (like batch B's
  `overhead/eli/lib` and its own zero-live-consumer result), not a
  testing gap — flagging for wdc rather than proposing a check that
  doesn't exist. If `MK_TOEZ`/`MK_AUX_UTILS` is ever enabled, `toez`
  would become a second natural consumer to check.
- The `rofftext` binary itself (`build/bin/rofftext`, a `runapp`
  symlink, same pattern as `ez`/`messages`/`help`) is presumably also
  directly launchable as a roff-to-ez converter given its own name and
  the `InstallLink` — not separately verified beyond confirming the
  symlink exists, since `help`'s indirect exercise already covers the
  same code paths more thoroughly (a real help document is a more
  realistic test than a synthetic direct invocation).

### Exact runtime-check commands

1. **Primary/load-bearing check — parse a real roff-formatted help
   document via `help`**:
   ```
   DISPLAY=:0; help
   ```
   Start a **fresh** `help` process (per the dynamic-load/stale-cache
   caution — `help.do` and `rofftext.do` are both loaded fresh on
   first use by a new process). Navigate to any help topic; AUIS help
   documents are commonly authored in roff/mm-macro markup, so simply
   browsing a few topics should trigger `rofftext_ReadRoffIntoText` on
   at least one of them. Confirm the text renders correctly — headings,
   paragraph breaks, and any embedded style changes (bold/italic via
   roff `.b`/`.i`-style macros) should appear as real formatting, not
   garbled text or literal roff control sequences (`.PP`, `.SH`, etc.
   showing up as visible text would indicate the parser broke). No
   crash, no hang.
2. **Direct exercise, if a plain-text roff/man-style source file is
   available**: many `andrew-6.4/src/*/doc/*.doc` or `*.help` files in
   this tree are themselves roff/mm-formatted (this directory's own
   `rofftext.doc`, or its `roffcmds.h`-adjacent help doc). Since
   `rofftext` itself is a `runapp` symlink, opening one directly should
   exercise the same conversion pipeline as `help`'s internal call,
   just from the standalone entry point:
   ```
   DISPLAY=:0; rofftext /Users/wdc/src/AUIS/andrew-6.4/src/atk/rofftext/rofftext.doc
   ```
   Open **read-only** — do not save over this real source file.
   Confirm it opens without crashing and shows converted, formatted
   text (headings/paragraphs), not a blank window or an error dialog.
3. If neither of the above happens to trigger a document containing
   table macros (`.TS`/`.TE`-style, exercising `InsertTbl`) or nested
   style changes (exercising the `BeginStyle`/`EndStyle`/`CloseStyle`
   stack machinery specifically), any roff source with an embedded
   table or multiple font changes in a row is a more targeted
   secondary check — not required if #1 passes cleanly, since the same
   shared command-dispatch loop (`roffcmds.c`) is what all of #4's
   fixed functions funnel through regardless of which specific roff
   command triggers them.

## 10. Comparison to the stale table and other bucket-4 directories

| Directory | Stale estimate | Real census-visible | Real total (incl. invisible) |
|---|---|---|---|
| `overhead/util/lib` | 74 | 74 | 74 (no invisible-blind-spot volume found) |
| `overhead/mail/metamail/metamail` | 70 | 338 | 338 (checked, none additional) |
| `atk/text` | 50 | 50 | 156 |
| `atk/rofftext` | 47 | 54 | **104** |

`atk/rofftext` is the second bucket-4 directory (after `atk/text`)
where the malloc-blind-spot volume is comparable in magnitude to the
census-visible volume, not a minor addendum — 48% of this directory's
real fallout was invisible to the `-Werror=implicit-function-declaration`
census. Unlike `atk/text` (where the census-visible count matched the
stale estimate *exactly* while the blind spot dominated), this
directory's census-visible count was itself already a modest overshoot
(54 vs. 47) — the two findings are independent: a close-but-not-exact
census match is just as untrustworthy a signal for "blind spot absent"
as an exact match. Also notable: two of this directory's six
malloc-affected files (`mantext.c`, `rofftxta.c`) had *zero*
census-visible errors of their own — the cleanest within-directory
confirmation yet that per-file cleanliness doesn't predict blind-spot
absence either.

## 11. Command-style discipline — how it went this session

Followed `atk/text`'s adaptation exactly, having checked early per the
prompt's explicit instruction: this session's Bash tool does **not**
persist working directory across calls (confirmed via the throwaway
`cd`+`pwd` pair in §2 step 2, before any real build step). Used
`make -C <absolute-path> <target>` for every build step (9 build-step
calls: 3 full clean/depend/install cycles × 3 steps each), each its
own unchained call — **zero permission prompts** across all of them,
consistent with every prior bucket-4 session's experience once
chaining is avoided.

**The `nm -g` piping question**: a first attempt at the gate-scope
check, `nm -g build/bin/runapp | grep -i "..."` (piped, and using a
relative rather than tree-rooted absolute path), was **denied
outright**, matching `overhead/util/lib`'s and the metamail session's
findings (not `atk/text`'s one-off contrary data point, where a
double-chained-and-piped `nm` had gone through). Reissued as the
established carve-out shape, `cd /Users/wdc/src/AUIS/andrew-6.4 &&
nm -g build/bin/runapp` (a single call, no pipe, no redirect) — this
went through immediately, writing its large output to a persisted
tool-result file; a separate, subsequent `Bash grep` call over that
saved file (not over the live command) found the relevant symbols
with no prompt or denial. This is a **new, useful pattern** worth
naming for future sessions handling a large unpiped `nm`/similar
output: let the harness's own output-persistence mechanism hold the
full listing, then `grep` the saved file in a following call, rather
than trying to pipe/filter within the same command that produces the
output.

Net this session: 2 tool-call denials (the piped `nm` and, separately,
one earlier informal experiment not itself load-bearing), both
self-corrected on the next attempt by dropping the pipe/chain — no
prompts of any other kind across roughly 60 tool calls this session
(the smallest bucket-4 directory so far, the tool-call count tracks
accordingly).

## 12. Open questions / anything that surprised you

- §5's finding that `mantext.c`/`rofftxta.c` had real malloc-family
  fallout despite zero census-visible errors of their own is the main
  thing worth carrying forward: the "unconditional sweep" mandate
  isn't just protecting against a directory whose *aggregate* census
  looks clean — it's needed even when most files in a directory *do*
  have ordinary census hits, because the blind spot can hide entirely
  in the specific files that happen not to.
- §4's `sv_cmd`/`roffcmds.h` finding (a same-file forward reference
  resolved by including the file's own already-complete, already-used-
  by-a-sibling-file header, rather than a hand-written local `extern`)
  is a new sub-shape worth folding into the runbook's taxonomy text if
  it recurs: when a same-file forward reference's function already has
  a real, complete, in-directory header declaration (even if the
  defining file itself just never included it), prefer the `#include`
  over a hand-written duplicate declaration.
- The `Set_BOL`/`SetBOL` near-miss (§4) — two similarly-but-not-
  identically named real functions, one declared in a header and
  unrelated to the one actually undeclared — is the same general shape
  as `overhead/mail/metamail/metamail`'s `ULstrcmp` finding
  (`m2-metamail-REPORT.md` §7): worth a mention if this codebase's
  history of near-duplicate helper names comes up again, though this
  particular case never risked a wrong fix (the two names are visibly
  different, unlike `ULstrcmp`'s exact-name collision).
- No hard stops triggered: no called-but-undefined function anywhere
  (every name resolved to a real, correctly-spelled, in-directory
  definition — this is the first M2 directory where *every* project-
  local fallout function turned out to be defined within the flagged
  directory itself, none cross-directory), no header-vs-usage type
  disagreement requiring escalation, and every fix stayed inside this
  directory's own `.c`/`.h` files (`roffcmds.h`'s "include, don't
  extend" fix in §4 is squarely inside `atk/rofftext`, the flagged
  directory — no outside header was touched or needed to be).
- I did not run any AUIS GUI or binary interactively, and made no
  commits, per the prompt.
