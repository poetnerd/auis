# M3 Wave 6, Batch AMS1: `ams/libs/ms` — ansify K&R→ANSI rollout, plus the `ams/msclients/cui` COMPILERFLAGS closure add-on

Read `sonnet-playbook.md` and `rollout-procedure.md` in full before
starting (same hard rules as every M3 batch: no fossil commits ever,
stop at the gate, write `m3-ams1-session.diff` in the tree root and
`m3-ams1-REPORT.md` in `revival/doc/claude-history/`, command style
for an unattended session). Also read `m3-rollout-runbook.md`'s
"Current standing per-batch checklist" section in full, and
`claude-history/fdplumb-REPORT.md` in full — this is the required
briefing `m3-batches.md` calls out for this directory (same one M2
required for its own pass here). This is one of the four flagged-risky
batches (T1, I2, AMS1, C1 per the 2026-07-30 amendment) that keep full
orchestrator pre-diagnosis instead of a delegate-side Gate 0 — the
orchestrator has already done that legwork below; apply it and go
straight to the real run. Do not re-derive it.

## Scope

**One directory, `ams/libs/ms`** — 114 nominal `.c` files, but see the
generated-file exclusion below (113 real). **Zero `.ch` files** — this
is a plain C library (`libmssrv.a`, the actual `MS_*` message-server
API implementation), not a Class subtree. That means **no `-pe`/`.eh`
Export rollout at all for this directory** — the only work here is a
plain `ansify --dir` K&R→ANSI pass. Don't add `CLASSFLAGS = -pe`
anywhere in this directory; there is nothing for it to apply to.

This directory already carries the M2-era `COMPILERFLAGS` guard
(`-std=gnu89 -Wno-implicit-int -Werror=implicit-function-declaration
-Wno-incompatible-function-pointer-types -Wno-return-type`) — confirmed
present in its `Imakefile`. Its own implicit-declaration surface was
already closed by M2; `ansify`'s job here is purely mechanical
conversion of existing K&R definitions to ANSI syntax, not declaration
discovery.

**Second, smaller task folded in per the 2026-07-31 wdc-approved
amendment** (`m3-batches.md`'s Wave 6 section — read the amendment
*and* the correction directly under it before starting, both below
too): close `ams/msclients/cui`'s COMPILERFLAGS gap (141 undeclared
functions across its 4 files, found at A1). This is secondary to the
main `ams/libs/ms` conversion and should not block this batch's
close-out if it proves larger than expected — see "Task 2" below for
the corrected mechanism (the amendment's original proposed mechanism
does not work; already verified empirically, do not redo the failed
approach).

## Pre-diagnosis already done (Task 1: `ams/libs/ms`)

### 1. Generated-source exclusion — `prsdate.c` is NOT real source, do not run `ansify` on it

Same mechanism I2 already documented for `atk/eq/eqparse.c` and
`atk/rofftext/num.c`: the Imakefile uses `Parser(prsdate,)` (the
`Parser(classname,bisonargs)` Imake macro) to generate `prsdate.c`/
`prsdate.h` from the real, fossil-tracked `prsdate.gra` grammar source
via `mkparser`/yacc, edited afterward by `prsdate.scr`. Confirmed:
`fossil finfo src/ams/libs/ms/prsdate.c` returns "no history for
file" — not fossil-tracked. **Exclude `prsdate.c` from the real
`ansify` run.** This is why the real file count is 113, not the
nominal 114 (`ls *.c | wc -l`) — same undercounting pattern I2 already
flagged for its two generated files.

As with I2's two generated directories, the subtree-local gate needs
an explicit `make depend` between `clean` and `install`
(`make clean && make depend && make -k install`, not
`make clean && make -k install`) so `prsdate.c`/`prsdate.h` actually
regenerate — don't skip or collapse this step.

### 2. `fdplumb` include-order history — read, but NOT currently a live risk; don't "fix" it

