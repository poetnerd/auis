# M2 rollout point 3, batch B: `overhead/eli/lib`, `ams/libs/cui`, `ams/msclients/nns`, `overhead/mail/metamail/richmail`, `overhead/index`

## 1. Status

Stopped at the (only) gate. No commits made. Working tree: 34 files edited
(5 `Imakefile`s + 29 `.c`/`.h` files) across the 5 directories. Session
diff at tree root: `m2-batch3b-session.diff`. Full gate log at the
canonical path `~/src/AUIS/andrew-6.4/dependInstall.log`.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md`,
   `m2-rollout-runbook.md`, `m2-batch3b-prompt.md`, and the three prior
   rollout points' reports (`m2-pilot-eq-REPORT.md`,
   `m2-batch2-REPORT.md`, `m2-batch3a-REPORT.md`) in full.
2. Checked each of the 5 directories' Imakefiles for `Parser()`/
   `LexFile` before flagging: only `overhead/eli/lib` has a
   generated-source gap (`FlexOrLexFileRule(elil)` + `Parser(eliy,-d)`,
   producing `eliy.h`/`eliy.c`/`elil.c`'s scanner body via the `depend::`
   target only) — confirmed by rebuild failure until `make depend` was
   run before `install`, matching the pilot's finding. The other 4
   directories have no generated-source gap.
3. For each directory: flagged its `Imakefile` with the runbook's exact
   four-flag `COMPILERFLAGS` override, verified via `make Makefile` +
   `grep -n COMPILERFLAGS Makefile` (override line always appears after
   `system.mcr`'s default), then `make clean && make depend && make -k
   install`, using `CDEBUGFLAGS="-ferror-limit=0 -g -O0"` on each
   directory's fix-surfacing pass (all 5 directories have files with
   17-34 diagnostics in one pass, well within reach of clang's default
   20-error-per-file cap — used the raised limit from the start rather
   than risk the same undercount batch A found), fixed fallout, then
   rebuilt clean twice more with the plain recipe to confirm determinism.
4. No reliable stale count existed for this batch (the runbook's census
   table only lists directories 1-42/1-892; these 5 fall in its "~60
   more directories" bucket) — derived the real count for each directory
   empirically from its own `-k`, `-ferror-limit=0` build. Two
   directories turned out much larger than that stale bucket's 1-42
   range would suggest: `ams/libs/cui` (350 instances) and
   `ams/msclients/nns` (51 instances) — both exceed 42. See §3 for
   per-directory detail; see §7 for the taxonomy note this prompts.
5. Ran the full tree-wide gate: `cd src && make Clean` (exit 0), then
   backgrounded `make dependInstall > ~/src/AUIS/andrew-6.4/dependInstall.log 2>&1`,
   confirmed completion via `pgrep -f 'make dependInstall'` returning
   empty.
6. `fossil status` (below) confirms exactly the 34 files touched. No
   commit made. `fossil diff > m2-batch3b-session.diff` written to the
   tree root.

## 3. Per-directory findings

### `src/overhead/eli/lib` — 38 real instances

Generated-source gap confirmed (see §2 step 2): `make clean && make -k
install` alone hit a fatal "file not found" on `eliy.h`/`eliyy.c` before
reaching any real diagnostic; `make depend` first (per
`rollout-procedure.md`'s corrected recipe) fixed it.

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1-2 | `fnnode.c:43,99` | `malloc`, `free` | Missing standard-library header |
| 3-4 | `node.c:43,109` | `malloc`, `free` | Missing standard-library header |
| 5 | `bucket.c:75` | `strcmp` | Missing standard-library header |
| 6-7 | `buktnode.c:56,175` | `malloc`, `free` | Missing standard-library header |
| 8-9 | `cons.c:54,74` | `malloc`, `free` | Missing standard-library header |
| 10-14 | `ecommon.c:384(×2),766,900,1083` | `malloc`, `strlen`, `strcpy`, `strcmp`, `realloc`, `free` | Missing standard-library header |
| 15 | `ecommon.c:608` | `eliyylex` | Same-directory forward reference (defined in sibling generated file `elil.c`, no header declares it anywhere) |
| 16-17 | `errstk.c:39(×2)` | `realloc`, `malloc` | Missing standard-library header |
| 18-19 | `prims1.c:329,412` | `free`, `malloc` | Missing standard-library header |
| 20-24 | `prims1.c:728,734,738,743,1602` | `dbg_fclose` | Missing in-tree/project header, new shape — see §7 |
| 25 | `prims1.c:2231` | `realloc` | Missing standard-library header |
| 26 | `prims2.c:1013` | `free` | Missing standard-library header |
| 27 | `prims2.c:1573` | `system` | Missing standard-library header |
| 28 | `prims2.c:1652` | `malloc` | Missing standard-library header |
| 29-30 | `stk.c:41(×2)` | `realloc`, `malloc` | Missing standard-library header |
| 31 | `stk.c:112` | `strcmp` | Missing standard-library header |
| 32-35 | `str.c:53,70,74,84` | `malloc`, `strlen`, `free`, `strcpy` | Missing standard-library header |
| 36-38 | `sym.c:55,75` (+1 more implied by identical pattern) | `malloc`, `free` | Missing standard-library header |

Fix: `#include <stdlib.h>` in `fnnode.c`, `node.c`, `buktnode.c`,
`cons.c`, `errstk.c`, `prims2.c`, `stk.c`, `str.c`, `sym.c`;
`#include <string.h>` in `bucket.c`, `stk.c`, `str.c`;
`#include <stdlib.h>` + `#include <string.h>` in `ecommon.c`. One
untyped `extern int eliyylex(void);` added in `ecommon.c` (real
definition traced to the flex-generated preamble in `elil.c`, which
declares it locally as `extern int eliyylex (void);` right before its
own `YY_DECL` macro use — no header reaches it). One untyped
`extern int dbg_fclose();` added in `prims1.c`, next to that file's
pre-existing `extern char *AndrewDir();` local-extern habit — see §7 for
why this doesn't fit either previously-seen shape of the "header exists,
incomplete" sub-case cleanly.

### `src/ams/libs/cui` — 350 real instances, 68 unique functions

No generated-source gap. Far larger than the runbook's stale "~60 more
directories, 1-42 each" bucket implied — see §7. Both files
(`cuilib.c`, 4027 lines; `andmchs.c`, 523 lines) needed
`-ferror-limit=0` from the start; the default 20-error cap truncated
both on the first attempt (`fatal error: too many errors emitted,
stopping now`).

