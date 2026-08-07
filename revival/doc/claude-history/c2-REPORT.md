# M4 Wave 7, Batch C2: `contrib/zip/utility` STRICT_COMPILERFLAGS — REPORT

## 1. Status

**Stopped at Gate 1** (the task's only/final gate — C2 is not one of the
two elevated-risk batches `m4-batches.md` names, `C1`/`contrib/zip/lib`
and `AMS2`/`atkams/messages/lib`, so no tree-wide gate is required or
was run). `contrib/zip/utility` builds clean under
`$(STRICT_COMPILERFLAGS)` — zero real `error:` lines, confirmed across
two separate `clean && depend && install` cycles (one with
`-ferror-limit=0 -g -O0` for full-census fixing, one more from a fresh
`make clean` at the directory's normal flags for determinism). **No
fossil commit made.** `fossil diff > c2-session.diff` (tree root, 731
lines) has the full diff. 5 files touched (`Imakefile` + 4 `.c` files —
`lt.c`, `ltv.c`, `sched.c`, `schedv.c`; `ltapp.c` and `schedapp.c`
needed zero changes, both compiled clean on the first pass).

## 2. What I did, in order

1. Read `revival/doc/m4-rollout-runbook.md` in full and
   `revival/doc/m4-batches.md`'s Wave 7 section (confirmed C2's census:
   67 errors, flagged alone — "Shares lifecycle-method/lt/sched classes
   with `zip/lib`'s history (M3 C1/C2's `FinalizeObject`/
   `InitializeObject` findings) — do after C1, not before, so any
   shared pattern is already known"). Also read `c1-REPORT.md` in full
   for the established fix idioms and report structure, and
   **`revival/doc/claude-history/m3/m3-c2-REPORT.md` in full** — this
   is the critical piece of context the task brief pointed at: M3's own
   `C2` batch (2026-08-01) is *this exact directory*, and it already
   found and fixed the lifecycle-method asymmetry the task brief warned
   about (§3 below has the detail).
2. Checked `src/contrib/zip/utility/Imakefile` for an existing M2-era
   `COMPILERFLAGS` override — **none found** (only `CLASSFLAGS =
   $(CLASSINCLUDES) -pe`, no compiler-flag line at all). Added
   `COMPILERFLAGS = $(STRICT_COMPILERFLAGS)` as a fresh line — nothing
   to replace, nothing stacked.
