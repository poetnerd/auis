# M4 rollout runbook: tree-wide compiler strictness

Procedure for M4 (`roadmap.md` → Medium-term → ANSI C conversion →
M4 — Global strictness). Read `rollout-procedure.md` first (shared
M1–M4 session rhythm) — it still applies once real fixing batches
exist, just not from session 1 the way M2/M3 needed it. Written
2026-08-01, immediately after M3's close-out; no M4 execution has
started yet.

## Verified starting state (2026-08-01, checked against the real tree, not assumed)

`src/config/darwin/system.mcr`'s global `COMPILERFLAGS` still fully
suppresses all four target warnings:

```
COMPILERFLAGS = -std=gnu89 -Wno-implicit-int -Wno-implicit-function-declaration -Wno-incompatible-function-pointer-types -Wno-return-type
```

The "ratchet each completed subtree" language in `roadmap.md`'s M3
bullet never actually executed — `grep -rl "strict-prototypes"
--include=Imakefile src/` returns zero matches, tree-wide. M2 left 28
directories with a narrower per-directory override
(`-Werror=implicit-function-declaration` only, the other three flags
still suppressed); M3 didn't add to or change that. **M4 starts from a
clean slate, not a partially-done ratchet** — don't assume any
directory is pre-verified against the full flag set just because it
went through M2 or M3.

## Why this milestone didn't PRE-build a directory batch map (unlike M2/M3) — updated 2026-08-01