`claude-history/fdplumb-REPORT.md` (required reading above) and
`[[project_fdplumb_include_order_abi]]` describe a real historical bug:
`fdplumb.h`'s `#define open dbg_open` (etc.) poisons `open()`'s
declaration into a variadic prototype if `fcntl.h` hasn't been parsed
yet, corrupting the `mode` argument on arm64. The memory note's
original framing was caller-side ("`#include <andrewos.h>` must
precede `#include <ms.h>` in every file that reaches `fdplumb.h`").
**Verified today (orchestrator, before writing this prompt) that this
framing is now stale**: the 2026-07-17 fossil commit (`6782de786a`)
actually fixed this at the header level — `fdplumb.h` now
`#include`s `<sys/types.h>`/`<fcntl.h>` itself, inside its own
`#ifndef PLUMBFDLEAKS` guard, *before* the `#define open dbg_open`
block, with a comment explaining exactly why. This makes the fix
self-contained and immune to caller include order.

A full-directory audit (all 114 `.c` files in `ams/libs/ms`) found 17
files where `andrewos.h`/`fcntl.h` textually follows `#include <ms.h>`
(`bldattr.c`, `blddate.c`, `clonemsg.c`, `findmbox.c`, `getdiri.c`,
`getnmct.c`, `hdlnew.c`, `instinfo.c`, `msjournal.c`, `msparse.c`,
`muclient.c`, `newmail.c`, `papanote.c`, `parseraw.c`, `shrkdate.c`,
`site.c`, `util.c`) — **this is not a live risk given the header-level
fix; do not "fix" these by reordering includes**, that would be
unnecessary churn on files `ansify` has no other reason to touch.
Only re-flag this as live if you find `fdplumb.h` itself has
regressed (it's in `overhead/util/lib`, out of this batch's scope —
just confirm on sight if you happen to look at it, don't go looking
for it specially).

### 3. Standing checklist already run once, findings below — re-confirm quickly as part of your own pass

- **`.ch` presence** (check 1): zero. Confirmed above — no `-pe`/`.eh`
  work in this batch.
- **Predefined-macro typo grep** (check 2): clean, no
  single-underscore typos found.
- **Empty-parens lifecycle-method grep** (check 3): not applicable —
  no classes, no `InitializeClass`/`InitializeObject`/
  `FinalizeObject` anywhere in this directory.
- **Stranded old-style forward declaration vs. narrow ANSI param**
  (check 8, `grep -nE 'static\s+\w[\w ]*\s+\w+\(\);' *.c`): 40 raw
  hits, 8 of them in the excluded `prsdate.c` (ignore). The other 32
  were individually cross-checked against their real K&R definitions
  today — every one uses only `int`/`long`/`unsigned long`/pointer/
  struct-pointer parameters (`chzdirs.c:ConvertDropoffCode`,
  `bldattr.c:BuildHashVals`, `cvtold.c:IsNewFrom`, `epoch.c:RealEpoch`,
  all 9 in `rawdb.c`, all 8 in `msdir.c`, all 3 in `recon.c`, all 5 in
  `mswp.c`, `subswalk.c:RealSubsTreeWalk`, `util.c:CheckLabel`) — **zero
  narrow-byval-param risk found, this check is clear**. Still worth a
  quick re-grep after your real `ansify` pass in case conversion
  reveals something the pre-diagnosis missed, but don't expect to find
  anything here.
- **Installed-header grep for converted non-static helpers** (check
  4): the directory's only installed header, `ams/libs/hdrs/ms.h` (276
  lines), currently has **zero** empty-parens function declarations of
  any kind — clean baseline. Still run this for real once you know the
  actual set of non-static helpers `ansify` converts (same as every
  prior batch — this pre-check only confirms the starting state, not
  the post-conversion one).
- **Concurrent-commit merge check** (check 6) and the milestone-agnostic
  checks (liveness census, anchored `malloc`/`free`/`realloc`/`calloc`
  grep): not pre-run, yours to do during the real pass as usual.

## Task 1: `ams/libs/ms`

1. Real `ansify --dir src/ams/libs/ms`, excluding `prsdate.c` (rename
   it out of the way or otherwise keep `ansify` from touching it —
   whatever mechanism I2 used, match it).
2. Run checklist items 4 and 6 (installed-header grep, concurrent-commit
   check) for real, plus the milestone-agnostic checks, as noted above.
3. Investigate any DRIFT/skip finding the same way every prior M3 batch
   has — check `.ch` vs `.c` by hand (N/A here, no `.ch`, so this
   reduces to checking real call sites vs. what `ansify`'s DB/parser
   inferred), consult `porting-assessment.md`/`m3-rollout-runbook.md`
   for a matching pattern first, cite the specific finding you're
   matching against. Given the directory's size (113 real files, the
   single largest file count of any M3 batch so far) expect a higher
   raw count of routine fallout than smaller batches — that's expected
   given the scale, not itself a signal of trouble.
