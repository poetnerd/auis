# M4 Wave 6, Batch AMS2: `atkams/messages/lib` STRICT_COMPILERFLAGS — REPORT

## 1. Status

**Stopped at Gate 1** (the task's only/final gate). `atkams/messages/lib`
builds clean under `$(STRICT_COMPILERFLAGS)` — zero real `error:` lines,
confirmed on three separate `clean && depend && install` cycles (one
with `-ferror-limit=0 -g -O0` for full-census fixing, two more at the
directory's normal flags for determinism). Per this batch's
elevated-risk flag (same tier as C1), also ran the pre-authorized
tree-wide gate: `make dependInstall` from `src/` — **22,290-line log,
zero `error:` lines, zero `make: ***` failures, clean exit**. **No
fossil commit made.** `fossil diff > ams2-session.diff` (tree root, 941
lines) has the full diff. 15 files touched (`Imakefile` + 3 `.ch` files
+ 11 `.c` files).

## 2. What I did, in order

1. Read `revival/doc/sonnet-playbook.md` and
   `revival/doc/claude-history/ams1-REPORT.md` in full (this task's
   required continuity briefing), plus the relevant sections of
   `revival/doc/m4-rollout-runbook.md` (fix policy for
   incompatible-function-pointer-types, the `-Wformat` scope ruling,
   Phase 2's gate rule) and `revival/doc/m4-batches.md`'s Wave 6
   section (confirmed AMS2's census: 65; confirmed AMS2 flagged for a
   mandatory tree-wide gate, "same rule M2/M3 both applied").
2. Confirmed AMS1's flagged carry-forward finding before touching
   anything (§8 of `ams1-REPORT.md`): read `ams/libs/ms/update.c`
   directly — `MS_FastUpdateState`/`MS_UpdateState` are indeed now
   `int` there (AMS1's own fix), and this directory's three `.ch`
   files (`ams.ch:134/148`, `amsn.ch:103/117`, `amss.ch:103/117`) still
   declared `returns long;` for both — confirmed still accurate, not
   stale, before acting on it.
3. Replaced this directory's M2-era override
   (`COMPILERFLAGS = -std=gnu89 -Wno-implicit-int
   -Werror=implicit-function-declaration
   -Wno-incompatible-function-pointer-types -Wno-return-type`) with
   `COMPILERFLAGS = $(STRICT_COMPILERFLAGS)` in `Imakefile` — replaced,
   not stacked.
4. `make Makefile`, confirmed the override (line 295) wins over
   `system.mcr`'s suppressed default (line 177).
5. Fix-surfacing build: `make clean`, `make depend` (only the usual
   `makedepend` "cannot find include file <system header>" noise, not
   real errors — same benign gap seen in every prior batch, this
   compiler's search path differs from `makedepend`'s), `make -k
   install CDEBUGFLAGS="-ferror-limit=0 -g -O0"`. **Real count: 74
   errors** — actually *worse* than the 65-error census by 14%, same
   direction as every prior batch's pre-`-Werror=format` undercount,
   but a much smaller gap than AMS1's 66% (this directory apparently
   had less latent format fallout than `ams/libs/ms`). Breakdown: 59
   `implicit-int`, 6 `incompatible-function-pointer-types`, 9
   `-Wformat`/`-Wformat-insufficient-args` diagnostics across 6 real
   call sites (two sites, `amsutil.c:305` and `captions.c:705`, each
   raise 2 diagnostics on the same line; `stubs.c:554` raises both a
   `-Wformat-insufficient-args` and a `-Wformat` diagnostic on the same
   line — see §3c for the per-site breakdown).
6. Fixed all 59 `implicit-int` sites (§4), all 6
   `incompatible-function-pointer-types` sites (§5), all 8 format sites
   including one genuine pre-existing bug (§6/§7).
7. Rebuilt: **0 errors, exit 0** on the very next attempt after all of
   the above.
8. Fixed the flagged `.ch` width-drift item (§8): narrowed `ams.ch`,
   `amsn.ch`, `amss.ch`'s `MS_FastUpdateState()`/`MS_UpdateState()`
   from `returns long;` to `returns int;`, and narrowed the six
   corresponding wrapper-method definitions
   (`ams__MS_FastUpdateState`, `ams__MS_UpdateState`,
   `amsn__MS_FastUpdateState`, `amsn__MS_UpdateState`,
   `amss__MS_FastUpdateState`, `amss__MS_UpdateState`) from `long` to
   `int` in `ams.c`/`amsn.c`/`amss.c`, matching.
9. Re-ran `make clean && make depend && make -k install
   CDEBUGFLAGS="-ferror-limit=0 -g -O0"` after the `.ch` change: **0
   errors**. Confirmed via `grep` on the regenerated `ams.ih`/`amsn.ih`
   that the dispatch-macro casts changed from `long (*)(struct ams *)`
   to `int (*)(struct ams *)` — classpp correctly regenerated the typed
   dispatch from the edited `.ch`, not left stale.
10. Two more full `clean && depend && install` cycles at the
    directory's normal (non-`-O0`) flags for determinism — both clean,
    0 errors.
11. Downstream linkage check (§9 below — no relink needed anywhere,
    this directory produces no static libraries).
12. Tree-wide gate (pre-authorized for this batch): from `src/`, `make
    dependInstall > /Users/wdc/src/AUIS/andrew-6.4/dependInstall.log
    2>&1` (background, confirmed alive via log growth before waiting on
    it). Completed clean: 22,290 log lines, `grep -c "error:"` → 0,
    `grep -n "^make: \*\*\*"` → no matches, log ends normally on
    `touch install.time` / `touch install.doc`. Also confirmed
    `atkams/messages/lib`'s own segment of the log (lines
    19061–22081) carries no compiler errors or warnings beyond the
    `makedepend` "cannot find include file" noise seen throughout the
    whole log (a `grep -i error` false-positive matched only the
    unrelated directory name `overhead/errors`).
13. `fossil status`: exactly 15 `EDITED` files, zero `ADDED`/`DELETED`,
    no commit. `fossil diff > ams2-session.diff` at the tree root.

## 3. Findings

### 3a. `-Wimplicit-int`: 59 sites, all fixed by giving the true
    signature sourced from an existing declaration in scope

Same method as AMS1: for every bare `Name() {`/`static Name();` site,
searched for (a) another declaration of the same name already in the
tree, and (b) the function's own body/callers, before picking a type.
**Zero casts, zero guesses — every site had a pre-existing typed
declaration somewhere in the same file to source from.**

- **`text822.c` (27 sites, all `int`)**: a duplicate, untyped block of
  13 forward declarations (lines 79–91) sat directly beneath an already
  fully-typed block of the same 16 names (lines 55–71, all `static int
  Name();`/`static char *Name();` as appropriate) — pure redundant
  cruft, fixed by adding `int` to match the block just above. The other
  14 sites were the functions' own K&R definitions (`ignoretoken`,
  `RotateThirteen`, `FindParam`, `InsertProperObject`, `ParseEncoding`,
  `getcdecoding`, `ungetcdecoding`, `getc64`, `ungetc64`, `getcqp`,
  `ungetcqp`, `hexchar`, `char64`, `PlainAsciiText`), each already
  committed to `int` by the same lines-55–71 forward declarations —
  spot-checked two bodies (`FindParam`, `PlainAsciiText`) directly to
  confirm every `return` statement in each carries an `int`-shaped
  value, consistent with the forward declaration.
- **`messages.c` (16 sites, all `int`)**: 13 extern forward
  declarations in a large `extern void Name();`-dominated block (13 of
  ~120 entries lacked the `void`/type — a fairly obvious visual outlier
  once flagged) plus the definitions of `NoOp`, `DirectlyClassify`,
  `CheckMenuMasks`. Verified each against its own real definition or an
  existing cross-file agreement before typing `int`:
  `AppendMessageToFile`/`AppendMessageToRawFile`/`GenNodeName`/
  `GetFolderName`/`OrgHit`/`PrepareAppendFileName`/`ReadByName`/
  `ReadNamedFolder`/`SetSubStatus`/`countdots` all already have `int
  Name(...)` at their own definitions elsewhere in the file. `NoOp` is
  assigned as an `int (*)()`-typed function-pointer initializer four
  lines below its definition (`int
  (*messtextv_ReverseSearchCmd)()	= NoOp`, etc.) — `int` is the only
  type that doesn't create a fresh mismatch there. `DirectlyClassify`
  is registered via `proctable_DefineProc("messages-directly-classify",
  DirectlyClassify, ...)`, the standard `int`-returning command-handler
  contract used throughout this codebase's menu/proc system. `
  CheckMenuMasks` was the most interesting case: `messaux.c:87` already
  carries `extern int CheckMenuMasks();` with a comment explicitly
  noting it forward-references this exact untyped site in
  `messages.c` — someone had already worked out the right answer from
  the caller side; I matched it. `CheckMenuMasks`'s own body has no
  `return` statement at all (falls off the end, tolerated by
  `-Wno-return-type`, same as many other procedure-shaped handlers in
  this codebase).
- **`stubs.c` (6 sites: 2 `void`, 4 `int`, one function pair
  reconsidered from the pre-existing `int` forward declaration)**:
  `RememberMessage`/`ReportErrorHistory` had a pre-existing `static int
  Name();` forward declaration (lines 73–74) that disagreed with their
  actual behavior — both functions only ever `return;` with no value
  (2 bare `return;` mid-body each) or fall off the end, and every
  caller anywhere in the tree (`RememberMessage(text, moretext);`,
  `RememberMessage(text, NULL);`, `ReportErrorHistory(fp);`) calls them
  as bare statements, never using a return value. Per the fix policy's
  "prefer the true signature end-to-end," I corrected **both** the
  stale `int` forward declarations and the definitions to `void`,
  rather than propagating the pre-existing declaration's wrong type — a
  narrow but real exception to "match the existing declaration" when
  the existing declaration itself is the thing that's wrong. Also fixed
  the duplicate untyped decls of the same two names (lines 84–85) to
  `void` to match. `Messages_Global_Error_Count` (line 117) is a static
  `int` counter (confirmed via `priorerrors`, the local it's assigned
  into elsewhere in the file, itself declared `int`) — typed `int`.
  `DidRestart` (line 219) has zero live callers in this build (its only
  in-tree caller, `ams/libs/snap/cuisnap.c:2997`, is in `libsnap.a`,
  which is **not** linked into `amsn.do`'s `NLIBS` — SNAP_ENV is off in
  this build) but the same-contract sibling in `ams/msclients/cui/
  cuifns.c:482` defines `void DidRestart()`, matching the bare-statement
  call convention — typed `void` to match that established contract.
- **`ams.c` (6 sites, all `int`)**: one duplicate untyped forward decl
  (`TimerReport`) plus 5 K&R definitions
  (`DisplayAMS_ERRNO`/`UpdateServerState`/`HandleTimer`/`RestartTimer`/
  `TimerReport`), all already committed to `int` by a fully-typed
  forward-declaration block at lines 64–75.
- **`sendmsg.c` (3 sites, all `int`)**: one duplicate untyped decl
  (`UnlinkCKPFile`) plus `Submit`/`UnlinkCKPFile`'s own definitions,
  both already `int` per forward declarations at lines 79/83.
- **`amsutil.c` (1 site, `int`)**: `GetBinaryOptions`'s own definition,
  already `extern int GetBinaryOptions();` forward-declared at line 55
  with an explicit same-file-forward-reference comment.

### 3b. `incompatible-function-pointer-types`: 6 sites, all fixed with
    a `(procedure)` cast — the codebase's own established idiom for
    this exact "genuinely polymorphic callback slot" shape

All 6 sites are calls into the `atk/basics` framework APIs
(`message_AskForStringCompleted`, `im_SetDeleteWindowCallback`,
`im_AddFileHandler`) whose `.ch` files declare the callback parameter's
type as literally `procedure` — `class.h`'s own `typedef int
(*procedure)();`, i.e. an intentionally untyped, unspecified-argument
function-pointer type used throughout this codebase (dozens of
already-committed, pre-M4 sites use `(procedure)` casts for exactly
this: `proctable_DefineProc`, `init_Load`'s `errorProc`,
`im_EnqueueEvent`, `keystate.c`'s `*function = (procedure)...`).
This is **not** a bare ad-hoc `(void(*)())` invented to silence the
compiler — it's the API's own real, declared parameter type, and the
cast states it exactly:

- `ams.c:1687`: `message_AskForStringCompleted(...)`'s
  `completionProc`/`helpProc` params — cast `(procedure)FolderComplete`
  (real signature `enum message_CompletionCode(char*, long, char*,
  long)`) and `(procedure)FolderHelp` (real signature `void(char*,
  long, int(*)(), long)`).
- `sendmsg.c:967,1602,1611` (3 sites, same function):
  `im_SetDeleteWindowCallback(...)`'s `cb` param — cast
  `(procedure)delete_sendmsg_win` (real signature `void(...)`, declared
  `extern void delete_sendmsg_win();`).
- `mailobj.c:254`: `im_AddFileHandler(...)`'s `proc` param — cast
  `(procedure)MetaOutput` (real signature `void(FILE*, struct
  mailobj*)`).

Each cast is listed here per the fix policy's rule 5. No bare
argument-untyped casts anywhere in this batch — every cast target is
the framework's own named `procedure` typedef, not an invented shape.

### 3c. `-Wformat`/`-Wformat-insufficient-args`: 9 diagnostics across
    6 real call sites — 1 genuine pre-existing logic bug, 5 mechanical
    width fixes

**Genuine bug — `stubs.c:554`, `SnarfFile`'s "not a regular file"
message silently dropped its filename argument:**

```c
if ((stbuf.st_mode & S_IFMT) != S_IFREG) {
    fprintf(fp, "File %s is not a regular file, but has mode %#o.\n", stbuf.st_mode);
    return 1;
}
```

Two `%` conversions (`%s`, `%#o`), one argument. Every sibling message
in this same function (`Could not resolve tilde in %s (%d).`, `Could
not stat file %s (%s).`, `Could not open file %s (%s).`) passes the
resolved filename (`fname` or `Buf`) as the first argument — this one
line simply dropped it. As written, `stbuf.st_mode` (a `mode_t`,
`unsigned short`) would be read by `%s` as a `char *` (near-certain
garbage-pointer dereference or crash), and `%#o` would then read
whatever garbage followed on the stack — exactly the two diagnostics
clang raised (`-Wformat-insufficient-args` and the `char*`-vs-`mode_t`
type mismatch). Fixed by adding the missing `Buf` argument (the
resolved, post-`TildeResolve` filename in scope at this point, matching
the immediately-preceding `stat(Buf, &stbuf)` call the mode came from):

```c
fprintf(fp, "File %s is not a regular file, but has mode %#o.\n", Buf, stbuf.st_mode);
```

**Reachability**: `SnarfFile` is called only from the "prepare an
automatic bug report" path (`PrepareAutoBugFile`, gathering
`~/preferences`, `~/.preferences`, `~/.Xdefaults`, `~/.AMS.flames`,
`~/.MS.spec`, `~/MS-Errors` into a diagnostic file), and only reached
when one of those paths exists but is **not** a regular file (a
directory, symlink loop, device node, etc.) — narrow but real: any user
whose `~/.Xdefaults` happens to be a symlink, for instance, would hit
this exact line when filing a bug report. Not independently
runtime-verified this session (would require crafting exactly that
filesystem condition); flagged for wdc in §11 below.