Full per-call-site table omitted for space (68 unique function names,
many called 10-134 times each — `ReportError` alone accounts for 134 of
the 350). Grouped by taxonomy:

- **Missing standard-library header** (`cuilib.c`: `free`, `malloc`,
  `atoi`, `exit`, `realloc`; `andmchs.c`: `exit`, `wait`). Fix:
  `#include <stdlib.h>` in both files, `#include <sys/wait.h>` in
  `andmchs.c`.
- **Missing in-tree/project header, sub-case 1 (header exists, just not
  included)**: `getaddr`, `vdown`, `fwriteallchars`, all three declared
  in `overhead/util/hdrs/util.h` (`getaddr` at line 311, `vdown` at 246,
  `fwriteallchars` at 122) and reached only in `cuilib.c` (6 call
  sites); `andmchs.c` already includes `util.h`. Fix:
  `#include <util.h>` in `cuilib.c`.
- **Missing in-tree/project header, no header anywhere (sub-case 3),
  fdplumb family**: `dbg_fclose` (24 sites), `dbg_close` (3),
  `dbg_vfclose` (2), `dbg_pclose` (1), `dbg_pipe` (1), `dbg_dup2` (2, in
  `andmchs.c`) — same gap as `overhead/eli/lib` above, see §7.
- **Missing in-tree/project header, no header anywhere, `ams/libs/shr`
  family**: `BuildNickName`, `LowerStringInPlace`, `ReduceWhiteSpace`,
  `bone`, `lc2strncmp`, `FindTreeRoot` — all defined in
  `ams/libs/shr/utils.c`/`findroot.c`, no header in the tree declares
  any of them.
- **Missing in-tree/project header, no header anywhere, consumer-supplied
  UI callback interface**: `ReportError` (134), `ReportSuccess` (68),
  `ChooseFromList` (9), `GetStringFromUser` (5), `GetBooleanFromUser`
  (10), `DirectoryChangeHook` (5), `SubscriptionChangeHook` (1),
  `ConsiderLoggingRead` (1), `SetTerminalParams` (1, `andmchs.c`) — none
  of these are defined anywhere in `ams/libs/cui` itself; each is
  implemented separately by whichever front end links `libcui.a`
  (`ams/msclients/cui/cuifns.c`+`cui.c`, `atkams/messages/lib/stubs.c`,
  `rdemo/messages/stubs.c`, `ams/msclients/vui/*`) — a
  library-calls-back-into-its-linker pattern, structurally the same
  shape as `atk/raster/cmd`'s `dispbox.h` callers in batch A but with
  *no* header at all, anywhere, for the interface. All consumer
  implementations found are consistent, untyped K&R (implicit `int`),
  so declared the same way.
- **`ams/libs/ms` cross-directory, no header, deliberately NOT the
  existing `long`-typed local-extern habit**: `MS_CUI_Init`,
  `MS_GenTempFileName`, `MS_WriteUnscribedBodyFile` (`cuilib.c`),
  `MS_FastUpdateState` (`andmchs.c`, separately). `cuilib.c` already has
  a large pre-existing local-extern block declaring ~35 other `MS_*`
  functions as `extern long ...`. Checked each of these four against its
  real definition instead of assuming the existing bucket's type is
  correct: `ams/libs/ms/gentname.c`, `unscrib.c`, and `update.c` all
  define their functions with implicit (K&R) `int` return, and the
  build's actually-linked implementation (`SNAP_ENV` is commented out in
  `config/allsys.h`, so `ams/libs/nosnap/nosnap.c` is what links, not
  `ams/libs/snap/cuisnap.c`) defines `MS_CUI_Init` as explicit `int`, not
  `long`. Declared all four as `int` in a separate block, with a comment
  explaining the deliberate deviation from the existing (and, on this
  evidence, itself already-questionable) `long` bucket — per the M2
  caution about not introducing a fresh wrong-width declaration while
  fixing an M2 warning. Did not touch the pre-existing `long` bucket
  itself (out of scope, not part of this directory's fallout).
- **Same-file forward reference** (`cuilib.c`, ~26 functions defined
  later in the file and used earlier: `BumpNeedsPurging`,
  `CUI_AppendFileToVice`, `CUI_BuildNickName`, `CUI_CheckNewMessages`,
  `CUI_CopyViceFileTails`, `CUI_FixAttribute`,
  `CUI_FixAttributeByNumber`, `CUI_GenTmpFileName`,
  `CUI_GetBodyToLocalFile`, `CUI_GetCuid`, `CUI_HandleMissingFolder`,
  `CUI_MarkDirectoryForPurging`, `CUI_PrintBodyFromCUIDWithFlags`,
  `CUI_PrintUpdatesWithFlags`, `CUI_PurgeMarkedDirectories`,
  `CUI_ReallyGetBodyToLocalFile`, `CUI_ReportAmbig`, `CUI_ResendMessage`,
  `CUI_RewriteHeaderLineInternal`, `CheckEmsgConsistency`,
  `FindQuotedString`, `FreeCustomizationHeaders`, `GetHeaderContents`,
  `GetViceFileToNewString`, `PutStringToViceFile`, `pfclose`) — matched
  `cuilib.c`'s own pre-existing forward-declare convention (it already
  has two such blocks: one `extern long MS_*...` list, one
  `long CUI_*...` list for functions "that return a long").
- **Same-directory (cross-file) forward reference — new two-way
  finding, see §7**: `Machine_Init`, `CUI_InitializeKeepalives`,
  `CUI_GenLocalTmpFileName` defined in `andmchs.c`, called from
  `cuilib.c`; `CUI_SetMachineName`/`CUI_SetMachineType` (`void`-returning
  — the only two non-implicit-int functions in this directory's fallout)
  and `CUI_GenTmpFileName` defined in `cuilib.c`, called from
  `andmchs.c` — i.e. the two files in this directory call back and forth
  into each other with no shared header for any of it.
- One follow-on fix, directly caused by my own `#include <stdlib.h>` in
  `andmchs.c`: a pre-existing `#ifndef _IBMR2 / extern char *malloc();
  #endif` (wrong return type, `char *` instead of `void *`) started
  conflicting with `stdlib.h`'s correct prototype
  (`conflicting types for 'malloc'`). Removed the stale declaration —
  `stdlib.h` now supplies the correct one. Not a new fallout instance;
  downstream of fixing #26 above (`malloc`/`free`/`exit` in the same
  file), documented in place with a comment.