The original plan below argued M4 might not need a directory batch map
at all: it's flipping a global default in one file, on a tree that's
already had two full milestones of preparation (M1's typed dispatch,
M3's ANSI conversion), so the flip might land close to clean. **Phase 1
ran and disproved that** — 1,778 real errors across 83 of 91
directories, comparable in scale to a meaningful fraction of M2's
original census, not a small residual. Per this section's own original
fallback plan ("if Phase 1 turns up enough fallout to need batching,
build that map then, from real per-directory counts"), that map now
exists: **`m4-batches.md`**, built entirely from real Phase 1 error
counts, not a guessed proxy. Originally 24 batches across the same 7
dependency-order waves M3 used; consolidated 2026-08-02 to **Batch 0 +
18 sessions** (small/mid mechanical batches merged within their waves —
per-session fixed overhead dominated exactly there, and M2/M3's own
late-stage throughput of 300–500+ mechanical instances per session
proves the merged 65–181-error sizes conservative; the 8 outlier
singles and the two deliberately-unmerged risk batches I3/C2 are
unchanged). The waves are grouping convention in M4, not dependency —
`COMPILERFLAGS` doesn't propagate, so batches can legally run in any
order, and C1/AMS1 may be pulled earlier if front-loading discovery of
new fallout shapes becomes worth it.

(Original reasoning, kept for the record: M2 and M3 needed per-directory
rollout because each directory's fallout was independent and
unpredictable in advance, and the fix itself required per-instance
judgment. `m3-batches.md`'s own first draft over-plans before checking
real scope — 24 batches, more checkpoints than M2's 13, before anyone
had verified M3's actual footprint was even comparable to M2's — and
had to be redone once corrected. M4's plan tried to apply that lesson
by not pre-building a map at all, deferring to a real census first.
That part of the lesson held: the map wasn't built until real numbers
existed. The part that didn't hold was the *guess* that M4's residual
would be small — coincidentally, the corrected batch count landed on
the same number, 24, as `m3-batches.md`'s own over-eager first draft,
this time for real reasons: 8 outlier directories each needing their
own session, not an estimation error.)

## Execution mechanism corrected 2026-08-02 (wdc) — per-batch overrides, not a standing global flip

Phase 1 (below) flipped `system.mcr`'s **global** default directly to
get the real census. That's a fine one-time diagnostic technique — it
answers "what does the whole tree look like under the new flags" in a
single build — but it is the **wrong mechanism for landing the actual
fix**, and using it that way is what produced the broken-tree state
wdc had me revert on 2026-08-02: a global default change makes every
one of the 91 directories fail at once, so the tree stays fully
unbuildable from the moment it's committed until every one of the
batch map's sessions is fixed — a single long unbuildable window with no safe place
to pause, unlike M2 or M3.

M2 and M3 never had this problem because they never touched the global
default until the very end (M2 still hasn't — see "Verified starting
state" above). Each flagged only its own directory via a **per-directory
`COMPILERFLAGS` override that restates the whole flag set** (`COMPILERFLAGS`
doesn't compose the way `CLASSFLAGS` does — a directory override must
restate all of it, not just append, or it silently loses `-std=gnu89`
and the other suppressions; same mechanical note M2's own runbook
carried). **M4 now uses that same mechanism for Phase 2** — the global
flip moves to the very end (Phase 3), after every batch is already
fixed, where it's a no-op confirmation rather than a breaking event.
Consequence: the tree stays fully buildable after every single M4
commit, the same property M2/M3 always had, and Phase 2 can pause
between any two batches indefinitely (across a resource-budget
boundary, a week, whatever) with zero risk — exactly the concern that
prompted this correction.

**Define the flag string once, not 82 times (added 2026-08-02).** The
original draft had every batch restate the full flag string in each
directory's Imakefile. Across 18 delegated sessions and ~82 Imakefiles
that's copy-paste drift waiting to happen — and this exact flag string
already has a known silent-failure trap (below). Instead, Batch 0
defines the string once in `src/config/darwin/system.mcr`:

```
STRICT_COMPILERFLAGS = -std=gnu89 -Wno-return-type -Werror=implicit-int -Werror=int-conversion -Werror=incompatible-function-pointer-types -Werror=implicit-function-declaration -Werror=format
```

(the full flag set Phase 1 proved safe, minus `strict-prototypes`,
with `-Wformat` promoted to `-Werror=format` per the scope ruling
below; `system.mcr`'s definitions land in every generated Makefile, so the
macro is visible tree-wide while the *default* `COMPILERFLAGS` stays
suppressed and untouched until Phase 3). Each batch directory's
override is then one line in its Imakefile:

```
COMPILERFLAGS = $(STRICT_COMPILERFLAGS)
```

Batch 0 must verify the mechanism once before any batch relies on it:
add the override to one directory, regenerate its Makefile, and
confirm a known-bad file **actually errors** — not just that the build
is quiet. The reason for that paranoia: `-Werror=a,b,c` (one flag,
comma-separated list) is silently treated as a single unrecognized
warning option and does nothing; the flag string must be a **separate
`-Werror=` flag per warning name** (found and fixed live during Phase
1's first attempt; the "unknown warning option" text appears as a
warning, not an error, so it's easy to miss). Every batch session
still spot-checks one real file fires an expected error before
trusting its build's silence.

**M2-era overrides (28 directories): replace, don't stack.** Those
Imakefiles already carry a narrower `COMPILERFLAGS` override
(`-Werror=implicit-function-declaration` only). When such a directory
comes up in a batch, its existing override line is **replaced** by the
`$(STRICT_COMPILERFLAGS)` line — two assignments in one Imakefile
means the last silently wins and the diff misleads. The strict set is
a superset, so nothing is lost.

## Task breakdown

### Phase 0 — Pre-flip audit — RUN 2026-08-01, results below

Specific known risks flagged in M3's own closing lessons
(`claude-history/m3/m3-c2-REPORT.md` §21), scoped before touching any
flag:

- **`ams/libs/ms`'s silent parser gap — CLOSED.** Read
  `m3-ams1-REPORT.md` §4 directly: all 16 functions were hand-converted
  that same session (verified against `ansify`'s own formatting
  conventions), plus 16 more stale cross-file forward declarations for
  those same functions. Both subtree gates (this directory and
  `ams/msclients/cui`) ran clean twice. Nothing left to do here.
- **`FreeMessageContents` — FIXED, ruling CONFIRMED (2026-08-01, wdc).**
  §6 of `m3-ams1-REPORT.md`: giving the function its real 2-arg
  prototype surfaced a real ~30-year-old bug — `unscrib.c:163`'s
  `UnformatMessage` was calling it with only 1 argument (the 2nd,
  `FreeSnapshot`, was reading garbage under K&R's no-arity-check
  calling convention). Fixed as `FreeMessageContents(Msg, FALSE)` —
  don't free the snapshot during an in-place reformat, since `Msg`
  survives and is reused and the snapshot summary shouldn't be
  invalidated by a reformat that doesn't change the message's actual
  content. wdc's ruling confirms this reading and adds the likely
  history: `FreeSnapshot` was probably added to the signature later for
  other call sites that do need it, and this one call site simply never
  got updated — a missed update at one site, not an unfinished
  `UnformatMessage`. Verified still applied in the tree
  (`unscrib.c:163`). Closed, no further action.
- **`fix-missing-static-decl`'s non-idempotency — ruled out of scope
  for M4.** This is an `ansify`-pipeline bug; M4 does not re-run
  `ansify` or perform any further K&R *conversion* (that was M3's job,
  now closed). The bug is real and worth fixing before any *future*
  K&R conversion work, but nothing in M4's own task list exercises it.
  No action needed now.
- **Directory liveness — RE-CHECKED, no new surprises.** A `make -n
  install` dry run is not, on its own, a reliable liveness signal — it
  reads as trivial ("touch install.time / install.doc") for directories
  that are fully up to date from the last build, live or not (confirmed
  directly: `overhead/util/lib`, the most heavily-worked M2/M3
  directory, gives the identical trivial output). Corrected to a real
  signal instead: does the directory's own `.c` source actually have
  matching `.o` output from the last full build. Across all 91
  directories, only 4 show zero `.o` files: `contrib/wpedit` and
  `overhead/malloc` (both already known-inert, flagged during M3's
  O4/C2) plus `inst` (same O4 finding — grouped with `overhead/malloc`
  there) — all three reconfirmed, nothing new. One minor, harmless new
  observation: `atkams/messages/cmd/dumpbin.c` exists but is never
  compiled by the directory's `all::` target (only `NormalObjectRule()`
  is present, no `ProgramTarget` invoking it) — the directory itself is
  live (installs real fonts/templates for `messages`), this one file is
  just dead weight. No action needed.
- **NEW finding, not in M3's carried-forward list — `-Wstrict-prototypes`
  has a scope M4's original flag set didn't account for.** See its own
  section below; this is the one Phase 0 result that changes the plan.

### `-Wstrict-prototypes` — scope finding, 2026-08-01, changes the flag set below

`roadmap.md`'s M4 bullet named `strict-prototypes` as one of the four
"type-safety set" flags to flip alongside `implicit-int`,
`int-conversion`, and `incompatible-function-pointer-types`. Checked
directly before including it: `-Wstrict-prototypes` doesn't only flag
leftover K&R-style function *definitions* (the small, real residual
list — `atk/image/tif.c`'s two functions, `overhead/class/lib`'s
`class_EnterInfo`, both already known from M3 as harmless "self-healing"
parser bailouts) — it *also* flags every plain empty-parens
`extern int foo();` **declaration**, which this codebase uses
extensively and deliberately as the C89-legal "unspecified arguments"
idiom. M2 and M3 both used this idiom repeatedly and correctly on
purpose — `AMS1`'s own `moreprintf`/`errprintf2` fix (`m3-ams1-REPORT.md`
§7.3) picked it *specifically because* it's the C-standard-correct
answer for a pseudo-variadic function compatible with a later full
prototype. Flipping `-Werror=strict-prototypes` globally would turn
every one of those intentional declarations into a build error too —
there's no compiler-flag granularity that separates "leftover K&R
definition" from "deliberate unspecified-args declaration"; `clang`
raises the identical diagnostic for both.

**Measured, not guessed**: a declaration-shaped grep
(`^\s*(extern|static)?\s*(void|int|char|long|short|unsigned|struct\s+\w+
|boolean|Boolean|float|double)\s*\*{0,2}\s*\w+\s*\(\s*\)\s*;`) finds
**~6,024 matches tree-wide, ~1,055 inside installed headers alone**.
Directly confirmed with the compiler, not just the grep: compiling
`atk/value/entrtext.c` with `-Werror=strict-prototypes` fails
immediately — not from anything in the file itself, but from
`build/include/class.h`'s own `extern int class_EnterInfo();` and three
sibling declarations, a foundational header included nearly everywhere.
**The other three flags, checked the same way on three real files
across three different risk tiers (`atk/value/entrtext.c`,
`ams/libs/ms/freemsg.c`, `contrib/zip/lib/zipobj.c`, all compiled with
`-Werror=implicit-int,int-conversion,incompatible-function-pointer-types`),
all exit 0** — only benign `-Wdeprecated-non-prototype` informational
warnings, nothing blocking. Those three flags behave exactly like M4's
original "small residual" expectation. `strict-prototypes` alone does
not.

**Recommendation, not yet ruled on**: drop `strict-prototypes` from
M4's initial global flip. It doesn't close a correctness bug class the
way the other three do (M2/M3's whole finding pattern was real
~30-to-35-year-old bugs; `strict-prototypes`'s fallout here is almost
entirely intentional, correct 1988-era idiom, not latent defects) —
converting ~6,000 legitimate declarations to satisfy it would be
exactly the "wholesale modernization" this project's own strategic
decision (`porting-assessment.md` §"compiler leniency over wholesale
modernization") already chose not to do elsewhere (writable-strings,
c99). If ever wanted, it's its own separately-scoped effort, sized
similarly to M2/M3, not a same-day addition to M4's flip. The small
*definition*-only residual list (`tif.c`, `class_EnterInfo`) can be
hand-fixed on its own regardless of this ruling — those are real
leftover K&R, not idiom, and are cheap either way.

### `-Wformat` scope — measured 2026-08-02, operative ruling below

The per-batch flag set carries `-Wformat` as warning-only, and Phase
2's expected-shapes list names "real format-string width bugs" — but
the 1,743-error batch sizing counts none of it, which left each
delegate to decide independently whether format warnings are in scope.
Measured against the current baseline build log (clang reports
`-Wformat` by default, so the post-revert `dependInstall.log` already
contains them): **~715 unique warning sites tree-wide**, dominated by
exactly the known-dangerous LP64 width class ("format specifies type
'int' but the argument has type 'long'") that already produced real
runtime bugs in this tree (the embedded-inset printf id-truncation,
the scanf `%d`-into-`long*` half-write class). Heavily skewed: the top
three files (`rastvaux.c` 201, `dispbox.c` 84, `rasterv.c` 66) are all
in `atk/raster/cmd` (batch I1), and #4 (`figorrec.c` 58) is
`atk/figure` (batch I3).

**Operative ruling (wdc, 2026-08-02): all `-Wformat` sites are
in-batch must-fix, enforced as `-Werror=format` in
`STRICT_COMPILERFLAGS`.** An earlier draft of this ruling proposed
treating printf-direction sites as log-only ("display truncation, not
corruption"); wdc overruled it from direct experience: a datastream
writer using `%d` for a `long` object/view id truncated the ids on
write, the reader then found object and view ids that didn't match,
and **the file silently lost the object and its view** — real data
loss, from a printf. In this codebase printf is a *serialization*
path, not just a display path (datastream writers, `.ez` files,
journals), and classifying each site as "display-only vs. re-parsed"
costs more judgment than the mechanical `%d`→`%ld` fix it would avoid.
The 2026-07-26 id-truncation sweep (~60 sites) caught the instances
that were then reachable; the ~715 residual sites are the same class.
The scanf direction is must-fix a fortiori (a `%d` into a `long*`
writes 32 bits and leaves the upper half garbage — LP64 bug variant
#4, memory corruption).

Consequences: batch effective sizes grow by their share of the ~715
sites (skewed hard toward I1 and I3 — see `m4-batches.md`); the
per-directory format counts were never censused, so each batch's
Gate-0 dry run under `-Werror=format` is what reveals its real total.
The work is M2-shaped mechanical volume, well inside proven
throughput. Enforcing via `-Werror=format` (not bare `-Wformat`) means
the batch gates prove the directory clean and regressions can't creep
back in.

### Fix policy for `incompatible-function-pointer-types` (added 2026-08-02)

The 515 function-pointer-type errors are M4's judgment-heavy third,
and the one shape where a careless "fix" can *reintroduce* the exact
hazard M1 existed to eliminate: casting a mismatched function pointer
back to an untyped type silences the compiler while preserving — or
creating — the arm64 untyped-dispatch ABI bugs this project has
already root-caused twice (the classpp >8-arg stack-spill vtable bug;
the `(-1)` zero-extension observer bug). Rules for every batch, in
order of preference:

1. **Treat each site as a potential real interface bug first.** M3's
   experience was that arity/type disagreements were *constantly*
   genuine ~30-to-35-year-old bugs, not noise. Read the callee's real
   signature and the caller's real intent before touching anything.
2. **Prefer fixing the true signature end-to-end** — the function's
   definition, its declaration(s), and the `.ch` method type if one is
   involved — so the types simply agree. This is the M2/M3 house
   pattern (DRIFT checklist, `m3-rollout-runbook.md`).
3. **A cast is acceptable only for genuinely polymorphic dispatch
   tables** (a table field deliberately holding functions of differing
   signatures), and then **always to the full, correct target
   prototype** — a cast that states the real parameter list the call
   site uses.
4. **Bare `(void(*)())` or otherwise argument-untyped casts are
   banned.** No exceptions inside M4 batches; a site that seems to
   need one is a finding to escalate to the orchestrator, not a fix.
5. **Every cast that does land is listed in the batch report** with
   one line on why the table is genuinely polymorphic — so the
   orchestrator can audit the judgment calls cheaply.

### Phase 1 — One-time census probe — RUN 2026-08-01/02, then REVERTED, results preserved below

Temporarily edited `system.mcr`'s global default directly (see
"Execution mechanism corrected" above for why this was a probe, not
the landing mechanism) to get one real, accurate, tree-wide error
count:

```
COMPILERFLAGS = -std=gnu89 -Wno-return-type -Werror=implicit-int -Werror=int-conversion -Werror=incompatible-function-pointer-types -Werror=implicit-function-declaration -Wformat
```

`make Clean && make -k dependInstall CDEBUGFLAGS="-O -ferror-limit=0"`
(always `-k`, per `rollout-procedure.md`'s "Logging" section; always
`-ferror-limit=0` for a census — a single file's diagnostics silently
truncate past clang's default cap of 20, undercounting real volume).

**Result: 1,778 real errors across 83 build directories** (82 of the
91 source directories with their own `.c` files, plus `atk/basics/lib`,
which has no `.c` of its own and only duplicates `atk/basics/x`'s
errors — see `m4-batches.md`'s corrected accounting) —
`implicit-int` 1,079, `incompatible-function-pointer-types` 515,
`implicit-function-declaration` 183, `int-conversion` 0 (clean,
confirmed tree-wide). Far past the "small residual" this plan
originally hoped for. Full per-directory counts and the batch map
built from them: `m4-batches.md`.

**Also found and fixed a real prerequisite blocker along the way**:
classpp itself (`overhead/class/pp/class.c` and `overhead/class/lib/
class.c`) failed to compile under the new flags — `static pathopen();`
missing a return type in both files, plus classpp calling `exit`/
`qsort`/`free` with no `#include <stdlib.h>` and calling `PushFile`/
`PopFile` (real functions in `classpp.l`) with no declaration anywhere
reachable from `class.c`. This cascaded into `overhead/class/testing`
failing with "command not found" since the `class` binary never got
built — masking whatever that directory's own real fallout would have
been. Fixed (mechanical, same category as any other file's fallout —
not the circular "running `ansify` on classpp" problem M3 correctly
avoided): added `#include <stdlib.h>`, two `extern` forward
declarations, two `int` return types. Verified: `class.c` compiles
clean in both directories, `class` builds and installs, `overhead/
class/testing`'s cascade failure is gone. **This fix is now Batch 0 in
`m4-batches.md`** — reverted with everything else below, so it must be
reapplied and committed first when Phase 2 starts (it's independent of
any specific wave/batch, and every other batch depends on classpp
actually building).

**Reverted 2026-08-02** (wdc, resource-budget concern — the week's
budget was already ~75% consumed by M3's final waves and close-out
plus M4's Phase 0/1, and Phase 2 couldn't complete this week): all 33
edited files (`system.mcr`, the 29 M2-era
Imakefile overrides, both `class.c` fixes, two fossil-tracked generated
`machdep` Makefiles) reverted via `fossil revert` to the last commit.
Verified via a full rebuild afterward: back to the exact pre-M4
baseline, zero real errors (only the known `"Internal error: unknown
recognizer type"` string-literal false positive). **Nothing was lost**
— every number, every fix, and the full batch map are captured here
and in `m4-batches.md`; Phase 2 can re-derive the exact same starting
point from this doc alone.

### Phase 2 — Per-batch rollout (the actual fixing work, not yet started)

**Batch 0 first, once, committed**: reapply the classpp fix (Phase 1
above) and add the `STRICT_COMPILERFLAGS` macro to `system.mcr`,
verifying on one directory that the macro propagates and a known-bad
file genuinely errors (see "Execution mechanism" above).
Orchestrator-sized; everything else depends on it.

Then for each of `m4-batches.md`'s 18 sessions, same shape as every
M2/M3 session:

1. For a merged batch (or any batch large enough to warrant it), run
   the Gate-0 dry-run shape first — flip the batch's flags, build,
   triage the error mix *before* fixing. If a merged batch's
   function-pointer share looks judgment-heavy, split it back to its
   original halves and re-brief; that's the intended hedge, not a
   failure.
2. Add `COMPILERFLAGS = $(STRICT_COMPILERFLAGS)` to each batch
   directory's `Imakefile` (replacing, not stacking on, any M2-era
   override — see "Execution mechanism"). `system.mcr`'s *default*
   stays untouched until Phase 3 — every directory outside the current
   batch keeps building under the old suppressed default the whole
   time.
3. Regenerate the directory's Makefile from the edited Imakefile, then
   `make clean` so the next build compiles under the new flags; spot
   check one file fires an expected error before trusting silence.
4. Fix that batch's real fallout. Expected shapes, per M3's own
   pattern: genuine remaining K&R spots `ansify` silently missed (the
   recurring parser-gap theme — multi-line declarations, brace-glued/
   star-glued styles, comment-continuation lines — `atk/image/tif.c`'s
   `pickTileContigCase`/`pickTileSeparateCase` already known from Phase
   0), function-pointer mismatches under the fix policy above (real
   signatures preferred, untyped casts banned), format-string width
   fixes under `-Werror=format` (all must-fix, both directions — see
   the `-Wformat` scope ruling; printf is a serialization path in this
   codebase, wdc's datastream data-loss ruling), and any last
   `.ch`-vs-implementation
   DRIFT-shaped disagreements this milestone's stricter type checking
   newly exposes. M3's own experience: arity/type checking found
   genuine ~30-to-35-year-old interface bugs constantly, not rarely —
   budget for this, don't treat a "routine" batch as unlikely to find
   anything, per the C2 session's explicit lesson.
5. Gate: subtree-local (`make clean && make depend && make -k install`)
   always required. Tree-wide gate reserved for the same class of
   elevated-risk batch M2/M3 both used it for — `m4-batches.md` already
   flags `C1` (`contrib/zip/lib`) and `AMS2` (`atkams/messages/lib`) for
   this. Full `make Clean; make World` + runtime-pass checkpoints run
   after waves 2, 4, and 6 (consolidated 2026-08-02 from every-wave:
   with the batch count down to 18 and no cross-directory propagation,
   seven full-rebuild cycles were pure overhead — waves 1/3/5 are
   covered by the checkpoint that follows them, wave 7 by the Phase 5
   completion gate). `COMPILERFLAGS` doesn't propagate to consumers
   the way M1's typed `.ih` casts did (it only affects `.c` files
   compiled inside the flagged directory itself), so subtree-local is
   structurally sufficient for everything else, same reasoning M2
   established and reused by M3.
6. Runtime check, user confirmation, commit — two commits (src, then
   docs ticking the batch in `m4-batches.md`), same convention as
   every prior milestone.

Tree stays fully buildable after every one of these commits — a
resource-budget pause between any two batches is always safe, which is
the entire point of this correction.

### Phase 3 — Global flip + cleanup (only after all 18 sessions are done)

Once every batch in `m4-batches.md` is checked off: flip `system.mcr`'s
default `COMPILERFLAGS` to `$(STRICT_COMPILERFLAGS)` (folding
`implicit-function-declaration` in tree-wide too, not just the
M2-flagged directories), then delete all the now-redundant one-line
per-directory `COMPILERFLAGS = $(STRICT_COMPILERFLAGS)` overrides
(mirroring the cleanup M2 still owes `system.mcr` for its own
`implicit-function-declaration`-only overrides — do both at once,
they're the same mechanical cleanup) in one commit, verified by a
regenerated-Makefile diff showing no behavior change. Follow with a
full `make Clean && make dependInstall` tree-wide gate — at this point
it should be a pure confirmation (everything already fixed
directory-by-directory), not a discovery step.

### Phase 4 — Parked, not blocking M4 completion

- **c99 migration** — roadmap says "consider c99 after" conversion
  completes; M3's completion makes this newly relevant to *evaluate*,
  but it's a separate decision, not part of closing M4.
- **Writable-strings** — already deferred, unchanged by M4.

### Phase 5 — Completion gate

`make Clean && make World` (the same handoff-gate pattern M3 used at
each wave boundary and at its own close) plus a broader-than-usual
runtime pass — this is the tree's first-ever fully-strict build, wider
blast radius than any single M2/M3 batch, so the runtime check should
cover a representative sample across apps and insets (`ez`, `help`,
`messages`, `cuin`, `table`, `zip`, `calc`, `html`), not just the
directories touched in Phase 2's fixing pass.

## Carried forward from M3's closing lessons (`m3-c2-REPORT.md` §21)

- A "routine, Gate 0 only" delegation shape (dry-run first, real
  fixing pass second) earned its keep repeatedly in M3 — keep it for
  any Phase 2 batch large enough to warrant two gates.
- Directory liveness needs a positive check every time (Phase 0
  above), not an occasional deeper dive.
- Always confirm a backgrounded build is genuinely alive (`pgrep` +
  log-tail) immediately after launching it, before waiting on it — a
  tree-root-vs-`src/`-root mistake cost real time late in M3.