**5 mechanical width fixes, 6 diagnostics** (same recurring LP64 format-serialization
class documented in `sonnet-playbook.md` and exercised repeatedly by
AMS1):
- `amsutil.c:305`: `sprintf(MyBuf, "0x%x/0x%x", ...)` against
  `MyOpts.PermOpts[i]`/`MyOpts.OptMask[i]`, both `long` per `struct
  OptionState`'s real definition (`amsutil.ch:110-115`) — fixed to
  `%lx`/`%lx`.
- `captions.c:705`: the `AMS_ERRCAUSE`/`AMS_ERRVIA` macro family
  (`(mserrcode >> N) & 0xFF`, `mserrcode` is `extern long`) — same
  exact pattern AMS1's report already named as recurring across this
  directory family — fixed the two `%d`s to `%ld`.
- `writeone.c:122,169` (2 identical-text sites, one function called
  from two branches): `text_GetLength(...) - kids` — `text_GetLength`
  returns `long` per `smpltext.ch:82-83`, so the subtraction's result
  type is `long` even though `kids` (the second operand and second
  `%d`) is `int` — fixed the first `%d`→`%ld`, left the second as `%d`
  since `kids` itself is genuinely `int`.
- `stubs.c:575`: `stbuf.st_size` is `off_t` (`long long` on this
  platform, same as AMS1's `rawfil.c` finding) — cast to `(long long)`
  and `%lld`, matching AMS1's established pattern for this exact type.

## 4. `.ch`-vs-implementation width drift (the specific known lead) —
    confirmed and fixed

Re-verified AMS1's §8 flag against current source before acting (not
trusted blindly): `ams/libs/ms/update.c:76,80` define `int
MS_FastUpdateState()`/`int MS_UpdateState()` (AMS1's fix, still in
place), and every live caller tree-wide already agrees on `int` or
implicit-int-as-int (confirmed via the same cross-file grep AMS1's
report used). This directory's three `.ch` files
(`ams.ch:134/148`, `amsn.ch:103/117`, `amss.ch:103/117`) still declared
`returns long;` for both methods, and the six hand-written wrapper
definitions (`ams__MS_FastUpdateState`/`ams__MS_UpdateState` in
`ams.c`, `amsn__MS_FastUpdateState`/`amsn__MS_UpdateState` in
`amsn.c`, `amss__MS_FastUpdateState`/`amss__MS_UpdateState` in
`amss.c`) were `long`-returning, calling the now-`int` bare C
functions.