### `src/ams/msclients/nns` — 51 real instances

No generated-source gap. Compiles cleanly (all real
`-Werror=implicit-function-declaration` diagnostics resolved); the
directory's own pre-existing SSLLIB link failure (undefined `SSL_*`
symbols from `libmail.a`'s `tlscon.o`) remains at the link step,
unaffected by and unrelated to this fix work — see §5.

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `conf.c:131` | `exit` | Missing standard-library header |
| 2 | `conf.c:360` | `dbg_fclose` | fdplumb family, see §7 |
| 3 | `gl.c:76` | `dbg_closedir` | fdplumb family |
| 4,6,7 | `gl.c:114,157,161` | `dbg_close` | fdplumb family |
| 5,13,17 | `gl.c:135,145,284` | `mkdir` | Missing standard-library header (`sys/stat.h`) |
| 8 | `gl.c:203` | `realloc` | Missing standard-library header |
| 9 | `gl.c:208` | `malloc` | Missing standard-library header |
| 10 | `gl.c:265` | `qsort` | Missing standard-library header |
| 11 | `gl.c:272` | `free` | Missing standard-library header |
| 18 | `gl.c:336` | `MS_ParseDate` | `ams/libs/ms`, no header anywhere |
| 19-20 | `mcb.c:68,94` | `malloc`, `free` | Missing standard-library header |
| 21 | `nns.c:102` | `exit` | Missing standard-library header |
| 22 | `nns.c:116` | `MS_Initialize` | `ams/libs/ms`, no header anywhere |
| 23,25 | `nns.c:126,144` | `GetNewReadyBox` | Same-file forward reference (defined at 160) |
| 24 | `nns.c:131` | `MS_UpdateState` | `ams/libs/ms`, no header anywhere |
| 26 | `nns.c:186` | `dbg_closedir` | fdplumb family |
| 27,29,31 | `nns.c:251,271,293` | `CloseMSDir` | `ams/libs/ms`, no header anywhere |
| 28 | `nns.c:262` | `OpenMSDirectory` | `ams/libs/ms`, no header anywhere |
| 30 | `nns.c:269` | `ReadOldMSDirectoryHead` | `ams/libs/ms`, no header anywhere |
| 32 | `nns.c:327` | `malloc` | Missing standard-library header |
| 33 | `nns.c:338` | `ReadRawFile` | `ams/libs/ms`, no header anywhere |
| 34 | `nns.c:339` | `ParseMessageFromRawBody` | `ams/libs/ms`, no header anywhere |
| 35 | `nns.c:340` | `AddNetnewsWideReplyHeader` | Same-file forward reference (defined at 513) |
| 36 | `nns.c:342` | `InventID` | `ams/libs/ms`, no header anywhere |
| 37 | `nns.c:343` | `BuildDateField` | `ams/libs/ms`, no header anywhere |
| 38 | `nns.c:344` | `BuildReplyField` | `ams/libs/ms`, no header anywhere |
| 39 | `nns.c:345` | `BuildAttributesField` | `ams/libs/ms`, no header anywhere |
| 40 | `nns.c:346` | `BuildCaption` | `ams/libs/ms`, no header anywhere |
| 41,43,45,47,49 | `nns.c:348,368,402,431,445,468` | `FreeMessage` | `ams/libs/ms`, no header anywhere |
| 42,50 | `nns.c:397,559` | `AddHeader` | `ams/libs/ms`, no header anywhere |
| 44 | `nns.c:407` | `IsMessageAlreadyThere` | `ams/libs/ms`, no header anywhere |
| 46 | `nns.c:423` | `WritePureFile` | `ams/libs/ms`, no header anywhere |
| 48 | `nns.c:436` | `AppendMessageToMSDir` | `ams/libs/ms`, no header anywhere |
| 51 | `nns.c:454` | `dbg_close` | fdplumb family |
| — | `stb.c:69,83,96` | `malloc`, `free`, `realloc` | Missing standard-library header |

`ams/libs/hdrs/ms.h` was checked and confirmed to declare none of the
`ams/libs/ms` functions above (only mentions `FreeMessage` in a
comment) — genuinely no header anywhere, matching the runbook's own
warning that `ams/libs/ms` functions turn up undeclared across many
consumer directories. Fix: `#include <stdlib.h>` in `conf.c`, `gl.c`,
`mcb.c`, `nns.c`, `stb.c`; `#include <sys/stat.h>` in `gl.c`; local
`extern int` blocks for the fdplumb family (`conf.c`, `gl.c`, `nns.c`)
and the `ams/libs/ms` family (`gl.c`: `MS_ParseDate`; `nns.c`: 15
functions); same-file forward-declare block in `nns.c` for
`GetNewReadyBox`/`AddNetnewsWideReplyHeader`. None of the 5 files had
any pre-existing local-extern convention to match.

### `src/overhead/mail/metamail/richmail` — 44 real instances

No generated-source gap (`richlex.c` looks lexer-generated by name but
is a plain hand-written file, confirmed by reading it — no
`Parser()`/`LexFile` in the Imakefile, no bison/flex invocation anywhere
in this directory).

| # | File:line | Missing declaration | Taxonomy category |
|---|---|---|---|
| 1 | `richtext.c:121` | `getpid` | Missing standard-library header |
| 2 | `richtext.c:152` | `exit` | Missing standard-library header |
| 3 | `richtext.c:167` | `richtext_main` | Same-file forward reference (defined at 172, five lines later) |
| 4,6 | `richtext.c:189,193` | `strcpy`, `strcmp` | Missing standard-library header |
| 5 | `richtext.c:191` | `atoi` | Missing standard-library header |
| 7 | `richtext.c:210` | `strncmp` | Missing standard-library header |
| 8 | `richtext.c:319` | `isatty` | Missing standard-library header |
| 9,10,11 | `richtext.c:327,332,354` | `tgetent`, `tgetstr`, `tgetnum` | Missing standard-library header (`termcap.h`) |
| 12-15 | `richtext.c:419,483,525,589` | `controloutput` | Same-file forward reference (defined at 618) |
| 16,17 | `richtext.c:622,899` | `tputs` | Missing standard-library header (`termcap.h`) |
| 18,19 | `richlex.c:184,234` | `strcmp`, `strcpy` | Missing standard-library header |
| 20 | `richset.c:118` | `exit` | Missing standard-library header |
| 21 | `usascii.c:58` | `strcmp` | Missing standard-library header |
| 22,24 | `iso2022.c:88,105` | `strncmp`, `strcmp` | Missing standard-library header |
| 23,25-38 | `iso2022.c:92,108,...,301` (15 sites) | `controloutput` | Cross-file, same directory, no header — see below |
| 27,32,37 | `iso2022.c:161,188,197` | `controlputc` | Cross-file, same directory, no header |
| 39 | `richtoatk.c:22` | `strcmp` | Missing standard-library header |
| 40 | `richtoatk.c:59` | `ignoretoken` | Same-file forward reference (defined at 78) |

