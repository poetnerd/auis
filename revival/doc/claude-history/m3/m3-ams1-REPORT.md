# M3 Wave 6, Batch AMS1: `ams/libs/ms` ansify rollout + `ams/msclients/cui` COMPILERFLAGS closure — REPORT

## 1. Status

**Both tasks complete and gate clean (twice each).** `ams/libs/ms`: real
`ansify` pass over all 113 real `.c` files (excluding the generated
`prsdate.c`), all fallout fixed, subtree gate (`clean` → `depend` →
`-k install`) clean twice. `ams/msclients/cui`: all 140 undeclared
external calls categorized and given real `extern` declarations, the
standard M2 `COMPILERFLAGS` guard added for real, subtree gate clean
twice. **No fossil commit made.** `fossil status`/`fossil extras` in
§9. `m3-ams1-session.diff` (tree root, scoped to `src/ams/libs/ms` and
`src/ams/msclients/cui`) has the full diff.

This batch surfaced a genuinely new, previously-uncatalogued `ansify`
parser gap (§4) and one genuine pre-existing latent bug (§6) — both
investigated, fixed, and flagged below per the prompt's Delegation
instructions. Task 2 also required departing from the prompt's
originally-suggested declaration style for exactly two functions
(§7) — flagged there with full reasoning.

## 2. What I did, in order

1. Read `m3-ams1-prompt.md` in full, then `sonnet-playbook.md`,
   `rollout-procedure.md`, `m3-rollout-runbook.md`'s "Current standing
   per-batch checklist" section, and `claude-history/fdplumb-REPORT.md`
   (the required briefing) — per the prompt's instructions for this
   flagged-risky, full-pre-diagnosis batch.
2. Confirmed the pre-diagnosis's two load-bearing facts directly:
   `fossil finfo src/ams/libs/ms/prsdate.c` → "no history for file"
   (generated, excluded); the directory's `Imakefile` already carries
   the M2 `COMPILERFLAGS` guard (confirmed at line 30, unchanged from
   pre-diagnosis).
3. Ran the real (non-dry-run) `ansify` pass on all 113 real files
   (positional file-list form, matching I2's mechanism for excluding a
   generated file — see §3).
4. Investigated and fixed all fallout: 10 same-run compile failures (a
   variant of the standing item-8 stranded-forward-declaration shape,
   §5), then a definitive, tool-logic-based sweep that found 14 more
   *silently* un-converted K&R functions ansify's own reporting never
   flagged (§4) — fixed all 14 by hand, plus every stale cross-file
   forward declaration for any of the 26 total fixed functions found
   tree-wide within the directory. One of those fixes exposed a
   genuine pre-existing call-site bug (§6), fixed with the safest
   available choice and flagged for confirmation.
5. Ran checklist items 4 (installed-header grep) and 6
   (concurrent-commit check) for real, plus the milestone-agnostic
   liveness census and anchored `malloc`/`free`/`realloc`/`calloc`
   grep (§8).
6. Gated `ams/libs/ms` clean twice (`make clean`, `make depend`,
   `make -k install`, separate calls, absolute path).
7. For Task 2: re-derived the 140-name undeclared-call list via the
   orchestrator's documented scouting mechanism (temporary
   `COMPILERFLAGS` + `-ferror-limit=0`, no `#include <ms.h>` retry per
   the prompt's explicit instruction not to repeat the already-failed
   approach) — got exactly the same 140 names. Categorized all 140
   (§7), wrote `extern` declarations into whichever of the 4 files
   call them, added the real `COMPILERFLAGS` line to the Imakefile,
   and gated clean twice — after resolving a real, substantially-
   larger-than-expected complication involving two pseudo-variadic
   functions (§7.3).

## 3. `ansify --dir` results for `ams/libs/ms` vs. pre-diagnosis

**`prsdate.c` exclusion confirmed working as pre-diagnosed.** Used the
positional-file-list form (`ansify <explicit list of 113 files>`,
matching I2's documented mechanism) rather than `--dir`, since `--dir`
would have picked up `prsdate.c` too. Verified `make clean` deletes
`prsdate.c`/`prsdate.h` (both are `y_FILE`/generated via
`Parser(prsdate,)`) and `make depend` regenerates them — ran this
exact sequence for every gate (never skipped or collapsed to
`clean && install`).

**First real run: 113 files, 10 compile failures, 0 DRIFT findings.**
Matches the pre-diagnosis's prediction of "no DRIFT" (no `.ch` here,
so DRIFT is structurally about signature-DB mismatches that can't
occur) but **the 10 compile failures were NOT predicted** — the
pre-diagnosis's item-8 check (stranded old-style forward declaration
vs. narrow ANSI param) used a `static`-only grep
(`static\s+\w[\w ]*\s+\w+\(\);`) and concluded "zero narrow-byval-param
risk found, this check is clear." The real fallout was almost entirely
`extern`-prefixed stranded declarations (10 of 12 instances), a
variant the pre-diagnosis's own grep pattern structurally could not
match. Full detail in §5.

**Second-order finding, not in the pre-diagnosis at all: a genuinely
new `ansify` parser gap, silently missing 14 real K&R function
definitions with zero self-reported skip/DRIFT.** Full detail in §4 —
this is the batch's most significant surprise and needed a definitive,
tool-logic-based re-scan (not just a sharper grep) to find completely.

**Re-confirmed after all fixes**: a final `ansify` re-run (all 113
files) reports **0 compile failures, 0 DRIFT findings**, and every
file shows "no K&R definitions converted" (full idempotency — nothing
left to convert). Confirmed via a from-scratch Python re-implementation
of `ansify`'s own `HDR`/`parse_decl_block` detection logic (see §4)
that zero K&R headers remain undetected anywhere in the 113 files.

