# M3 Wave 2 batch B1: `atk/basics/common` — first large-scale `-pe`/`.eh` rollout

## 1. Status

Stopped at the (only) gate — subtree-local `make -C
/Users/wdc/src/AUIS/andrew-6.4/src/atk/basics/common clean`, then
`depend`, then `-k install`, each a separate call, run twice for
determinism. Both passes green, zero real `error:` lines, identical
install-output line counts (173 `install -c` lines both times). No
tree-wide gate run (this directory is not `atkams/messages/lib` or
`contrib/zip/lib`, per the prompt). No commit made. Working tree: 46
files edited (`Imakefile` + 45 of the directory's 48 `.c` files —
`basinit.c`, `physical.c`, `point.c` have zero diff, see §6's new
finding for why). Session diff at tree root:
`m3-b1-basics-common-session.diff` (12,112 lines). This report lives in
`revival/doc/claude-history/` per the standard convention.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (including its
   "Command style" section), `m3-rollout-runbook.md` (including its
   full "Findings from real sessions" — O1 through O4), `m3-batches.md`,
   `porting-assessment.md` §14 and §17, and skimmed
   `claude-history/m3-o4-overhead-grabbag-REPORT.md` — all before
   starting.
2. Confirmed the 41 `.ch` count and re-verified all 6 pre-diagnosed
   findings via a fresh `ansify --dry-run --dir src/atk/basics/common`
   (§3) — exact match to the prompt's baseline (48 files, 0 compile
   failures, 1 DRIFT, 5 skipped).
3. Re-ran the two standing greps (empty-parens lifecycle methods,
   `_STDC_`-style macro typos) — both clean, as the prompt already
   found, and re-confirmed clean again after conversion (§4).
4. Hand-applied the 3 `InitializeObject` folds (`init.c`, `keystate.c`,
   `event.c`) and the `gifin_load_cmap` array-parameter fold (`gif.c`),
   verifying each individually with `make <base>.o` (§5). Left the 2
   dead-code methods (`im__PlayActions`, `view__InsertGraphic`) K&R,
   matching the prompt's "no real requirement either way."
5. Did the `-pe`/`.eh` rollout: added `CLASSFLAGS = $(CLASSINCLUDES)
   -pe` to the Imakefile, regenerated the Makefile, confirmed the flag
   landed in the `.ch.eh:` rule, force-regenerated all 41 `.eh` files
   (§7).
6. Ran `ansify --dir src/atk/basics/common` for real. 6 compile
   failures surfaced (`im.c`, `init.c`, `keystate.c`, `menulist.c`,
   `message.c`, `messitem.c`) — none were DRIFT or a new parser
   bailout; all were ordinary compile-gate fallout (mostly the
   established "rock" idiom — a class method's `void *` parameter,
   typed for the first time via `-pe`, assigned into a `long`-typed
   internal field/helper without a cast). Investigated each with a
   scratch driver importing `ansify`'s own `run_fix_tools`/
   `convert_file` directly (same technique O1-O4 used) to see full,
   untruncated compiler output instead of the tool's own
   12-line-truncated report. Fixed all 6 (§8), verifying each with a
   direct `make <base>.o`.
7. Re-ran `ansify --dir` a second time to confirm a clean baseline:
   48 files, 0 DRIFT, 2 skips (the pre-authorized dead-code ones), 1
   "compile failure" — which was the well-documented
   `fix-missing-static-decl` non-idempotency bug re-firing against my
   own hand-retyped `messitem_Create` declaration and correctly
   auto-reverting to my already-fixed version (§8, `messitem.c`) — not
   a new problem.