4. Subtree-local gate: `make -C <absolute-path> clean`, then `depend`
   (load-bearing here for `prsdate.c` regeneration, don't skip), then
   `-k install` — separate calls, absolute path, not chained, **twice**
   for determinism.
5. No tree-wide gate required for this batch (Wave 6 has a second
   batch, AMS2, after this one — the wave-end gate belongs there, per
   `m3-batches.md`).

## Task 2: `ams/msclients/cui` COMPILERFLAGS closure (secondary add-on)

**The amendment's originally-proposed mechanism does not work —
verified empirically by the orchestrator before delegating, do not
retry it.** `cui.h` is already `#include`d in all 4 of `ams/msclients/
cui`'s files (`cui.c`, `cuifns.c`, `morprntf.c`, `unixmach.c`).
Temporarily adding `#include <ms.h>` on top (real edit, real
`COMPILERFLAGS`, real `make -k all`, reverted afterward) leaves the
same ~141 names undeclared, because **neither header contains a single
function prototype** — both are type/struct/macro-only. `ms.h`'s own
header comment even says "This is only for the message server, not the
CUI. CUI clients should include cui.h instead" — but `cui.h` doesn't
declare the `MS_*`/`CUI_*` API either. Do not spend time trying
`#include` again.

The tree's actual established convention for this API (confirmed in
`ams/libs/cui/cuilib.c`, which already carries the M2 `COMPILERFLAGS`
guard and already declares most of this same function set) is
scattered old-style per-file `extern RETTYPE FuncName();` declarations
— the same idiom A1's `cvEng` fix already used. The correct approach:

1. Once you've converted `ams/libs/ms`'s definitions to ANSI in Task 1,
   you'll have each function's real typed signature in hand. Use that
   (and `ams/libs/cui/cuilib.c`'s existing declarations, where a
   function isn't in `ams/libs/ms` at all) to write proper `extern
   RETTYPE FuncName(ARGTYPES);` declarations for each of
   `ams/msclients/cui`'s undeclared external calls, placed directly in
   whichever of the 4 files calls them (matching the existing
   `cvEng`/`StripWhiteEnds` declaration style already in `cuifns.c`).
2. The orchestrator's own scouting run (temporary `COMPILERFLAGS` +
   `#include <ms.h>`, `-ferror-limit=0`, reverted) produced this list
   of 140 distinct undeclared names — treat it as a strong starting
   point, not gospel; re-derive it yourself the same way if anything
   looks off, since the `#include <ms.h>` addition may have changed
   which are visible relative to A1's original 141-count:

   ```
   AddHeads  AlterSubLine  CUI_CheckMailboxes  CUI_CheckVersion
   CUI_CloneMessage  CUI_CopyViceFileTails  CUI_CreateNewMessageDirectory
   CUI_DeleteMessage  CUI_DisambiguateDir  CUI_EndConversation
   CUI_FixAttribute  CUI_FlagUrgency  CUI_GenLocalTmpFileName
   CUI_GenTmpFileName  CUI_GetAMSID  CUI_GetAttrName
   CUI_GetBodyToLocalFile  CUI_GetCuid  CUI_GetFileFromVice
   CUI_GetHeaderContents  CUI_GetHeaders  CUI_GetPartialBody
   CUI_GetSnapshotFromCUID  CUI_HandleMissingFolder  CUI_Initialize
   CUI_MarkAsRead  CUI_MarkRepliedTo  CUI_NameReplyFile
   CUI_PrintBodyFromCUIDWithFlags  CUI_PrintUpdatesWithFlags
   CUI_ProcessMessageAttributes  CUI_PurgeMarkedDirectories
   CUI_ReconstructDirectory  CUI_RemoveDirectory  CUI_RenameDir
   CUI_ReportAmbig  CUI_ResendMessage  CUI_RewriteHeaderLine
   CUI_SetClientSignalHandler  CUI_SetClientVersion  CUI_SetPrinter
   CUI_StoreFileToVice  CUI_SubmitMessage  CUI_UndeleteMessage
   CUI_ValidateFile  CalcSkip  ChooseFromList  CloneMessage
   DescribeFlags  DescribeHeads  DisplayFile  DisplayMessage  EditFile
   EditLocalFile  ExposeSubscriptions  FindBodyStartInForwardedViceFile
   FlagUrgency  GetBodyFromCUID  GetBooleanFromUser  GetHeadersSinceDate
   GetNextCommand  GetStringFromUser  GetSubject  LowerStringInPlace
   MS_AppendFileToFolder  MS_CheckAuthentication  MS_ConvertOldMail
   MS_DebugMode  MS_DeleteAttr  MS_Die  MS_DisambiguateFile
   MS_DoIHaveMail  MS_EditMessage  MS_Epoch  MS_FastUpdateState
   MS_GetAssociatedTime  MS_GetDirAttributes  MS_GetDirInfo
   MS_GetPartialFile  MS_GetSearchPathEntry  MS_GetSubscriptionEntry
   MS_GetVersion  MS_InstallWelcomeMessage  MS_MatchFolderName
   MS_MergeDirectories  MS_NameChangedMapFile  MS_NameSubscriptionMapFile
   MS_OpenDebuggingPipescript  MS_ParseDate  MS_RebuildMasterUpdateFiles
   MS_RebuildOneMasterUpdateFile  MS_RebuildOneSubscriptionMap
   MS_RebuildSubscriptionMaps  MS_ScavengeDirectory  MS_SetAssociatedTime
   MS_SetSubscriptionEntry  MS_StorePartialFile  MS_TakeHints
   MS_UnlinkFile  MS_WriteUnscribedBodyFile  MapcarFunctionToFileLines
   MarkSeenLast  MoreSelect  ParseDirName  ParseFileName
   ParseMessageNumber  PrintCaption  PrintTimeStampAndNewline
   ProcessCommand  ProcessMultipleCommands  PutStringToViceFile
   RealWhoIs  RedirectOutput  ReportError  ReportSuccess
   ResetTerminalParams  SendSomeLines  SendSomeMail  SetDebugMode
   SetEditorToUse  SetTerminalParams  SpitOutChars  StepThroughMsgs
   StoreMessage  StorePartialFile  ULstrcmp  ULstrncmp  ViewMessage
   amsconfig  atoi  cui_prog_main  dbg_close  dbg_fclose  errprintf2
   fselect  getcharwithpausecheck  moreprintf  nontext  vdown  writeall
   ```

3. **A rough same-day categorization pass by the orchestrator (quick
   single-line grep, likely incomplete — verify properly yourself)**
   found only ~39 of these defined in `ams/libs/ms` and ~43 in
   `ams/libs/cui`; the rest didn't match a simple single-line grep,
   most plausibly because they're same-directory cross-file helpers
   within `ams/msclients/cui` itself with multi-line K&R definitions
   (`RETTYPE\nFuncName(args)`, which a single-line regex misses) — the
   same species A1 already fixed for `GetHeadersFn`/`MaybeFlagSomething`
   in this exact directory. Functions in that category don't need
   anything from `ams/libs/ms` at all — just an ordinary same-directory
   forward declaration, same as A1's fix. A few (`atoi`) are plain
   libc (needs `<stdlib.h>`); `dbg_close`/`dbg_fclose` are the
   `fdplumb`-renamed `close`/`fclose` (int-returning, non-variadic,
   safe — declare or pull in via ordinary header order, no ABI risk
   like the `open`/`dbg_open` case). Do the real categorization
   yourself rather than trusting this rough pass.
4. Once all real declarations are in place, add the same
   `COMPILERFLAGS` line every other M2/M3-guarded directory uses to
   `ams/msclients/cui`'s `Imakefile`, then gate the directory (`clean`,
   `depend`, `-k install`, twice).