## 4. NEW FINDING (flagged for orchestrator ruling): `ansify`'s K&R
   parameter-block parser silently drops any function whose
   declaration doesn't fit "one declaration statement per physical
   source line" — no skip, no DRIFT, no diagnostic of any kind

**This is a genuinely new, previously-uncatalogued tool-gap category**,
distinct from every parser-gap shape in O2/O4/B1/B3/A1's taxonomy
(`(void)`-misparse, `DECLARE<N>` macro, function-pointer-returning-
pointer, brace-glued-to-parameter/local-variable,
double-pointer-loses-a-star, array-brackets-before-name,
space-before-asterisk). All of those are **reported** skips
(`skipped ...: reason`) — the defining, worse characteristic of this
one is that it produces **zero output of any kind**: the function is
silently left untouched, counted nowhere, with no trace in `ansify`'s
own report that anything was missed.

**Root cause** (traced directly in `ansify`'s source,
`parse_decl_block`/`DECL_LINE`): the parser consumes a K&R parameter
block line-by-line, and `DECL_LINE`'s regex requires **each physical
line to be a single, complete, semicolon-terminated declaration
statement**. Two source shapes violate this and are common in this
1988-era codebase:
1. **A single declaration statement spanning multiple physical lines**
   (only the last line carries the terminating `;`) — e.g.
   `dbgprntf.c`'s `dbgprintf`: `char *format,\n *s1,\n ... *s20;`
   (21 lines, one statement).
2. **Multiple complete declaration statements crammed onto one
   physical line** — e.g. `cvtold.c`'s `rmlock`:
   `char name[]; int lockFD;` (two statements, one line) — `DECL_LINE`
   matches up through the first `;` then fails because trailing text
   remains on the line.

Either shape causes `parse_decl_block` to return `None` at the first
non-conforming line, which discards the entire candidate silently
(`convert_file`'s `if not cand: out.append(line); continue` path — no
skip is ever recorded because the code path that records skips
requires the candidate to have been accepted first).