8. While investigating the 6 compile failures, traced a `no K&R
   definitions converted` result for `physical.c`/`point.c` (and a
   partial-conversion result for several other files) to a genuinely
   new, previously-undocumented `ansify` parser gap (§6) — confirmed
   its root cause directly in the tool source, confirmed its blast
   radius stays inert (zero compile-gate impact, all affected functions
   are non-`__`-named free functions, not class methods), left the
   affected functions K&R (consistent with every other "self-healing,
   no action needed" parser gap in the taxonomy), and documented it in
   full below since the prompt asked for exactly this kind of
   scale-driven new finding.
9. Subtree-local gate, twice, both green (§9).
10. Identified runtime consumers (§10) and confirmed `fossil
    status`/`fossil extras` (§11) — 46 files edited, all inside the
    directory, no stray files (cleaned up my own scratch-probe backup
    files, `*.probe-orig`, before the final check), no commit made.

## 3. Confirming the 41-`.ch` count and the 6 pre-diagnosed findings

```
$ find src/atk/basics/common -maxdepth 1 -name '*.ch' | wc -l
41
```

Fresh dry run, same day as the prompt's own baseline:

```
$ ./revival/tools/ansify --dry-run --dir src/atk/basics/common
...
48 file(s); 0 compile failure(s); 1 DRIFT finding(s)
```

All 6 findings matched exactly:

```
skipped event__InitializeObject: no signature in DB
skipped gifin_load_cmap: unparseable K&R declarations
skipped im__PlayActions: no signature in DB
DRIFT   init__InitializeObject: .c has 2 params, .ch has 0+1
skipped keystate__InitializeObject: no signature in DB
skipped view__InsertGraphic: no signature in DB
```

No discrepancy — nothing to flag here.

## 4. Standing greps (empty-parens lifecycle methods, macro typos)

```
$ grep -rEn "__(InitializeClass|InitializeObject|FinalizeObject)\(\)" src/atk/basics/common/*.c
(no output)
$ grep -rn "_STDC_\b\|_cplusplus\b\|_FILE_\b" src/atk/basics/common/*.c *.h *.ch
(no output)
```

Both clean before starting, and re-confirmed clean again after the
full conversion (same commands, same directory, run a second time
post-conversion) — the `-pe`/`.eh` rollout and `ansify --dir` run
didn't introduce either pattern anywhere in this directory.

## 5. The 4 hand-fixes, verified individually

`init.c:127`, `keystate.c:202`, `event.c:56` — each `InitializeObject`
folded from split-declaration K&R to a single-line ANSI header, exactly
as specified in the prompt (classpp's own hardcoded 2-param convention
for `InitializeObject`/`FinalizeObject`, `porting-assessment.md` §17):

```c
boolean init__InitializeObject(struct classheader *classID, struct init *init)
boolean keystate__InitializeObject(struct classheader *classID, struct keystate *self)
boolean event__InitializeObject(struct classheader *classID, struct event *self)
```

`gif.c:425` — `gifin_load_cmap`'s multi-dimensional array parameter
folded:

```c
static int gifin_load_cmap(BYTE cmap[3][256], int ncolors)
```

Each verified individually with a direct `make <base>.o` (0 errors,
only benign `-Wdeprecated-non-prototype` warnings from other
still-K&R methods in the same file, expected at this stage) *before*
the `-pe` flag was even added — matching the prompt's required order.

Left K&R, no action (matching the prompt's "no real requirement either
way" framing): `im__PlayActions` (`im.c:2719`) and `view__InsertGraphic`
(`view.c:457`) — both confirmed dead (never called anywhere in the
tree) in the prompt's own pre-diagnosis; re-confirmed still true after
this session's changes.

## 6. New finding: a silent, unreported `ansify` helper-parser gap — K&R declaration blocks with the opening brace glued to the last parameter's `;`

`convert_file`'s per-function pipeline (`revival/tools/ansify`) has two
stages that matter here: `HDR`+`BARE_PARAMS` regexes recognize a
K&R-style header line, then `parse_decl_block()` consumes the
following parameter-declaration lines until it finds a line whose
*stripped* text starts with `{`. Traced directly in the tool source
(`parse_decl_block`, ~line 204):

```python
while i < len(lines):
    s = lines[i].strip()
    if s.startswith('{'):
        return i, '\n'.join(decls)
    if s == '' or s.startswith('/*') or s.startswith('*'):
        i += 1
        continue
    if DECL_LINE.match(lines[i].rstrip('\n')):
        decls.append(s)
        i += 1
        continue
    return None
```

`DECL_LINE` requires the line to end (after the terminating `;`, plus
optional whitespace/comment) at end-of-string. A very common style in
this directory's oldest files (1988-era `point.c`/`rect.c`/
`physical.c`, and isolated functions elsewhere) puts the opening brace
on the **same physical line** as the last parameter declaration,
either glued directly (`long width;{`) or with a space
(`struct point * LogicalPoint; {`). Neither shape matches
`s.startswith('{')` (the line starts with the declaration, not `{`)
nor `DECL_LINE` (text follows the `;` besides whitespace/comment), so
`parse_decl_block` returns `None`, the candidate is dropped entirely,
and the loop just re-emits the header line verbatim and moves to the
next line — **with no skip message, no DRIFT report, nothing at all**.
This is a different failure mode from every previously-documented
parser gap (O2's `(void)` misparse, O2's `DECLARE<N>` macro misparse,
O4's function-pointer-returning-pointer gap, this directory's own
`gifin_load_cmap` multi-dim-array gap) — all of those are caught by
the *later* `parse_local_decls` stage and produce a `skipped: ...`
report; this one fails one stage earlier, before any candidate is ever
recorded, so it's invisible to `ansify --dir`'s own report and to a
dry-run census alike.

**Scope in this directory**, confirmed via `grep -lE ';[ \t]*\{[ \t]*$'`
across all 48 `.c` files: `cursor.c`, `describe.c`, `im.c`, `observe.c`,
`owatch.c`, `physical.c`, `point.c`, `rect.c`, `region.c`, `view.c` — 10
files show the pattern somewhere. Two files (`physical.c`, `point.c`)
are affected **wholesale** — every real function in each file uses this
brace style, so `ansify` converted literally nothing in either file
(matching the dry-run's own "no K&R definitions converted" for both,
which reads identically whether a file is trivial or entirely
parser-invisible — no way to tell from the report alone). The other 8
files have only a handful of affected functions each (mostly
old-style static helpers, e.g. `im.c`'s `HandleProc`/`RecordProc`
family, which is why those files still show a nonzero `converted:`
count and a real diff in `fossil status` despite also containing
parser-invisible functions).

**Why this is benign here**: none of the affected functions in this
directory are `__`-named class methods (checked: `physical.c`/
`point.c` have no `.ch` at all; the affected functions in the other 8
files are all plain static/file-local helpers, not `NAME__Method`
shapes) — the `HDR`/`BARE_PARAMS` gate that leads into
`parse_decl_block` runs *before* `convert_file` even checks whether a
name is a class method, so in principle this gap **could** silently
hide a real class method from the `-pe`/`.eh` conflict-detection path
too (a method written in this brace style would never even become a
DRIFT/skip candidate, and would then conflict with its `-pe`-regenerated
`.eh` the same way O4's truly-empty-parens finding did) — but the
subtree-local gate (§9), run clean twice after the full `-pe` rollout,
proves no such conflict exists anywyhere in this directory's actual 41
classes. Left every affected function K&R, matching the established
"self-healing, no action needed" treatment for every other parser gap
in the taxonomy — these were already correctly compiling before this
session and remain so now; nothing was at risk.

**Recommendation for the tool** (not fixed here, per the Delegation
ruling — tool construction stays top-level): treat a line matching
`;\s*\{\s*$` the same as a bare `{` line in `parse_decl_block` — i.e.
split it into the trailing declaration (still added to `decls`) plus a
synthetic brace-line, rather than bailing. Worth doing before B2/B3
(more class-directory-heavy Wave 2 batches) in case a directory there
has an actual `__`-named class method written in this style, which
would currently fail entirely silently until a `-pe`-regen-side
compile conflict surfaced it by chance (the way this directory's
own gate cycle happened to prove there wasn't one, here).

## 7. The `-pe`/`.eh` rollout

### Imakefile diff

```diff
 /* M2 ANSI sweep: catch implicit function declarations (revival/doc/roadmap.md, M2) */
 COMPILERFLAGS = -std=gnu89 -Wno-implicit-int -Werror=implicit-function-declaration -Wno-incompatible-function-pointer-types -Wno-return-type
+
+/* M3 ANSI sweep: typed .eh export prototypes (revival/doc/roadmap.md, M3) */
+CLASSFLAGS = $(CLASSINCLUDES) -pe
```

Confirmed the flag landed in the generated Makefile's `.ch.eh:` rule
(`make -C src/atk/basics/common Makefile`, then `grep -n CLASSFLAGS
Makefile`):

```
39:        CLASSFLAGS = $(CLASSINCLUDES)
293:CLASSFLAGS = $(CLASSINCLUDES) -pe
305:.ch.ih: ; $(CLASS) -s $(CLASSFLAGS) $*.ch
306:.ch.eh: ; $(CLASS) -s $(CLASSFLAGS) $*.ch
```

Same shape O4 confirmed for `overhead/class/testing`: line 39 is the
tree-default from `imake.tmpl`, line 293's restatement (from the
Imakefile) wins since make uses the last definition.

### `.eh` regen

Removed all 41 existing `.eh` files, then force-regenerated each via
`make -C src/atk/basics/common <name>.eh` (41 explicit targets in one
call, each individually invoking `class -s ... -pe <name>.ch` per the
Makefile's rule — confirmed in the command echo). All 41 regenerated
successfully; re-confirmed the 41 count after.

### `-pe`-regen-side fallout beyond the 6 pre-diagnosed findings: none new

Rebuilding the 4 pre-diagnosed hand-fixed files immediately after the
`-pe` regen (before running `ansify --dir`) surfaced exactly the
expected shape: `init.c`'s still-K&R `init__Load` conflicted with its
now-typed `.eh` prototype — not a new finding, just confirmation that
the `-pe` flag makes every remaining K&R method in a flagged directory
immediately visible as a conflict, which is precisely why
`ansify --dir` runs next. No `-pe`-regen-side fallout of the O4
"truly-empty-parens lifecycle method" shape was found anywhere else in
this directory (the standing grep in §4 came up empty both before and
after).

## 8. Compile-gate fallout: 6 real fixes, all in the initial `ansify --dir` pass

All 6 are variants of two already-documented shapes (the "rock" idiom
cast — `porting-assessment.md`'s Pilot B/point-9 findings — and the
O1/O3/O4 `fix-missing-static-decl` non-idempotency bug), plus two
genuinely new sub-patterns worth calling out (a real ~35-year-old
caller bug, and a cross-`.ch` rock-type disagreement). None were DRIFT,
none were new parser bailouts.

### `im.c` — 10 errors, all fixed

`im.c` is the biggest class in the directory (85 methods, 67
classprocs, 50 helpers converted) and had by far the most fallout,
all of the same general shape: a method whose `.ch`-declared parameter
is `void *` (typed for the first time via this session's `-pe`
rollout) gets forwarded, unmodified, into internal file-local state
that is genuinely `long` (a struct field or a static helper's own
K&R-declared parameter). Six straightforward rock-idiom casts:

- `im.c:1218` `HandleProc(self, procTableEntry, object, rock, NULL)` →
  `(long)rock` (`im__HandleMenu`'s `void *rock` forwarded into the
  static `HandleProc`'s own `long rock`).
- `im.c:2268`, `im.c:2274` `thisHandler->functionData = functionData;`
  → `(long)functionData` (`im__AddZombieHandler`'s `void
  *functionData` into `struct zombiehandler`'s `long functionData`
  field).
- `im.c:2357` `newEvent->data = interactionData;` → `(long)`
  (`im__SetInteractionEvent`'s `void *interactionData` into
  `im_InteractionEvent.data`, declared `long` in `im.ch`).
- `im.c:3392` `rm_PostResource( path, data, type );` → `(long)data`
  (**a cross-`.ch` rock-type disagreement**, see below).
- `im.c:3869` `self->delete_window_rock = rock;` → `(long)rock`
  (`im__SetDeleteWindowCallback`'s `void *rock` into `im`'s own `long
  delete_window_rock` field).

**A real ~35-year-old caller bug, found and fixed**: `im.c:1556-1557`,
inside `im__WantColormap`:

```c
im_InstallColormap(self, *cmap);
view_ReceiveColormap(requestor, *cmap);
```

`cmap` is the function's own `struct colormap *cmap` parameter;
`*cmap` dereferences it to a bare `struct colormap` **by value**,
which the `-pi`-typed dispatch macros (`im_InstallColormap`/
`view_ReceiveColormap`, both `void *`-cast per M1's already-completed
tree-wide typed-`.ih` rollout) correctly reject as
"passing struct colormap to parameter of incompatible type void*."
This is a genuine dereference bug — should be plain `cmap`, not
`*cmap` — but was invisible for the whole time these macros have been
typed (since M1, 2026-07-10) because nothing had forced a clean
rebuild of `im.o` since. Fixed both call sites (drop the stray `*`);
the third call in the same function, `im_InstallColormap(self,
*inherited)`, is correct as written (`inherited` is declared `struct
colormap **inherited`, so `*inherited` is the right single-pointer
dereference) and was left untouched. Verified with `make im.o`, 0
errors.

**A cross-`.ch` rock-type disagreement, found, worked around, flagged
(not fixed in the `.ch`)**: `im.c:3392`'s `im__PostResource` forwards
its `data` parameter (typed `void *` per **`view.ch`**'s declaration —
`view` is the "declaring" class per the signature DB, confirmed via
`build/desc/im.desc`: `PostResource ... declared by: view`) into
`rm_PostResource`, whose own class (`rm.ch`) declares the equivalent
parameter `long data`. Separately, **`im.ch`'s own restated override**
of `PostResource` (line 134-135) declares it `long data` too —
disagreeing with `view.ch`'s `void *datum` for the exact same method.
Per `porting-assessment.md`'s point-10-batch-3 finding #2 ("classpp
does NOT... override macros take the defining class's decl"), classpp
ignores a subclass's restated override for codegen purposes, so
`im.ch`'s stale `long` restatement has zero runtime effect — `ansify`
correctly used `view.ch`'s `void *` truth when typing `im.c`'s
definition, matching the compiled behavior. Fixed the actual compile
error with a `(long)` cast at the `rm_PostResource` forwarding call
(same as the other rock-idiom fixes above). **Left `im.ch`'s own
inconsistent restated declaration untouched** — it's dead documentation
with no observable effect (matches O4's `dialog__InitializeClass`
precedent in `porting-assessment.md` §17: a real, benign, ~35-year-old
interface inconsistency, logged not fixed, since nothing consumes it).
Flagging here for wdc's awareness in case a documentation cleanup is
wanted later.

All 10 verified together with a final `make im.o`: 0 errors.

### `init.c` — 2 errors, fixed

`init.c:907`, `init.c:913` (both inside `init__Load`):
`currentErrorRock = errorRock;` → `(long)errorRock` (same rock idiom —
`init__Load`'s `void *errorRock` parameter into the file-local `static
long currentErrorRock`). Verified with `make init.o`: 0 errors.

### `keystate.c` — 1 error, fixed

`keystate.c:209`, inside `keystate__SetOverride`: `self->functionData =
functionData;` → `(long)functionData` (`keystate.ch`'s `void
*functionData` parameter into `keystate`'s own `long functionData`
field — noted in passing that `GetOverride`'s out-param is `long
*functionData`, a harmless pre-existing asymmetry with `SetOverride`'s
`void *`, not touched). Verified with `make keystate.o`: 0 errors.

### `menulist.c` — 5 errors, fixed

Same rock idiom, `menulist_AddToML`'s `void *functionData` (already
`.ch`-typed at M1 time — this is the exact class
`porting-assessment.md`'s point-9 finding #2 already named,
"`menulist_AddToML` (236 sites: 139 pointer-ish, 51 integer) which
went `void *`") and `ChainBeforeML`/`ChainAfterML`'s `void *key`, both
forwarded into `struct itemlist`/`struct headerlist` fields declared
`long` in `menulist.ch`:

- `menulist.c:539` `thisItem->functionData = functionData;` →
  `(long)functionData`
- `menulist.c:657`, `681`, `708`, `722` (all four, identical text
  `tempHeader->assocKey = key;`) → `(long)key`

Verified with `make menulist.o`: 0 errors.

### `message.c` — 1 error, fixed

`message.c:120`, inside `message__AskForStringCompleted`: forwards its
own `void *functionData` (per `message.ch`) into
`msghandler_AskForStringCompleted`, whose own class (`msghndlr.ch`)
declares the equivalent parameter `long functionData` — the same
cross-`.ch` rock-type-disagreement shape as `im.c`'s `PostResource`
case above, same fix direction (cast at the forwarding call site, no
`.ch` edit):

```c
return msghandler_AskForStringCompleted(handler, priority, str, defaultString, buffer, bufferSize, keystate, completionProc, helpProc, (long)functionData, flags);
```

Verified with `make message.o`: 0 errors.

### `messitem.c` — 1 error, fixed; then a documented non-idempotency re-trigger, auto-reverted correctly

`messitem.c:32`, a **pre-existing** (not tool-inserted) empty-parens
forward declaration `static struct messitem *
messitem_Create();` conflicting with the newly-typed definition
(`char wh` — a narrow, promotion-affected parameter, the classic O1/O3
non-idempotency trigger shape). Retyped in place:

```c
static struct messitem * messitem_Create(char *s1, char *s2, char wh);
```

Verified with `make messitem.o`: 0 errors. Re-running the full
`ansify --dir` driver a second time (to get a clean confirmation
count, §2 step 7) reproduced the exact O1/O3/O4-documented
`fix-missing-static-decl` non-idempotency bug against my own fix — it
doesn't recognize a full ANSI prototype as "already declared" and
inserted a fresh conflicting empty-parens stub, which correctly failed
the compile gate and auto-reverted to my typed version. Not a new
problem; re-confirmed `messitem.o` still builds clean afterward (0
errors) — the file is in its correct final state.

## 9. Subtree-local gate: clean/depend/`-k install`, twice, both green

```
$ make -C src/atk/basics/common clean   # (both times)
$ make -C src/atk/basics/common depend  # (both times; same benign
                                         #  pre-existing makedepend
                                         #  warnings both times —
                                         #  stdio.h/sys/types.h/
                                         #  stddef.h/ctype.h lookup
                                         #  quirks, unrelated to this
                                         #  session)
$ make -C src/atk/basics/common -k install   # (both times)
```

Pass 1: exit 0, `grep -c "error:"` → 0, install completed through
`touch install.time`/`touch install.doc`. Pass 2: identical — exit 0,
0 errors, and the `install -c` line count matched exactly (173 both
times, byte-identical after stripping the leading `+ install -c`
prefix) — deterministic. All 41 `.do` files present in
`build/dlib/atk/` afterward.

## 10. Runtime consumers and exact check commands for wdc

`atk/basics/common` builds no static library and no `.a` at all — every
one of its 41 classes is its own independently dynamically-loaded
`.do` (confirmed: the directory's Imakefile uses `DynamicObject`/
`DynamicMultiObject` for every class, and `grep -n
"basics/common"` against `atk/apps/Imakefile` — the `runapp`
loader — found no static linkage). This means the consumer set is
structurally universal: **any AUIS class-based app that touches
views, windows, images, menus, key bindings, or the `.ezinit`/init-file
loading path loads one or more of these `.do` files on demand**,
which in practice is every GUI app in the tree (`ez`, `messages`,
`help`, and everything built on `runapp`). This matches the prompt's
own framing exactly.

Primary check (exercises the class loader across most of this
directory just by starting up — `im`, `view`, `graphic`, `event`,
`init`, `keystate`, `environ` are all loaded during normal window
creation and `.ezinit` processing):

```
DISPLAY=:0; ez
```

Confirm the window opens, a document loads, and basic interaction
(typing, scrolling, opening a menu) works with no crash.

```
DISPLAY=:0; messages
```

Confirm the message list opens and a message can be read, with no
crash.

Targeted checks for this session's specific fixes:

- **`im.c`'s colormap fix** (`im_InstallColormap`/`view_ReceiveColormap`
  dereference bug, §8) — colormap installation only has an observable
  effect on displays actually switching colormaps (rare on modern
  hardware/X servers); no dedicated UI action isolates it further.
  Covered incidentally by ordinary `ez`/`messages` use; not expected to
  be visibly different before/after (the bug was a hard compile error
  once typed, not a runtime behavior change — this fix is required for
  `im.c` to compile at all under `-pe`, not a behavior fix).
- **`im.c`'s menu/zombie/interaction-event rock casts** — open a popup
  or pull-down menu in `ez` (`im_HandleMenu`/`AddToML` path) and
  confirm menu selection still dispatches commands correctly; these
  are the load-bearing paths for the `HandleProc`/`AddZombieHandler`/
  `SetInteractionEvent`/`SetDeleteWindowCallback` fixes.
- **`init.c`'s `Load`/`ErrorMsg` rock cast** — exercised automatically
  on every `ez`/`messages` startup (`.ezinit` processing); an
  intentionally malformed line in a scratch copy of `.ezinit` would
  trigger the `ErrorMsg` path if wdc wants to verify the error-rock
  round-trip specifically, but ordinary startup already covers the
  common path.
- **`keystate.c`'s `SetOverride` rock cast** — used by prefix-key
  sequences (e.g. multi-key command bindings); exercise any
  multi-keystroke command in `ez`.
- **`menulist.c`'s chained-menu rock casts** — open a menu that chains
  to another menulist (e.g. a submenu with a "more..." continuation, if
  present in the test document's menu set) and confirm selection still
  resolves correctly.
- **`message.c`'s `AskForStringCompleted` cross-`.ch` cast** — trigger
  any completing-prompt dialog in `ez` or `messages` (e.g. a
  filename-completion prompt) and confirm it still accepts input and
  completes correctly.
- **`gif.c`'s `gifin_load_cmap` array-parameter fold** — open a
  GIF image inset in `ez` and confirm it renders with correct colors
  (exercises the colormap-loading path directly).

## 11. `fossil status`/`fossil extras` — 46 files changed, no commit made

```
$ fossil status | grep -c "^EDITED"
46
```

`Imakefile` plus 45 of the directory's 48 `.c` files (`basinit.c`,
`physical.c`, `point.c` have zero diff — `basinit.c` because its one
function has genuinely empty parens with nothing to convert;
`physical.c`/`point.c` because every function in each is affected by
the new parser gap in §6). `fossil extras` checked: after removing my
own scratch-probe backup files (`*.probe-orig`, left by the
investigation technique in §2 step 6 — not part of `ansify`'s own
pipeline, cleaned up before this check), the only untracked files left
are expected build byproducts (`*.o`, `*.do`, `*.eh`, `*.ih`,
`Makefile`, `Makefile.BAK`, `index` — the `.do` directory index file
`doindex` writes, `install.time`, `install.doc`) — no stray diagnostic
files. No commit made.

## 12. Open questions / anything that surprised you

- §6's new `ansify` parser-gap finding (silent, unreported skip for
  K&R declaration blocks with the brace glued to the last parameter's
  `;`) is this session's one genuinely new pattern, per the prompt's
  ask. It's inert in this directory (confirmed via the clean
  double-gate) but structurally *could* hide a real class method from
  `-pe`/`.eh` conflict detection in a future batch — worth a tool fix
  before B2/B3 if a class-heavy directory there hits it for real rather
  than by luck.
- The `im.ch`/`view.ch` and `message.ch`/`msghndlr.ch` cross-`.ch`
  rock-type disagreements (§8) are a new *sub-pattern* of the already-
  documented rock idiom: not a single `.ch` vs. its own `.c`
  (ordinary DRIFT) and not a parent/child override disagreement
  (point-10-batch-3's already-documented shape), but two *unrelated*
  classes' `.ch` files disagreeing about the type of a value passed
  between them at a call site. Both instances found were low-risk
  (fixed with a cast at the forwarding call, no `.ch` edit needed) but
  worth watching for at scale.
- `im.c:1556-1557`'s stray-`*`-dereference bug (§8) is a real,
  ~35-year-old latent bug, invisible until this session's `make clean`
  forced the first real rebuild of `im.o` since M1's typed-`.ih`
  rollout went live (2026-07-10) — matches the `porting-assessment.md`
  §14 point-9 precedent ("First live caller bug requiring a semantic
  `.c` fix") exactly, just found here instead.
- I did not run any AUIS GUI or terminal binary interactively, and made
  no commits, per the prompt.