**Fixed as AMS1 recommended — narrowed the `.ch` declarations to
`returns int;`, and narrowed the six wrapper definitions to `int`, not
widened `ams/libs/ms`'s side.** Checked every caller of the
class-dispatched methods (`ams_MS_UpdateState`, `ams_MS_FastUpdateState`,
and the `amsn_`/`amss_` forms) before changing width: all 4 real call
sites (`ams.c:1370,1374,1554`, `mailobj.c:246`) use the result only in
`if (...)` truthiness context or discard it entirely — no assignment to
a wider variable anywhere, so narrowing is safe. Rebuilt after the
change and confirmed via direct inspection of the regenerated
`ams.ih`/`amsn.ih` that classpp correctly re-emitted the typed dispatch
macros as `int (*)(struct ams *)` (previously `long (*)(struct ams
*)`) — the type change propagates through the whole dispatch chain,
not just the `.ch` source.

Note: `amss.c` is only compiled when `SNAP_ENV` is defined
(`#ifdef SNAP_ENV` around its `DynamicMultiObject(amss.do, ...)` rule
in the Imakefile); `SNAP_ENV` is **not** defined anywhere in this
build's configuration (`grep -rn SNAP_ENV` across `Makefile` and
`build/etc/AndrewSetup` returns nothing), so `amss.c`/`amss.ch`'s fixes
here are correctness-for-consistency only, not independently compiled
or verified this session — flagged in §11.

