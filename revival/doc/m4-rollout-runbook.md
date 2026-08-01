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

## Why this milestone doesn't need a directory batch map (unlike M2/M3)

M2 and M3 needed per-directory rollout because each directory's
fallout was independent and unpredictable in advance, and the fix
itself required per-instance judgment (M2: find the real declaration;
M3: tool-driven conversion with real interface bugs surfacing
constantly). M4 is different in kind: it's flipping a global default
in one file, on a tree that's already had two full milestones of
preparation (M1's typed dispatch, M3's ANSI conversion) specifically
so this flip lands close to clean. The right shape is **flip once,
census once, then react to whatever's actually left** — not
pre-build another wave/batch map the way `m3-batches.md` did.

(That file's own first draft *did* over-plan before checking real
scope — it split into 24 batches, more checkpoints than M2's 13,
before anyone had verified M3's actual footprint was even comparable
to M2's. It wasn't, and the map had to be redone. The lesson carries
directly here: don't build M4's batch structure before the census in
Phase 1 exists. If Phase 1 turns up enough fallout to need batching,
build that map then, from real per-directory counts — the way M2 and
M3 both eventually did it right, just later than they should have.)

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

### Phase 1 — Global flip + one tree-wide census

Edit `system.mcr` directly (not a per-directory override this time —
that's the whole point of doing it globally). Pending confirmation of
the recommendation above, this is the corrected flag set (dropping
`strict-prototypes`):

```
COMPILERFLAGS = -std=gnu89 -Werror=implicit-int,int-conversion,incompatible-function-pointer-types,implicit-function-declaration -Wformat
```

(Folding `implicit-function-declaration` into the global default too,
not just the 28 M2-flagged directories — M3 means the whole tree
should now be ready for it. `-Wformat` catches the remaining scanf
`%d`/`%ld` LP64 Variant 4 automatically — checked too, ~368 raw `%d`/
`%ld` sites across a two-directory sample but only 3 real `-Wformat`
hits in a spot-checked file, in line with M1's already-completed
printf/scanf id-truncation sweep, not a surprise like `strict-prototypes`
was.)

One `make Clean && make -k dependInstall` (always `-k`, per
`rollout-procedure.md`'s "Logging" section — a single first-failing
file would hide the true scope). This is M4's actual point-0 census.
Given two milestones of prep, expect it to be small relative to M2's
2,353 or M3's tree-wide conversion volume — but confirm, don't assume;
the malloc-blind-spot and silent-parser-gap precedents both say a
clean-looking pass can still be hiding real fallout.

### Phase 2 — Cleanup

Once the global default subsumes them, the ~28 now-redundant
per-directory `COMPILERFLAGS` overrides M2 left behind (`grep -rl
"Werror=implicit-function-declaration" --include=Imakefile src/`) are
dead weight — delete them, one mechanical commit, verified by a
regenerated-Makefile diff showing no behavior change (the global
default now says the same thing).

### Phase 3 — Fix real residual fallout

Batch by whatever Phase 1's census actually shows — directory
clusters or error-pattern clusters, decided from real counts, not
guessed in advance. If the census is small enough, this could be one
or two sessions rather than another multi-wave rollout; if it's
larger than expected, build a batch map at that point the way M2/M3
eventually converged on (aggressive batching from the start, sized by
real data, not M3-batches.md's first-draft mistake of guessing before
checking).

Expected shapes, per M3's own pattern: genuine remaining K&R spots
`ansify` silently missed (the recurring parser-gap theme — multi-line
declarations, brace-glued/star-glued styles, comment-continuation
lines), real format-string width bugs from `-Wformat`, and any last
`.ch`-vs-implementation DRIFT-shaped disagreements this milestone's
stricter type checking newly exposes (M3's own experience: arity/type
checking found genuine ~30-to-35-year-old interface bugs constantly,
not rarely — budget for this, don't treat a "routine" batch as
unlikely to find anything, per the C2 session's explicit lesson).

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
directories touched in Phase 3's fixing pass.

## Carried forward from M3's closing lessons (`m3-c2-REPORT.md` §21)

- A "routine, Gate 0 only" delegation shape (dry-run first, real
  fixing pass second) earned its keep repeatedly in M3 — keep it for
  any Phase 3 batch large enough to warrant two gates.
- Directory liveness needs a positive check every time (Phase 0
  above), not an occasional deeper dive.
- Always confirm a backgrounded build is genuinely alive (`pgrep` +
  log-tail) immediately after launching it, before waiting on it — a
  tree-root-vs-`src/`-root mistake cost real time late in M3.