3. Regenerated the Makefile. **Hit a real mechanism snag not seen in
   prior batches**: plain `xmkmf` (the tool named generically in the
   task brief) resolves imake's own bundled `/opt/homebrew/.../config`
   templates, not this tree's own `src/config` ATK templates — the
   result is a Makefile whose `NormalObjectRule()`/`InstallClassFiles(...)`/
   etc. macro calls are left completely unexpanded (literal text, no
   real build rules), which then fails immediately (`missing separator.
   Stop.`) the moment any target is invoked. The tree's own convention
   (visible in the pre-existing Makefile's own `IMAKE_CMD` rule) is
   `make Makefile` from inside the directory, which re-invokes imake
   with `-I$(IRULESRC)` pointing at `src/config` — that produces a
   correctly-expanded Makefile. Confirmed the fix by direct comparison:
   the working `contrib/zip/lib` directory's own Makefile has zero
   literal, unexpanded imake macro calls; mine did until I switched
   mechanisms. (One further wrinkle: the `Makefile:: Imakefile ...`
   rule is timestamp-gated and the double-colon rule fires per matching
   prerequisite, so a same-second `touch` doesn't reliably trigger
   regeneration — used `touch -m -t` to force a strictly-later mtime.)
   Confirmed the override reached the generated Makefile (line 296,
   last `COMPILERFLAGS` assignment in the file, winning over
   `system.mcr`'s still-suppressed default at line 177) and spot-checked
   that untyped `static Name();` declarations exist throughout
   `ltv.c`/`schedv.c` (confirmed real fallout was coming) before
   trusting a build.
4. `make clean`, `make depend` (benign `makedepend` "cannot find
   include file <system header>" noise only, matching every prior
   batch), then `make -k install CDEBUGFLAGS="-ferror-limit=0 -g -O0"`.
   **Real count: 80 errors — 19% over the 67-error census**, same
   direction as every prior M4 batch's undercount (pre-`-Werror=format`
   census tools don't see format fallout). Breakdown (§3–5 below have
   the full triage):
   - **64 `-Wimplicit-int`** across `ltv.c` (46) and `schedv.c` (18).
   - **13 `-Wformat`** across `ltv.c` (8), `schedv.c` (4), `sched.c` (1).
   - **2 `-Wimplicit-function-declaration`** (`strcpy`, one each in
     `lt.c` and `sched.c`).
   - **1 `-Wincompatible-function-pointer-types`** (`ltv.c`, a
     genuinely polymorphic dispatch-table callback passed to
     `suite_Apply`).
   - **0 `-Werror=int-conversion`** hits (confirmed the flag reached
     the compiler by grepping the logged `cc` invocation lines — all 6
     `.c` files show the full `STRICT_COMPILERFLAGS` string).
   - `ltapp.c` and `schedapp.c` compiled **zero errors on the first
     pass** — confirmed by grepping the log directly for their `cc`
     invocations and the absence of any subsequent `error:` line before
     the next file's `cc` line.
5. Fixed all 64 `implicit-int` sites (§3), the 1
   `incompatible-function-pointer-types` site plus the 2
   `implicit-function-declaration` sites (§4), all 13 `-Wformat`
   diagnostics (§5, including one genuine bug). One rebuild pass
   converged clean — no second round needed.
6. Rebuilt: **0 errors, exit 0**, full install completed (all 6 `.do`s
   registered, indexed, and installed to `build/dlib/atk/`; `.ih`/`.ch`
   headers installed; `bin/sched` symlink recreated; `itcCR.scd`
   installed to `build/lib/zip/`).
7. One more full `clean && depend && install` cycle at the directory's
   normal (non-`-O0`) flags for determinism — clean, 0 errors,
   confirmed via grepping the logged `cc` lines that the full
   `STRICT_COMPILERFLAGS` string (not a stale/suppressed variant)
   reached every compile.
8. `.ch`-vs-implementation lifecycle-method check (§6 below) and
   downstream linkage check (§7 below — this directory produces zero
   static libraries; no relink needed anywhere).
9. `fossil status`: exactly 5 `EDITED` files, all inside
   `src/contrib/zip/utility/`, zero `ADDED`/`DELETED`, no commit.
   `fossil diff > c2-session.diff` at the tree root (731 lines).

## 3. `-Wimplicit-int`: 64 sites, all fixed by sourcing the true type
    from a pre-existing typed declaration in the same file

Same authoring pattern C1 documented for the sibling `zip/lib`
directory: each affected file carries an early, fully-typed forward-
declaration block for its static helpers, and a **later, untyped
duplicate block** (`static Name();`, no return type) that predates (or
was simply never reconciled against) the typed block. The K&R
**definitions** further down the file match the untyped duplicate, not
the typed original. In every one of the 64 sites in this directory, the
typed reference was `int` — matching C1's exact finding for this
author's habitual status-return convention.

**`ltv.c` — 46 sites** (20 duplicate forward declarations at lines
197–216, plus 26 untyped K&R definitions scattered through the file —
23 unique names, `Begin_Chain_Button`/`End_Chain_Button`/
`Delete_Chain_Button`/`Rename_Chain_Button`/`Left_Chain_Button`/
`Right_Chain_Button` sharing one typed six-name forward declaration at
lines 187–188, plus a special case: `Build_Menu`'s definition splits
`static` and the function name across two physical lines,
`static\nBuild_Menu()`, both needing the type inserted on the `static`
line). All 23 names (`Detect`, `Initialize`, `Build_Chain`,
`Modify_Chain`, `End_Chain`, `Track_Enclosure`, `Cancel_Enclosure`,
`Clear_Enclosure`, `Draw_Enclosure`, `Invert_Enclosure`, `Neighbor`,
`Show_Chain_Names`, `Clear_Chain_Names`, `Name_Chain`,
`Split_Chain_Name`, `Passivate`, `Activate`, `Lighten_Background`,
`Show_Background`, `Build_Menu`, plus the six `*_Chain_Button` names)
trace to an explicit `static int Name();` forward declaration at lines
74–115 (or the six-name comma block at 187–188) — read every one before
typing, zero guesses.

**`schedv.c` — 18 sites** (6 duplicate forward declarations at lines
119–124, plus 12 untyped K&R definitions — 6 for the duplicated names
themselves, `Initialize`/`Handle_Slot_Hit`/`Remember_Slot_Hit`/
`Move_Slot`/`Normalize_Previous_Slot_Figure`/
`Normalize_Current_Slot_Figure`, sourced from an explicit `static int
Name();` block at lines 68–73; plus 6 more, `Extend_Button`/
`Split_Button`/`Clear_Button`/`Save_Button`/`Print_Button`/
`Quit_Button`, sourced from a six-name comma block at lines 112–113).

**Zero guesses, zero casts for this category** — every one of the 64
sites had a directly-checkable, unambiguous typed sibling declaration
in the same file before I touched it; none required inferring a type
from body/caller behavior the way C1 occasionally had to (this
directory's authoring pattern turned out to be even more consistently
pre-typed than `zip/lib`'s — no orphan untyped name lacked a reference
declaration anywhere in either file).

## 4. `-Wincompatible-function-pointer-types` (1) and
    `-Wimplicit-function-declaration` (2)

**`ltv.c:498` — cast to the field's own declared type, a genuinely
polymorphic dispatch slot.** `suite_Apply( Buttons, Detect, self, 0 )`
passes `Detect` (real K&R signature `(struct ltv *, struct suite *,
struct suite_item *, long)`, no return statement) to `suite.ch`'s
`Apply(long (*proc)(), void *anchor, void *datum)` — `suite.ch` lives
in `atk/apt/suite`, outside this batch's scope, so no declaration-side
fix is possible here even if one were warranted. Confirmed this is a
genuinely polymorphic, tree-wide dispatch slot (not something local to
this call site) by checking every other `suite_Apply` call site in the
tree: `atk/bush/bushv.c` passes `ResetSelectedState`/`Passivator`/
`ToggleCaptionDetail`, `atk/apt/suite/suiteev.c` passes
`CopySelected`/`ResetItemBreaks` — each a differently-shaped concrete
function tied to its own caller's `self` type, exactly the same species
as C1's `zipedit.ch` `keyboard_processor` finding. Per the fix policy
(cast only for genuine polymorphism, and then to the full, correct
target prototype — the field's own declared type, never bare/void):
```c
suite_Apply( Buttons, (long (*)())Detect, self, 0 );
```
This matches the slot's own real declared type (`long (*)()`) exactly
— same idiom C1 used for `zipedit.ch`'s macro-level cast, just applied
at the one call site since the declaring `.ch` (`suite.ch`) is out of
this batch's scope to touch.

**`lt.c:88-89`, `sched.c:70` — true-signature fix, no cast (missing
`#include`).** Both files call `strcpy` without ever including
`<string.h>` (`lt.c` only pulled in `class.h`/`lt.eh`/`zip.ih`/
`raster.ih`/`errno.h`; `sched.c` only `sched.eh`/`zip.ih`/`errno.h`).
Fixed by adding `#include <string.h>` to both — the established pattern
already used tree-wide (e.g. `contrib/zip/lib/zipd000.c`). `lt.c`'s
`strcpy` targets take `void *foreground`/`void *background` parameters
(the function's own K&R declaration) passed straight through to
`strcpy`'s `const char *` parameter — a `void *`-to-`char *` argument
conversion is implicit and warning-free in C, so no further change was
needed once the real prototype was visible.

**Fix-policy compliance summary**: 1 cast total in this batch, targeted
at the codebase's own already-declared function-pointer type (the
`suite.ch` slot's own type, not an invented shape) for a confirmed,
tree-wide-corroborated genuinely-polymorphic dispatch table — never a
bare/void cast. 2 of the 3 raw function-pointer/declaration diagnostics
needed no cast at all, just a missing standard-library include.

## 5. `-Wformat`: 13 diagnostics across 11 call sites — 1 genuine
    pre-existing bug, 12 mechanical LP64 width fixes

**Genuine bug — `sched.c:75`, the "Unable to Open" diagnostic prints
the wrong field, a `struct zip_stream *` pointer instead of the actual
filename string:**
```c
strcpy( self->stream_name, stream_name );
...
if ( status = zip_Open_Stream(self->zip, &self->stream,
                        self->stream_name, NULL ) )
  { DEBUG(Open Failure);
  printf( "Schedule: Unable to Open %s\n", self->stream );
  }
```
`self->stream` is declared `struct zip_stream *stream;` in `sched.ch`
(line 67); the actual char-buffer filename that was just written three
lines above via `strcpy` is `self->stream_name` (`sched.ch:68`,
`char stream_name[512]`). On entry to this branch,
`zip_Open_Stream` has already returned non-zero (open failed), so
`self->stream` holds whatever `zip_Open_Stream` left it as on failure —
printing it with `%s` reads that value as a NUL-terminated string, not
the filename that failed to open. This is a genuine, ~decades-old
diagnostic-message bug: every "Unable to Open" failure message for the
`sched` scheduler class has been printing garbage/an unrelated pointer
value instead of the filename users actually need to see, since this
code was written (`sched.c`'s own header dates it 10/10/88). Checked
the sibling `lt.c`'s analogous `Read_Visuals` open-failure path for the
same pattern — it does **not** have this bug (uses `DEBUG` macros only
on that path, no direct `printf` of the wrong field). Fixed:
```c
printf( "Schedule: Unable to Open %s\n", self->stream_name );
```
**Reachability**: `sched__Create` is the constructor called every time
a `sched` (scheduler) document is opened by filename — not a rare path;
any attempt to open a non-existent or unreadable schedule file hits
this message. Low severity (a diagnostic string, not a data-loss or
corruption path like C1's halo bug), but a real, always-broken
user-facing error message. Not independently runtime-verified this
session (would require opening `sched` against a deliberately
non-existent filename and confirming the message text) — flagged for
wdc in §10.

**12 mechanical LP64 width fixes** (the recurring `long`-status-vs-`%d`
pattern already documented in the `sonnet-playbook.md` and hit
repeatedly by C1 in the sibling directory):
- **`ltv.c`** (8 sites): `:494` (`status`, a `register long` in
  `Initialize`'s "Failed to create 'Chains' Image" diagnostic — a
  stray `/*===*/`-prefixed leftover debug `printf`, left as-is except
  for the width fix, not in scope to remove); `:1155` (`number`,
  `register long` in `Rename_Exception`'s duplicate-chain-name
  renaming loop); `:1164` (`facility`/`status`, both `long` parameters
  of `Rename_Exception`, 2 conversions in one `sprintf`); `:1356`,
  `:1383` (`status`, `register long` in `Save_Command`/`Print_Command`'s
  error-message construction); `:1498` (`status`/`facility`, `long`
  parameters of the file-scope `Exceptions` handler, 2 conversions).
- **`schedv.c`** (4 sites): `:569`, `:601` (`status`, `register long`
  in `Save_Button`/`Print_Button`'s error-message construction, the
  exact twin of `ltv.c`'s `Save_Command`/`Print_Command`); `:660`
  (`status`/`facility`, `long` parameters of `schedv`'s own file-scope
  `Exceptions` handler, 2 conversions — the exact twin of `ltv.c:1498`).
- **`sched.c`** (1 site): the `printf` fixed as the genuine bug above
  also had its argument's type corrected as part of the same edit (the
  new argument, `self->stream_name`, is a `char[512]`, matching `%s`
  correctly — no separate width issue once the field itself was
  corrected).

All 12 width fixes are pure `%d`→`%ld` corrections — the underlying
value was always computed correctly, only the printed text
representation was wrong on 64-bit. `ltv.c` and `schedv.c` are visibly
twin view classes (layer-tool and scheduler, near-identical structure —
matches the directory's own description) and hit the *exact* same bug
shape in the *exact* same functions (`Save_Command`/`Save_Button`,
`Print_Command`/`Print_Button`, both files' own `Exceptions` handler) —
consistent with both being written by the same author in the same
pass, as C1 also observed for its own twin-file pairs.

## 6. `.ch`-vs-implementation drift / M3-lifecycle-pattern-family check

**Checked explicitly, per the task brief's specific flag** — the task
context said this directory "shares lifecycle-method/lt/sched classes'
relationship" with `zip/lib`'s M3 history, and to flag clearly if M4's
stricter typing surfaced anything in the same family (another
`Initialize`/`Finalize` asymmetry, another silently-missing class
method, another `.ch` self-type typo).

**Nothing new found — and there is a specific, checkable reason why
not.** `revival/doc/claude-history/m3/m3-c2-REPORT.md` is not merely
related history — **it is M3's own batch for this exact directory**
(`contrib/zip/utility`), run 2026-08-01. That session found and fixed,
under orchestrator ruling, precisely the family of bug the task brief
described:
- `ltv.ch`/`schedv.ch`'s `InitializeClass`/`FinalizeObject` restated
  with the wrong (broken) shape — fixed to true-empty-parens
  `InitializeClass()` and self-only `FinalizeObject(struct CLASS
  *self)` (M3 §3a/§12).
- `ltapp.ch`/`schedapp.ch`'s `InitializeObject` classproc parameters
  declared with **no parameter name**, causing classpp to silently
  drop the class-name type from the emitted prototype entirely — fixed
  by naming the parameters (M3 §13.3, the already-documented B2
  finding-3 tool workaround).
- All four `.ch`'s `Set_Debug` given a real `boolean` type (M3 §9/§15).

I re-verified directly (not just trusted the prose) that all of these
fixes are **still present and correct in the current tree** (§6's grep
above, run before doing any work in this session): `ltv.ch`/`schedv.ch`
show the safe self-only/empty-parens shapes, `ltapp.ch`/`schedapp.ch`
show named `self` parameters, `fossil timeline` confirms M3's C2
session's fixes are part of the committed history this checkout is
built from. Since M3's compile-driven (`-pe`) pass already exercises
the exact same classpp-generated-prototype machinery `STRICT_COMPILERFLAGS`
doesn't touch (`CLASSFLAGS = $(CLASSINCLUDES) -pe` was already active
in this directory's Imakefile before I started — not something this
M4 batch added), there was no fresh surface for this milestone's
warning flags to expose anything in that specific family: the
compile-time class-interface check M3 exists to run was already
green, and it stayed green through this session's changes (build was
clean with no `.ch` edits at all). **No `.ch` file was touched this
session.**

What M4's own (different) checking *did* find in this directory is the
`sched.c:75` diagnostic-message bug (§5) — a real, ~decades-old bug,
but a different species (implementation-level format-string mismatch,
not a `.ch`-vs-`.c` interface/lifecycle-method disagreement). Worth
distinguishing clearly for the log: this is not a "same family as M3"
finding, it just happens to also be old and also happens to live in
`sched.c`.

## 7. Downstream consumer confirmation

- **This directory produces zero static libraries.** Its Imakefile has
  no `NormalLibraryTarget`/`LibraryTarget`/`InstallLibrary` rule —
  every one of its 6 build products (`DOBJS`: `lt.do`, `ltv.do`,
  `ltapp.do`, `sched.do`, `schedv.do`, `schedapp.do`) is a `.do`
  dynamic-load target. `InstallLink(runapp, ${DESTDIR}/bin/sched)`
  only symlinks the generic `runapp` launcher binary to `bin/sched` —
  it does not statically link any of this directory's code into
  `runapp`.
- **`nm -g` against `runapp` was denied by the permission classifier**
  (the same known environmental quirk C1's report documented — a flat
  denial tied to the current window, not a bug in this session's
  work). Fell back to Imakefile-based analysis, consistent with C1's
  own fallback and this project's independently-established
  `project_runapp_static_link.md` finding (only `text`/`matte`/
  `lpair`/`scroll` are statically linked into `runapp`) — `zip/utility`
  is not among them, corroborated directly by its Imakefile having no
  library-target rule at all.
- **Tree-wide grep for any other Imakefile referencing this
  directory's `.do` outputs by name**, word-boundary-anchored to avoid
  the false-positive risk of short names like `lt.do`/`ltv.do` matching
  as substrings of unrelated files (confirmed one such false positive
  during this check: `atk/prefed/Imakefile`'s own `pvaltv.do`/
  `pvaltvl.do` etc. contain the literal substring `ltv.do` but are
  unrelated `.do` files, not references to this directory's `ltv.do`):
  **zero real matches** outside `contrib/zip/utility/Imakefile` itself.
- **Tree-wide grep for a static-library consumer** (`liblt`, `libsched`,
  `lt.a`, `sched.a`): zero matches.
- **`contrib/zip/lib`** (the sibling C1 batch): its own Imakefile
  neither produces anything this directory could consume nor references
  any of this directory's outputs — the two directories are independent
  siblings under `contrib/zip/`, related only by sharing the `zip`
  figure-editor's data-object class (`zip.ch`/`zip.ih`, included by
  both) and by the `Set_Debug`/lifecycle-method authoring pattern noted
  in §6, not by any build dependency.
- **Conclusion: no relink or reload of `runapp`, `cuin`, or any other
  binary is needed anywhere in the tree.** This directory's own
  `.do`s were reinstalled fresh by this session's own gate runs
  (`build/dlib/atk/{lt,ltv,ltapp,sched,schedv,schedapp}.do`, all with
  fresh mtimes from the final clean gate), and nothing else in the tree
  references them.

## 8. Files touched (compile status)

All 6 `.c` files compile clean (0 errors), verified across 2 full
`clean && depend && install` cycles in this directory:

| File | What changed |
|---|---|
| `Imakefile` | `COMPILERFLAGS = $(STRICT_COMPILERFLAGS)` added (no prior override existed) |
| `lt.c` | 1 `#include <string.h>` added (fixes `strcpy` implicit-declaration) |
| `ltv.c` | 46 `implicit-int` fixes (`int`, including the 2-line `Build_Menu` special case) + 8 format width fixes (`%ld`) + 1 function-pointer cast (`suite_Apply`'s `Detect` argument, cast to the slot's own declared type `long (*)()`) |
| `sched.c` | 1 `#include <string.h>` added (fixes `strcpy` implicit-declaration) + **1 genuine bug fix** (§5, "Unable to Open" diagnostic now prints `self->stream_name` instead of the wrong-typed `self->stream`) |
| `schedv.c` | 18 `implicit-int` fixes (`int`) + 4 format width fixes (`%ld`) |
| `ltapp.c` | No changes — compiled clean on the first pass |
| `schedapp.c` | No changes — compiled clean on the first pass |

## 9. Open questions / anything that surprised me

- **The `xmkmf`-vs-`make Makefile` Makefile-regeneration mechanism is a
  real trap, not specific to this directory.** Plain `xmkmf` silently
  produces a Makefile with every ATK-specific imake macro
  (`NormalObjectRule()`, `InstallClassFiles(...)`, etc.) left
  completely unexpanded — not an error, not a warning, just literal
  text that then fails with an opaque `missing separator. Stop.` the
  moment `make` tries to use it. The task brief's own suggestion ("check
  a recently-touched sibling directory like `contrib/zip/lib` if
  unsure") is exactly right and is what caught this — worth promoting
  to a standing note in the runbook for future M4 batches, since it's
  easy to mistake the resulting `missing separator` error for something
  wrong with the *Imakefile* edit itself rather than the regeneration
  command used.
- **This directory's `implicit-int` population was even more uniformly
  pre-typed than `zip/lib`'s** — every one of the 64 sites across both
  files traced cleanly to an explicit, unambiguous typed forward
  declaration already present in the same file; no orphan name needed
  inference from body/caller behavior the way C1 occasionally had to.
- **The M3-lifecycle-pattern-family check (§6) came back clean, with a
  specific, checkable reason**: this is the *same directory* M3's own
  C2 batch already fixed for exactly that family, verified still intact
  in the current tree. Flagging this explicitly since the task brief
  asked for it by name — not a coincidence that nothing new turned up,
  but not something to interpret as "M4 checking didn't work" either;
  M4's warning-driven gate and M3's arity/DRIFT gate check genuinely
  different things, and both are independently green here.
- **Function-pointer-cast policy compliance**: 1 cast in the entire
  80-error-fixing pass, targeted at a real, tree-wide-corroborated
  polymorphic dispatch slot (`suite.ch`'s `Apply` callback, cast to its
  own already-declared type), never bare/void. In line with C1's own
  near-zero-cast outcome.
- Made no fossil commit, touched no file outside this directory's own 5
  (confirmed via `fossil status` before and after), did not touch
  `revival.md`/`porting-changelog.md`/`m4-batches.md` (leaving that
  documentation update to the orchestrator's review pass, matching the
  pattern from every prior M4 batch's own close-out).

## 10. Suggested runtime checks for wdc

Per `rollout-procedure.md`'s Runtime check rules: `nm -g` was attempted
first and denied by the permission classifier (§7 — a known
environmental quirk, not a signal to skip verification); Imakefile
analysis substituted. Never launch GUI apps from this session; no saves
against unversioned fixtures.

- **`sched`** (GUI, native Terminal.app only, never from this session;
  `build/bin/sched` is a symlink to `runapp`): run `sched`, create a
  new schedule document, add a few slots, save, reopen — exercises the
  bulk of the 64 `implicit-int` fixes across both `ltv.c`/`schedv.c`'s
  view-class draw/hit-handling code. Also confirms `lt`'s companion
  layer-tool machinery loads correctly, since `ltv.do`/`lt.do` share
  the same rebuild.
- **`sched.c`'s "Unable to Open" bug fix (§5), the batch's one genuine
  finding**: from a real Terminal.app, attempt to open a `sched`
  document by a deliberately non-existent filename (a scratch name,
  never a real fixture) and confirm the resulting diagnostic message
  now shows the filename that failed to open, not a garbled/unrelated
  value. Low-severity (diagnostic text only, not a data-loss path) but
  the one behavior-visible change in this batch.
- **`suite_Apply`/`Detect` cast fix (§4)**: in a `sched` (or `lt`, if
  separately launchable) document with multiple named chain/slot
  entries, exercise whatever UI path triggers `Detect`'s
  `suite`-item-attribute dispatch (per `ltv.c`, this is the
  left/right-name-item selection-detection path in the chain-editing
  view) — confirms the cast fix didn't change observable dispatch
  behavior.
- **PostScript/format-width fixes** (`ltv.c`'s `Save_Command`/
  `Print_Command`, `schedv.c`'s `Save_Button`/`Print_Button`): save and
  print (or "print to file") a document from both `lt`/`ltv` and
  `sched`/`schedv` — confirms error-path messages (if deliberately
  triggered, e.g. by making the target unwritable) show well-formed
  status numbers, not truncated/garbled values, after the `%d`→`%ld`
  fixes. Not otherwise expected to be visible in the success path,
  since these are error-message-only fixes.
