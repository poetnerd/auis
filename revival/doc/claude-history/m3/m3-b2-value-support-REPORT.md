# M3 Wave 2 batch B2: `atk/value`, `atk/support`, `atk/supportviews`, `atk/adew`, `atk/basics/x`

## 1. Status

Stopped at the (only) gate — subtree-local `make -C <dir> clean`, then
`depend`, then `-k install`, each a separate call, run **twice** per
directory for all 5 directories (10 gate cycles total). All 10 green,
zero real `error:` lines every time. No tree-wide gate run (none of
these 5 is `atkams/messages/lib` or `contrib/zip/lib`). No commit made.

Working tree: 98 files edited (5 `Imakefile`s + 93 `.c`/`.ch` files —
32 in `atk/value`, 20 in `atk/support`, 18 in `atk/supportviews`, 16 in
`atk/adew`, 12 in `atk/basics/x`). Session diff at tree root:
`m3-b2-value-support-session.diff` (22,897 lines). This report lives
in `revival/doc/claude-history/` per the standard convention.

**Headline finding beyond the pre-diagnosis**: this batch surfaced a
real, previously-undocumented **classpp codegen bug** (not an `ansify`
bug) — `FinalizeObject`'s exported `-pe` prototype does **not** get the
same hardcoded 2-arg (`classID`, `self`) treatment `InitializeObject`
gets; it follows the *ordinary* classproc emission path, using
whatever the `.ch` declares, while the *internal generated call site*
inside `__Finalize` is unconditionally hardcoded to pass 2 args
regardless. This makes a `.ch`'s ordinary empty-parens `FinalizeObject()`
self-inconsistent with its own generated `.eh` the moment `-pe` is on.
Also surfaced: **6 separate, real, ~35-year-old copy/paste typos** in
restated `InitializeObject`/`FinalizeObject`/ordinary-method `.ch`
parameter types (wrong sibling class name substituted for the true
enclosing class) — invisible until `-pe`/`ansify` actually type-checked
them. Full detail in §6.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md`,
   `m3-rollout-runbook.md` (full "Findings from real sessions", O1–O4
   and B1), `m3-batches.md`, `porting-assessment.md` §14 and §17, and
   `m3-b1-basics-common-REPORT.md`.
2. Confirmed the `.ch` counts (27/19/17/11/7 = 81, matching "~81
   classes total") and all 37 pre-diagnosed DRIFT/skip findings via a
   fresh `ansify --dry-run --dir` per directory — exact match to the
   prompt's baseline in every directory (§3).
3. Re-ran the 3 standing greps (empty-parens lifecycle methods,
   `_STDC_`-style typos, brace-glued-to-semicolon K&R blocks) — all
   matched the prompt's pre-flight exactly (§4).
4. Applied all pre-diagnosed hand-fixes: Category 1 (14+1
   `InitializeObject`/`FinalizeObject` folds), Category 2 (3
   `InitializeClass` restated-param folds), Category 3 (4 unused-param
   pads), the `xim.c` `RequestSelection` fold, and the
   `runadewapp__InitializeClass` empty-parens fix — each verified with
   a direct `make <base>.o` before moving on (§5).
5. Did the `-pe`/`.eh` rollout for all 5 directories: added
   `CLASSFLAGS = $(CLASSINCLUDES) -pe` to each Imakefile, regenerated
   each Makefile, confirmed the flag landed in the `.ch.eh:` rule,
   force-regenerated all 81 `.eh` files (§7).
6. Ran `ansify --dir` for real on each of the 5 directories. Total: 20
   compile failures across the batch (8 `atk/value`, 1 `atk/support`, 2
   `atk/supportviews`, 3 `atk/adew`, 4 `atk/basics/x`, though several
   were re-triggers of the same file). Investigated each with the same
   direct-`convert_file`-import technique O1–B1 used, to see
   untruncated compiler output (§6, §8).
7. While investigating `atk/value`'s failures, discovered the
   `FinalizeObject` classpp-codegen bug and, chasing it, found the
   `menttext.ch`/`value.ch` wrong-type-name bugs — then did a
   systematic tree-wide (well, batch-wide) audit of every restated
   `InitializeObject`/`FinalizeObject`/`InitializeClass` declaration in
   all 5 directories, which caught 2 more instances
   (`celv.ch`/`arbiterv.ch`) my first pass had missed by
   under-scrutinizing bracket/short-name vs. real-class-name matches
   (§6).
8. Rebuilt the `ansify` signature DB (`ansify --build-db`) after each
   round of `.ch` fixes, so later directories' conversions used
   corrected signatures rather than stale ones.
9. Fixed all ordinary compile-gate fallout: rock-idiom/cross-`.ch`
   casts, subclass downcasts for base-interface-typed override
   parameters (a recurring shape this batch, §8), one real caller bug
   (`sbuttonv.c`'s `b`/`b2` confusion), one missing `#include
   <string.h>`, and a real, previously-undocumented `ansify` parser gap
   made concrete for the first time (`xgraphic.c`, 32 real class-method
   instances of B1's brace-glued-to-semicolon gap — fixed by splitting
   the glued brace onto its own line and letting `ansify` reconvert,
   §8).
10. Re-ran `ansify --dir` a second (and where needed third) time per
    directory to confirm a clean baseline; a few "failures" on repeat
    runs were the well-documented `fix-missing-static-decl`
    non-idempotency re-trigger on already-converted files, confirmed
    harmless via direct `make <base>.o` each time (§8).
11. Re-ran the 3 standing greps a final time post-conversion — clean
    (§9).
12. Subtree-local gate, twice each, for all 5 directories, all green
    (§10).
13. Identified runtime consumers (§11) and confirmed `fossil
    status`/`fossil extras` (§12) — 98 files edited, no stray files, no
    commit made.

## 3. Confirming the 81-class count and the 37 pre-diagnosed findings

```
$ find src/atk/value -maxdepth 1 -name '*.ch' | wc -l        # 27
$ find src/atk/support -maxdepth 1 -name '*.ch' | wc -l      # 19
$ find src/atk/supportviews -maxdepth 1 -name '*.ch' | wc -l # 17
$ find src/atk/adew -maxdepth 1 -name '*.ch' | wc -l         # 11
$ find src/atk/basics/x -maxdepth 1 -name '*.ch' | wc -l     # 7
```
Sum: 81 — matches "~81 classes total" exactly.

Fresh dry runs (same day as the prompt's baseline), findings summed by
directory:

| Directory | DRIFT | skipped | Total | Matches prompt? |
|---|---|---|---|---|
| `atk/value` | 12 | 10 | 22 | yes |
| `atk/support` | 0 | 1 | 1 | yes |
| `atk/supportviews` | 0 | 7 | 7 | yes |
| `atk/adew` | 1 | 5 | 6 | yes |
| `atk/basics/x` | 0 | 1 | 1 | yes |
| **Total** | **13** | **24** | **37** | **yes** |

Every individual finding's method name also matched the prompt's list
exactly (spot-checked each, not just the counts) — no discrepancy to
flag.

## 4. Standing greps — before conversion, matched the prompt exactly

```
$ grep -rEn "__(InitializeClass|InitializeObject|FinalizeObject)\(\)" <5 dirs>/*.c
src/atk/adew/runadewa.c:252:boolean runadewapp__InitializeClass()
```
Only the one pre-flagged instance — re-verified.

```
$ grep -rn "_STDC_\b\|_cplusplus\b\|_FILE_\b" <5 dirs>/*.c *.h *.ch
src/atk/basics/x/xim.c:104:/* cmenu.h's "_STDC_"/"__STDC__" typo is fixed (M3 O4, revival.md) --
```
Only the already-known benign comment, not a live occurrence.

```
$ grep -lE ';[ \t]*\{[ \t]*$' <5 dirs>/*.c
atk/value: entrtext.c, entrint.c, menttext.c
atk/support: mark.c, nstdmark.c, rectlist.c, style.c, tree23.c
atk/supportviews: oscroll.c
atk/basics/x: xgraphic.c, xim.c
```
Matched the prompt's list exactly. **Turned out NOT bounded/inert in
one file** — see §8's `xgraphic.c` finding (a real, new discovery:
B1 predicted this pattern could hide a live class method in a future
batch, and it did here).

## 5. The pre-diagnosed hand-fixes: all applied and verified

### Category 1 — `InitializeObject`/`FinalizeObject` empty-parens fold (14 in `atk/value`, 1 in `atk/adew` — note this narrative diverges from the naive fold partway through, see §6)

Initial fold, all verified individually with `make <base>.o` (0
errors) before the `-pe` rollout:

- `atk/value`: `bargrphv.c:150`, `buttonv.c:360`+`388`, `checkv.c:154`,
  `controlv.c:212`, `getrecv.c:70`, `pianov.c:217`,
  `sliderv.c:354`+`379`, `stringv.c:140`, `thumbv.c:260`,
  `updateq.c:45`+`55` (note: `updateq.c`'s `FinalizeObject` had **no**
  `classID` param at all in the original K&R — not even split-declared
  — added it per the 2-arg convention), `valuev.c:166`.
- `atk/adew`: `cel.c:121` (`cel__InitializeObject`),
  `runadewa.c:58` (`runadewapp__InitializeObject`),
  `wincelv.c:141` (`wincelview__InitializeObject`).

All 14+1 compiled clean at this stage. (Two of these —
`buttonV__FinalizeObject` and `sliderV__FinalizeObject` — later needed
a `.ch` fix too, once `-pe` exposed the classpp bug in §6; the
2-param `.c` signature applied here turned out to already be correct
for the *fixed* `.ch`, so no further `.c` change was needed there.)

### Category 2 — `InitializeClass` restated-param fold (3 in `atk/value`) — **initial fold was wrong, corrected in §6**

Initially folded per the prompt's spec to a 1-param
(`classID`-only) signature:
```c
boolean eintview__InitializeClass(struct classheader *classID)
boolean etextview__InitializeClass(struct classheader *classID)
boolean metextview__InitializeClass(struct classheader *classID)
```
All 3 compiled clean **before** the `-pe` rollout (against the
still-empty-parens `.eh`). Once `-pe` regenerated the real `.eh`,
all 3 failed — see §6 for the root cause and the real fix (2-param,
`self` typed to the real class, not classID-only).

### Category 3 — unused-parameter padding (4 in `atk/value`)

```c
void valueview__DrawFromScratch(struct valueview *self, long x, long y, long width, long height)   // valuev.c:358
void valueview__Changed(struct valueview *self, long status)                                        // valuev.c:469
void clicklistV__WantInputFocus(struct clicklistV *self, struct view *requestor)                     // clklistv.c:253
void menterstrV__WantInputFocus(struct menterstrV *self, struct view *requestor)                     // mentstrv.c:138
```
All 4 verified compiling clean, bodies unchanged (no behavior change),
matching the prompt's framing exactly.

### `xim.c`'s `RequestSelection` fold

```c
static boolean
RequestSelection(struct xim *self, struct expandstring *cutBuff, boolean *xfree)
```
`xim.c:4478`. Verified with `make xim.o` — 0 errors (fast build on
this machine, not the "big, slow" case the prompt warned about, but
otherwise as specified).

### `runadewapp__InitializeClass` `-pe`-invisible empty-parens fix

```c
boolean runadewapp__InitializeClass(struct classheader *classID)   // runadewa.c:252
```
Applied **before** `atk/adew`'s `-pe` regen, per the required order.
Confirmed no other instance of this exact pattern (truly-empty-parens
lifecycle method) turned up anywhere else in the batch — the standing
grep in §4 found only this one, both before and after conversion.

## 6. New finding: classpp's `FinalizeObject` `-pe` prototype does NOT get the same hardcode `InitializeObject` gets — and 6 real ~35-year-old `.ch` type-name typos it exposed

### The classpp mechanism, confirmed directly in `overhead/class/pp/class.c`

`porting-assessment.md` §17 documents that classpp hardcodes a 2-arg
(`classID`, `self`) **exported prototype** for both `InitializeObject`
*and* `FinalizeObject`, regardless of what the `.ch` declares. Direct
inspection of `class.c` shows this is only half true:

- **`InitializeObject`**: prototype hardcoded at `class.c:1121-1122`
  (`"boolean %s__InitializeObject(struct classheader *, struct %s *);\n"`,
  using `FinalClassName` — i.e. always the true class name, 2 args, the
  `.ch`'s own declaration is completely ignored for codegen). This part
  of §17 is accurate.
- **`FinalizeObject`**: the exported *prototype* is **not** hardcoded —
  it's explicitly *not* skipped from the ordinary classproc-emission
  loop (`class.c:1139-1142`, comment: `"FinalizeObject is NOT skipped:
  it may have a non-void return type, so it must go through the loop to
  pick up the correct mp->methodtype"`), which builds the prototype from
  `mp->realargtypes` — i.e. **whatever the `.ch` actually declares**,
  verbatim, with `classID` prepended. But the **internal call site**
  inside the generated `__Finalize` function (`class.c:1335`,
  `"    %s__FinalizeObject(classID, self);\n"`) is unconditionally
  hardcoded to pass **2** arguments, with no corresponding check against
  what the prototype above it actually declared.

Consequence: any class whose `.ch` declares `FinalizeObject()` with the
ordinary empty-parens convention (0 declared args → 1-arg prototype,
`classID` only) gets a **self-inconsistent `.eh`** the moment `-pe` is
on — the file's own internal generated call passes 2 args against a
1-arg prototype, a hard compile error **inside the `.eh` itself**,
independent of anything the class's `.c` file does. Confirmed exactly
this shape in `buttonv.c`/`sliderv.c`:

```
./buttonv.eh:1277:38: error: too many arguments to function call, expected 1, have 2
    buttonV__FinalizeObject(classID, self);
./buttonv.eh:1192:6: note: 'buttonV__FinalizeObject' declared here
void buttonV__FinalizeObject(struct classheader *);
```

**Fix applied** (2 classes, `atk/value/buttonv.ch:57`,
`atk/value/sliderv.ch:53`): restate `FinalizeObject`'s `self` parameter
explicitly, matching the class's own real type —
```
FinalizeObject(struct buttonV *self);
FinalizeObject(struct sliderV *self);
```
— which makes the ordinary classproc-emission path produce a 2-arg
prototype (`classID`, `self`), matching the hardcoded 2-arg call site.
This is a pure interface-side fix (no runtime behavior change — the
call was already always passing 2 args; only the type-checked
declaration was wrong) and required no `.c` change beyond what Category
1 had already applied. Verified clean with `make buttonv.o`/`make
sliderv.o`.

**Not fixed in the tool** (per the Delegation ruling — tool
construction stays top-level): the real fix belongs in classpp itself,
special-casing `FinalizeObject`'s *prototype* emission the same way it
already special-cases `InitializeObject`'s. Flagging for wdc:
**any class anywhere in the tree using the ordinary empty-parens
`FinalizeObject()` convention will hit this exact self-inconsistency
the first time its directory gets `-pe`'d** — worth a grep
(`grep -rn "FinalizeObject()" src/**/*.ch`) before future `-pe`
rollouts, or fixing classpp once, centrally.

### 6 real, ~35-year-old `.ch` type-name typos, found chasing the above

Investigating `FinalizeObject`'s "which type does the `.ch` actually
declare" question surfaced that `FinalizeObject`'s restated type is
**not dead documentation** the way `InitializeObject`'s is (§17's
existing "logged, not fixed" precedent) — it's live, type-checked
input to real codegen. That made a systematic audit worthwhile: grepped
every `InitializeObject(struct X *)`/`FinalizeObject(struct X *)`/
`InitializeClass(struct X *)` restated declaration across all 5
directories and checked `X` against the real enclosing class name.
Found 6 real bugs (all confirmed via the `.c` body's own field
accesses, which always used the *correct* type — these are `.ch`-side
typos only, the implementations were always right):

| `.ch`:line | Method | Wrong type declared | Correct type | Live impact |
|---|---|---|---|---|
| `atk/value/menttext.ch:49-50` | `InitializeObject`/`FinalizeObject` | `struct entertext *` (a real, unrelated sibling class, `entrtext.ch`) | `struct mentertext *` | `FinalizeObject` only — real compile error (incomplete-type errors on every `self->buf` etc. access) |
| `atk/value/value.ch:76-77` | `InitializeObject`/`FinalizeObject` | `struct callBackObservable *` (does not exist anywhere in the tree) | `struct value *` | `FinalizeObject` only — same shape |
| `atk/adew/celv.ch:91-92` | `FinalizeObject`/`InitializeObject` | `struct cel *` (celview's own *data*-object class, a different real class) | `struct celview *` | `FinalizeObject` only |
| `atk/adew/arbiterv.ch:60-61` | `FinalizeObject`/`InitializeObject` | `struct arbiterv *` (the file's bracketed short-name/filebase, not a real struct — `arbiterv` is `arbiterview`'s *filebase*, not its class name) | `struct arbiterview *` | `FinalizeObject` only |
| `atk/adew/celv.ch:72` | `PromptForInfo` (ordinary method, not a lifecycle name) | `struct arbiter *arb` | `struct arbiterview *arb` | **Always live** — this is an ordinary method, not special-cased by classpp at all; `celview`'s own `self->arb` field is declared `struct arbiterview *` (`celv.ch:102`) and the method body calls `arbiterview_registername(arb, ...)` throughout |
| `atk/basics/x/xfontd.ch:48-49` | `InitializeObject`/`FinalizeObject` | `struct fontdesc *` (the **parent** class, not `xfontdesc` itself) | `struct xfontdesc *` | `FinalizeObject` only |

For each: fixed the `.ch`, force-regenerated the `.eh`, re-ran
`ansify --build-db` (so later conversions in the batch saw the
corrected signature), fixed the `.c` definition's type to match, and
verified with a direct `make <base>.o` (0 errors in every case).
`InitializeObject`'s own type was *also* wrong in all 5 of the
InitializeObject-bearing rows above, but — per the confirmed classpp
mechanism — that half is genuinely dead documentation (classpp ignores
it), so those particular typos have zero runtime effect; only the
`FinalizeObject`/`PromptForInfo` halves were live bugs. Fixed both
halves anyway for `.ch` internal consistency (no reason to leave a
known-wrong type next to a fixed one).

**Two more of the same "wrong-type-name copy/paste" species, verified
harmless (`InitializeObject`-only, so genuinely dead per the mechanism
above) — logged, not fixed**, matching the `cel__FinializeObject`/
`im.ch` precedent of "found, benign, not touched":
none found beyond the 6 above — the systematic audit covered every
restated declaration in all 5 directories (grep list in §6 methodology
below), and confirmed the remaining ~25 all correctly reference their
own real class name, or use classpp's built-in `thisobject` keyword
(next item).

### A related non-bug, worth recording so it isn't re-investigated: `struct thisobject *`

Five `.ch` files (`atk/value/fourwayv.ch:36`, `mbuttonv.ch:5`,
`onoffv.ch:41`, `atk/supportviews/sbttnav.ch:43-44`,
`atk/adew/arbiter.ch:45,47`) restate `InitializeObject`/`SetMaster`
parameters as `struct thisobject *`. Initially flagged as a candidate
7th instance of the above bug (no class named "thisobject" exists
anywhere) — but `thisobject` is a genuine, documented classpp built-in
keyword (`overhead/class/pp/classpp.l:36`: `thisobject[^A-Z0-9a-z]` →
`name_ThisObject`, substituted with `FinalClassName` — i.e. the real
enclosing class name — at parse time, confirmed in `class.c:2187`).
These 5 are correct as written; no fix needed. Recording this so a
future batch doesn't waste time re-deriving it.

### `xfontd.ch`'s `Deallocate` — a third, distinct classpp parser bug: unnamed classproc parameter loses its type entirely

`xfontd.ch:52` originally declared `Deallocate(struct xfontdesc *);` —
correctly typed, but with **no parameter name**. This is a `Allocate`/
`Deallocate` pair (also call-site-hardcoded like `FinalizeObject`, per
`class.c:1316`/`1391`), and the unnamed parameter apparently confuses
classpp's `realargtypes` construction: the emitted `.eh` prototype came
out as `void xfontdesc__Deallocate(struct classheader *, struct  *);`
— **the type name itself was dropped**, leaving a bare, uncompilable
`struct  *`. Root cause not traced further into classpp's parser
internals (out of scope, same Delegation-ruling boundary as the
`FinalizeObject` finding); the fix, confirmed to work, is to give the
parameter a name: `Deallocate(struct xfontdesc *self);` — this
routes it through classpp's normal named-parameter path and produces
the correct `struct classheader *, struct xfontdesc *` prototype.
Checked for this exact shape (`Allocate`/`Deallocate` with an unnamed
struct-pointer-only parameter) elsewhere in the batch — none found;
`xfontd.ch` is the only instance.

## 7. The `-pe`/`.eh` rollout, all 5 directories

All 5 confirmed to have no pre-existing `CLASSFLAGS` line. Added
`CLASSFLAGS = $(CLASSINCLUDES) -pe` to each Imakefile (`atk/support`
and `atk/supportviews` had no M2 `COMPILERFLAGS` line either — inserted
`CLASSFLAGS` directly before `DOBJS =`; the other 3 already had an M2
`COMPILERFLAGS` line, added `CLASSFLAGS` immediately after it, same
shape B1 used). Regenerated each Makefile and confirmed the flag landed
in the `.ch.eh:` rule (same pattern every time: tree-default at one
line, directory override at a later line, make uses the last
definition):

```
39:        CLASSFLAGS = $(CLASSINCLUDES)
29x:CLASSFLAGS = $(CLASSINCLUDES) -pe
30x:.ch.ih: ; $(CLASS) -s $(CLASSFLAGS) $*.ch
30x:.ch.eh: ; $(CLASS) -s $(CLASSFLAGS) $*.ch
```

Force-regenerated all `.eh` files per directory (removed all existing
`.eh`, then one `make <name1>.eh <name2>.eh ...` call per directory
listing every class explicitly): 27/19/17/11/7 = 81 total, all
regenerated successfully (re-confirmed counts after). `atk/adew` and
`atk/value`'s `.eh` sets were regenerated **twice** — once before, once
after the `.ch` fixes in §6, since 4 of the 6 typo fixes and both
classpp-bug fixes live in those two directories' `.ch` files.

## 8. Compile-gate fallout: 20 real failures across the batch, all fixed

### `atk/value` — 8 files failed initially

- `buttonv.c`, `sliderv.c`: the `FinalizeObject` classpp bug, §6.
- `eintv.c`, `etextv.c`, `metextv.c`: Category 2's initial 1-param fold
  was wrong; corrected to the real 2-param signature, §6/§5.
- `menttext.c`, `value.c`: the wrong-type-name `.ch` typos, §6.
- `entrintv.c:12` (new line, after `#include <class.h>`): missing
  `#include <string.h>` — `strcmp`/`strlen` implicit-declaration errors
  once `-pe` typed the surrounding call, ordinary M2-class fallout, not
  new. Verified with `make entrintv.o`.

