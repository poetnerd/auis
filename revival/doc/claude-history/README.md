# claude-history — retired Claude-written working docs

This directory holds Claude-written working documents whose work is
**complete or retired**: task prompts (specs written for delegated
sessions), investigation write-ups, and rollout runbooks. They are
kept because they document how each piece was actually specified,
investigated, and gated — often the best surviving design document
for the module or fix they produced.

**Placement policy (decided 2026-07-19):** active prompts live
directly in `revival/doc/` so a fresh instance finds its task where
the roadmap's Delegated work queue and `sonnet-playbook.md` say it
is. Moving a prompt here (`fossil mv`, then repoint any references)
is part of retiring it, done during the close-out doc update.

**Subdirectory extension (decided 2026-07-25):** a milestone whose
own prompt/report volume gets large enough to clutter this directory's
flat listing gets its own subdirectory here instead (`fossil mv` into
it, same repoint-references discipline). Two cases so far: `m2/`
(2026-07-25), holding all of M2's prompts, reports, and its own
runbook, and `m3/` (2026-08-01), holding M3's prompts, reports, its
runbook, and its batch/wave map — see the dedicated sections below.
Retired docs for milestones that don't reach that scale (M1's own
runbook, most standalone investigations) stay flat at this directory's
top level, as before.

## Active Prompts (in `revival/doc/`, not here)

Current queue and suggested order live in `roadmap.md` → "Delegated
work queue". As of 2026-07-24:

1. `bcc-direct-insertion-prompt.md` — blind-copy direct-insertion
   root cause
2. `mime-attachment-icon-prompt.md` — root-cause a `multipart/mixed`
   attachment rendering as a bare `?` instead of an
   `[attachment: ...]` line (added 2026-07-21, follow-on to the
   just-retired `mime-display` task below)

Standing (never retire on completion; they recur):
`sonnet-playbook.md` (delegation briefing),
`update-revival-doc-workflow.md` (revival.md refresh procedure),
`activating-new-subtree.md` (2026-07-25, generalizes
`new-tree-porting-prompt.md` below into a repeatable playbook for
bringing any gated/inert directory into the live build, including the
M2/M3 catch-up steps that directory-specific prompt predates).

## Prompt History

The prompts fall into four project chunks; within each, the order
they were developed:

### 1. Tree build-out (July 5)

- `new-tree-porting-prompt.md` (2026-07-05) — fresh-instance guide
  for compile-fixing the newly enabled `AMS_ENV`/`CONTRIB` trees.
  The earliest delegation-style prompt; that porting phase completed
  with the `messages` launch (2026-07-05) and the calc/zip inset
  build-outs (2026-07-11).

### 2. Inset fix specs (July 12)

- `calc-ghost-fix-prompt.md` (2026-07-12) — spec for the calc inset
  ghost-text fix (XftDrawRect band-fill in `graphic_WHITE` mode).
  Fix landed and was user-confirmed the same day; lived in the tree
  root until this directory was created.

### 3. AMS-over-IMAP milestone specs (July 16–19)

Written one per milestone as the project advanced (plan of record:
`../ams-IMAP-project.md`); each was executed by a delegated session
against live Fastmail and closed with its milestone:

- `smtp-send-prompt.md` (~2026-07-16) — Milestone 1, SMTP send
  behind `dropoff()`; three stages, two gates. Done 2026-07-17 —
  first SMTP mail ever sent by AMS.
- `imap-spike-prompt.md` (2026-07-17) — Milestone 2, go/no-go
  spike; produced the hand-roll decision and the transport findings
  that shaped 3a. Done same day.
- `imap-protocol-prompt.md` (2026-07-17/18) — Milestone 3a, tlscon
  hardening + `imap_prot`, the tree's first born-ANSI module. Done
  2026-07-18.
- `imap-sync-prompt.md` (2026-07-18) — Milestone 3b, `imapsync`
  one-way mirror through the store's own code. Done 2026-07-18.