## 5. Downstream linkage confirmation

Per the playbook's linkage rule and this task's specific instruction
to check for static consumers of this directory's outputs:

- **This directory produces zero static libraries.** Its Imakefile has
  no `NormalLibraryTarget`/`InstallLibrary` rule — every one of its 14
  build products is a `DynamicObject`/`DynamicMultiObject` (`.do`)
  target (`msgsa.do`, `captions.do`, `folders.do`, `sendmsg.do`,
  `messages.do`, `text822.do`, `text822v.do`, `amsutil.do`, `ams.do`,
  `options.do`, `messwind.do`, `mailobj.do`, `mailobjv.do`, `amsn.do`,
  plus `amss.do` under `#ifdef SNAP_ENV`). The `LIBS`/`SLIBS`/`NLIBS`
  variables in its own Imakefile (`libmsshr.a`, `libmail.a`,
  `libcparser.a`, `liberrors.a`, `libcui.a`, `libcuin.a`, `libmssrv.a`,
  etc.) are libraries this directory **consumes**, produced by other
  directories (`ams/libs/ms`, `overhead/mail/lib`, `ams/libs/cui`,
  etc.) — not anything it produces itself.
- **`nm -g build/bin/runapp`**: zero symbols matching `amsn_`/
  `messages_`/`ams__MS_` — confirmed this directory's code is never
  statically linked into `runapp` (`messages` is a symlink to
  `runapp`, but all of this directory's actual mail-handling code
  loads dynamically at runtime via the `.do` files installed to
  `build/dlib/atk/`, exactly as `sonnet-playbook.md` describes).