`controloutput`/`controlputc` are the most structurally interesting
finding here: `iso2022.c` (one of the shared `RICHOBJS`, compiled once
and linked into both the `richtext` and `richtoatk` programs) calls
both, but neither is defined in `iso2022.c` itself or in any header —
each of the two consuming programs supplies its **own** implementation
(`richtext.c:604/618`, `richtoatk.c:88/93`), the same
library-calls-back-into-its-linker shape found in `ams/libs/cui` above,
just contained within one small directory instead of spanning several.
Fix: local `extern int controloutput(), controlputc();` in `iso2022.c`.
Also: `#include <stdlib.h>`/`<string.h>`/`<unistd.h>`/`<termcap.h>` in
`richtext.c`; `<string.h>` in `richlex.c`/`usascii.c`/`iso2022.c`/
`richtoatk.c`; `<stdlib.h>` in `richset.c`; same-file forward-declare
blocks in `richtext.c` (`richtext_main`, `controloutput`) and
`richtoatk.c` (`ignoretoken`) — no pre-existing local-extern convention
found anywhere in this directory, so used plain untyped K&R style
throughout, consistent with the rest of this batch.

### `src/overhead/index` — 45 real instances

No generated-source gap. See §4 for the `index.h` re-evaluation the
prompt specifically asked for.

19 distinct `index_*`/`recordset_*` functions, called from `testidx.c`
(the directory's own test program), `index.c` itself, and `indexio.c`,
all defined within this same directory (`index.c`/`rcordset.c`/
`indexio.c`) but never declared in `index.h`:
`index_Create`, `index_AddPrimary`, `index_Close`, `index_Enumerate`,
`index_AddSecondary`, `recordset_Free`, `index_GetData`,
`index_DeletePrimary`, `index_DeleteSecondary`, `index_Dump`,
`index_CPut`, `index_HashPresent`, `index_HashAdd`, `index_PurgeBucket`,
`index_FreeHL`, `index_HashRemove`, `index_CWrite`, `index_FreeIndex`,
`recordset_Add` (26 call sites across `testidx.c`/`index.c`, all
implicit/K&R `int`-returning, confirmed from their real definitions).
Plus standard-library gaps: `malloc` (`index.c:143`, `indexio.c:165`,
`rcordset.c:49`), `free` (`index.c:220`, `indexio.c:345`,
`rcordset.c:119`), `realloc` (`rcordset.c:163`).

Fix: extended `index.h` directly with all 19 declarations (see §4 for
why); `#include <stdlib.h>` in `index.c`, `indexio.c`, `rcordset.c`.

## 4. The `overhead/index`/`index.h` re-evaluation (prompt's explicit ask)

Rollout point 2's `atk/help/src/helpdb.c` fix (batch2 report §3) already
found 4 of these exact functions (`index_Close`, `index_Enumerate`,
`index_GetData`, `recordset_Free`) undeclared, and added local `extern`s
in `helpdb.c` rather than editing `index.h` — because at that time
`overhead/index` was *not* the flagged directory, `index.h` lived
outside it, and the M2 hard-stop against editing files outside the
flagged directory applied (taxonomy sub-case 2's first shape).

Now that `overhead/index` itself is flagged, I re-evaluated `index.h` on
its own terms rather than assuming that reasoning still applies,
per the prompt's explicit instruction:

- **`overhead/index` has its own instances of the same gap**: yes, 19 of
  them (see §3 table above) — a strict superset of the 4 `helpdb.c`
  already needed, all called from files inside this same directory.
- **Is extending `index.h` directly correct now?** Checked both
  conditions the taxonomy's sub-case 2 second shape requires:
  - `index.h` lives *in* `overhead/index` itself (confirmed:
    `src/overhead/index/index.h`) — not an outside/broad utility header
    like `util.h`/`cmdraw.h` from earlier examples.
  - It is small (130-ish lines before this edit) and clearly a dedicated
    single-file public API for this directory's own `index_*`/
    `recordset_*` family — not a many-callers utility header. It already
    declares 12 sibling functions from the same family in the exact
    style I extended it with (`extern TYPE Foo();`).
  - Checked all 4 consuming files (`testidx.c`, `index.c`, `indexio.c`,
    `rcordset.c`) for a competing local-extern habit for this specific
    function family: `grep -n "^extern"` returned **zero** matches in
    any of the 4 files — no local-extern precedent exists anywhere in
    this directory to compete with extending the header.
  - Conclusion: **taxonomy sub-case 2's second shape applies** (header
    in the flagged directory, dedicated API, no competing local-extern
    habit) — extended `index.h` directly, matching `atk/basics/x/
    menubar.h` and `atk/raster/cmd/dispbox.h`'s precedent from batch A,
    not `atk/help/src/helpdb.c`'s local-extern precedent from batch 2.
    Point 2's original reasoning (don't edit the outside header) simply
    no longer applies now that the header is inside the flagged
    directory — confirmed empirically rather than assumed, per the
    prompt's instruction.
- **Interaction with `helpdb.c`'s pre-existing local externs**: checked
  for a redeclaration conflict. `helpdb.c:76-79` already has
  `extern int index_Close(); extern int index_Enumerate();
  extern int index_GetData(); extern int recordset_Free();` — byte-
  identical in type (`int`, untyped K&R) to what `index.h` now also
  declares. C permits repeated compatible `extern` declarations, so no
  conflict is expected; confirmed by the tree-wide gate rebuilding
  `atk/help/src` cleanly (§5) with both `index.h`'s new declarations and
  `helpdb.c`'s own now-redundant ones in scope simultaneously. Did not
  remove `helpdb.c`'s local externs — that file is outside this
  directory's scope for this M2 pass, and the redundancy is harmless.