- `imap-writeback-prompt.md` (2026-07-19, retired 2026-07-23) —
  Milestone 4, the change-journal writeback, three gates, the largest
  and last of this milestone sequence (M1-4 now all closed; only M5
  OAuth2 remains, and wdc is considering postponing it behind the ANSI
  conversion project since it isn't needed for Fastmail).
  - **Gate 1** (`fb4876a`/`aeef522`/`8a94941`): capture + suppression —
    `msjournal.c`, hooks in the four `ams/libs/ms` mutation points.
    Found the variadic-caller ABI hazard (bug class #6 in
    `sonnet-playbook.md`, `porting-assessment.md` §18) — a K&R-style
    extern declaration of a genuinely variadic function crashes on
    arm64 regardless of word width, a caller-side counterpart to the
    `fdplumb` include-order hazard above.
  - **Gate 2** (`83dc58c`/`6879cdf`): `imap_prot`'s write entry points
    plus real flags/purge replay. **A real incident happened here**: a
    Gate 1 test suite that was safe when written (no write code
    existed yet) became unsafe once replay went live, and running it
    as a required regression check permanently deleted one real
    message from wdc's live account. Not recoverable (Time Machine
    excludes the scratch path by policy; the local copy was already
    gone too) — wdc chose not to pursue further recovery. Fixed same
    day: retargeted the suite to the dedicated `Revival/WritebackTest`
    sandbox behind a hardcoded-literal assertion, independent of the
    mutable folder variable, that every subsequent destructive test in
    this project now follows; a related test-design bug (suppression
    checks comparing pre/post journal bytes, broken once replay
    started legitimately consuming the journal) was fixed alongside
    it. See `imap-writeback-REPORT.md`'s "CRITICAL FINDING" section for
    the full incident writeup.
  - **Gate 3** (`df2a94c`/`164f736`): append-record replay — APPEND the
    local body (CRLF-normalized; Fastmail rejects bare-LF literals, a
    real bug found live), then delete the local native-id copy through
    the suppressed purge path so the message reappears under its
    proper deterministic id on the same run's mirror pass. Crash-safe
    resume and the purge safety valve both proven via fabricated-crash
    test cases.
  - **wdc's own by-hand acceptance test** against the real account
    (not delegated) surfaced several more real, now-documented findings:
    two Set-Options toggles (`EXP_FILEINTOMENU`, `EXP_MARKASUNREAD`)
    gate entire menus regardless of message/folder state, off by
    default; "File Into → By Name"/"Append By Name" both delete the
    original whenever you have modify rights (the only truly
    non-destructive GUI copy path is "Send/File Marked → Copy All
    Into"); an "unparsable id" replay warning turned out to be the
    *correct*, harmless outcome of deleting a message before its first
    sync, not a bug — traced to two genuinely different, deliberately
    non-overlapping id alphabets (native `ams_genid` mixed-case
    base64 vs. the new deterministic scheme's uppercase-only
    base32hex, documented in `ams-IMAP-project.md` §3). All fixed or
    documented same day; see `imap-writeback-REPORT.md`'s later dated
    sections and `mail-quickstart.md`.

### 4. Budget-crunch delegation queue (July 19)

Five gated prompts plus the standing `sonnet-playbook.md`, written
in one pass (commit 8e565c9fda) so Sonnet-class sessions could
carry work forward independently: `bcc-direct-insertion`
(still active), and:

- `strlit-sweep-prompt.md` (2026-07-19, retired 2026-07-23) —
  writable-string-literal Gate-1 census. The premise (fix mutator
  functions file-by-file) held for the census itself, but the actual
  fix turned out to be one line up a level: `config/darwin/system.mcr`
  never set `-fwritable-strings`, and Apple clang (unlike real gcc
  since 4.0) still implements it. Flag added, full clean rebuild
  verified live (`char *p = "hello"; p[0]='H';` bus-errors without it,
  clean with it). A follow-up `-Wwrite-strings` compile-only scan
  quantified the scope the census couldn't see by grep alone: 26,628
  literal→`char*` sites tree-wide, 13,937 of them inert
  `struct classheader` boilerplate in every class-based file's
  generated `.ih`/`.eh`, most of the remaining 12,691 concentrated in
  never-mutated static descriptor/keybinding tables. A full source
  cleanup to become flag-independent was scoped and explicitly
  rejected (multiple sessions of real work, including
  memory-ownership-changing rewrites of ~8 real mutator functions, for
  what the flag already gives for free) — see
  `../porting-assessment.md` issue #1 for the full tiered breakdown
  and the future-Linux-port contingency (expect `clang`, not `gcc`,
  to carry the flag there too). The three confirmed
  reachable-by-literal call sites (`help`'s default topic, cui's
  `~/.cuirc`, fdbbdf's `hexout` padding) were fixed anyway as
  belt-and-suspenders, despite not being required. See
  `strlit-REPORT.md`.
- `folder-visibility-prompt.md` (2026-07-19, retired 2026-07-22) —
  mirrored-folder default visibility. Gate 1's own premise
  (subscription status gates default visibility) was investigated,
  found real but incomplete, and a fix proposed
  (`folder-visibility-REPORT.md`) — then *falsified* the same day by
  wdc directly testing it live (Ask and Show-All subscription both
  failed to make INBOX visible). Root-caused instead via live
  instrumentation (temporarily hardcoded `foldersDebugging`/
  `MSDebugging` debug levels, reverted after): the real gate is
  `AMS_OnlyMail`, a site-config global defaulting to `1` without
  `RUN_AMDS_ENV` (this revival never defines it), which silently
  restricts the default "Expose New" view to `$HOME/.MESSAGES` only,
  excluding every other mspath root before subscription status is
  even checked. Fixed with `AMS_OnlyMail: No` in `build/etc/
  AndrewSetup` (no rebuild needed); `revival/tools/write-andrewsetup`
  added since that file lives outside `build/`'s tracked tree and
  doesn't survive `make Clean`. Confirmed live: INBOX visible by
  default with either Ask or Show-All subscription (Show-All chosen
  — see the report's addendum for the semantic difference). See
  `folder-visibility-REPORT.md`'s "Correction" section for the full
  falsification-and-recovery trail — a second case study (after
  fdplumb) in a prompt's own investigation needing to be corrected by
  direct testing rather than more static reading.
- `fdplumb-prompt.md` (2026-07-19, retired same day) — the
  fd-ledger criticals / preference-blackout investigation. Gate 1
  was closed by a Fable static-analysis session before any Sonnet
  session ran it (see `fdplumb-REPORT.md`, kept here): fdplumb
  exonerated, wrappers proven detect-only, profile.c
  failure-latching fixed. Retired with its premise corrections
  banner intact — a case study in a prompt being answered rather
  than executed.
- `mime-display-prompt.md` (2026-07-19, retired 2026-07-21) — MIME
  body display in `messages`: new born-ANSI `mimepart` module
  (`src/ams/libs/hdrs/mimepart.h` + `src/ams/libs/shr/mimepart.c`,
  multipart split/QP/base64/alternative-select/html-strip/UTF-8
  conversion, standalone-tested), wired into `text822.c`'s
  `ReadMessage`: multipart/alternative prefers text/plain, html-only
  gets the interim strip shim, multipart/mixed lists non-text parts
  as `[attachment: ...]` lines, UTF-8 text/plain finally renders
  instead of falling to a metamail button. Three gates, all closed
  (see `mime-display-REPORT.md`, kept here). Gate 3's by-hand
  acceptance against wdc's real mailbox found and fixed two more
  pre-existing, unrelated bugs load-bearing for this feature actually
  working: `GetHeader`'s header/body-boundary check was CRLF-blind,
  and `text822.do`'s Imakefile link line silently omitted
  `libmsshr.a` (`-undefined dynamic_lookup` masked it at build time;
  first real call to a new library symbol crashed at runtime). One
  known follow-on left open, queued as its own prompt:
  `../mime-attachment-icon-prompt.md` — a `multipart/mixed`
  attachment renders as a bare `?` instead of the expected
  `[attachment: ...]` line; root cause not yet found.
- `m2-census-prompt.md` (2026-07-19, retired 2026-07-24, now in
  `m2/` — see the dedicated M2 section below) — M2 point-0
  `-Wincompatible-pointer-types` census. Classified all 67
  `int*/long*` and 18 `char**→char*` instances into 13 shared root
  shapes (`m2/m2-census-REPORT.md`); found the `char**`
  cluster's real bug was in `ams.ch` itself — three `CUI_*` methods
  typed `char *` when their real `cuilib.c` implementations take
  `char **`, so clang's own "remove &" fix-it would have broken all
  18 correct callers — and a live, reachable bug via the
  stretch-goal sweep: `MS_ParseDate` writing through a class-typed
  `long *` into a real `int *` implementation, hit by
  `captions__MarkRangeOfMessages`'s uninitialized `long` locals.
  Also fixed `fontdesc_StringBoundingBox`'s `.ch` signature, the odd
  one out among its `long *`-typed siblings, found while surveying
  the `char**` cluster's `long*`/`int*` sibling group. All three
  verified via full rebuild plus a human smoke test (folder
  visiting/Expose All, mark-by-date, compose/send including a
  rejected malformed address) and committed as three separate
  checkins (`0a6cf595ef`, `f4a9d6909b`, `c496c2a9ea`). The other 45
  `int*/long*` instances (caller declares `int`, callee wants
  `long *` — the stack-overrun direction, worse than a half-fill)
  are cataloged but not yet fixed; see the report's "Open questions"
  for next steps.

## Runbook & Investigation History

Retired here 2026-07-19; in development order:

- `m1-rollout-runbook.md` (~2026-07-08) — the per-directory
  procedure for the M1 typed-dispatch rollout, with the empirical
  fallout taxonomy from the pilot directories. M1 completed
  tree-wide 2026-07-10. **Residual use:** this remains the
  procedure to follow when an inert directory (e.g.
  `contrib/zip/utility`, ness) is first brought into the build and
  needs its `.ch`-typing pass.
- `m1-point10-batches.md` (2026-07-09) — point 10's exhaustive
  batch list and paste-ready session prompts; all batches ticked
  complete 2026-07-10. Its inert-directory survey is still the best
  inventory of what's outside the default build.
- `zip-black-render-investigation.md` (2026-07-11) — the
  solid-black-at-`-O` bisection trail, ending at the signed/unsigned
  `returns char` `.ch` declaration mismatch. Resolved and committed
  2026-07-11.
- `calc-text-rendering-investigation.md` (2026-07-11/12) — the calc
  inset's crash + three Xft rendering bugs, including the
  AA-erase-by-overdraw ghost and the XQuartz recomposite-lag
  discovery (fixed with the self-XCopyArea kick, a general fix not
  calc-specific). All items resolved 2026-07-12; the one open
  cosmetic item ("=" reverse video) is tracked in `roadmap.md`, not
  here.
- `fdplumb-REPORT.md` (2026-07-19) — the Gate-1 report that
  answered `fdplumb-prompt.md` (above) by static analysis. Fixes it
  staged were committed and runtime-verified the same day. Unlike
  the other closed investigations, part of it is still live: its
  "Untouched hazards" list (unchecked malloc in RegisterOpenFile,
  openprofile's `~`-expansion comma bug, gethome's 100-byte buffer,
  prefed's unverified save path) is documented nowhere else.

## M2 — ANSI C prototype sweep (`m2/` subdirectory)

M2 (Medium-term → ANSI C conversion, `roadmap.md`) closed in its
entirety 2026-07-25: every directory in the active tree now compiles
with `-Werror=implicit-function-declaration`, closing LP64 bug class 1
(an undeclared function returning a pointer or other wide value gets
silently truncated through implicit `int`) for good, tree-wide. Retired
as one batch the same day, moved into its own subdirectory rather than
flat here given the volume: a shared runbook plus one prompt/report
pair per rollout point, 29 files total. Read `m2/m2-rollout-runbook.md`
first for any individual point below — it carries the flag mechanics,
the full fallout taxonomy (with every sub-case/sub-shape found along
the way), the gate-scope ruling, and a per-point status narrative more
detailed than this summary. The milestone-shared session rhythm (build
steps, command-style, logging conventions) lives in the still-active
`../rollout-procedure.md`, not retired here since M3/M4 reuse it
directly.

**Final tally**: 3,888 fallout instances fixed across 29 directories
(cross-checked: the running "possible genuine bug/typo" tracker in the
final report reached exactly "3888 instances/29 directories," still
empty — no real bug/typo ever found across the entire sweep, every
name resolved to a real, correctly-spelled, findable definition).
Every rollout point's own gate came back green; the tree-wide
completion gate (2026-07-25) found zero new errors anywhere, only the
same 4 pre-existing baseline errors every gate since rollout point 1
has documented. Plus a separate, earlier point-0 census under a
different flag (`-Wincompatible-pointer-types`, 85 instances, folded
into M1/M2's boundary — see below).

Rollout points, in order (each is `m2/m2-<name>-prompt.md` +
`m2/m2-<name>-REPORT.md` unless noted):

0. **`census`** (2026-07-19) — the point-0
   `-Wincompatible-pointer-types` triage, described in full above
   under "Budget-crunch delegation queue"; 85 instances, a different
   diagnostic than the rest of M2, not counted in the 3,888.
1. **`pilot-eq`**, `atk/eq` (2026-07-24) — 10 instances, 4 files. The
   pilot that established the fallout taxonomy's first three
   categories and the `depend`-before-`install` generated-source
   lesson.
2. **`batch2`**, 8 small/leaf directories — `atk/frame`, `atk/adew`,
   `atk/value`, `atk/lookz`, `atk/help/src`, `atk/extensions`,
   `overhead/cmenu`, `overhead/fonts/cmd` (2026-07-24) — 70 instances.
3. **`batch3a`**, mid-size batch A — `atk/basics/x`, `atk/basics/
   common`, `atk/figure`, `atk/syntax/tlex`, `atk/raster/cmd`
   (2026-07-24) — 137 instances. Also found and fixed a real,
   pre-existing `atk/figure` text-rendering bug (LP64 cast, unrelated
   to M2 itself) during its runtime check.
4. **`batch3b`**, mid-size batch B — `overhead/eli/lib`, `ams/libs/
   cui`, `ams/msclients/nns`, `overhead/mail/metamail/richmail`,
   `overhead/index` (2026-07-24) — 528 instances, the largest batch by
   volume until bucket 4 began.
5. **`utillib`**, `overhead/util/lib` (2026-07-24) — 74 instances.
   Bucket 4's first directory; first exact stale-estimate match.
6. **`metamail`**, `overhead/mail/metamail/metamail` (2026-07-24) —
   338 instances. Runtime check found a real, pre-existing,
   still-unfixed metamail crash (`SIGTTOU` in terminal job-control
   code) — confirmed unrelated and out of scope, documented in
   `roadmap.md`/`revival.md`.
7. **`text`**, `atk/text` (2026-07-24) — 156 instances. First
   directory where the malloc-family blind spot (clang treats
   `malloc`/`free`/`realloc`/`calloc` as builtins, invisible to the
   very diagnostic being swept for) dominated the real total rather
   than causing a near-miss — upgraded the sweep from conditional to
   unconditional for every later point.
8. **`rofftext`**, `atk/rofftext` (2026-07-24) — 104 instances.
9. **`table`**, `atk/table` (2026-07-24) — 186 instances. Found the
   `AUXMODULE`-guarded double-underscore declaration sub-case (a real,
   correctly-typed declaration hidden from the one caller that needed
   it by an unrelated header-generation optimization).
10. **`mail-lib`**, `overhead/mail/lib` (2026-07-25) — 124 instances.
    Widest consumer fan-out examined up to that point (~25 directories
    via `libmail.a`).
11. **`messageslib`**, `atkams/messages/lib` (2026-07-25) — 336
    instances. The `messages` GUI app's actual backend — both the
    subtree-local and full tree-wide gate required. Concrete LP64
    finding: 6 functions genuinely `long`-returning, not the default
    `int`.
12. **`ziplib`**, `contrib/zip/lib` (2026-07-25) — 256 instances. The
    tree's known highest-defect-density directory; both gates
    required. Runtime check found two new, real, pre-existing (not
    regression) bugs unrelated to M2 — zip/calc insets failing to load
    when embedded in a mixed-content document, and a text-block
    horizontal-drag lock — both logged in `roadmap.md` for dedicated
    investigation.
13. **`amsms`**, `ams/libs/ms` (2026-07-25) — 1,569 instances, by far
    the largest single directory in the whole sweep (114 files, ~38%
    of the original tree-wide census). Confirmed the runbook's own
    "a handful of functions dominate" prediction empirically (top 10
    functions = 45.6% of the total). Both the subtree-local and final
    tree-wide gate required — the latter doubling as M2's own
    completion gate. Also where the malloc-family blind spot's
    invisibility itself stopped holding (`malloc`/`free`/`realloc`
    became census-visible partway through the project, most likely a
    toolchain update mid-session — flagged as an open question, not
    resolved).

A fixed tree-wide checkpoint gate (run directly by the orchestrator,
no delegated session — pure verification, no fix work) also closed out
bucket 4 (points 5–12) before point 13 started, per the gate-scope
ruling's own schedule.

## M3 — K&R→ANSI definition conversion + `-pe`/`.eh` Export rollout (`m3/` subdirectory)

M3 (Medium-term → ANSI C conversion, `roadmap.md`) closed in its
entirety 2026-08-01: every active directory's K&R function definitions
converted to ANSI via `ansify` (`revival/tools/ansify`), plus the
`-pe`/`.eh` classpp Export-side rollout giving every class directory
typed dispatch prototypes. Retired as one batch the same day, moved
into its own subdirectory rather than flat here, same pattern as
`m2/`: a shared runbook, a batch/wave map, and one prompt/report pair
per session, 38 files total. Read `m3/m3-rollout-runbook.md` first —
it carries the flag mechanics, the full fallout taxonomy (parser gaps,
classpp codegen bugs, the DRIFT/stranded-declaration checklist, every
sub-case found along the way), the gate-scope ruling, and a per-batch
findings narrative more detailed than this summary. `m3/m3-batches.md`
is the exhaustive directory-to-batch/wave map. The milestone-shared
session rhythm lives in the still-active `../rollout-procedure.md`,
not retired here since M4 reuses it directly.

**Final tally**: 15 sessions across 7 dependency-order waves, all 91
active directories with `.c` files converted. Unlike M2 (zero real
bugs found), M3's typed-dispatch/arity checking surfaced numerous
genuine ~30-to-35-year-old interface bugs along the way — see
`revival.md`'s "Old bugs never found till now" for the narrative
write-ups, and the runbook's per-batch findings entries for full
technical detail. Also found and fixed several real bugs in the
project's own tooling (`ansify` parser gaps, two `classpp`/`class.c`
codegen bugs) — three of those got their own standalone fix-and-verify
sessions rather than riding along inside a batch (see below).

Sessions, in order (each is `m3/m3-<name>-prompt.md` +
`m3/m3-<name>-REPORT.md` unless noted):

**Wave 1 — overhead** (28 dirs, 305 files): `o1-utillib`
(`overhead/util/lib`, 2026-07-25) — first real M3 session, 0 DRIFT,
found a critical `ansify` compile-gate safety-gap bug (fixed in the
tool). `o2-imagecodecs` (`overhead/image/jpeg`+`tiff`, 2026-07-25/26) —
vendored codec libraries, 3 new tool-parser quirks. `o3-mail-eli-bison`
(6 dirs, 2026-07-26) — found the `regcomp`/`regexec` 35-year-old typo.
`o4-overhead-grabbag` (19 dirs, 2026-07-26) — completes Wave 1; first
real `-pe`/`.eh` rollout, found the `cmenu.h` `_STDC_`/`__STDC__` macro
typo.

**Wave 2 — atk/basics+support** (19 dirs, 197 files): `b1-basics-common`
(`atk/basics/common`, 41 classes, 2026-07-26) — first large-scale
`-pe`/`.eh` rollout, found the `ansify` brace-glued parser gap.
`b2-value-support` (5 dirs, 81 classes, 2026-07-30) — first batch to
edit `.ch` files, found the classpp `FinalizeObject` codegen bug and 6
real `.ch` type-name typos. `b3-leaf-dirs` (13 dirs, 61 files,
2026-07-30) — completes Wave 2; found the `InitializeClass`/
`FinalizeObject` restated-param over-counting bug and a brace-glued-fix
regression (both same-day tool fixes, see below).

**Wave 3 — atk/text** (1 dir, 30 files): `t1-atk-text` (2026-07-30) —
one of four flagged-risky batches (full orchestrator pre-diagnosis);
closes Wave 3, found 2 real ~35-year-old bugs plus the
stranded-forward-declaration tooling-interaction pattern.

**Wave 4 — insets** (17 dirs, 139 files): `i1-insets-batch1` (10 dirs,
70 files, 2026-07-30) — first delegate-side Gate 0 batch, found 4 tool
bugs and 3 real `.ch`-vs-implementation bugs. `i2-insets-batch2` (6
dirs, 2026-07-31) — flagged-risky (`atk/figure` history), completes
Wave 4, found 8 real bugs plus 2 bounded tool quirks.

**Wave 5 — apps** (9 dirs, 29 files): `a1-apps` (2026-07-31) — only
batch in the wave; found a live LP64 runtime crash (`cvEng()`, `cuin
dirinfo`) via wdc's own testing, not static analysis.

**Wave 6 — atkams/ams** (5 dirs, 148 files): `ams1` (`ams/libs/ms`, 113
files + the `ams/msclients/cui` COMPILERFLAGS closure, 2026-08-01) —
flagged-risky, largest single directory in M3, found a genuinely new
"worse than every prior" silent parser gap (16 functions, zero trace)
and a real ~30-year-old bug still open/unverified
(`FreeMessageContents`). `ams2` (4 dirs, 34 files, 2026-08-01) —
completes Wave 6, routine Gate-0, 2 real `.ch`-vs-usage bugs.

**Wave 7 — contrib** (12 dirs, 81 files): `c1` (`contrib/zip/lib`, 41
files, 2026-08-01) — flagged-risky (tree's highest-defect-density
directory), opens Wave 7, corrected the orchestrator's own wrong
pre-diagnosis of the `FinalizeObject` codegen rule, found 2 more real
~35-year-old bugs. `c2` (11 dirs, 40 files, 2026-08-01) — closes Wave 7
and M3 entirely; caught the same `FinalizeObject`/`InitializeClass`
mistake recurring in its own prompt, one new deliberately-not-tool-
fixed parser gap, 2 more real bugs, and closed the project's oldest
standing M3-era gate blocker (`contrib/zip/utility/ltapp.c`, open since
2026-07-11). Full `make Clean; make World` clean rebuild — the
M3-to-M4 handoff gate.

**Standalone tool-fix sessions** (not batch conversions — fixes to
`ansify`/`class.c` themselves, per the Delegation ruling that tool
construction stays a dedicated task, not batch-riding fallout):
`classpp-finalizeobject-fix` (2026-07-30) — the `FinalizeObject`
prototype/call-site self-inconsistency found in `b2-value-support`.
`ansify-brace-glued-fix` (2026-07-30) — the brace-glued-to-parameter
parser gap found in `b1-basics-common`, plus a retrospective re-run
across all 7 already-`-pe`'d directories. `ansify-brace-body-corruption-fix`
(2026-07-30) — a same-day regression in the brace-glued fix, found live
in `b3-leaf-dirs`.