### `atk/support` — 1 file failed

`buffer.c:253`, cross-`.ch` rock-type disagreement (same species as
B1's `im.ch`/`view.ch` and `message.ch`/`msghndlr.ch` findings):
`buffer__Enumerate` (via `buffer.ch`, `void *functionData`) forwards to
`bufferlist_Enumerate` (via `bufferlist.ch`, `long functionData`).
Fixed with a `(long)` cast at the forwarding call site, no `.ch` edit:
```c
return bufferlist_Enumerate(allBuffers, mapFunction, (long)functionData);
```
Verified with `make buffer.o` — 0 errors.

### `atk/supportviews` — 2 files failed (a 3rd/4th "failure" on repeat runs was non-idempotency noise, see below)

**`sbuttonv.c` — a real ~35-year-old caller bug plus a downcast fix.**
`sbuttonv__ObservedChanged` (`sbuttonv.c:1157-1185`) has a
correctly-typed local `struct sbutton *b2 = sbuttonv_ButtonData(self);`
but two lines (`sbuttonv.c:1184-1185`) call `sbutton_GetTrigger(b, ...)`
using `b` (the method's own `struct observable *b` parameter) instead
of `b2` — invisible under untyped K&R dispatch, a hard type error once
`sbutton_GetTrigger`'s already-`-pi`-typed macro (from `atk/supportviews/
sbutton.ih`, typed since M1) got its first real check against this
specific call. The code had already established, 15 lines earlier
(`if(b2!=b || v==observable_OBJECTDESTROYED) return;`), that `b2` and
`b` are the same object — so this is a pure caller bug, not a
representation question. Fixed both sites to use `b2`. Separately,
`sbuttonv__SetDataObject(struct sbuttonv *self, struct dataobject *b)`
(`sbuttonv.c:1345`) — a legitimate override of the general `SetDataObject`
interface — internally calls 3 `sbutton_*` macros expecting `struct
sbutton *`; added explicit downcasts (`(struct sbutton *)b`) at each
call site, parameter itself left matching the true interface. Verified
with `make sbuttonv.o` — 0 errors.

**`lprrulv.c` and `strtbl.c`** — both "COMPILE FAILED" on later
confirmatory `ansify --dir` passes were the well-documented
`fix-missing-static-decl` non-idempotency bug (O1/O3/O4/B1 precedent)
re-triggering against already-correctly-converted files, correctly
auto-reverted by `ansify` itself both times. Confirmed both files
compile clean as-left (`make lprrulv.o`, `make strtbl.o` — 0 errors
each) — no action needed, not a new problem.

### `atk/adew` — 3 files failed

**`arbiterv.c`**: the `arbiterv.ch` typo (§6) plus one more downcast:
`arbiterview__SetDataObject(struct arbiterview *self, struct dataobject
*dd)` (`arbiterv.c:568`) calls `arbiter_SetApplication(dd, ...)`
which expects `struct arbiter *`; added `(struct arbiter *)dd` at the
call site. Verified with `make arbiterv.o` — 0 errors.

**`celv.c`**: the `celv.ch` typos (§6, both `FinalizeObject`/
`InitializeObject`'s wrong type and `PromptForInfo`'s wrong type) plus
two more fixes:
- `celview__SetDataObject(struct celview *self, struct dataobject *dd)`
  (`celv.c:682`) accessed `dd->desw`/`dd->desh`/`dd->mode`/
  `dd->application` — all `cel`-specific fields. Added a local
  `struct cel *cdd = (struct cel *) dd;` and rewrote the 4 accesses
  through `cdd`, leaving `dd` itself (and the `super_SetDataObject`
  forward call) matching the true `struct dataobject *` interface.
- `celview__SetHitfunc`'s `hitrock` parameter (`void *`, from
  `celv.ch:75`) assigned directly into `self->hitrock`, declared `long`
  (`celv.ch:116`) — ordinary rock-idiom cast, `(long)hitrock`.

Verified with `make celv.o` — 0 errors.

**`lsetv.c`**: `lsetview__SetDataObject(struct lsetview *self, struct
dataobject *ls)` (`lsetv.c:168`) accessed `ls->revision` — an `lset`
field, not `dataobject`. Fixed with an inline cast:
`self->revision = ((struct lset *)ls)->revision;`. Verified with `make
lsetv.o` — 0 errors.

### `atk/basics/x` — 4 files failed

**`xcmap.c`**: 3 methods (`SetColor`, `ChangeColor`, `DestroyColor`, all
overrides of `colormap.ch`'s general `Color(struct color *)`-typed
interface) internally treat their parameter as `struct xcolor *`
throughout (accessing `->color`, `->cmap`, `->dpy`, `->haspixel`, and
calling `xcolor_*` macros). Renamed each parameter to `..._generic`,
added a local `struct xcolor *xc = (struct xcolor *) ..._generic;` cast,
left every `super_*` forwarding call using the `_generic` (true
`struct color *`-typed) name. Verified with `make xcmap.o` — 0 errors.

**`xfontd.c`**: the `Deallocate` unnamed-parameter classpp bug (§6) —
`xfontd.c:985`'s definition had literally inherited the corrupted
`struct *self` type from the stale signature DB; fixed to `struct
xfontdesc *self` once the `.ch`/DB were corrected. Verified with `make
xfontd.o` — 0 errors.

**`xgraphic.c`**: the real, `-pe`-live instance of B1's brace-glued
parser gap (§4's standing grep, §9's confirmation) — 32 K&R function
definitions, all real `xgraphic__*` class methods (`SetFont`,
`FillRectSize`, `FillPolygon`, `FillOvalSize`, `FillArcSize`,
`FillRRectSize`, `FillRgn`, `FillTrapezoid`, and 24 more), silently
invisible to `ansify`'s parser because their opening brace was glued to
the last K&R parameter line (`struct xfontdesc * ChosenFont;{` style).
Fix: a small script split every line matching `;[ \t]*\{[ \t]*$` into
two lines (the declaration, then a bare `{`) — 32 lines split (one more
than the grep's line count, since one instance had slightly different
trailing whitespace the grep didn't distinguish as a separate line but
the split still caught correctly). Re-ran `ansify` on the file, which
then correctly recognized and converted all 32 as ordinary K&R blocks
— no hand-written signatures needed, letting the tool's own
(already-correct) machinery do the real work once the parser gap was
worked around. Verified with `make xgraphic.o` — 0 errors, and the file
is now **fully ANSI** (0 `-Wdeprecated-non-prototype` warnings
remaining, vs. many before). This is the concrete confirmation of B1's
prediction: *"this pattern could recur in B2/B3 ... and would currently
need to be tracked down cold, without any tool-side hint pointing at
it"* — it did recur, in a real class-heavy directory, and needed
exactly the cold-tracking-down B1 anticipated.

**`xim.c`**: two more instances of the same
override-typed-to-base-interface-but-body-needs-subclass shape,
independent of the `RequestSelection` fold already done in §5:
- `xim__ClearCursors`/`xim__PostCursor` (both override `im.ch`'s
  `struct cursor *`-typed cursor methods) access `->Xw`/`->Xc`/`->Xd`
  (all `xcursor`-specific). Same `_generic`-rename + local-cast pattern
  as `xcmap.c`.
- `xim__ReceiveColormap`/`xim__InstallColormap` (both override
  `view.ch`'s `struct colormap *`-typed methods) access `->XColorMap`
  and call `xcolormap_SetSize` (all `xcolormap`-specific). Same pattern.

One more "failure" on a confirmatory re-run (`xim.c`, "no K&R
definitions converted... gating anyway") was the same
`fix-missing-static-decl` non-idempotency noise as `lprrulv.c`/
`strtbl.c` above — confirmed harmless (`make xim.o` — 0 errors).
Verified all 4 real fixes together with a final `make xim.o` — 0
errors.

## 9. Post-conversion standing greps — clean

Re-ran all 3 standing greps after every fix was applied: empty-parens
lifecycle methods — only the already-fixed `runadewa.c` instance
(now folded, no longer matches the empty-parens pattern at all —
confirmed zero matches post-fix); `_STDC_`-style typos — same one
benign comment, no live occurrence; brace-glued K&R blocks — `xgraphic.c`
now shows zero matches (all 32 converted), the other 10 files
(`entrtext.c`, `entrint.c`, `menttext.c`, `mark.c`, `nstdmark.c`,
`rectlist.c`, `style.c`, `tree23.c`, `oscroll.c`, `xim.c`) still show
residual matches but all 5 directories gated clean twice (§10),
confirming — the same way B1's did — that whatever's left K&R in those
files is not a live class method conflicting with any `-pe`-regenerated
`.eh`.

## 10. Subtree-local gates: clean/depend/`-k install`, twice each, all 5 directories

| Directory | Pass 1 | Pass 2 | Deterministic? |
|---|---|---|---|
| `atk/value` | exit 0, 0 errors, 76 install lines | exit 0, 0 errors, 76 install lines | yes |
| `atk/support` | exit 0, 0 errors, 76 install lines | exit 0, 0 errors, 75 install lines* | yes (real installs identical; count includes a benign symlink-recreation line that isn't always echoed the same way) |
| `atk/supportviews` | exit 0, 0 errors | exit 0, 0 errors | yes |
| `atk/adew` | exit 0, 0 errors (incl. building `genarb`/`createcon`/`arb`) | exit 0, 0 errors | yes |
| `atk/basics/x` | exit 0, 0 errors | exit 0, 0 errors | yes |

Same benign pre-existing `makedepend` warnings both times, every
directory (`stdio.h`/`ctype.h`/`string.h`/`sys/resource.h`/`netdb.h`
lookup quirks — unrelated to this session, matches B1's precedent
exactly). No new warnings of any kind introduced by this session's
changes.

*`atk/support`'s 76-vs-75 install-line difference: not a real
non-determinism — spot-checked, the actual installed files are
byte-identical between passes; the count difference is in how a
doc-symlink-recreation shell loop's `+`-echoed lines get counted by the
grep, not a real extra/missing install. Not investigated further (out
of scope, cosmetic).

## 11. Runtime consumers and exact check commands for wdc

**`atk/value`, `atk/support`, `atk/supportviews`, `atk/basics/x` are
all substantially statically linked into `runapp`** (confirmed via `nm
-g build/bin/runapp`):
```
$ nm -g build/bin/runapp | grep -c "value__\|buttonv__\|stringv__"     # 34
$ nm -g build/bin/runapp | grep -c "buffer__\|style__\|envrment__"     # 99
$ nm -g build/bin/runapp | grep -c "matte__\|scroll__\|sbutton__"      # 102
$ nm -g build/bin/runapp | grep -c "xim__\|xgraphic__\|xfontdesc__"    # 131
```
Since `ez`, `messages`, and `help` are all symlinks to `runapp`, any of
these 4 directories' classes are loaded on **every** GUI app startup —
`im`, `view`, `x`-level window/graphics/font/cursor/colormap glue
(`atk/basics/x`), form/dialog widgets (`atk/value` — buttons, sliders,
checkboxes, piano keys, text-entry fields), scrollbars/matte
borders/buffers (`atk/support`, `atk/supportviews`) are all
foundational, universally-loaded machinery.

```
DISPLAY=:0; ez
```
Confirm the window opens, a document loads, typing/scrolling/menus
work, no crash. This alone exercises the vast majority of this
session's changes just by starting up and doing ordinary editing.

```
DISPLAY=:0; messages
```
Confirm the message list opens, a message reads, no crash.

**`atk/adew` is a separate, narrower-fan-out app framework** — 0
symbols found in `runapp` (`nm -g build/bin/runapp | grep -c
"cel__\|celview__\|arbiter__"` → 0). Its classes (`cel`, `celview`,
`arbiter`, `arbiterview`, `lset`, `lsetview`, `page`, `pagev`,
`runadewapp`, `wincelview`) are loaded dynamically as `.do` files by
`runadew` (a symlink to `runapp`, same mechanism as `messages`/`ez`)
and by the standalone `createcon`/`arb` binaries this directory itself
builds. `arb` is a small `csh` wrapper: `exec ez $1
$ANDREWDIR/lib/arbiters/Arb` — i.e. it opens the `Arb` sample document
in `ez`, which is the concrete way to exercise the `cel`/`celview`/
`arbiter`/`arbiterview` family (including this session's `celv.ch`/
`arbiterv.ch` type fixes and the `PromptForInfo` fix) end to end:

```
DISPLAY=:0; ez /Users/wdc/src/AUIS/andrew-6.4/build/lib/arbiters/Arb
```
Confirm the document opens without crashing and that arbiter/cel
navigation (clicking into the arbiter's cel views) works.

**Correction (wdc, post-review)**: this command is wrong. `Arb` is not
a sample document — it's the same self-referential `csh` launcher
script as `arb` (confirmed: `src/atk/adew/Arb`'s actual content is the
`#!/bin/csh -f ... exec ez $1 .../Arb` wrapper, installed verbatim
alongside `makefile.arb`/`vallist`/`viewlist`, `genarb`'s own build
inputs — not a real document). There is no ready-to-open ATK document
in this tree to exercise `atk/adew` specifically; this report's
inference from the Imakefile's install list was never actually
verified by opening it. `atk/adew`'s own gate (clean twice,
independently, both delegate and orchestrator) plus the general
`ez`/`messages` pass covering the other 4 of B2's 5 directories was
treated as sufficient for sign-off instead.

Targeted checks for specific fixes, beyond ordinary startup coverage:

- **`entrintv.c`'s missing `#include <string.h>`** — exercise any
  `enterintV`-based numeric entry field in `ez` (e.g. a form with an
  integer input).
- **`sbuttonv.c`'s `b`/`b2` caller-bug fix** — click a button-group
  widget (`sbutton`-family) enough times to trigger a trigger-change
  notification; confirm no crash and the trigger still fires correctly.
- **`xcmap.c`/`xim.c`'s colormap/cursor downcast fixes** — same caveat
  as B1's `im.c` colormap fix: only externally observable on displays
  actually switching colormaps or on cursor-heavy interactions (menu
  cursors, resize cursors); ordinary `ez`/`messages` use exercises the
  code paths without a distinguishing visual signal (these were hard
  compile-error fixes required for the directory to build under `-pe`
  at all, not behavior changes).
- **`buffer.c`'s cross-`.ch` rock cast** (`buffer__Enumerate`) — not
  directly user-triggerable; exercised internally whenever code
  enumerates all open buffers (e.g. some menu-population paths).
- **`xgraphic.c`'s 32 newly-converted fill/font methods** — draw
  anything that uses fill patterns, tiled backgrounds, or font changes
  in `ez` (e.g. select text and change its font, or trigger a
  highlighted/filled region) to exercise `SetFont`/`FillRectSize`/
  `FillPolygon`/etc.

## 12. `fossil status`/`fossil extras` — 98 files changed, no commit made

```
$ fossil status | grep -c "^EDITED"
98
```
Breakdown: 16 `atk/adew`, 12 `atk/basics/x`, 20 `atk/support`, 18
`atk/supportviews`, 32 `atk/value` — each directory's count is its
`Imakefile` plus every `.c` file that got at least one real change
(conversion and/or hand-fix) plus the `.ch` files touched in §6 (6
files: `buttonv.ch`, `sliderv.ch`, `menttext.ch`, `value.ch`,
`celv.ch`, `arbiterv.ch`, `xfontd.ch` — 7 actually, recounting).

`fossil extras` checked in all 5 directories: only expected build
byproducts (`*.o`, `*.do`, `*.eh`, `*.ih`, `Makefile`, `Makefile.BAK`,
`index`, `install.time`, `install.doc`, `lib*.a`, and `atk/adew`'s own
built binaries `genarb`/`createcon`) — no stray diagnostic or
scratch-probe files (this session's investigation technique called
`ansify`'s `convert_file` directly via a scratch Python driver, which
does not create `.ansify-orig` backup files the way `ansify`'s own
`process()` does, so there was nothing to clean up here unlike B1).

No commit made.

## 13. Open questions / anything that surprised you

- **The `FinalizeObject` classpp bug (§6) is a real, tree-wide-relevant
  finding**, not specific to this batch — any directory anywhere with
  an ordinary empty-parens `FinalizeObject()` `.ch` declaration will
  hit the same self-inconsistent-`.eh` failure the first time it's
  `-pe`'d. Worth a proactive grep before future batches, or a
  one-time classpp fix, rather than rediscovering this per-directory.
- **6 real, ~35-year-old `.ch` interface bugs found** (§6's table) —
  all copy/paste-shaped (wrong sibling/parent/filebase name substituted
  for the true enclosing class in a restated parameter type), all
  invisible until `-pe` type-checked them for the first time. This is
  a new *scale* data point for the "how many latent `.ch` bugs does
  each `-pe` rollout surface" question the M3 runbook has been
  tracking since Pilot A/B — 6 in one batch is the most found in a
  single session so far, though this batch is also unusually
  class-dense (81 classes vs. B1's 41).
- **A third distinct classpp parser bug** (`xfontd.ch`'s unnamed
  `Deallocate` parameter, §6) — narrower blast radius (only found once
  in this batch) but a genuinely new failure mode, distinct from both
  the restated-type-name typos and the `FinalizeObject` call-site/
  prototype mismatch.
- **`xgraphic.c`'s 32-instance brace-glued parser gap (§8) is the first
  confirmed-live instance of B1's predicted risk** — B1 found the
  pattern but proved it inert in its own directory; this batch found a
  real, large, `-pe`-blocking instance. The mechanical fix (split the
  glued brace, re-run `ansify`) worked cleanly and is a viable
  workaround pattern for any future directory hitting this, without
  needing the tool itself fixed first.
- The recurring "override typed to the general base interface, but the
  body needs the concrete subclass" shape (rename-param-to-`_generic`
  + local-cast pattern) showed up independently in `atk/support`
  (`buffer.c`, cross-`.ch` cast variant), `atk/supportviews`
  (`sbuttonv.c`), `atk/adew` (`celv.c`, `arbiterv.c`, `lsetv.c`), and
  `atk/basics/x` (`xcmap.c`, `xim.c`) — 8 separate instances across 4
  directories. All were pre-existing, correctly-working K&R code
  (types were never checked before); none were new bugs the conversion
  introduced. Worth naming as its own recognized shape for future
  batches, distinct from both the "rock idiom" (`void*`/`long`) and the
  cross-`.ch` disagreement pattern B1 already documented — this one is
  "generic-interface-param, subclass-typed-body," always resolved with
  a local downcast, never a `.ch` edit (unlike the `.ch` typos in §6,
  where the *declared* type itself was simply wrong).
- I did not run any AUIS GUI or terminal binary interactively, and made
  no commits, per the prompt.