- **Tree-wide grep for any Imakefile referencing this directory's own
  `.do` outputs as a dependency**: only `atkams/messages/lib/Imakefile`
  itself and `rdemo/messages/Imakefile` mention `amsn.do`/`messages.do`/
  `mailobj.do` by name — and `rdemo/messages/` has its own fully
  independent, separately-maintained local copies of `amsn.c`/
  `sendmsg.c`/`stubs.c` (confirmed via `ls rdemo/messages/*.c`) built
  from `rdemo/messages/Imakefile`'s own `DynamicMultiObject` rules —
  it does **not** consume this directory's build output, just happens
  to share source-file names from a historical fork. No relink needed
  there or anywhere else.
- **Grep for `libmsshr.a`/`libmail.a` consumers tree-wide** (per the
  task's specific instruction): found ~25 Imakefiles referencing those
  two libraries, but every one of them (including this directory's own
  `LIBS` line) is a *consumer* of `ams/libs/ms`'s and
  `overhead/mail/lib`'s outputs — neither library is produced by
  `atkams/messages/lib`, so this batch's changes don't affect any of
  those directories' link inputs.
- **Conclusion: no relink of `runapp`, `cuin`, or any other binary is
  needed anywhere in the tree as a result of this batch.** The tree-wide
  `make dependInstall` gate (§2 step 12) independently confirms this —
  every directory that depends on anything this batch touched rebuilt
  clean in the same pass.

## 6. Files touched (compile status)

All 15 files compile clean (0 errors), verified across 3 full `clean &&
depend && install` cycles in this directory plus one tree-wide `make
dependInstall`:

| File | What changed |
|---|---|
| `Imakefile` | `COMPILERFLAGS` replaced with `$(STRICT_COMPILERFLAGS)` |
| `ams.c` | 6 `implicit-int` fixes (`int`) + 2 `(procedure)` casts (§3b) + 2 `.ch`-width-drift wrapper narrowings (`long`→`int`, §4) |
| `ams.ch` | 2 `.ch`-width-drift narrowings (`returns long;`→`returns int;`, §4) |
| `amsn.c` | 2 `.ch`-width-drift wrapper narrowings (`long`→`int`, §4) |
| `amsn.ch` | 2 `.ch`-width-drift narrowings (§4) |
| `amss.c` | 2 `.ch`-width-drift wrapper narrowings (§4) — **not independently compiled this session, SNAP_ENV off, see §4 note** |
| `amss.ch` | 2 `.ch`-width-drift narrowings (§4) — same caveat |
| `amsutil.c` | 1 `implicit-int` fix (`int`) + 1 format width fix (`%lx`/`%lx`) |
| `captions.c` | 1 format width fix (`AMS_ERR*` macro family, `%ld`/`%ld`) |
| `mailobj.c` | 1 `(procedure)` cast (§3b) |
| `messages.c` | 16 `implicit-int` fixes (13 `int`, 3 definitions `int`) |
| `sendmsg.c` | 3 `implicit-int` fixes (`int`) + 3 `(procedure)` casts (§3b, same function, 3 call sites) |
| `stubs.c` | 6 `implicit-int` fixes (2 corrected to `void` against a stale pre-existing `int` forward decl, 4 `int`) + **1 genuine pre-existing bug fix** (§3c, `SnarfFile`'s missing-argument `fprintf`) + 1 format width fix (`off_t` cast + `%lld`) |
| `text822.c` | 27 `implicit-int` fixes (all `int`, 13 duplicate forward decls + 14 K&R definitions) |
| `writeone.c` | 2 format width fixes (`%ld`, one function called from two branches) |

## 7. Open questions / anything that surprised me

- **`amss.c`/`amss.ch`'s fixes are unverified by compilation** —
  `SNAP_ENV` is off in this build, so `amss.do` never gets built and
  these two files' `implicit-function-declaration` fallout (a full
  `extern int MS_*` family, absent entirely from `amss.c`'s top-of-file
  declarations, unlike `amsn.c` which already has one) was never
  surfaced by this session's builds. I made the `.ch`-width-drift edit
  to `amss.ch`/`amss.c` for consistency (the "SIX files must all agree"
  comment in `amsn.c:34` — `ams.ch`/`amss.ch`/`amsn.ch` are meant to be
  identical specs), but if `SNAP_ENV` is ever enabled, `amss.c` will
  very likely need its own `extern int MS_*` prototype block added
  (mirroring `amsn.c:82-90`) before it would compile clean under
  `STRICT_COMPILERFLAGS` — flagging this now so it isn't a surprise
  later, not something I could verify or fix blind this session.
- **`stubs.c:554`'s `SnarfFile` bug (§3c) is this session's most
  consequential finding** — a real, if narrow-trigger, bug in the
  automatic-bug-report file-gathering path. Recommend the runtime check
  in §8 specifically target it, though safely (not by corrupting a real
  dotfile).
- **The census undercount this time was much smaller than AMS1's** (65
  → 74, 14% over, vs. AMS1's 66%) — this directory apparently had a
  lighter latent `-Wformat` population than `ams/libs/ms`.
- Made no fossil commit, touched no file outside this directory's own
  15 (confirmed via `fossil status` before and after), did not touch
  `revival.md`/`porting-changelog.md`/`m4-batches.md` (leaving that
  documentation update to the orchestrator's review pass, matching the
  pattern from AMS1's own close-out).

## 8. Suggested runtime checks for wdc

Per `rollout-procedure.md`'s Runtime check rules: `nm -g` first to
confirm live consumers (done in §5 above — this directory is entirely
`.do`-dynamic, no static relink to verify), never launch GUI apps from
this session, no saves against unversioned fixtures.

- **`messages`** (GUI, native Terminal.app only, never from this
  session): open a folder, read a message, compose/send, check mail —
  exercises the bulk of `amsn.do`'s (and its sibling `.do`s') rebuilt
  code paths, confirming the 59 `implicit-int` fixes and the 6
  `(procedure)`-cast fixes didn't change observable behavior. The
  `(procedure)`-cast sites specifically worth exercising: ask a
  question with completion in the "file into folder" prompt (exercises
  `FolderComplete`/`FolderHelp` at `ams.c:1687`), close a compose
  window (exercises `delete_sendmsg_win` at `sendmsg.c:967/1602/1611`),
  and view a MIME attachment that invokes metamail (exercises
  `MetaOutput` at `mailobj.c:254`).
- **`.ch` width-drift fix (§4)**: no separate runtime check needed
  beyond the general `messages` smoke test above — `MS_UpdateState`/
  `MS_FastUpdateState` are exercised on essentially every folder-read
  and directory-unlock operation, and the fix is width-only (no
  behavior change, every caller already used the value as a boolean).
- **`SnarfFile` bug fix (§3c)** — the highest-value but hardest-to-
  safely-reproduce check: from a **scratch** home directory or test
  fixture (never a real `~/.Xdefaults`/`~/.MS.spec`/etc.), create one
  of the paths `PrepareAutoBugFile` snarfs (e.g. `~/.AMS.flames`, which
  is not a common real dotfile) as a directory instead of a regular
  file, then trigger the bug-report path in `messages` (if it exposes
  one) and confirm the resulting report correctly names the offending
  path instead of crashing or printing garbage. If `messages` doesn't
  expose an easy manual trigger, a **code read-through re-confirmation
  only** (checking my `Buf`-vs-`fname` reasoning in §3c against
  `SnarfFile`'s actual call chain) is a safe fallback, same caution
  AMS1's report used for its own hard-to-reproduce `subs.c` fix.
- **`amss.c`/SNAP_ENV**: no runtime check possible in the current
  build (dead code path, not compiled) — flagged in §7 as a
  known-unverified area for whenever SNAP_ENV work resumes, not
  something to chase now.
