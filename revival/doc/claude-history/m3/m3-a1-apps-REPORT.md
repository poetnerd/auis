# M3 Wave 5, Batch A1: 9 app/leaf directories — ansify + `-pe`/`.eh` rollout — REPORT

## 1. Status

**Gate 1 complete.** Real (non-dry-run) `ansify --dir` ran on all 9
directories; `CLASSFLAGS += -pe` added for real (and left in place) on
the 3 directories with `.ch` files (`atk/typescript`, `atk/help/src`,
`atk/ez`); all fallout fixed; every directory gated clean twice,
serially; one wave-end tree-wide gate ran twice, both clean except the
two already-documented, pre-existing `contrib/zip/utility/ltapp.c`
errors (unrelated to this batch, queued for Wave 7 C2). **No commit
made.** `fossil status` shows exactly 32 edited files (29 batch source
files + 3 Imakefiles) — see §10.

Both Gate 0 rulings are recorded below for context (§3.1: the
orchestrator root-caused and fixed `ansify --build-db`'s same-classname
collision bug for real, superseding my temporary hand-patch — commits
`fa0705b6`/`214801d5`; §3.2: agreed to proceed with the glued-asterisk
spacing fix by hand, as I proposed). Gate 0's original findings/
classification below are preserved as written; Gate 1 results are
appended starting at §9.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `m3-a1-apps-prompt.md`,
   `rollout-procedure.md`, `m3-rollout-runbook.md` in full — the
   "Current standing per-batch checklist" and "Session structure going
   forward" sections, plus all findings entries O1 through I2 — and
   `porting-assessment.md` §14 (Delegation ruling, tool verdicts) and
   §17 (`InitializeClass`/`InitializeObject`/`FinalizeObject`
   special-casing, both documented exceptions), plus `m3-batches.md`'s
   Wave 5 section for scope confirmation (matches the prompt exactly:
   9 directories, 29 files).
2. Confirmed the `ansify` signature DB (`build/desc/`) needed a
   rebuild (5 classes in this batch's own directories were missing
   `.desc` files entirely — `typescript`, `typescriptapp`, `typetext`,
   `pipescriptapp`, `helpapp` — none reported as `--build-db` failures,
   just stale/absent). Ran `ansify --build-db`: 564 classes, 2 failed
   (both pre-existing, already documented: `atk/value/sliderv.ch`'s
   case-insensitive collision with `atk/image/sliderv.ch`, fixed
   2026-07-30 but the collision itself recurs every rebuild by design;
   `contrib/atkbook/console/disk1.ch`, the known unresolvable-superclass
   dead directory). Neither is new, neither touches this batch.
3. For each of the 9 directories, in the prompt's order: `.ch`
   presence check → `ansify --dry-run --dir` → the full standing
   checklist (macro-typo grep, empty-parens-lifecycle grep,
   installed-header grep for non-static helpers, restated-lifecycle-
   param `.ch` check with real `.c` param-count verification, `fossil
   status` concurrent-commit check, liveness census, anchored
   `malloc`/`free`/`realloc`/`calloc` grep, stranded-forward-
   declaration grep with real-definition parameter-type cross-check).
4. Investigated two anomalies the dry-run surfaced that didn't fit any
   documented shape (§3.1, §3.2) — for both, per the prompt's
   allowance, temporarily added `CLASSFLAGS = $(CLASSINCLUDES) -pe` to
   the relevant directory's `Imakefile`, ran `make Makefile`,
   force-generated the `.eh` file(s) with the real `class` binary,
   compile-checked (`make <file>.o`), then `fossil revert`ed the
   Imakefile and deleted every scratch `.eh`/`.ih`/`.o` file before
   moving on. Confirmed via `fossil status` after each that nothing
   was left dirty.
5. Classified every remaining finding against the runbook/
   `porting-assessment.md` taxonomy, citing the specific entry each
   matches.

## 3. Findings needing an orchestrator ruling

### 3.1 UNCLASSIFIED: `ansify --build-db` silently corrupts signature-DB entries for any two `.ch` files anywhere in the tree that share the same filename

**This is a tool-infrastructure bug, not a source bug, and not
previously documented anywhere in the taxonomy.** It directly affects
2 of this batch's 5 `.ch` files (`atk/typescript/tscript.ch`,
`atk/typescript/typetext.ch`) and, in one case, would have caused
`ansify` to silently emit a **wrong-but-coincidentally-plausible**
type — not just a missed conversion.

**Reproduction.** `ansify --build-db`'s real, on-disk output for
`build/desc/typescript.desc` and `build/desc/typetext.desc` differs
from what an isolated, single-file invocation of the identical
underlying command (`class -s -D -N -I<dir> -I<incdir> <ch>`, the
exact args `build_db()` uses) produces:

- `typescript.desc` (from `tscript.ch`): the real DB is **missing 6
  entries** that `tscript.ch` genuinely declares and an isolated dump
  correctly includes — `SetTitle`, `GetTitle`, `SetFrame`, `GetFrame`
  (methods), `Create`, `CreatePipescript` (classprocs). Confirmed by
  diff: 1362 real lines vs. 1402 isolated lines, the missing block
  matching exactly these 6 signatures.
- `typetext.desc` (from `typetext.ch`): the reverse — the real DB
  **fabricates 3 entries that shouldn't exist at all**
  (`InitializeObject`/`InitializeClass`/`FinalizeObject`, with
  `defined by: typetext` but no `declared by:` line — a format no
  genuine entry in the DB has), while an isolated dump correctly has
  none of them (`typetext.ch` has no `classprocedures:` section at
  all — same shape as I2's `chartx1a` finding, where the `.c` still
  defines the lifecycle methods without the `.ch` declaring them).

**Root cause (empirically bounded, not traced to source).** I ran a
tree-wide scan: for every one of the 566 `.ch` files, compare
`build_db()`'s real output against an isolated single-file dump.
**27 mismatches found, and every single one is a `.ch` file whose
bare filename (not classname) is duplicated elsewhere in the tree** —
confirmed by checking `find src -name <basename>` for each: `hello.ch`
(8 copies), `hellov.ch` (17), `hgghview.ch` (6), `sliderv.ch` (2, the
already-known collision — but this shows its underlying *content* can
also be corrupted independently of the already-fixed case-insensitive
destination-filename collision), `tm.ch`/`tm19.ch`/`tmv.ch` (2 each),
`butt.ch`/`hgghapp.ch`/`hwapp.ch`/`hggh.ch` (2-5 each), and this
batch's `tscript.ch`/`typetext.ch` (2 each). Every non-mismatched file
I checked in this batch (`tscripta.ch`, `fcomp.ch`, `pscripta.ch`,
`hlptextv.ch`, `helpdb.ch`, `help.ch`, `helpa.ch`, `eza.ch`) has a
**unique** basename tree-wide. I did a cursory read of
`overhead/class/pp/class.c` for an obvious shared-cache/tmpfile
mechanism keyed by basename (`grep` for `tmpnam`/`mktemp`/`/tmp/` and
the `.desc`-writing code around line 1627) and found nothing obviously
responsible in a quick read — I did not trace it further; this is
tool-construction work that belongs with whoever rules on the fix, per
the Delegation ruling (`porting-assessment.md` §14).