## 5. Subtree-local gate vs. tree-wide gate

Fourth data point for `m2-rollout-runbook.md`'s "Gate scope" question,
requested explicitly by this prompt. Ran the full gate as directed:
`cd src && make Clean` (exit 0), then backgrounded `make dependInstall
> ~/src/AUIS/andrew-6.4/dependInstall.log 2>&1`, confirmed completion
via `pgrep -f 'make dependInstall'` returning empty.

```
$ grep -c "error:" dependInstall.log
4
$ grep -n "error:" dependInstall.log
110503:  200 |                 ErrorA(FATAL, "Internal error: unknown recognizer type", buff);
175253:clang: error: linker command failed with exit code 1 (use -v to see invocation)
225615:ltapp.c:115:27: error: incompatible integer to pointer conversion ...
225627:ltapp.c:123:32: error: incompatible integer to pointer conversion ...
```

Same 4 pre-existing baseline errors as every prior gate (recognizer-type
false positive, `ams/msclients/nns`'s own SSLLIB link failure —
confirmed by context: the linker error at line 175253 sits directly
after `building (dependInstall) (.../ams/msclients/nns)` at line 173640,
with no other directory's build in between — `contrib/zip/utility/
ltapp.c`'s two int-conversion errors). Confirmed all 5 directories
descended:

```
$ grep -n "building (dependInstall) (.*\(overhead/eli/lib\|ams/libs/cui\|ams/msclients/nns\|overhead/mail/metamail/richmail\|overhead/index\))" dependInstall.log
3373:building (dependInstall) (.../src/overhead/index)
8258:building (dependInstall) (.../src/overhead/eli/lib)
27309:building (dependInstall) (.../src/overhead/mail/metamail/richmail)
157471:building (dependInstall) (.../src/ams/libs/cui)
173640:building (dependInstall) (.../src/ams/msclients/nns)
```

**The tree-wide gate found nothing beyond what the 5 subtree-local
builds already showed** — same result as the pilot, batch 2, and batch
3A. This is the fourth consistent data point, at the largest volume yet
(528 instances total this batch, more than double batch A's 137,
dominated by `ams/libs/cui`'s 350) and including a directory
(`ams/libs/cui`) whose library is linked into `atkams/messages/lib`'s
`amsn.do` — the actual mail-store logic behind the GUI `messages` app
(§9) — still showing zero cross-directory fallout. Per the runbook's own
framing, this batch was flagged as "likely the last batch where the
full tree-wide gate is required before that relaxation gets an explicit
ruling from wdc" — nothing here argues against that relaxation; if
anything, four-for-four (including the two heaviest directories examined
so far by instance count) is a stronger case for it than existed after
batch A.

## 6. `ams/msclients/nns`'s SSLLIB link failure — independent of this session's fix work

Confirmed independent, not affected by or related to the M2 fix work
here. Evidence:

- All of this session's `ams/msclients/nns` fixes are `#include`
  additions and local `extern int Foo();` declarations resolving
  `-Werror=implicit-function-declaration` diagnostics at *compile* time
  (§3 table) — none reference SSL, TLS, or anything in `libmail.a`.
- The link failure is entirely about undefined `SSL_*`/`TLS_*`/`X509_*`
  symbols referenced from `libmail.a[28](tlscon.o)` — a file in a
  different library (`overhead/mail/lib`, not `ams/msclients/nns`),
  already compiled and archived before `nns`'s own link step runs. My
  changes never touch `tlscon.c`/`libmail.a`.
- Determinism check (§2 step 3, both follow-up passes after the
  fix-surfacing pass): the build now fails **identically** at the link
  stage both times, with **zero** compile-time errors either time —
  i.e. every one of the 51 real instances is durably fixed, and the only
  remaining failure is this pre-existing, unrelated missing-OpenSSL-at-
  link-time condition (the same baseline error every prior rollout point
  has also seen, unrelated to whichever directory was being flagged that
  session).
- This means `nns` cannot currently be linked into a runnable binary in
  this environment at all (confirmed: no `nns` binary exists anywhere
  under `build/` or in the source directory) — a pre-existing condition,
  not a regression from this session, and out of scope to fix here (it
  needs a real OpenSSL library available at link time, unrelated to
  ANSI/implicit-declaration work).

## 7. New taxonomy findings / corrections

- **New shape within "missing in-tree/project header": a header exists,
  declares *some* of a wrapper family, but the family spans many more
  functions than the header covers, and the gap recurs unpredictably
  across many unrelated consumer directories** — the `fdplumb.c` `dbg_*`
  family. `overhead/util/hdrs/fdplumb.h` `#define`s `open`/`fopen`/
  `close`/`fclose`/`popen`/`pclose`/`qopen`/`qclose`/`topen`/`tclose`/
  `dup`/`dup2`/`pipe`/`socket`/`socketpair`/`opendir`/`closedir` (16
  renames) to their `dbg_*` equivalents, but only *declares* 6 of them
  (`dbg_open`, `dbg_fopen`, `dbg_popen`, `dbg_qopen`, `dbg_topen`,
  `dbg_opendir`) — the other 10, including the ones actually hit this
  batch (`dbg_fclose`, `dbg_close`, `dbg_dup2`, `dbg_pipe`, `dbg_pclose`,
  `dbg_vfclose`), are silently undeclared everywhere they're used. This
  doesn't cleanly fit either previously-documented shape of sub-case 2:
  `fdplumb.h` is a genuinely broad, many-consumers utility header (like
  `util.h`), but *neither* `overhead/eli/lib` nor `ams/libs/cui` nor
  `ams/msclients/nns` had any pre-existing local-extern habit for this
  specific family before this session (unlike `cmdraw.h`/`index.h`'s
  batch-2 examples, which both had an existing local-extern precedent to
  match) — this is each directory's *first* encounter with the gap, not
  a case of matching an established local habit. Treated it the same
  way regardless (local extern, not editing the outside header, per the
  M2 hard-stop), but flagging this as a fourth sub-shape: *no header
  reaches part of a wrapper family, and no directory-local habit exists
  yet either* — the fix is still local-extern by default (the M2
  hard-stop against editing outside headers applies regardless of
  whether local precedent exists), but "check for existing local
  precedent first" as written doesn't fully cover this case since there
  often isn't one to find. Hit in 3 of this batch's 5 directories
  (`overhead/eli/lib`, `ams/libs/cui`, `ams/msclients/nns`) — likely to
  recur in any directory that touches file I/O, making `fdplumb.h`
  itself a good candidate for a real housekeeping pass (declare the
  remaining 10 `dbg_*` functions properly) whenever it or a directory
  that heavily exercises it gets attention outside M2.
- **Confirms and extends the `ams/libs/ms`-functions-are-widely-
  undeclared pattern**: `MS_ParseDate`, `MS_Initialize`,
  `MS_UpdateState`, `CloseMSDir`, `OpenMSDirectory`,
  `ReadOldMSDirectoryHead`, `ReadRawFile`, `ParseMessageFromRawBody`,
  `InventID`, `BuildDateField`, `BuildReplyField`,
  `BuildAttributesField`, `BuildCaption`, `FreeMessage`, `AddHeader`,
  `IsMessageAlreadyThere`, `WritePureFile`, `AppendMessageToMSDir`,
  `MS_CUI_Init`, `MS_GenTempFileName`, `MS_WriteUnscribedBodyFile`,
  `MS_FastUpdateState` — 21 more `ams/libs/ms` functions with no header
  anywhere, found across `ams/msclients/nns` and `ams/libs/cui` this
  batch, joining the runbook's own prediction that `ams/libs/ms` itself
  (892 predicted instances, saved for last) is the root cause of a
  recurring cross-directory gap. None of this batch's fixes touched
  `ams/libs/ms` itself — all local externs in the consuming directories,
  per the M2 hard-stop.
- **New finding: consumer-supplied callback interfaces with *zero*
  declaring header anywhere, not just an incomplete one** — `ams/libs/
  cui`'s `ReportError`/`ReportSuccess`/`ChooseFromList`/
  `GetStringFromUser`/`GetBooleanFromUser`/`DirectoryChangeHook`/
  `SubscriptionChangeHook`/`SetTerminalParams`/`ConsiderLoggingRead`, and
  `overhead/mail/metamail/richmail`'s `controloutput`/`controlputc`.
  Structurally distinct from ordinary sub-case 3 (a single project
  function defined in one place, undeclared): here the function is
  defined *multiple times*, once per program that links the library,
  with no canonical implementation and no header, ever, anywhere. Both
  instances resolved the same way as ordinary sub-case 3 (local extern,
  sourced from any one of the consistent real implementations, since all
  found implementations agreed on the untyped K&R shape) — but worth
  naming as its own recognizable shape for future directories, since
  "which implementation is the real one" doesn't have a single answer
  the way it does for sub-case 3's simple case.
- **Volume finding, not a taxonomy category**: this batch is the first
  where the runbook's stale per-directory bucketing ("~60 more
  directories, 1-42 each") was *wrong*, not just imprecise — `ams/libs/
  cui` (350) and `ams/msclients/nns` (51) both exceed the bucket's
  stated 42-instance ceiling, `ams/libs/cui` by nearly an order of
  magnitude. The prompt itself anticipated this ("no reliable stale
  per-directory count exists for this batch... derive the real count
  yourself") — confirming that instruction was necessary, not just
  cautious.
- **§4 above is the taxonomy resolution the prompt specifically asked
  for**: sub-case 2's second shape (header in the flagged directory,
  extend it) applied to `overhead/index`/`index.h`, not the first shape
  (`atk/help/src/helpdb.c`'s local-extern precedent from rollout point
  2) — confirmed empirically, not assumed, exactly as the prompt
  required.
- **"Possible genuine bug/typo" category remains empty.** Every
  unfamiliar-looking function name across all 5 directories (528 total
  instances this batch) resolved to a real, correctly-spelled
  definition somewhere in the tree once grepped. No new candidate.
- One self-inflicted-and-fixed finding, not a taxonomy category: adding
  `#include <stdlib.h>` to `andmchs.c` exposed a pre-existing
  wrong-return-type `extern char *malloc();` (see `ams/libs/cui`'s
  entry in §3) as a hard conflict rather than a silent latent bug —
  removed the stale declaration. Worth watching for in any future
  directory: `#include <stdlib.h>` can turn a previously-silent
  wrong-typed local `malloc`/`realloc`/`free` extern into a compile
  error, which is a *good* outcome (surfaces a real, if currently
  harmless, latent type bug) but should be expected, not treated as
  unrelated new fallout.

## 8. Files touched (compile status)

All 34 edited files compile clean (0 real `-Werror=implicit-function-
declaration` errors) in their final state, confirmed by two more clean
`make clean && make depend && make -k install` passes per directory
after the fix-surfacing pass (three-plus total per directory).
`ams/msclients/nns` additionally confirmed deterministic at the link
stage: both follow-up passes fail identically and only at the
pre-existing SSLLIB link step (§6), not at compilation.

- `src/overhead/eli/lib/Imakefile`, `bucket.c`, `buktnode.c`, `cons.c`,
  `ecommon.c`, `errstk.c`, `fnnode.c`, `node.c`, `prims1.c`, `prims2.c`,
  `stk.c`, `str.c`, `sym.c`
- `src/ams/libs/cui/Imakefile`, `andmchs.c`, `cuilib.c`
- `src/ams/msclients/nns/Imakefile`, `conf.c`, `gl.c`, `mcb.c`, `nns.c`,
  `stb.c`
- `src/overhead/mail/metamail/richmail/Imakefile`, `iso2022.c`,
  `richlex.c`, `richset.c`, `richtext.c`, `richtoatk.c`, `usascii.c`
- `src/overhead/index/Imakefile`, `index.c`, `index.h`, `indexio.c`,
  `rcordset.c`

Regenerated `Makefile`s in all 5 directories are not fossil-tracked, not
source edits.

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
config-db:    /Users/wdc/.fossil
checkout:     cd8811554bbccd7933693b36fd48c274bf01488f 2026-07-24 19:33:14 UTC
parent:       42e1ac6d0b8c94a36e98f6d4f15167908fe1258f 2026-07-24 19:32:59 UTC
tags:         andrew-6.4
comment:      docs: root-cause + fix for figotext label corruption (fontdesc.c LP64 cast bug); new open item, italic text sizing non-monotonic (user: wdc)
EDITED     src/ams/libs/cui/Imakefile
EDITED     src/ams/libs/cui/andmchs.c
EDITED     src/ams/libs/cui/cuilib.c
EDITED     src/ams/msclients/nns/Imakefile
EDITED     src/ams/msclients/nns/conf.c
EDITED     src/ams/msclients/nns/gl.c
EDITED     src/ams/msclients/nns/mcb.c
EDITED     src/ams/msclients/nns/nns.c
EDITED     src/ams/msclients/nns/stb.c
EDITED     src/overhead/eli/lib/Imakefile
EDITED     src/overhead/eli/lib/bucket.c
EDITED     src/overhead/eli/lib/buktnode.c
EDITED     src/overhead/eli/lib/cons.c
EDITED     src/overhead/eli/lib/ecommon.c
EDITED     src/overhead/eli/lib/errstk.c
EDITED     src/overhead/eli/lib/fnnode.c
EDITED     src/overhead/eli/lib/node.c
EDITED     src/overhead/eli/lib/prims1.c
EDITED     src/overhead/eli/lib/prims2.c
EDITED     src/overhead/eli/lib/stk.c
EDITED     src/overhead/eli/lib/str.c
EDITED     src/overhead/eli/lib/sym.c
EDITED     src/overhead/index/Imakefile
EDITED     src/overhead/index/index.c
EDITED     src/overhead/index/index.h
EDITED     src/overhead/index/indexio.c
EDITED     src/overhead/index/rcordset.c
EDITED     src/overhead/mail/metamail/richmail/Imakefile
EDITED     src/overhead/mail/metamail/richmail/iso2022.c
EDITED     src/overhead/mail/metamail/richmail/richlex.c
EDITED     src/overhead/mail/metamail/richmail/richset.c
EDITED     src/overhead/mail/metamail/richmail/richtext.c
EDITED     src/overhead/mail/metamail/richmail/richtoatk.c
EDITED     src/overhead/mail/metamail/richmail/usascii.c
```

Exactly the 34 files from §8's list, unchanged since the mid-session
check. No commit made. `m2-batch3b-session.diff` (748 lines) written to
the tree root.

## 9. Runtime-check commands for wdc

**Load-bearing**: nothing from this batch should be committed until
these are confirmed. Run from native Terminal.app (not an IDE terminal),
`DISPLAY=:0;` set as its own statement first. Static/dynamic linkage and
actual consumers confirmed via `nm -g`/`nm -u` against `build/bin/
runapp`, `build/dlib/atk/*.do`, and each directory's own Imakefile
`LIBS`/`SLIBS` line — not assumed, per the prompt's explicit
instruction to identify the real consumer for the messaging-adjacent and
mail/index directories rather than assuming `messages` alone covers
them.

1. **`overhead/eli/lib`** (`libeli.a` — the ELI expression-language
   interpreter used by ATK's `eq` inset's "compute" support): **has no
   live runtime consumer in this build**, confirmed rather than assumed.
   Its two Imakefile-listed consumers are `ams/ms` (`messageserver`) and
   `rdemo/messages` — `ams/ms` is guarded by `#ifdef SNAP_ENV` in
   `src/ams/Imakefile`, and `SNAP_ENV` is commented out in
   `config/allsys.h` (confirmed the same way §3's `MS_CUI_Init`
   investigation did), so it's never in `SUBDIRS`; `rdemo` isn't
   referenced anywhere in the top-level `src/Imakefile` at all. Neither
   directory shows a `building (dependInstall)` line anywhere in the
   gate log. `libeli.a` itself builds and installs cleanly
   (`build/lib/libeli.a` exists), but there is currently no binary in
   the active tree that links it, so no GUI or CLI runtime check is
   possible right now. This is a genuine structural finding, not a gap
   in this session's testing — flagging for wdc rather than proposing a
   check that doesn't exist. If `ams/ms`/`SNAP_ENV` is ever revived,
   that would become the natural consumer to check against.

2. **`ams/libs/cui`** (`libcui.a` — CUI's message-server client
   protocol library): messaging-adjacent as the prompt anticipated, but
   the actual consumer set is broader than just the standalone `cui`
   client. Confirmed via each consumer Imakefile's `LIBS`/`SLIBS`/
   `NLIBS` line:
   - **`atkams/messages/lib`** links `libcui.a` into `amsn.do`
     (confirmed: `nm -g build/dlib/atk/amsn.do` shows `CUI_SetMachineName`/
     `CUI_GenTmpFileName` defined directly in the `.do`) — this is the
     dynamically-loaded mail-store logic the GUI `messages` app actually
     uses (per `sonnet-playbook.md`'s own note). **This is the primary,
     highest-value check**:
     ```
     DISPLAY=:0; messages
     ```
     Since `.do` files are cached for the life of the process
     (`m2-batch3a-REPORT.md` §11.1's lesson), start a **fresh**
     `messages` process, not a reused one. Confirm the folder list
     loads, open a message, and send a piece of test mail to
     `wdc@fastmail.com` (per `sonnet-playbook.md`'s hard rule — test
     emails go only there) to exercise the delivery-string/machine-name
     path (`CUI_SetupDeliveryString`, `CUI_SetMachineName`/
     `CUI_SetMachineType`, all touched this session) and the
     `MS_CUI_Init`/`Machine_Init` cross-file path at connection setup.
   - **`ams/msclients/cui`** builds `build/bin/cuin` (the interactive,
     terminal-based CUI client, NOSNAP variant — confirmed by binary
     presence; the SNAP variant `cuis` is not built since `SNAP_ENV` is
     off). This is the most direct exercise of `ams/libs/cui`'s own
     interactive-callback code (`ReportError`/`ReportSuccess`/
     `ChooseFromList`/`GetBooleanFromUser`/etc., §3/§7's largest fallout
     category):
     ```
     DISPLAY=:0; /Users/wdc/src/AUIS/andrew-6.4/build/bin/cuin
     ```
     Run a few basic commands (list folders, read a message, `quit`) at
     its prompt. This is a terminal program, not an X11 GUI window, so
     it's safe to run in Terminal.app directly.
   - `ams/msclients/vui` and `ams/msclients/imapsync` also link
     `libcui.a` but are secondary/legacy paths; not proposing a
     dedicated check for either unless the two above turn up a problem.

3. **`ams/msclients/nns`**: NNS is a netnews-to-AMS gateway daemon, not
   an interactive client — confirmed by reading `nns.c`'s own header
   comment ("This program is a !&@^#% messageserver client" that polls
   a directory of ready-boxes and feeds them into AMS as netnews
   messages). **No runtime check is possible for this directory right
   now**: it cannot be linked into a binary at all in this environment
   (§6 — the pre-existing SSLLIB failure blocks the final link step
   unconditionally, unrelated to this session's fixes). The correct
   check here is the compile-level one already performed and reported
   in §3/§6: zero implicit-function-declaration errors, deterministic
   across two independent rebuilds. If a real OpenSSL library is ever
   linked in (out of scope for M2), `nns`'s day-to-day operation is a
   cron-style batch job against a live netnews feed, not something to
   runtime-check interactively even then.

4. **`overhead/mail/metamail/richmail`**: two standalone CLI tools,
   confirmed present at `build/bin/richtext` and `build/bin/richtoatk`
   (both plain Mach-O executables, no `.do`/symlink involvement, same
   pattern as batch 2's `fdbbdf`). `richtext` reformats RFC 1341/1521
   `text/richtext` MIME bodies for terminal display (the `TERMLIB`/
   termcap fixes, §3, are exercised directly by its cursor-control
   output path); `richtoatk` converts richtext to an ATK `.ez` datastream
   fragment. Neither is invoked from any other Imakefile in the tree
   (checked, not assumed) — they're metamail's helper programs, run by
   metamail itself when it encounters a `text/richtext` MIME part, not
   by `messages`/`ez` directly.
   ```
   printf '<nl><bold>Hello<nl></bold>\n' | /Users/wdc/src/AUIS/andrew-6.4/build/bin/richtext
   ```
   Confirm it exits 0 and shows the word "Hello" with terminal
   bold-on/off control sequences around it (or plain text if `$TERM`
   isn't set to something `tgetent` recognizes — either way, confirm no
   crash and no garbled/missing text, which is what the `controloutput`/
   `controlputc` and `strcmp`/`strncmp` fixes touch).
   ```
   printf '<nl><bold>Hello<nl></bold>\n' | /Users/wdc/src/AUIS/andrew-6.4/build/bin/richtoatk > /tmp/m2-batch3b-richtoatk-test.ez
   ```
   Confirm it exits 0 and the output file contains plausible ATK
   datastream markup (`\begindata{text...}`-style framing per
   `atk-datastream-format.md`), not empty or truncated.

5. **`overhead/index`** (`libindex.a` — a simple on-disk hash-bucket
   index/database, used by ATK's `help` system): confirmed the real
   consumer via `nm -g build/dlib/atk/helpdb.do`, which shows all 19 of
   this session's newly-declared `index_*`/`recordset_*` functions
   (plus the 12 already-declared siblings) compiled directly into the
   `.do` — `libindex.a` is statically archived into `helpdb.do`, the
   dynamically-loaded database backend behind the `help` app (this is
   also §4's `helpdb.c`, whose own local `extern`s from rollout point 2
   now sit alongside `index.h`'s new declarations with no conflict,
   confirmed by this same `.do` building cleanly in the gate).
   ```
   DISPLAY=:0; help
   ```
   Confirm the help browser launches, shows its default page, and a
   search/navigation action (following a link, or the help index)
   works without crashing — the same check batch 2's report proposed
   for `atk/help/src`, now doubly relevant since both the `helpdb.c`
   consumer side (batch 2) and the `index.h`/`index.c` library side
   (this batch) have both been touched by M2. Additionally, a direct,
   dedicated CLI exercise of the library itself, independent of `help`:
   ```
   cd /tmp && /Users/wdc/src/AUIS/andrew-6.4/src/overhead/index/testindex
   ```
   (the directory's own test program, already built during this
   session's subtree rebuilds — not installed to `build/bin`, run it
   from its source location; it creates/reads/deletes index files in
   the current directory, so `cd /tmp` first rather than running it
   in the source tree). Confirm it exits 0 and prints results consistent
   with a successful create/add/enumerate/delete cycle, not an error or
   crash — this exercises all 19 of this session's newly-declared
   functions directly, independent of `help`'s own indirection.

## 10. Open questions / anything that surprised you

- `overhead/eli/lib` having **zero** live runtime consumers (§9.1) was
  the biggest surprise this batch — not just "hard to test" like batch
  2's `fdbbdf`, but genuinely unreachable from any binary in the active
  build. Worth a `roadmap.md` note if `SNAP_ENV`/`ams/ms` status is ever
  revisited; not acted on here (out of scope for M2, and the M2 fix
  itself is still correct and complete regardless of whether anything
  currently links the library).
- `ams/libs/cui`'s real size (350 instances, 68 unique functions) was
  far outside the runbook's stale "~60 more directories, 1-42 each"
  bucket — the prompt's explicit instruction not to trust that table for
  this batch was necessary, not just cautious (§7).
- The two-way, header-less cross-file calling between `cuilib.c` and
  `andmchs.c` (§3/§7) — each file calls functions defined in the other,
  with zero shared header for any of it, in a directory that also has
  two *separate*, coexisting local-extern conventions already
  (`environ.c`-style habitual local-extern blocks, `MS_*`/`CUI_*`-long
  forward-declare lists) — is the messiest single-directory structure
  encountered in the M2 rollout so far. No hard stop was needed (every
  real signature was findable from its actual K&R definition), but it
  took noticeably more cross-referencing than any prior directory to be
  confident no declaration was guessed rather than sourced.
- The `fdplumb.h` partial-declaration gap (§7) recurring independently
  in 3 of this batch's 5 directories, with no shared local-precedent
  pattern between them, suggests it's a good candidate for a real fix
  (declaring the missing 10 `dbg_*` names in the header itself) whenever
  someone has a housekeeping session for `overhead/util/lib`/
  `overhead/util/hdrs` — flagging for wdc, not done here since it's
  outside all 5 flagged directories.
- No hard stops triggered: no called-but-undefined function anywhere
  (every name resolved to a real, correctly-spelled definition), no
  header-vs-usage type disagreement requiring escalation (the
  `MS_CUI_Init`-family int-vs-long question in §3 was resolved by
  sourcing the real, actually-linked definition rather than trusting the
  existing possibly-wrong `long` bucket — a judgment call made *toward*
  correctness, not a disagreement needing a ruling), and every fix
  stayed inside its own flagged directory's `.c`/`.h` files (§4's
  `index.h` extension is squarely inside `overhead/index`, the flagged
  directory).
- I did not run any AUIS GUI or binary interactively, and made no
  commits, per the prompt.