5. **This task is secondary — do not let it block Task 1's close-out.**
   If it turns out substantially bigger than this pre-diagnosis
   suggests, stop, write up exactly what's done vs. remaining in the
   report, and flag it clearly for the orchestrator rather than
   presenting partial state as complete. A same-day follow-up commit
   (same pattern as the `atk/chart` COMPILERFLAGS follow-up) is a fine
   outcome if Task 1 and Task 2 end up split.

## Gate

Stop after `ams/libs/ms` gates clean (twice) and Task 2 is either
complete-and-gated-clean (twice) or clearly flagged as
partial/deferred with exact status. **Do NOT commit. Do NOT run any
AUIS GUI/terminal binary interactively** — the orchestrator will
present runtime-check suggestions to wdc separately after
independently re-verifying your work.

## Report

Write `revival/doc/claude-history/m3-ams1-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- `ansify --dir` results for `ams/libs/ms` vs. the pre-diagnosis above
  (flag any drift, especially anything the stranded-forward-decl or
  installed-header checks turn up that pre-diagnosis missed).
- Confirmation that `prsdate.c` was excluded and that `make depend`
  ran before `install` in the gate.
- Any new DRIFT/skip/compile-fallout finding not covered above, and
  how you resolved it (or, if genuinely unresolved, clearly flagged
  for the orchestrator — retype/signature rulings and hard-stop
  adjudication stay top-level per `rollout-procedure.md`'s Delegation
  section).
- Per-file `ansify` conversion counts (methods/helpers — no classprocs,
  no classes in this directory).
- Gate results (twice) for `ams/libs/ms`.
- Full status of Task 2: the real (corrected) categorization of all
  140 names, exactly which declarations were added where, gate results
  for `ams/msclients/cui` (twice) if completed, or precise remaining
  scope if deferred.
- `fossil status`/`fossil extras` confirming exactly which files
  changed, no commit made.
- A "Suggested runtime checks for wdc" section per
  `rollout-procedure.md`'s Runtime check rules (`nm -g` against
  `runapp`/the relevant `.do`/binary to find live consumers first —
  `libmssrv.a` is linked into `cuin`, `messages`, and other AMS
  clients; never launch GUI apps from the session; no saves against
  unversioned fixtures/mail folders).
- Anything that surprised you or didn't match this prompt's
  expectations.
