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

### Phase 0 — Pre-flip audit (top-level, cheap, before touching any flag)

Specific known risks flagged in M3's own closing lessons
(`claude-history/m3/m3-c2-REPORT.md` §21), worth resolving or at least
scoping before the global flip surfaces them as build failures instead
of known findings:

- `ams/libs/ms`'s "worse than every prior" silent parser gap — 16
  functions `ansify` never even reported as skipped, zero trace. Found
  during `AMS1`; confirm it's been fully hand-converted, not just
  logged.
- `FreeMessageContents` — a real ~30-year-old bug found during `AMS1`,
  logged as open/unverified. Rule on it before the flip makes its
  build behavior change out from under an open finding.
- `fix-missing-static-decl`'s non-idempotency — recurred in at least 5
  M3 batches (O1, O2, O3, B1/B2's brace-glue variant, C2). M3's own
  closing note says this is worth a real tool fix "if M4's scope
  includes further K&R conversion exposure" — decide that here, before
  Phase 1, not mid-fixing-pass.
- Directory liveness — confirm real Makefile targets (`make -n
  install` doing more than touching `install.time`), not just a
  `dependInstall.log` "building" line. Two M3 directories
  (`overhead/malloc`/`inst` at O4, `contrib/wpedit` at C2) were false
  positives on the log-grep signal alone. Cheap to re-verify across
  all 91 directories once, before a global flip makes liveness
  mistakes more consequential than a per-directory one.

### Phase 1 — Global flip + one tree-wide census

Edit `system.mcr` directly (not a per-directory override this time —
that's the whole point of doing it globally):

```
COMPILERFLAGS = -std=gnu89 -Werror=implicit-int,strict-prototypes,int-conversion,incompatible-function-pointer-types,implicit-function-declaration -Wformat
```

(Folding `implicit-function-declaration` into the global default too,
not just the 28 M2-flagged directories — M3 means the whole tree
should now be ready for it. `-Wformat` catches the remaining scanf
`%d`/`%ld` LP64 Variant 4 automatically, per the existing roadmap
note.)

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