**Found via**: not a routine grep. First 2 instances (`dbgprntf.c`'s
`dbgprintf`, `merge.c`'s `MS_EditMessage`) were found while
investigating checklist item 4's installed-header grep hit on
`dbgprintf()` in `ams.h`. Manual ad-hoc greps for the multi-line shape
then found 2 more but **also missed 3 real instances** (different
textual sub-shapes: a 2-line block whose second line ends in `;` not
`,`; a 2-statement-per-line block; a block preceded by a multi-
paragraph free-text comment with interior lines not starting with
`*`). Given three successive hand-written greps each missed real
instances, I wrote a definitive detector by importing `ansify`'s own
`HDR`/`RESERVED`/`BARE_PARAMS`/`parse_decl_block` and running them
directly against all 113 files — this is what actually closed out the
sweep (0 misses on re-run, confirmed twice).

**Full list — 14 functions, 11 files** (2 more, `dbgprintf` and
`MS_EditMessage`, were found and fixed before the definitive sweep
existed; both also appear as clean in the final 0-miss re-scan):

| File | Function | Real signature applied |
|---|---|---|
| `dbgprntf.c` | `dbgprintf` | `int dbgprintf(char *format, char *s1, ..., char *s20)` |
| `merge.c` | `MS_EditMessage` | `long MS_EditMessage(char *dirname, char *id, char *NewBodyFile, int Reparse)` |
| `merge.c` | `MS_MergeDirectories` | `long MS_MergeDirectories(char *SourceDirName, char *DestDirName)` |
| `cvtold.c` | `rmlock` | `static int rmlock(char name[], int lockFD)` |
| `cvtold.c` | `lock` | `static int lock(char *file, char *lockedFile, int *lockedFDp)` |
| `cvtold.c` | `SetHoldFromFile` | `static int SetHoldFromFile(char *fname, int *holdP)` |
| `epoch.c` | `AddWork` | `static int AddWork(char *Dir, int Recurse, int Depth)` |
| `epoch.c` | `DeleteThrough` | `static int DeleteThrough(char *dirname, char *date64, int anyKids)` |
| `getdiri.c` | `MS_GetDirInfo` | `long MS_GetDirInfo(char *DirName, int *ProtCode, int *MsgCount)` |
| `getpathe.c` | `MS_GetSearchPathEntry` | `int MS_GetSearchPathEntry(int which, char *buf, int lim)` |
| `mswp.c` | `MS_DomainHandlesFormatting` | `int MS_DomainHandlesFormatting(char *domname, int *codeP)` |
| `mvinvice.c` | `RenameEvenInVice` | `int RenameEvenInVice(char *ThisFileName, char *NewFileName)` |
| `restilde.c` | `ResolveTildes` | `int ResolveTildes(char *old, char **new, char *domain)` |
| `site.c` | `LookupInLocalDatabase` | `int LookupInLocalDatabase(PARSED_ADDRESS *Addr, int laType, char *IDpart, char *PostID, char *Domain, int UnderAMSDelivery, int NameSep, int MaxNameMatches, int *Answered, int *MswpCodeP)` |
| `strpself.c` | `SameName` | `static int SameName(char *n1, char *n2, int nlen, int namesep)` |
| `unscrib.c` | `MS_WriteUnscribedBodyFile` | `int MS_WriteUnscribedBodyFile(char *DirName, char *id, char *FileName)` |

All 16 (14 + the first 2) fixed by hand, matching `ansify`'s own
output conventions exactly (verified against `tidy_type`/`weave`'s
formatting rules in the tool source): real return type (or implicit
`int` where the K&R header had none), each parameter fully spelled out
in header order using its real declared type. `site.c`'s
`LookupInLocalDatabase` additionally had a ~10-line multi-paragraph
free-text documentation comment between the header and the parameter
block (the reason its interior lines broke the parser) — preserved
verbatim, moved to precede the now-collapsed signature line, with the
original per-parameter inline comments folded into the doc comment
(matching `ansify`'s own behavior of not preserving inline comments
during helper conversion).

**Also fixed: every stale cross-file `extern`/`static` empty-parens
forward declaration for these 16 functions found anywhere else in the
113 files** (16 additional declaration-only edits across 9 more
files — `convmail.c`, `disambig.c`, `enssubs.c`, `findmbox.c`,
`flushdir.c`, `hdlnew.c`, `purge.c`, `rawdb.c`, `rebldmap.c`,
`rebldmuf.c`, `renadir.c`, `subs.c`, `unscrib.c`, `util.c`, plus
`bldpvmap.c`/`findmbox.c`/`rawdb.c`/`rebldmap.c`/`rebldmuf.c` for
`MS_GetSearchPathEntry` specifically) — found by grepping each fixed
function's name tree-wide for `NAME\s*(\s*)\s*;` after each fix. None
of these were required for the gate to pass (pointer-only-param stale
declarations are ABI-compatible with a later full prototype in a
*different* translation unit; only same-TU conflicts are hard errors),
but left as an inconsistent mix of pre- and post-fix declarations
would be confusing and were free to fix while already touching each
function.

**What needs a ruling**: whether to fix `ansify`'s `DECL_LINE`/
`parse_decl_block` for real (handling both the multi-line-single-
statement and multi-statement-single-line shapes, and multi-paragraph
comment blocks) before the next batch that might hit it, or accept
per-instance hand-fixing as sufficient given the precedent A1 already
set for smaller parser gaps (Delegation ruling: tool construction
stays top-level, small instance counts don't by themselves justify a
standalone fix commit — but this instance count, 16 silently-missed
functions across 2 directories now, plus the fact that it's
**undetectable by `ansify`'s own report**, is larger and more
dangerous than any prior parser-gap finding). I did not attempt to
patch `ansify` itself.

## 5. Standing item-8 check: `extern`-prefixed stranded declarations,
   and typedef'd narrow types, both outside the pre-diagnosis's grep

The pre-diagnosis's item-8 check used
`grep -nE 'static\s+\w[\w ]*\s+\w+\(\);' *.c` (40 raw hits, 32 outside
`prsdate.c`, "every one uses only int/long/unsigned long/pointer/
struct-pointer parameters... zero narrow-byval-param risk found").
Real Gate-1 fallout: **12 stranded-declaration compile failures across
10 files**, of which **10 used `extern`, not `static`** (a keyword
variant the pre-diagnosis's pattern structurally cannot match) and
**2 used `static` but take `Boolean`-typed parameters**
(`mswp.c`'s `CheckFolderAddress`, `rawdb.c`'s `SetProgressMark`) —
`Boolean` is `typedef short Boolean` (confirmed:
`build/include/ams/ms.h:45`), a narrow, promotion-risky type the
pre-diagnosis's literal-type read apparently didn't resolve through
the typedef.

Full list of the 12 real conflicts (all: stale empty-parens
declaration vs. real narrow-or-pointer-typed definition, all fixed by
retyping the declaration to match the real definition):

| File | Function | Real signature |
|---|---|---|
| `direx.c` | `EnsurePrivacy` | `extern int EnsurePrivacy(char *DirName, Boolean SemiPrivate);` |
| `enssubs.c` | `EnsureSubMapCorrect` | `extern int EnsureSubMapCorrect(char *DirName, Boolean DoesExist);` |
| `enssubs.c` | `CheckSubMapCorrect` | `extern int CheckSubMapCorrect(char *DirName, char *MapFileName, char *NickName, Boolean DoesExist);` |
| `freemsg.c` | `FreeMessageContents` | `extern int FreeMessageContents(struct MS_Message *Msg, Boolean FreeSnapshot);` |
| `msdir.c` | `SetChainField` | `extern int SetChainField(struct MS_Message *Msg, struct MS_Directory *Dir, Boolean PlanningHeadWrite);` |
| `msdir.c` | `CloseNeedyDirs` | `extern int CloseNeedyDirs(Boolean InsistOnCloses);` |
| `msprint.c` | `PrintPendingRequests` | `extern int PrintPendingRequests(Boolean MustPrint);` |
| `mswp.c` | `CheckFolderAddress` | `static int CheckFolderAddress(char **recip, int *rcode, int *IsCertain, char *FullName, char *orgname, Boolean WouldCreate, char **pCellN);` |
| `rawdb.c` | `SetProgressMark` | `static int SetProgressMark(char *dirname, Boolean TurnOnMark, Boolean Quietly);` |
| `rebldmuf.c` | `CheckUpdateLine` | `extern int CheckUpdateLine(char *line, int *NumFastGood, int *NumSlowGood, int *NumBad, int *NumAbsent, int *NumProbablyGood, Boolean LineIsFromUpdateFile);` |
| `subs.c` | `SetFullProfileEntry` | `extern int SetFullProfileEntry(Boolean DoSubs, char *FullName, char *NickName, int status, Boolean DoProf, char *time64, int filedate, Boolean NeedToCheckPath);` |
| `update.c` | `UpdateState` | `extern int UpdateState(Boolean DoEverything);` |

All 12 confirmed fixed by direct `make <file>.o` recompilation before
proceeding, then the full directory re-gated.

**Recommendation for the standing checklist** (not applied by me —
Delegation ruling): broaden item 8's grep to
`(static|extern)\s+\w[\w ]*\s+\w+\(\s*\)\s*;` and, when manually
classifying a matched declaration's parameter types as
narrow/pointer/safe, resolve `typedef`s (at minimum `Boolean`) rather
than reading literal type keywords only.

## 6. Genuine pre-existing bug found and fixed:
   `unscrib.c`'s `UnformatMessage` called `FreeMessageContents` with
   1 argument instead of 2

While re-gating after all the above fixes, the directory-wide gate
failed with a *new* kind of error (not a declaration conflict):
`unscrib.c:163:28: error: too few arguments to function call, expected
2, have 1`. This is `UnformatMessage`'s call
`FreeMessageContents(Msg);` — `FreeMessageContents`'s real signature
(fixed in §5) is `(struct MS_Message *Msg, Boolean FreeSnapshot)`, two
params. This is the **only** call site tree-wide missing the second
argument (`freemsg.c:56`'s own internal call supplies both correctly).

**This is a real, ~30-year-old latent bug**, invisible under K&R
because an unspecified-args declaration never checks argument count;
my §5 fix (giving `FreeMessageContents` a real 2-arg prototype) is
what surfaced it, the same way A1's `helpa.c`/`cuifns.c` fixes
surfaced genuinely new-shaped fallout. Examined `FreeMessageContents`'s
body: `FreeSnapshot` controls only whether `Msg->Snapshot` gets freed;
everything else is unconditional. `UnformatMessage`'s purpose is to
reformat a message in place (write it out, free the in-memory raw
content, re-read and re-parse) — the `Msg` struct itself survives and
is reused, so freeing `Msg->Snapshot` here looks semantically wrong
(the snapshot summary should still be valid after a reformat that
doesn't change the message's actual content). Under the pre-existing
K&R call, the second argument's real value was whatever garbage
happened to be in that argument slot — genuinely undefined, not a
reliably-FALSE default.

**Fix applied**: `FreeMessageContents(Msg, FALSE)` — the conservative
choice (don't free the snapshot), reasoned as above. **Flagging this
choice for confirmation** rather than presenting it as unambiguously
correct — this is a real behavioral judgment call about 1988 code
intent, the kind of thing `rollout-procedure.md`'s Delegation section
reserves for a human ruling. If the correct semantics turn out to be
"yes, free the snapshot here," this is a one-line change
(`FreeMessageContents(Msg, TRUE);`) rather than something requiring
further investigation — I made the safer of the two choices (a
leaked/stale snapshot is a much smaller failure mode than a
use-after-free if something else expects the snapshot to survive)
specifically so an incorrect guess fails safe.

## 7. Task 2: `ams/msclients/cui` COMPILERFLAGS closure — full status

### 7.1 Re-derivation of the 140-name list

Ran the same scouting mechanism as the orchestrator (temporary
`COMPILERFLAGS` with `-Werror=implicit-function-declaration
-ferror-limit=0`, **without** retrying the already-failed
`#include <ms.h>` addition, per the prompt's explicit instruction) on
all 4 files (`cui.c`, `cuifns.c`, `morprntf.c`, `unixmach.c`).
Extracted 140 distinct undeclared names — **exact byte-for-byte match**
against the orchestrator's list in the prompt. No drift.

### 7.2 Real categorization of all 140 names

| Category | Count | Mechanism |
|---|---|---|
| Defined in `ams/libs/ms` (this batch's Task 1 directory) | 36 | Cross-checked against `libmssrv.a`'s real (post-Task-1) non-static symbol table; signatures taken directly from the real, now-ANSI definitions |
| Defined in `ams/libs/cui` (`cuilib.c`/`andmchs.c`; confirmed `andmchs.o`, not `pcmchs.o`, is what's actually archived into `libcui.a` per its Imakefile's `OBJS`) | 46 | Cross-checked against `libcui.a`'s real symbol table; signatures extracted via `ansify`'s own `parse_local_decls`/`weave` logic run in-process against `cuilib.c`/`andmchs.c` (18 needed manual extraction — same multi-line-K&R-block shape as §4, confirming this parser gap isn't unique to `ams/libs/ms`) |
| Same-directory helpers (defined in one of the 4 `cui` files, called before their own later definition — same species A1's `GetHeadersFn`/`MaybeFlagSomething` already fixed) | 55 | `ams/msclients/cui` was already `ansify`-converted by batch A1 (Wave 5) — most already fully typed; 3 (`SendSomeMail`, `RealWhoIs`, `moreprintf`) hit the same §4 parser gap and needed the same manual-signature treatment |
| `overhead/util/lib` (already ANSI) | 7 | `dbg_close`, `dbg_fclose` (fdplumb-renamed `close`/`fclose`, confirmed int-returning non-variadic, no ABI risk), `writeall`, `ULstrcmp`, `ULstrncmp`, `vdown`, `fselect` |
| Other AMS libraries | 3 | `CUI_EndConversation` (`ams/libs/nosnap/nosnap.c` — confirmed via `NOSNAPLIBS`/`libcuin.a`'s real `OBJS = nosnap.o`, not the `SNAP_ENV`-only `cuisnap.c`), `LowerStringInPlace` (`ams/libs/shr/utils.c`, `libmsshr.a`), `amsconfig` (`overhead/mail/lib/amsconf.c`, already ANSI, `libmail.a` — confirmed distinct from an unrelated same-named K&R function in `ams/libs/cui/pcmchs.c` that isn't linked here) |
| Plain libc | 1 | `atoi` — added `#include <stdlib.h>` to `cuifns.c` (its only caller) instead of an `extern` |
| **Total** | **140** | |

The 43/39/58 rough same-day orchestrator estimate (§ of the prompt)
undercounted the `ams/libs/ms` and `ams/libs/cui` buckets and
overcounted "remaining" — the real split (36/46/55/7/3/1 above) is
close but not identical; the difference is almost entirely the
`overhead/util/lib` and "other AMS libraries" categories the rough
pass folded into "remaining."

### 7.3 Substantially bigger than expected: two pseudo-variadic
   functions broke 177 call sites, needed a real declaration-strategy
   decision — RESOLVED, but flagged for confirmation

`moreprintf` (`morprntf.c`) and `errprintf2` (`cuifns.c`) are 1988-era
"many fixed named params" pseudo-variadic functions — the same species
as `ams/libs/ms`'s `dbgprintf` (§4): a large fixed parameter list
(`moreprintf`: 21; `errprintf2`: 13), called throughout `cui.c`/
`cuifns.c` with far fewer arguments than declared, relying on K&R's
tolerance for omitted trailing arguments (never read, since the
`printf`-style format string passed as the first argument never
references those slots).

**First attempt (typed-prefix-plus-`...`-variadic declaration) failed
with 177 compile errors** — declaring `moreprintf(char *format, ...)`
made every call supplying fewer than the format string's own worst-case
need fine, but I had also initially given both functions a plain fixed
full-arity typed declaration (matching Task 2's default recipe), which
directly conflicts with dozens of real, intentionally-under-supplied
call sites. Re-derived the true minimum required prefix
(`moreprintf`: `format` only; `errprintf2`: always at least
`s1, i, s2, s3, s4` — confirmed against every real call site) and
switched both to `(required-prefix, ...)`.

**That still failed for `errprintf2` specifically**, with a
`conflicting types` error at its own definition in the same file
(`cuifns.c:312`): `errprintf2`'s real definition is a **fully
ANSI-typed, non-variadic, fixed-13-argument function** (unlike
`moreprintf`, which stays K&R in `morprntf.c`) — a `...`-variadic
declaration is fundamentally incompatible with that fixed-arity
definition when both are visible in the same translation unit.

**Final fix, applied to both functions in all 3 places they're
declared** (`cui.c`, `cuifns.c` ×2): **old-style empty-parens
"unspecified arguments" declarations** — `extern int moreprintf();`
and `extern int errprintf2();`. This is not a fallback or a shortcut;
it is the C-standard-correct answer for this exact situation: an
unspecified-args declaration (a) satisfies
`-Werror=implicit-function-declaration` (an explicit declaration of
any kind counts as "declared"), (b) is compatible with **any** number
of arguments at a call site (the compiler performs no argument-count
or type checking against it at all — this is *why* the original K&R
code worked for decades), and (c) is compatible with a later
full-prototype definition in the same file (unlike a typed or
variadic declaration) — this is the exact same "unspecified-args is
always safe against a later full prototype" reasoning already
confirmed safe in §4's `ams.h`/`dbgprintf` cross-directory check,
just applied deliberately here rather than incidentally.

**Verified**: both gate runs (`clean`/`depend`/`-k install`, twice)
came back with **0 errors** after this fix; `cuin` linked and
installed successfully both times (confirmed via the link-and-install
lines in the build log, not just the file's presence).

**Flagging this for confirmation** (per the prompt's "stop, flag
clearly" instruction for a substantially-bigger-than-expected Task 2
complication) even though I resolved it and the gate is clean: the
choice to leave `moreprintf`/`errprintf2` deliberately unspecified-args
rather than genuinely converting them to real ANSI variadic (`...`
with `stdarg.h`/`vsnprintf` in their own bodies) is conservative and
scope-respecting (touches zero call sites and zero function bodies,
matching the letter of "add declarations, don't fix anything outside
this batch's scope") but leaves the underlying 1988-era pseudo-vararg
design pattern itself unconverted — a legitimate real-ANSI-variadic
rewrite of `moreprintf`/`errprintf2` (and `ams/libs/ms`'s `dbgprintf`)
would be its own, separately-scoped piece of work if ever wanted.

### 7.4 Declarations added — exactly where

- `cui.c`: 111 `extern` declarations (110 typed + `moreprintf`'s
  unspecified-args form) inserted as one block after the existing
  `extern char CUI_VersionString[];` line, before any function
  definition.
- `cuifns.c`: 43 `extern` declarations (41 typed + `moreprintf`/
  `errprintf2`'s unspecified-args forms) inserted after the existing
  `rpc_nerr` extern block, in the same style as A1's `cvEng`/
  `StripWhiteEnds` addition; `#include <stdlib.h>` added for `atoi`.
- `morprntf.c`: 1 declaration (`SpitOutChars`, same-file forward
  reference) inserted after the existing `GetLine` extern.
- `unixmach.c`: 5 declarations inserted after `#include <cuimach.h>`,
  before `main()`.
- `Imakefile`: added the standard M2-era `COMPILERFLAGS` line (matching
  `ams/libs/ms`'s and every other M2/M3-guarded directory's exact
  wording).

### 7.5 Gate results

Both runs (`make clean`, `make depend`, `make -k install`, separate
calls, absolute path) clean — 0 `error:` lines. Second run's log
confirms `cuin` relinked (`cc ... -o cuin cui.o cuifns.o unixmach.o
morprntf.o <all NOSNAPLIBS> ...`) and installed
(`install -c -m 0555 cuin .../build/bin/cuin`, plus the `cui` symlink)
both times.

## 8. Standing checklist for `ams/libs/ms` — items 4, 6, milestone-agnostic

**Item 4 (installed-header grep for converted non-static helpers).**
Extracted all 333 real non-static symbols from the rebuilt
`libmssrv.a` via `nm -g`, grepped `build/include` for an empty-parens
declaration of any of them. **3 hits, all benign, all investigated:**
- `MS_FastUpdateState()`/`MS_UpdateState()` in `atk/ams.ch`/
  `atk/amsn.ch` — both are genuinely zero-argument functions (`.c`
  confirms `MS_FastUpdateState() { return(UpdateState(FALSE)); }`, no
  params ever) — the `.ch` empty-parens entries are correct, not
  stale.
- `dbgprintf()` in `ams/libs/hdrs/ams.h` (`extern int dbgprintf();`,
  right below a `/*VARARGS*/` lint comment) — confirmed
  `dbgprntf.c` does not itself include `ams.h` (no same-TU conflict),
  and since every one of `dbgprintf`'s 21 parameters is `char *`
  (pointer, no default-argument-promotion risk), an unspecified-args
  declaration elsewhere in the tree calling it is ABI-safe regardless
  of the real definition's argument count — same reasoning independently
  re-derived and applied deliberately in §7.3.

**Item 6 (concurrent-commit check).** `fossil status` checked
repeatedly throughout the session; checkout stayed at
`53ab74a1c94d29116ed7ed90037b7c4047106f20` the entire time (matches
the parent-commit's own prior session), zero concurrent commits
landed.

**Milestone-agnostic: liveness census.** `ams/libs/ms` confirmed live
via a direct dependency grep (`grep -rl libmssrv src/*/*/Imakefile`):
linked into `ams/ms`, `ams/msclients/{imapsync,vui,cui,nns}`,
`atkams/messages/lib` (the `messages` GUI backend's `amsn.do`), and
`rdemo/messages`.

**Milestone-agnostic: anchored `malloc`/`free`/`realloc`/`calloc`
grep.** 46 files call malloc-family functions (45 real +
`prsdate.c`, excluded); **all 45 real files already have
`#include <stdlib.h>`** — clean, no fallout needed here (a difference
from several prior batches, e.g. A1, which found missing includes in
most of their directories).

## 9. `fossil status` / `fossil extras`

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
checkout:     53ab74a1c94d29116ed7ed90037b7c4047106f20 2026-07-31 23:22:09 UTC
parent:       692a421c14609973ea226866cea84769d513f75c 2026-07-31 23:18:02 UTC
tags:         andrew-6.4
comment:      docs: fold ams/msclients/cui COMPILERFLAGS closure into AMS1 scope (user: wdc)
ADDED      revival/doc/m3-ams1-prompt.md
EDITED     revival/doc/m3-batches.md
EDITED     src/ams/libs/ms/<110 files>
EDITED     src/ams/msclients/cui/Imakefile
EDITED     src/ams/msclients/cui/cui.c
EDITED     src/ams/msclients/cui/cuifns.c
EDITED     src/ams/msclients/cui/morprntf.c
EDITED     src/ams/msclients/cui/unixmach.c
```

**`revival/doc/m3-ams1-prompt.md` (ADDED) and `revival/doc/m3-batches.md`
(EDITED) predate this session** — already present/dirty in the
checkout before I started (part of the orchestrator's own baseline
setup for this batch, per the checkin comment: "docs: fold
ams/msclients/cui COMPILERFLAGS closure into AMS1 scope"). Not my
work; `m3-ams1-session.diff` is scoped to just
`src/ams/libs/ms`/`src/ams/msclients/cui` and excludes them.

**110 of the 113 real `ams/libs/ms` files are `EDITED`** — the 3
untouched (`chains.c`, `msjournal.c`, `substr.c`) genuinely contain no
K&R function definitions of any kind (confirmed: `ansify` reports "no
K&R definitions converted" for all three both before and after this
session, and my definitive parser-logic sweep, §4, found zero missed
candidates in them either).

**No fossil commit made.** `fossil extras` shows only ordinary
pre-existing build byproducts (`Makefile`, `install.time`, `.o`/`.a`/
`.do` files, prior sessions' `*-session.diff`/`*-REPORT.md` files) —
nothing new or unexplained.

## 10. Per-file `ansify` conversion counts

All counts below are `ansify`'s own reported methods/classprocs/
helpers (always 0/0/N — no `.ch` files, no classes in this directory);
"+hand-fixed" notes the functions from §4/§5/§6 requiring manual
intervention beyond what `ansify` converted on its own.

| File | Helpers | Notes |
|---|---|---|
| abspath.c | 1 | |
| addhdr.c | 4 | |
| altsnap.c | 1 | |
| apndfile.c | 8 | |
| autherr.c | 1 | |
| authuid.c | 1 | |
| bldattr.c | 4 | |
| bldcapt.c | 3 | |
| blddate.c | 1 | |
| bldpvmap.c | 1 | +1 stale-decl consistency fix (`MS_GetSearchPathEntry`) |
| bldreply.c | 1 | |
| bldwide.c | 1 | |
| chains.c | 0 | genuinely nothing to convert |
| chzdirs.c | 3 | |
| ckmail.c | 1 | |
| ckmiss.c | 1 | |
| clonemsg.c | 2 | +1 stale-decl consistency fix (`SetChainField`) |
| cmpfts.c | 2 | |
| convmail.c | 1 | +1 stale-decl consistency fix (`RenameEvenInVice`) |
| criterr.c | 1 | |
| crtdir.c | 1 | |
| cvtold.c | 2 | +3 hand-fixed (§4: `rmlock`, `lock`, `SetHoldFromFile`) + 4 fix-tool-inserted fwd-decls retyped |
| dbgmode.c | 1 | |
| dbgprntf.c | 0 | +1 hand-fixed (§4: `dbgprintf`) |
| dfltdir.c | 1 | |
| direx.c | 2 | +1 item-8 fix (§5: `EnsurePrivacy`) |
| disambig.c | 5 | +1 stale-decl consistency fix (`ResolveTildes`) |
| enssubs.c | 4 | +2 item-8 fixes (§5: `EnsureSubMapCorrect`, `CheckSubMapCorrect`) + 1 stale-decl consistency fix (`RenameEvenInVice`) |
| epoch.c | 4 | +2 hand-fixed (§4: `AddWork`, `DeleteThrough`) + 3 stale-decl consistency fixes |
| fatalerr.c | 1 | |
| findmbox.c | 3 | +1 stale-decl consistency fix (`MS_GetSearchPathEntry`) + 1 (`ResolveTildes`) |
| fixsub.c | 1 | |
| flush.c | 2 | |
| flushdir.c | 2 | +1 stale-decl consistency fix (`RenameEvenInVice`) |
| freedir.c | 1 | |
| freemsg.c | 3 | +1 item-8 fix (§5: `FreeMessageContents`) |
| gentname.c | 2 | |
| getascft.c | 1 | |
| getasct.c | 1 | |
| getbody.c | 1 | |
| getdiri.c | 0 | +1 hand-fixed (§4: `MS_GetDirInfo`) |
| getfile.c | 1 | |
| getfmt.c | 1 | |
| gethdr.c | 1 | |
| getnmct.c | 1 | |
| getnsub.c | 1 | |
| getnth.c | 1 | |
| getpathe.c | 0 | +1 hand-fixed (§4: `MS_GetSearchPathEntry`) |
| getsnap.c | 1 | |
| getsubs.c | 1 | |
| getvconf.c | 1 | |
| getvers.c | 1 | |
| hdlnew.c | 51 | +1 stale-decl consistency fix (`RenameEvenInVice`) |
| hdlpref.c | 1 | |
| headers.c | 1 | |
| init.c | 5 | |
| instinfo.c | 1 | |
| inventid.c | 1 | |
| itops.c | 1 | |
| logging.c | 1 | |
| logit.c | 3 | |
| merge.c | 0 | +2 hand-fixed (§4: `MS_EditMessage`, `MS_MergeDirectories`) + 1 stale-decl consistency fix (`RenameEvenInVice`) |
| msdir.c | 22 | +2 item-8 fixes (§5: `SetChainField`, `CloseNeedyDirs`) |
| msjournal.c | 0 | genuinely nothing to convert |
| msparse.c | 2 | |
| msprint.c | 7 | +1 item-8 fix (§5: `PrintPendingRequests`) |
| mswp.c | 31 | +1 item-8 fix (§5: `CheckFolderAddress`) + 1 hand-fixed (§4: `MS_DomainHandlesFormatting`) |
| mtchfold.c | 1 | |
| muclient.c | 2 | |
| mungenew.c | 2 | |
| mvinvice.c | 0 | +1 hand-fixed (§4: `RenameEvenInVice`) |
| namechg.c | 3 | |
| namemap.c | 1 | |
| newmail.c | 1 | |
| nonfatal.c | 1 | |
| openpipe.c | 1 | |
| papanote.c | 2 | |
| parseraw.c | 2 | |
| pfmsg.c | 1 | |
| prettyn.c | 2 | |
| prsdlib.c | 6 | |
| purge.c | 1 | +1 stale-decl consistency fix (`RenameEvenInVice`) |
| rawdb.c | 44 | +1 item-8 fix (§5: `SetProgressMark`) + 1 stale-decl consistency fix (`MS_GetSearchPathEntry`) |
| rawfil.c | 2 | |
| rebldmap.c | 1 | +2 stale-decl consistency fixes (`MS_GetSearchPathEntry`, `RenameEvenInVice`) |
| rebldmuf.c | 6 | +1 item-8 fix (§5: `CheckUpdateLine`) + 1 stale-decl consistency fix (`MS_GetSearchPathEntry`) |
| recon.c | 47 | +1 stale-decl consistency fix (`RenameEvenInVice`) |
| redslash.c | 1 | |
| renadir.c | 4 | +1 stale-decl consistency fix (`RenameEvenInVice`) |
| reply.c | 3 | |
| restilde.c | 0 | +1 hand-fixed (§4: `ResolveTildes`) |
| rmvdir.c | 2 | |
| rsndhdr.c | 1 | |
| safeexit.c | 1 | |
| scavenge.c | 3 | |
| setasct.c | 1 | |
| setknell.c | 1 | |
| setsubs.c | 1 | |
| shrkdate.c | 1 | |
| shrkname.c | 1 | |
| site.c | 2 | +1 hand-fixed (§4: `LookupInLocalDatabase`) + 1 stale-decl consistency fix (`LookupInLocalDatabase`) |
| stack.c | 7 | |
| storfile.c | 1 | |
| strpself.c | 2 | +1 hand-fixed (§4: `SameName`) |
| submsg.c | 4 | |
| subs.c | 26 | +1 item-8 fix (§5: `SetFullProfileEntry`) + 2 stale-decl consistency fixes (`RenameEvenInVice`, `ResolveTildes`) |
| substr.c | 0 | genuinely nothing to convert |
| subswalk.c | 2 | |
| uattrs.c | 3 | |
| unlnfile.c | 1 | |
| unscrib.c | 2 | +1 hand-fixed (§4: `MS_WriteUnscribedBodyFile`) + 1 genuine bug fix (§6: `FreeMessageContents` call-site arg count) + 1 stale-decl consistency fix |
| update.c | 3 | +1 item-8 fix (§5: `UpdateState`) |
| util.c | 3 | +1 stale-decl consistency fix (`ResolveTildes`) |

**Totals**: 415 helpers converted by `ansify` itself across 104 files;
16 functions across 12 files hand-converted (§4); 12 stranded-
declaration compile failures across 10 files fixed (§5); 1 genuine
pre-existing bug fixed (§6); ~16 additional stale-declaration
consistency edits across 9 more files (not required for the gate,
done for consistency while already touching each function).

## 11. Gate results (both tasks, both runs)

**`ams/libs/ms`**: `make -C .../src/ams/libs/ms clean`, `depend`,
`-k install` — separate calls, absolute path — run twice (the final
two runs, after all fixes including §6's late-discovered bug; earlier
gate attempts that hit real fallout are not counted as part of the
"twice"). Both clean, 0 `error:` lines. `prsdate.c`/`prsdate.h`
confirmed absent after `clean`, regenerated after `depend`, both
times.

**`ams/msclients/cui`**: same rhythm, run twice after §7.3's
declaration-strategy fix (earlier attempts that hit the 40-then-177
`moreprintf`/`errprintf2` errors are not counted). Both clean, 0
`error:` lines; `cuin` confirmed relinked and reinstalled both times
via the build log.

No tree-wide gate run — not required for this batch per the prompt
(Wave 6 has a second batch, AMS2, after this one; the wave-end gate
belongs there).

## 12. Open questions / what surprised me

- **§4's silent parser gap is the real surprise of this batch** — three
  successive hand-written greps each missed real instances before a
  from-scratch reimplementation of `ansify`'s own detection logic
  closed the sweep definitively. Worth considering whether the
  standing checklist should gain a step recommending exactly this
  technique (import `ansify`'s own `HDR`/`parse_decl_block` and run
  them directly) for any future directory this size, rather than
  relying on ad-hoc greps to prove completeness.
- **The pre-diagnosis's item-8 check had two real gaps** (`extern`
  vs. `static`-only pattern; typedef'd narrow types like `Boolean`
  not resolved) that together accounted for essentially all of the 12
  real stranded-declaration conflicts — worth folding into the
  standing checklist's item 8 wording.
- **Task 2 turned out to have a real, non-mechanical complication**
  (§7.3) that the prompt's own pre-diagnosis couldn't have anticipated
  (it only became visible once `ams/libs/ms`'s own real typed
  signatures were in hand and cross-referenced against live call
  sites) — resolved, but the declaration-strategy choice for
  `moreprintf`/`errprintf2` is flagged for confirmation, not presented
  as beyond question.
- `ams/msclients/cui` having already been `ansify`-converted by A1
  (Wave 5) was not mentioned in this batch's prompt — discovered
  directly (scratch-copy dry-run against the real files returned all
  zeros) and adjusted the categorization approach accordingly; worth a
  note in `m3-batches.md` for anyone else who touches this directory.
- Otherwise this batch matched the prompt's own framing: `ams/libs/ms`
  is genuinely the largest-file-count M3 directory so far, and its
  raw fallout count (12 item-8 fixes, 16 parser-gap fixes) is larger
  in absolute terms than prior batches but proportionally in line with
  its size — no single fix was individually surprising except §4 and
  §6.

## 13. Suggested runtime checks for wdc

Per `rollout-procedure.md`'s Runtime check rules: `nm -g` against
`runapp`/the relevant `.do`/binary first to find live consumers, never
launch GUI apps from this session, no saves against unversioned
fixtures.

**`libmssrv.a` (Task 1) — confirmed linked into multiple live
consumers.** `nm -g` on `build/dlib/atk/amsn.do` (the `messages` GUI
app's mail-store backend) and `build/bin/cuin` both show real
`_MS_*` symbols resolved from `libmssrv.a`, not undefined. Suggested
checks:
- **`messages`** (GUI, native Terminal.app only, never from this
  session): open a folder, read a message, check mail — exercises the
  bulk of `libmssrv.a`'s directory/message-read path
  (`msdir.c`/`rawdb.c`/`recon.c`, the three largest-helper-count files
  in the table above).
- **`cuin`** (CLI): run a normal session against a real mailbox —
  `dirinfo <folder>` (exercises `getdiri.c`'s `MS_GetDirInfo`, hand-
  fixed in §4), reformat/edit a message if `cuin` exposes that command
  (exercises `unscrib.c`'s `UnformatMessage`/`FreeMessageContents`
  path, §6's bug fix specifically — the most important single check
  in this batch, since it's the one place a wrong judgment call could
  show up as a leak or crash), a subscription-map rebuild command if
  available (exercises `subs.c`/`enssubs.c`'s §5 fixes).
- Point specifically at `MS_EditMessage`/`MS_MergeDirectories` if
  `cuin` or `messages` expose an "edit in place" or "merge folders"
  command — both are `merge.c` functions hand-fixed in §4 and were
  previously silently unconverted (K&R, but functioning) so this is a
  genuine first-time-typed exercise, not just a re-confirmation.

**`ams/msclients/cui`'s `cuin`/`cui` (Task 2) — confirmed built and
installed.** Suggested checks:
- Any command that prints via `moreprintf` with a short format string
  (nearly everything `cuin` prints) exercises §7.3's unspecified-args
  declaration choice — if this were somehow wrong, the failure mode
  would be a build/link error, not a runtime one (unspecified-args is
  a compile-time-only relaxation), so no special runtime risk here
  beyond ordinary use.
- Any `CUI_*`-prefixed command exercising message store/retrieve
  (`CUI_GetHeaders`, `CUI_DeleteMessage`, `CUI_SubmitMessage`, etc.)
  exercises the 46 `ams/libs/cui`-sourced declarations from §7.2.
- `dirinfo`/`headers`/`flag` were already runtime-confirmed working by
  wdc after A1's Wave 5 session (per that batch's own report) — this
  batch didn't change any of A1's fixes, only added declarations for
  calls A1's own session didn't need to reach.

**No live consumer / build-time only**: none for this batch — both
`libmssrv.a` and `cuin` are confirmed live, exercised binaries.