**Scope check against already-completed batches: none affected.** The
other 25 mismatched files are all in directories that are either
already-documented-dead (`contrib/atkbook`, `contrib/tm` — per
`rollout-procedure.md`'s Liveness census) or not in `m3-batches.md`'s
90-directory active list at all (`atk/examples`) — none are in any
already-committed M3 batch (O1-I2). `sliderv`/`sliderV` (`atk/value`,
committed in B2) is worth a heads-up regardless: the 2026-07-30 fix
addressed the *destination-filename* collision (refusing to let one
class's `.desc` overwrite another's), but this content-level
corruption is a **separate** mechanism the fix wasn't aware of and
doesn't address — B2 itself never depended on the DB for its `.ch`-file
`-pe` rollout (confirmed already in the runbook's classpp-fix entry:
"that batch's `-pe`/`.eh` rollout went through classpp directly against
the `.ch` file, never through this DB"), so no retroactive risk to
B2's committed work, but the DB's `sliderv.desc`/`sliderV` entry itself
may still be silently wrong content today. Not investigated further
here (out of this batch's directory scope) — flagging for awareness.

**What I did about it for this batch (reversible, non-source).**
Regenerated just these 2 `.desc` files by copying in the correct
isolated-invocation output (`build/desc/` is not fossil-tracked — a
disposable derived artifact, confirmed via `fossil finfo`). Re-ran
`ansify --dry-run --dir src/atk/typescript` afterward: `tscript.c` now
shows `12 methods, 5 classprocs, 52 helpers` (was `8 methods, 3
classprocs`, with the 6 missing signatures reported as "no signature
in DB" skips) and `typetext.c` now correctly shows the expected,
already-classified `skipped typetext__InitializeObject: no signature
in DB` (§4's chartx1a-pattern) instead of silently "converting" it
against fabricated data. Both now show 0 DRIFT, matching the rest of
the batch.

**What needs a ruling:**
1. Whether to fix `build_db()` for real before Gate 1 (my `.desc`
   patch is a hand workaround for exactly 2 files; it will be
   silently undone the next time anyone runs `ansify --build-db` for
   any reason), or accept the hand-patched DB as sufficient for this
   batch specifically and defer the tool fix.
2. Whether `sliderv`/`sliderV`'s DB content (separate from B2's
   already-committed, DB-independent `-pe` work) is worth a follow-up
   check — not blocking, since B2 never depended on it, but the DB
   itself may be feeding wrong data to any *future* batch that
   happens to invoke `ansify` against `sliderv.desc` for some other
   reason.
3. **Operational caveat regardless of the ruling**: whoever runs Gate
   1 should NOT run a fresh `ansify --build-db` between now and when
   `atk/typescript` is processed, unless the fix (or an equivalent
   re-patch) is in place first — a rebuild will silently re-corrupt
   both entries.

### 3.2 New parser-gap shape (likely self-classifiable, flagged for visibility): `ansify`'s file-local-helper parser doesn't recognize `struct TYPE* name;` (asterisk glued to the type, not the variable)

Found in `atk/help/src/help.c`: 4 file-local static helpers
(`SearchOverviews`, `SearchPrograms`, `TextviewProc`,
`ToggleProgramListSize`) reported `skipped ...: unparseable K&R
declarations` in the dry run. All 4 share the exact same parameter
declaration style: `register struct help* self;` (space *before* the
variable name, asterisk glued to `help`) — as opposed to the
successfully-converted helpers in the same file (e.g. `TogglePanels`,
`ToggleOverviews`), which use `register struct help *self;` (asterisk
glued to the variable name instead). I confirmed this is the sole
differentiator: copied `help.c` to a scratch directory, mechanically
replaced `struct help* self;` → `struct help *self;` (4 occurrences,
nothing else touched), and re-ran `ansify --dry-run --no-compile` —
all 4 previously-skipped helpers now convert cleanly (32 helpers →
36 helpers, 0 skips). No other directory in this batch hit this shape
(checked: it's the only "unparseable K&R declarations" skip anywhere
in the batch's 9 directories).

This doesn't match any of the specifically-catalogued parser-gap
shapes in O2/O4/B1/B3 (`(void)`-misparse, `DECLARE<N>` macro,
function-pointer-returning-pointer, brace-glued-to-parameter,
brace-glued-to-local-variable, double-pointer-loses-a-star,
array-brackets-before-name) — it's a new specific trigger, but it
belongs to the same general, well-precedented family: a **safe,
reported skip** (never silently drops or corrupts anything — same
"self-healing" character as every other parser-gap finding to date),
**small and bounded** (4 functions, 1 file, this batch), with an
**obvious, zero-risk fix** (add a space; matches the class of
mechanical stopgap B1/B2 already used for the brace-glue shapes before
those got a real tool fix). Given the precedent that every
similarly-shaped, similarly-bounded parser gap so far has been left as
a per-instance hand-fix rather than a tool patch (Delegation ruling:
tool construction stays top-level, small instance counts don't justify
a standalone fix commit), **I'd propose treating this the same way**:
normalize the 4 declarations' spacing by hand during Gate 1's
fallout-fix step (mechanical, no behavior change), and note the shape
here for whoever next touches `ansify`'s parser, rather than fixing
the tool now. Flagging prominently since it's genuinely new, not
because I think it's ambiguous — happy to proceed on this basis unless
told otherwise.

## 4. Per-directory Gate 0 results

| # | Directory | `.ch`? | `ansify --dry-run` | Standing checklist | Classification |
|---|---|---|---|---|---|
| 1 | `ams/msclients/nns` (10 files) | No | 0 skips/DRIFT, 65 helpers total | 4 files missing `#include <stdlib.h>` despite malloc/free calls — wait, see note below | Routine (§5) |
| 2 | `atk/typescript` (5 files) | Yes (5) | 0 DRIFT after DB fix (§3.1); 2 known-safe skips (`pipescriptapp__InitializeObject`, chartx1a-pattern); 4 unparseable skips in none of these files (the glued-asterisk shape is in `help.c`, not here) | 3 real narrow-param stranded-fwd-decl instances (T1 pattern, §5); no macro typos; no COMPILERFLAGS guard (§5 note) | Routine, pending §3.1 ruling |
| 3 | `atk/help/src` (5 files, `helpaux.c` has no own `.ch`) | Yes (4) | 0 DRIFT; 4 unparseable skips = §3.2's glued-asterisk shape, all in `help.c` | 1 benign stranded empty-parens classproc fwd-decl (`helpdb.c:81`, pointer-typed, safe); a genuine 35-year-old `struct classhead`/`classheader` typo in `helpaux.c` (§5, self-resolving) | Routine, pending §3.2 ruling |
| 4 | `ams/msclients/cui` (4 files) | No | 0 skips/DRIFT, 130 helpers total | `cui.c` missing `#include <stdlib.h>` | Routine (§5) |
| 5 | `atk/help/maint` (1 file) | No | 0 skips/DRIFT, 4 helpers | Clean | Routine |
| 6 | `atkams/messages/cmd` (1 file) | No | 0 skips/DRIFT, 3 helpers | Clean | Routine |
| 7 | `ams/msclients/imapsync` (1 file) | No | 0 skips/DRIFT, 2 helpers | Clean | Routine |
| 8 | `atk/ez` (1 file) | Yes (1) | 0 DRIFT; 1 known-safe skip (`ezapp__InitializeObject`, chartx1a-pattern, confirmed: `eza.ch` declares only `InitializeClass`) | `eza.c` missing `#include <stdlib.h>` | Routine |
| 9 | `doc/mkbrowse` (1 file) | No | 0 skips/DRIFT, 16 helpers | `browser.c` missing `#include <stdlib.h>` | Routine |

(Corrected note for row 1: the `#include <stdlib.h>` grep result for
`ams/msclients/nns` is that 4 of its 10 files call malloc-family
functions and **already have** `#include <stdlib.h>` — genuinely
clean, no action needed. The files needing the include added are
listed in full in §5.)

All 9 directories confirmed live in the current build (each has
exactly one `building (dependInstall) (.../src/<dir>)` line in the
most recent tree-wide gate log, `dependInstall.log`, from I2's
wave-end run).

## 5. Detailed standing-checklist results (routine, self-classified)

**1. `.ch` presence** — 3 of 9 directories have `.ch` files
(`atk/typescript`: 5, `atk/help/src`: 4, `atk/ez`: 1); the other 6
(`ams/msclients/nns`, `ams/msclients/cui`, `atk/help/maint`,
`atkams/messages/cmd`, `ams/msclients/imapsync`, `doc/mkbrowse`) have
none — `-pe`/`.eh` and DRIFT are structurally inapplicable there, per
O1's precedent. Matches the prompt's own prediction exactly.

**2. Predefined-macro typo grep** (`_STDC_`, `_cplusplus`, `_FILE_`) —
clean, zero hits across all 9 directories.

**3. Empty-parens lifecycle-method grep**
(`__(InitializeClass|InitializeObject|FinalizeObject)\(\s*\)`) — one
hit, `helpdb.c:81`: `boolean helpdb__InitializeClass();`. Confirmed
this is a **stranded old-style forward declaration** (standing item 8
shape), not the O4/B3 "truly empty parameter *definition*" hazard —
the real definition at `helpdb.c:140` is ordinary K&R with a named
`classID` param (`ansify` recognizes and converts it normally; dry run
confirms `helpdb.c` converts its `InitializeClass` cleanly as part of
its "11 classprocs"). Since the real param type is a pointer
(`struct classheader *`), not a narrow scalar, standing item 8's
hazard doesn't trigger either (pointers aren't subject to default
argument promotion) — harmless, no action needed, though it could be
tidied up in passing at Gate 1.

**4. Installed-header grep for converted non-static helpers** —
extracted every candidate non-static, non-`__`-prefixed top-level
function name defined across the batch's 29 files (144 candidates,
some false positives from a crude heuristic but no false negatives),
grepped the entire `build/include` tree for a matching empty-parens
declaration. **Zero matches** — clean, no cross-directory header
exposure risk found (O3's `IsOK822Atom`-shaped concern).

**5. Restated-lifecycle-param `.ch` check** — checked every
`InitializeClass(`/`FinalizeObject(` declaration in the batch's 5
`.ch` files. All 6 `InitializeClass` declarations use plain empty
parens (`InitializeClass() returns boolean;`) — the safe, undecorated
convention, no restated-param risk (§17's first exception doesn't
apply). 4 `FinalizeObject` declarations restate exactly one param
(self, sometimes renamed — `tscript.ch`'s `ap`), matching the
established-safe B2 workaround pattern, not the B3
double-restated-both-params bug shape. Verified all 4 real `.c`
definitions independently (`tscript.c`, `tscripta.c`, `helpaux.c`,
`hlptextv.c`) — each takes exactly the expected 2 real params
(`classID`, self-type). `InitializeObject` restatements (only
`tscript.ch` restates it, `InitializeObject(struct typescript *self)`)
are the always-safe case per §17 (classpp hardcodes InitializeObject's
prototype regardless of `.ch` content; restating self here is
cosmetic and, incidentally, is exactly why `ansify`'s DRIFT check
doesn't misfire on it either — the math coincidentally comes out
right: 1 declared arg + 1 assumed-implicit = 2, matching the real
hardcoded 2-param convention).

**6. Concurrent-commit merge check** — `fossil status` checked
repeatedly throughout the session (before, during, and after both
`-pe` experiments); checkout stayed at `0cd9f9ae4bc8d15435ef860601038985da1e1543`
the entire time, zero concurrent commits landed.

**7. Liveness census + anchored `malloc`/`free`/`realloc`/`calloc`
grep** — all 9 directories confirmed live (§4). Anchored grep
(`\bmalloc *\(|\bfree *\(|\brealloc *\(|\bcalloc *\(`) found 9 files
across the batch calling malloc-family functions without
`#include <stdlib.h>` — routine fallout, fix at Gate 1:
`atk/typescript/fcomp.c`, `atk/typescript/tscript.c`,
`atk/help/src/help.c`, `atk/help/src/helpa.c`,
`atk/help/src/helpaux.c`, `atk/help/src/helpdb.c`,
`ams/msclients/cui/cui.c`, `atk/ez/eza.c`, `doc/mkbrowse/browser.c`.
(`ams/msclients/nns`'s 4 malloc-calling files and
`ams/msclients/imapsync/imap_sync.c` already have the include —
clean.)

**8. Stranded old-style forward declaration vs. narrow parameter** —
found ~70 bare `static TYPE name();` forward declarations across the
batch (heavy use of this style, especially in `tscript.c`, `help.c`,
`helpaux.c`, `helpdb.c`, `gl.c`, `stb.c`). Cross-checked every one
against its real definition's actual parameter types (not just
existence). **3 real hazard instances, all in `atk/typescript/tscript.c`**
— narrow **by-value** (not pointer) params, the T1-established shape:
- `Typescript_DigitCmd` (line 118 decl, real def takes `char a` by value)
- `Typescript_SelfInsertCmd` (lines 119 and 188, duplicate decl; real
  def takes `register char a` by value)
- `typescript_handlereadonly` (line 138 decl; real def takes `char c`
  by value)

All other hits either return/take only pointer or `int`/`void`/`long`
types (no promotion-incompatibility risk) — e.g. `SetTitle`,
`WritePty`, `typescriptAddMenu`, `ShowHelp` all looked like candidates
on a naive scan but take `char *` (pointer), not `char` (scalar), so
they're safe. Routine, precedented fallout — fix at Gate 1 the same
way T1 did (retype the 3 stale declarations to match their real
definitions).

**Additional observation, not a standing-checklist item but worth
noting (matches I2's `atk/chart` precedent):** only 2 of this batch's
9 Imakefiles (`ams/msclients/nns`, `atk/help/src`) carry the M2-era
`COMPILERFLAGS = ... -Werror=implicit-function-declaration` guard; the
other 7 (`atk/typescript`, `ams/msclients/cui`, `atk/help/maint`,
`atkams/messages/cmd`, `ams/msclients/imapsync`, `atk/ez`,
`doc/mkbrowse`) have never had it. Same as I2's finding for
`atk/chart`: out of scope for a `-pe`-only batch to fix, noted for
awareness in case it's worth a similar same-day standalone follow-up.

## 6. `helpaux.c`'s `struct classhead`/`classheader` typo — real ~35-year-old bug, self-resolving, no action needed

`helpaux.c:448`'s `help__FinalizeObject` declares its first parameter
`struct classhead *classID;` — **`classhead`, not `classheader`**.
Confirmed via tree-wide grep this exact tag is never defined or
referenced anywhere else in the source tree; `struct classheader` (the
correct type, used identically by every other class in the codebase)
is defined in `build/include/class.h:112`. Same species as O3's
`regcomp`/`regexec` and O4's `_STDC_` findings — a typo invisible
under K&R (an incomplete/opaque forward-declared struct pointer
compiles and links fine when never dereferenced, which this parameter
never is) that only becomes checkable once real typing is involved.

**Confirmed self-resolving, no fix needed at Gate 1**: because
`ansify`'s classproc conversion path uses the signature DB (derived
from `help.ch`'s correct `struct classheader *` declaration) as ground
truth, not the `.c`'s stated (buggy) type — the real, non-dry-run
conversion silently emits the *correct* type
(`void help__FinalizeObject(struct classheader *classID, struct help
*self)`), fixing the 35-year-old typo as an incidental, zero-risk side
effect of the ordinary conversion. Verified directly: ran `ansify`
(non-dry-run, `--no-compile`) against a scratch copy of `helpaux.c` and
confirmed the converted output. Worth a line in `revival.md`'s "Old
bugs never found till now" per the established convention, but not a
blocker and not something Gate 1 needs to touch by hand.

## 7. Files touched

None (source). `build/desc/typescript.desc` and
`build/desc/typetext.desc` were corrected (non-source, non-fossil-
tracked derived artifacts — see §3.1); `src/atk/help/src/Imakefile`,
`src/atk/typescript/Imakefile`, `src/atk/ez/Imakefile` were each
temporarily edited and `fossil revert`ed within the same investigation
step. `fossil status` (verified clean, run last at the very end of
this session):

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
config-db:    /Users/wdc/.fossil
checkout:     0cd9f9ae4bc8d15435ef860601038985da1e1543 2026-07-31 19:09:08 UTC
parent:       0b40b488473c7e6aba7c605226f962871ecccdfb 2026-07-31 19:01:43 UTC
tags:         andrew-6.4
comment:      docs: atk/chart COMPILERFLAGS follow-up closed out (user: wdc)
```

(No `EDITED`/`ADDED`/`MISSING` lines — clean.) `fossil extras` shows
only ordinary, pre-existing build byproducts (`Makefile`,
`install.time`, `.o`/`.a`/`.do` files, `,help.alias`, etc.) already
present in the tree from the last full build — nothing new, no `.eh`
residue from either `-pe` experiment.

## 8. Open questions / what surprised me

- The `ansify --build-db` corruption (§3.1) was the real surprise of
  this batch — a routine batch turned up a tool-infrastructure bug of
  similar severity to I1's `sliderv`/`sliderV` collision, in a
  directory (`atk/typescript`) with nothing else notable about it.
  Worth considering whether a cheap, permanent addition to the
  standing checklist ("does this directory's own `.ch` filenames
  collide with any other `.ch` filename tree-wide?" — a single `find
  src -name '<basename>' | wc -l` per `.ch` file) would be worth
  adding, given how easy it is to check and how silent the failure
  mode is otherwise (no error, no failed-list entry — the corrupted
  entries look exactly like ordinary, successfully-generated `.desc`
  files).
- `atk/typescript`'s heavy use of `struct TYPE* name` (asterisk glued
  to the type) as a *K&R parameter declaration* style, distinct from
  every other batch's declarations so far, was unexpected — worth
  watching for in later waves' older/more-idiosyncratic directories.
- Otherwise this batch matched the prompt's own framing closely: small
  leaf directories, the 6 non-`.ch` ones genuinely trivial (0
  skips/DRIFT, only the routine missing-`<stdlib.h>` fallout), and the
  3 `.ch` directories' real complexity came entirely from tool-side
  anomalies rather than genuine `.ch`-vs-`.c` interface bugs (only one
  real interface-adjacent bug found all batch, `helpaux.c`'s
  `classhead` typo, and it turned out to need no action at all).

## 9. Suggested runtime checks for wdc (deferred to Gate 1's report)

Not included here — per `rollout-procedure.md`'s Runtime check rules,
this section belongs with the real, `-pe`-live, gate-clean result, not
the dry-run/classification pass. Gate 1's report will include the full
`nm -g` / `runapp`/`.do`-consumer survey and exact commands. Quick
preview based on what Gate 0 already shows: `atk/ez` (`eza`) and
`atk/typescript` (`tscript`, `typescript.tpl`, wired into `ez` as a
`typescriptinit`-invoked subprocess handler) both have obvious
GUI-visible consumers reachable via `runapp -d eza`; `ams/msclients/cui`
and `ams/msclients/nns` are CLI-launched (`cuin`/`nns` binaries,
confirmed present in `fossil extras`' build byproducts); `atk/help/src`
is the `help` app's backend (`InstallLink(runapp, .../bin/help)`,
confirmed in its Imakefile); `atk/help/maint`, `atkams/messages/cmd`,
`ams/msclients/imapsync`, and `doc/mkbrowse` all look like build-time/
maintenance/doc tooling with no live GUI-visible consumer (matches the
prompt's own prediction) — Gate 1 will confirm each with a real
`nm -g`/`runapp` search rather than asserting it here.

---

# Gate 1 (real run)

Resumed after both rulings (§3.1 fixed for real by the orchestrator,
`fa0705b6`/`214801d5`; §3.2 agreed as proposed). Verified before
starting: a fresh `ansify --build-db` now correctly resolves
`typescript`/`typetext` from the real `atk/typescript` source (496
classes, 70 reported failures — all in dead/demo trees: `atk/examples`,
`contrib/atkbook`, `rdemo/hide`, plus the already-known
`sliderv`/`sliderV` case pair); `grep -c "^Method:\|^Class Procedure:"`
on `typescript.desc`/`typetext.desc` gave 112/78, matching the
orchestrator's verification exactly. Ran directories in the prompt's
order, serially, one gated clean twice before starting the next.

## 9. Gate 1 results vs. Gate 0 predictions, per directory

### 1. `ams/msclients/nns` (no `.ch`)

Real `ansify --dir`: exactly matched the Gate 0 dry-run — 0 compile
failures, 0 DRIFT, 65 helpers converted across 10 files (`mc.c` and
`st.c` also got a `[fix-missing-static-decl]` modification, cosmetic).
No fallout to fix (Gate 0 had already confirmed all `<stdlib.h>`
includes present). Gated clean twice.

### 2. `atk/typescript` (5 `.ch` files) — `-pe` added for real

Added `CLASSFLAGS = $(CLASSINCLUDES) -pe` to the Imakefile for real.
First real `ansify --dir` attempt hit a **build-state gap identical to
T1's finding 5**: I had deleted the `.eh` files at the end of Gate 0's
temporary-`-pe` cleanup, so `ansify`'s own compile gate found no
`.eh`/`.ih` to check against (`fatal error: 'tscript.eh' file not
found`, same shape across all 5 files). Fixed by force-regenerating all
5 `.eh` files (`make pscripta.eh tscript.eh tscripta.eh typetext.eh
fcomp.eh`) before re-running `ansify --dir` — not a new finding, just
T1's own documented gap recurring because I'd left the directory in the
"no prior build" state myself.

Second run: `fcomp.c`, `pscripta.c`, `tscripta.c`, `typetext.c` all
converted clean immediately (matching Gate 0 exactly, including the 2
predicted-safe `skipped ...__InitializeObject: no signature in DB`
lines on `pscripta.c`/`typetext.c`). `tscript.c` failed its compile
gate — **this is exactly the 3 narrow-param stranded-forward-declaration
instances Gate 0 predicted** (`Typescript_DigitCmd`, `Typescript_
SelfInsertCmd` ×2 declarations, `typescript_handlereadonly`), now a
real, confirmed compile error (`conflicting types`) rather than a
predicted one. Fixed by retyping all 4 stale declaration lines to match
their real K&R definitions (T1's established pattern) — e.g.
`static void Typescript_DigitCmd();` → `static void
Typescript_DigitCmd(struct typescript *self, char a);`. Re-ran
`ansify --dir`: `tscript.c` converted clean (12 methods, 5 classprocs,
52 helpers — exactly Gate 0's DB-corrected prediction), 0 DRIFT
tree-wide for the directory.

Additional fallout (predicted at Gate 0): added `#include <stdlib.h>`
to `fcomp.c` and `tscript.c`. Gated clean twice (`make -C ... clean`,
`depend`, `-k install`, twice).

### 3. `atk/help/src` (4 `.ch` files) — `-pe` added for real

Normalized the 4 glued-asterisk declarations in `help.c` (§3.2) by hand
**before** the real `ansify --dir` run, per the agreed plan (`struct
help* self;` → `struct help *self;`, 4 occurrences: `SearchOverviews`,
`SearchPrograms`, `TextviewProc`, `ToggleProgramListSize`). Added
`CLASSFLAGS += -pe` for real, force-regenerated all 4 `.eh` files.

Real `ansify --dir`: `help.c` converted clean immediately — 36 helpers
(up from Gate 0's dry-run 32, confirming the spacing fix resolved
exactly the 4 predicted skips, 0 remaining). `helpaux.c`, `helpdb.c`,
`hlptextv.c` also converted clean on the first pass. `helpa.c` failed
its compile gate with a **new fallout shape not seen at Gate 0**:
`fix-missing-static-decl` inserted 5 brand-new empty-parens forward
declarations (`AddPath`, `ncproc`, `send_pack`, `show_usage`,
`unique_help`) for functions that had **no forward declaration at all**
in the pristine K&R source (confirmed via `fossil diff` — these 5 lines
are pure tool insertions, not something Gate 0's static-declaration
grep could have found since nothing existed yet to grep for). One of
them (`send_pack(char c, char *s, int sock)`, a narrow by-value `char`
first param) immediately conflicted with the stub the tool had just
inserted moments earlier in the same pipeline run — a same-run version
of the standing item-8 hazard, self-inflicted by `fix-missing-static-
decl` rather than pre-existing. Fixed by retyping all 5 inserted stubs
to match their real definitions (`static int send_pack(char c, char
*s, int sock);` etc.); re-ran `ansify --dir` and `helpa.c` converted
clean. **Not folded into §3.2 or the standing checklist as a formal new
taxonomy entry** (per Delegation, that's a tool-construction judgment
call) — flagged here for whoever next reviews `fix-missing-static-
decl`'s non-idempotency family (O1/O3 already cover the
already-declared case; this is the zero-prior-declaration case hitting
the same narrow-param hazard within one run).

Confirmed (§6's prediction): `helpaux.c`'s `help__FinalizeObject` now
reads `void help__FinalizeObject(struct classheader *classID, struct
help *self)` — the `classhead`/`classheader` typo self-resolved exactly
as predicted, no action needed.

Added `#include <stdlib.h>` to all 4 files with malloc-family calls
(`help.c`, `helpa.c`, `helpaux.c`, `helpdb.c`), as predicted. Gated
clean twice.

### 4. `ams/msclients/cui` (no `.ch`)

Real `ansify --dir`: `morprntf.c` and `unixmach.c` converted clean
immediately (matching Gate 0). `cui.c` and `cuifns.c` both failed their
compile gate with a **new fallout shape not seen at Gate 0**: a
function called (and, because of K&R's leniency, implicitly declared at
the call site) well before its own real definition later in the same
file, with **no forward declaration anywhere** — not the standing
item-8 shape (no explicit stale declaration to grep for) but the same
underlying species (a pre-1994 gap invisible under K&R, surfaced only
once the real, later definition goes ANSI-typed and conflicts with the
compiler's own inferred implicit declaration from the earlier call).
Two instances, both confirmed via `fossil cat` to be pristine,
pre-existing gaps (never declared, in this file's entire history):
- `cui.c`: `GetHeadersFn(char *arg, char fn)`, called at line ~1500,
  defined at line 1513, absent from the file's own giant `int NAME(),
  NAME(), ...;` K&R multi-name forward-declaration block (its sibling
  `GetHeaders` *is* in that block; `GetHeadersFn` never was). Fixed by
  adding `int GetHeadersFn(char *arg, char fn);` as a standalone typed
  declaration right after that block.
- `cuifns.c`: `MaybeFlagSomething(char *arg, Boolean DoSet)`, same
  shape, no declaration block in this file at all. Fixed with a
  standalone typed declaration immediately before its first use.

Re-ran `ansify --dir`: all 4 files converted clean, 0 DRIFT. Added
`#include <stdlib.h>` to `cui.c` (predicted at Gate 0). Gated clean
twice.

### 5. `atk/help/maint` (no `.ch`)

Real `ansify --dir` matched Gate 0 exactly: 0 compile failures, 0
DRIFT, 4 helpers converted in `mkindex.c`. No fallout. Gated clean
twice.

### 6. `atkams/messages/cmd` (no `.ch`)

Real `ansify --dir` matched Gate 0 exactly: 0 compile failures, 0
DRIFT, 3 helpers converted in `dumpbin.c`. No fallout. Gated clean
twice.

### 7. `ams/msclients/imapsync` (no `.ch`)

Real `ansify --dir` converted 2 helpers cleanly per Gate 0's
prediction, but the **subtree-local gate** (not `ansify`'s own
per-file gate, which passed) surfaced fallout Gate 0 could not have
seen: `imap_sync.c` turns out to already be almost entirely
hand-written ANSI C (unlike every 1988-era K&R file in this batch) —
only 2 file-local helpers were still K&R. Its ~25-entry static
forward-declaration block at the top of the file, however, is
uniformly old-style empty-parens (`static void vlog();`,
`static int base32hex_digit();`, etc.), stale against the real,
already-typed definitions below — the standing item-8 shape, but at a
scale (25 declarations) my Gate-0 grep hadn't specifically flagged
because it doesn't fire on files with 0 K&R conversions to trigger a
closer look. 4 of the 25 are hard errors: `vlog`/`loudlog` (variadic,
O3's variadic-stub sub-case: no empty-parens declaration can ever be
compatible with a `...` tail) and `base32hex_digit`/`hexval` (narrow
`char`-by-value params, T1's shape). Fixed by retyping all 25 stale
declarations to match their real (pre-existing, already-correct)
definitions in one pass. This surfaced a **second, chained finding**:
retyping `load_state`/`write_state` (both taking `struct sync_state
*`) produced a fresh `conflicting types` error — **B3 finding 8's
function-prototype-scope struct-tag trap**: `struct sync_state` first
appeared inside my new forward declarations' parameter lists (giving it
only prototype scope), while the real definitions later reference the
file-scope tag defined at line 522 — two distinct incomplete types.
Fixed the same way B3 did: added a plain `struct sync_state;` file-scope
forward declaration before the block. Verified with a direct `make
imap_sync.o` (0 errors) before re-gating. `<stdlib.h>` was already
present (per Gate 0). Gated clean twice after the fix (the first
gate attempt, before the struct-tag fix, is not counted as one of the
two — only the two post-fix clean runs count).

### 8. `atk/ez` (1 `.ch` file) — `-pe` added for real

Added `CLASSFLAGS += -pe` for real, force-regenerated `eza.eh`. Real
`ansify --dir` matched Gate 0 exactly: 4 methods, 1 classproc, 11
helpers converted, 1 predicted-safe skip
(`ezapp__InitializeObject: no signature in DB`, confirmed `eza.ch`
declares only `InitializeClass`), 0 compile failures, 0 DRIFT. Added
`#include <stdlib.h>` (predicted). Gated clean twice.

### 9. `doc/mkbrowse` (no `.ch`)

Real `ansify --dir` matched Gate 0 exactly: 16 helpers converted, 0
compile failures, 0 DRIFT. Added the predicted `#include <stdlib.h>`,
which surfaced a **new fallout shape not seen at Gate 0**: doing so
exposed a genuine pre-existing conflict the file's own comment already
flagged — line 47 read `char *malloc(),*realloc();  /* should include
stdlib.h */`, a stale K&R-style redeclaration that directly conflicts
with `<stdlib.h>`'s real `void *malloc(size_t)`/`void
*realloc(void *, size_t)` prototypes the moment the header is actually
included. Fixed by deleting the redundant declaration (exactly what the
file's own comment already said should happen). This directory also
uses `FlexOrLexFileRule(browserpp)` (flex-generated `browserpp.c` from
`browserpp.lex`/`.flex`) — confirmed the `rollout-procedure.md`
generated-source caution applies here too (`make clean` deletes the
generated `.l`/`.c`, `depend`+the ordinary build regenerates them
correctly; no separate action needed beyond following clean→depend→
install in order, which this batch already did throughout). Gated
clean twice.

## 10. Per-file `ansify` conversion counts (final, post-fallout)

| Directory | File | Methods | Classprocs | Helpers | Notes |
|---|---|---|---|---|---|
| `ams/msclients/nns` | conf.c | 0 | 0 | 3 | |
| | gl.c | 0 | 0 | 14 | |
| | gle.c | 0 | 0 | 10 | |
| | mc.c | 0 | 0 | 6 | |
| | mcb.c | 0 | 0 | 5 | |
| | mcbe.c | 0 | 0 | 5 | |
| | nns.c | 0 | 0 | 13 | |
| | st.c | 0 | 0 | 6 | |
| | stb.c | 0 | 0 | 5 | |
| | stbe.c | 0 | 0 | 2 | |
| `atk/typescript` (`-pe`) | fcomp.c | 0 | 1 | 12 | +stdlib.h |
| | pscripta.c | 2 | 0 | 0 | 1 safe skip |
| | tscript.c | 12 | 5 | 52 | +stdlib.h, 4 stale fwd-decls fixed |
| | tscripta.c | 3 | 2 | 0 | |
| | typetext.c | 5 | 0 | 0 | 1 safe skip |
| `atk/help/src` (`-pe`) | help.c | 0 | 1 | 36 | 4 glued-asterisk fixed pre-run |
| | helpa.c | 3 | 1 | 4 | 5 stale fwd-decls fixed |
| | helpaux.c | 7 | 8 | 14 | classhead typo self-resolved |
| | helpdb.c | 0 | 11 | 10 | +stdlib.h |
| | hlptextv.c | 1 | 3 | 1 | |
| `ams/msclients/cui` | cui.c | 0 | 0 | 71 | +stdlib.h, GetHeadersFn fwd-decl added |
| | cuifns.c | 0 | 0 | 36 | MaybeFlagSomething fwd-decl added |
| | morprntf.c | 0 | 0 | 2 | |
| | unixmach.c | 0 | 0 | 1 | |
| `atk/help/maint` | mkindex.c | 0 | 0 | 4 | |
| `atkams/messages/cmd` | dumpbin.c | 0 | 0 | 3 | |
| `ams/msclients/imapsync` | imap_sync.c | 0 | 0 | 2 | 25 stale fwd-decls retyped, struct-tag fix |
| `atk/ez` (`-pe`) | eza.c | 4 | 1 | 11 | +stdlib.h, 1 safe skip |
| `doc/mkbrowse` | browser.c | 0 | 0 | 16 | +stdlib.h, stale malloc decl removed |

**Directories that had zero `.ch` files and so skipped the `-pe`/`.eh`
half entirely**: `ams/msclients/nns`, `ams/msclients/cui`,
`atk/help/maint`, `atkams/messages/cmd`, `ams/msclients/imapsync`,
`doc/mkbrowse` (6 of 9, exactly as Gate 0 predicted).

## 11. Gate results

All 9 directories: `make -C <dir> clean`, `depend`, `-k install` —
separate calls, absolute paths, **twice each** (the second run always
after a `make clean`, not a no-op re-run), all clean (0 `error:` lines)
after fallout was fixed. `ams/msclients/imapsync` needed a third
clean-gate cycle because the struct-tag fix landed between attempts —
only the two post-fix clean runs are counted as its "twice."

Wave-end tree-wide gate (`make -C src dependInstall`, logged to
`~/src/AUIS/andrew-6.4/dependInstall.log`, run twice): both runs clean
except the identical 2 pre-existing `contrib/zip/utility/ltapp.c`
errors both times (`incompatible integer to pointer conversion passing
'boolean' ... to parameter of type 'void *'`, lines 115 and 123) —
already documented as a standing, unrelated gate blocker queued for
Wave 7 C2. All 9 batch directories confirmed present in the log
(`building (dependInstall) (.../src/<dir>)`, exactly once each, both
runs). This closes Wave 5.

## 12. `fossil status` / `fossil extras`

```
$ fossil status
...
EDITED     src/ams/msclients/cui/cui.c
EDITED     src/ams/msclients/cui/cuifns.c
EDITED     src/ams/msclients/cui/morprntf.c
EDITED     src/ams/msclients/cui/unixmach.c
EDITED     src/ams/msclients/imapsync/imap_sync.c
EDITED     src/ams/msclients/nns/conf.c
EDITED     src/ams/msclients/nns/gl.c
EDITED     src/ams/msclients/nns/gle.c
EDITED     src/ams/msclients/nns/mc.c
EDITED     src/ams/msclients/nns/mcb.c
EDITED     src/ams/msclients/nns/mcbe.c
EDITED     src/ams/msclients/nns/nns.c
EDITED     src/ams/msclients/nns/st.c
EDITED     src/ams/msclients/nns/stb.c
EDITED     src/ams/msclients/nns/stbe.c
EDITED     src/atk/ez/Imakefile
EDITED     src/atk/ez/eza.c
EDITED     src/atk/help/maint/mkindex.c
EDITED     src/atk/help/src/Imakefile
EDITED     src/atk/help/src/help.c
EDITED     src/atk/help/src/helpa.c
EDITED     src/atk/help/src/helpaux.c
EDITED     src/atk/help/src/helpdb.c
EDITED     src/atk/help/src/hlptextv.c
EDITED     src/atk/typescript/Imakefile
EDITED     src/atk/typescript/fcomp.c
EDITED     src/atk/typescript/pscripta.c
EDITED     src/atk/typescript/tscript.c
EDITED     src/atk/typescript/tscripta.c
EDITED     src/atk/typescript/typetext.c
EDITED     src/atkams/messages/cmd/dumpbin.c
EDITED     src/doc/mkbrowse/browser.c
```

Exactly 32 files (29 batch source files + the 3 Imakefiles that
permanently gained `CLASSFLAGS += -pe`) — matches expectations exactly,
no stray files. **No commit made.** `fossil extras` shows only ordinary
pre-existing build byproducts (`Makefile`, `install.time`, `.o`/`.a`/
`.do`/`.eh`/`.ih` files, prior sessions' `*-session.diff`/`*-REPORT.md`
files, `__pycache__`, etc.) already present in the tree from the last
full build and prior M3 batches — nothing new or unexplained.

## 13. Suggested runtime checks for wdc

Per `rollout-procedure.md`'s Runtime check rules: `nm -g` against
`runapp`/the relevant `.do` first to find live consumers, never launch
GUI apps from this session, no saves against unversioned fixtures.

**`atk/ez` + `atk/typescript` — statically linked into `runapp`,
confirmed live GUI consumers.** `nm -g build/bin/runapp` shows
`_ezapp__*`, `_typescript__*`, `_typescriptapp__*` (i.e. `tscripta`)
all defined directly in the `runapp` binary (not a `.do`) — `ez` and
`help` are both symlinks to `runapp` per the standing convention.
Suggested check: launch `ez` normally (native Terminal.app, not this
session), open a typescript window (`<ESC><TAB>typescript` or the
equivalent menu item) to exercise `tscript.c`'s converted methods —
title get/set, frame get/set, and the 3 retyped command handlers
(`Typescript_DigitCmd`/`Typescript_SelfInsertCmd`/
`typescript_handlereadonly`, exercised by typing regular printable
characters into the typescript's read-only/no-echo mode, e.g. right
after a password-style prompt) — and separately a pipescript
(`pscripta.c`'s `pipescriptapp`) to exercise the other `-pe`'d class.

**`atk/help/src` — dynamically loaded, confirmed live GUI consumer.**
`build/dlib/atk/` contains freshly-installed `help.do`, `helpa.do`,
`helpdb.do`, `helptxtv.do`; `help` is a `runapp` symlink
(`InstallLink(runapp, .../bin/help)` in the Imakefile). Suggested
check: launch `help` normally and browse a few topics (exercises
`help.c`'s 36 newly-converted helpers, including the 4 that hit the
glued-asterisk parser gap — `Search Overviews`/`Search Programs`/the
history-panel toggle commands specifically), confirm `helpaux.c`'s
converted `FinalizeObject` doesn't affect normal quit/cleanup behavior
(exit `help` normally, no crash).

**`ams/msclients/cui` + `ams/msclients/nns` — CLI binaries, confirmed
present.** `build/bin/cuin` (`cui` → `cuin` symlink) and
`build/etc/nns` both freshly installed. Suggested check (per the
standing `smtp-send-test`/AMS convention, read-only where possible):
run `cuin` interactively against a real mailbox and exercise
`GetHeaders`/`GetHeadersFn` (the two functions whose missing forward
declaration this batch added — e.g. any header-listing command) and
`FlagSomething`/`UnflagSomething`/`MaybeFlagSomething` (any
flag/unflag command). `nns` is the news-reader client; a basic
`nns`-driven newsgroup listing would exercise `gl.c`/`mc.c`/`st.c`'s
converted helpers, but I don't know this session's news-server
availability — defer to wdc's judgment on whether that's worth running.

**`ams/msclients/imapsync` — CLI binary, confirmed present, no
GUI-visible consumer.** `build/bin/imapsync` freshly installed.
Suggested check: a dry-run/read-only invocation against a real IMAP
account (per the standing "IMAP is strictly read-only from this
session" rule — wdc should run this, not delegate it back here) to
exercise the 25 retyped forward declarations and the `struct
sync_state` fix specifically — `load_state`/`write_state` are the two
functions that hit the struct-tag-scope trap, so a run that actually
reads and writes its own state file (e.g. a normal sync against a
scratch/test mailbox, not a production one) is the most direct
confirmation.

**No live consumer found (matches Gate 0's own prediction) — report-
only, per `m3-batches.md`'s note that some batches drop to this:**
- `atk/help/maint` (`mkindex`): installs to `build/etc/mkindex`, a
  maintenance tool for building help-index files, not part of any
  running app's normal path.
- `atkams/messages/cmd` (`dumpbin.c`): compiles to an object file via
  `NormalObjectRule()` but has **no** `ProgramTarget`/`InstallProgram`
  anywhere in its Imakefile, and a tree-wide grep of every other
  Imakefile found no reference to it either — `dumpbin.o` is built but
  never linked into anything. Genuinely inert; no runtime check
  possible or needed.
- `doc/mkbrowse` (`mkbrowse`): installs to `build/bin/mkbrowse`, a
  doc-generation tool (class-hierarchy browser output), not part of any
  running app's normal path. A build-time-only sanity check (`mkbrowse`
  against a small directory, capturing output to a scratch file) would
  be the only meaningful check, if wdc wants one — not GUI-visible
  either way.

## 14. Anything that surprised me / didn't match the prompt's expectations

- **Gate 0 undercounted the fallout in exactly the way the taxonomy
  warns about**: 4 of the 9 directories (`atk/help/src`,
  `ams/msclients/cui`, `ams/msclients/imapsync`, `doc/mkbrowse`) hit
  real compile fallout at Gate 1 that Gate 0's dry-run pass and standing
  checklist genuinely could not have predicted — not because the
  checklist was applied wrong, but because each shape only exists once
  `ansify` actually mutates the file (`fix-missing-static-decl`
  inserting a fresh conflicting stub, a struct tag first appearing in a
  newly-hand-written declaration) or only becomes visible once a
  directory's *own* real compile gate runs end-to-end
  (`ams/msclients/imapsync`'s 25-declaration block, invisible to a
  targeted grep because the file had 0 K&R conversions to draw
  attention to it). None of these were DRIFT or genuinely new *tool*
  bugs — every one matched an already-documented species (T1's
  stranded-declaration shape, O3's variadic-stub shape, B3's
  struct-tag-scope trap) recurring in a form Gate 0's static analysis
  couldn't reach. This matches the runbook's own framing (dry-run +
  checklist narrows the space but doesn't replace the real, `-pe`-live,
  compile-gated run) rather than indicating anything wrong with the
  Gate 0 pass itself.
- `ams/msclients/imapsync`'s `imap_sync.c` being almost entirely
  hand-written ANSI C already (only 2 K&R helpers) stood out — every
  other file in this batch is uniformly 1988-1994-era K&R. Consistent
  with it being one of the newer additions to this codebase (the AMS
  IMAP work), and its bug shapes (variadic-stub conflict, struct-tag
  trap) were correspondingly different in character from the rest of
  the batch's "1988-era multi-name declaration list" fallout.
- `atkams/messages/cmd/dumpbin.c` turned out to be even more inert than
  Gate 0's "build-time tooling" prediction — it isn't a tool at all in
  the sense of being linked into anything; it's a compiled-but-unused
  object file with no `ProgramTarget` anywhere in the tree referencing
  it.
- Otherwise the batch closed almost exactly as Gate 0 predicted: 6 of 9
  directories needed only the anticipated `#include <stdlib.h>` fix (or
  nothing at all), and the 3 `.ch` directories' `-pe` rollout went
  through cleanly once the two Gate-0-ruled findings were resolved.

---

# Post-Gate-1: wdc's runtime checks (2026-07-31)

wdc ran the suggested checks directly. Results and one real,
orchestrator-diagnosed-and-fixed bug found along the way:

- **`help`**: ran fine, no issues.
- **`ez`/`typescript`**: **§13's guidance above was wrong** — corrected
  here. `typescript` is not "wired into `ez`"; it's its own standalone
  `runapp`-loaded app (`build/bin/typescript -> runapp`, installed from
  `atk/apps/Imakefile`, launched as `typescript -d`, loading the
  `typescriptapp` class via `class_Load("typescript")`), unrelated to
  `ez`. It doesn't launch at all — traced to an already-known,
  already-documented, pre-existing bug (`roadmap.md`'s "typescript —
  crashes on launch (PTY failure + missing NULL check)", first found
  2026-07-10, explicitly marked "not a regression" even then). Confirmed
  the exact same code (`GetPtyandName` failure at `tscript.c:1273-74`,
  same "Can't connect subchannel" message) is still present, completely
  untouched by this batch's diff — only declaration typing changed
  nearby, not this logic. Not a checkin blocker; a separately tracked
  task, same as roadmap's own framing.
- **`cuin`**: `headers`/`browse` (exercises `GetHeadersFn`, this
  batch's fix) worked fine. `flag 1 test` / re-running `flag`/`unflag`
  (exercises `FlagSomething`/`UnflagSomething`/`MaybeFlagSomething`,
  this batch's fix) worked, but **`dirinfo amsdemo` crashed**:
  `malloc: *** error for object ...: pointer being freed was not
  allocated`, `Abort trap: 6`. **Root-caused by the orchestrator, not
  pre-existing**: `cvEng()` (`ams/libs/shr/utils.c`, returns `char *`,
  points at a static format buffer) is called at `cuifns.c:752`
  (`GetDirInfo`, the `dirinfo` command) with **zero declaration
  anywhere in `ams/msclients/cui`** — neither `ms.h` nor a full
  `cui.h` prototype set is included in any of this directory's 4
  files. Under K&R with no `-Werror=implicit-function-declaration`
  guard (this directory has never had the M2-era `COMPILERFLAGS` line
  — see below), the call silently implicit-declares as `int cvEng()`,
  truncating the real 64-bit pointer return on arm64 and corrupting
  the value passed to `moreprintf`'s `%s` — the same LP64-missing-
  prototype species this project has hit repeatedly (`LP64 Missing
  Prototypes`/`LP64 printf id truncation` memory entries). `dirinfo`
  is the only command in this batch's runtime checks that reaches
  `cvEng`; no other tested command shares this code path, which is
  why it wasn't caught until now (a "first-ever exercised path" find,
  same shape as `typescript`'s own PTY bug above).

  Systematically checked all other 140 functions implicitly declared
  in this directory's real (COMPILERFLAGS-flag-scouted) build —
  `cvEng` is the **only** one that returns a pointer anywhere in the
  tree; every other undeclared call (the bulk of it the `MS_*`/`CUI_*`
  AMS library API surface, plus assorted local helpers) genuinely
  returns `int`/`void`/`boolean`, matching K&R's implicit-int default
  exactly — no truncation risk, confirmed safe as-is.

  **Fix**: added `extern char *cvEng();` to `cuifns.c`, immediately
  after the existing `extern char *StripWhiteEnds();` line (matching
  this file's own established declaration style, and the same pattern
  `ams/libs/cui/cuilib.c` already uses for the identical function:
  `extern char *NextAddress(), *cvEng();`). Rebuilt `ams/msclients/cui`
  clean from scratch, twice, 0 errors. wdc re-ran `dirinfo amsdemo`:
  no crash, correct folder info, and confirmed the earlier `flag 1
  test` had genuinely persisted ("There is one user-defined flags in
  this folder: 'test'" — the "flags" instead of "flag" is a separate,
  pre-existing ~35-year-old grammar bug, confirmed untouched by this
  batch's diff, cosmetic only, not fixed).

  **Corrected COMPILERFLAGS scope assessment for `ams/msclients/cui`**:
  §5's original note framed this the same as `atk/chart`'s ("out of
  scope... noted for awareness," implying a similarly small fix).
  That undersold it. This directory has **141 distinct undeclared
  functions** (vs. chart's 10, all same-directory helper calls) —
  essentially the entire `MS_*`/`CUI_*` AMS API is called without
  `ms.h`/a full `cui.h` anywhere in these 4 files. Closing this gap
  for real would be its own investigation, closer to `AMS1`'s scale
  than `atk/chart`'s. `cvEng` (fixed above) was confirmed the only
  *live* danger in the current set; the rest is a real but
  non-urgent standing gap, flagged here for whoever scopes future
  waves, not attempted in this batch.

- **`imapsync`**: ran fine against a real sync.
- **`nns`**: not testable (no netnews access this session).
- **`mkbrowse`**: ran, "a bit finicky" per wdc but worked — no crash,
  exercised the `browser.c` stale-malloc-declaration fix.
- **`mkindex`**: no dedicated docs shipped with the tool; orchestrator
  read `main()`/`show_usage()` directly and gave wdc a safe `-n`
  (fake-it mode) example using a `key` line (the one opcode path
  explicitly gated behind real/non-fake-it mode in the source, so
  guaranteed no side effects) against a scratch `/tmp` input file. Ran
  clean: `mapping key 'testtopic' to file ...` printed as expected —
  confirms the file's 4 converted helpers work.

All 9 directories' live-reachable functionality now confirmed
working except `typescript` (pre-existing, tracked, non-blocking) and
`nns` (untestable this session, no live consumer to check against).

Files touched beyond Gate 1: `src/ams/msclients/cui/cuifns.c` gained
the `cvEng` declaration (2 lines). No other files changed. Rebuilt and
gated `ams/msclients/cui` clean, twice, after the fix.

---

**Gate 1 + runtime verification complete. Batch A1 closes Wave 5,
including one real bug found and fixed during wdc's own runtime
testing (not caught by Gate 0/Gate 1's static/compile-time checks —
`dirinfo` was the first exercised path to reach `cvEng`). Ready for
the two-commit checkin.**
