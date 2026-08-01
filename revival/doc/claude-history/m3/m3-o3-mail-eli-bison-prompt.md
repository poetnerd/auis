# M3 Wave 1 batch O3: `overhead/mail/lib`, `overhead/mail/cmd`, `overhead/mail/testing`, `overhead/eli/lib`, `overhead/eli/bglisp`, `overhead/bison`

Read `sonnet-playbook.md`, `rollout-procedure.md` (in full — the
"Command style" section matters for an unattended session),
`m3-rollout-runbook.md` (including its full "Findings from real
sessions" section — O1 and O2 are both there now), `m3-batches.md`,
and `porting-assessment.md` §14 and §17 before starting. Also skim
`m3-o1-utillib-REPORT.md` and `claude-history/
m3-o2-imagecodecs-REPORT.md` for what these sessions actually look
like in practice — O2 in particular found and worked around real
`ansify` parser limitations you may hit again here.

This is the third real M3 batch: six unrelated small/mid directories
in one session, same batching shape as M2's own rollout point 2 (8
unrelated small directories, one session) and this batch's own
`m3-batches.md` framing.

## Directory-specific findings (already done, don't redo)

None of these six directories has any `.ch` files (checked
2026-07-26) — no `-pe`/`.eh`-regen step applies to any of them, same
as O1 and O2.

**Two directories generate source and need `depend` before a
subtree-local build** (`rollout-procedure.md`'s generated-source
caveat — a bare `make clean && make -k install` hits a fatal "file not
found" on the missing generated file before reaching real fallout):
`overhead/mail/lib` (`FlexOrLexFileRule(parsel)`, `Parser(parsey,-d)`)
and `overhead/eli/lib` (`FlexOrLexFileRule(elil)`, `Parser(eliy,-d)`).
Always use `make clean && make depend && make -k install` for every
directory in this batch, not just these two — cheap insurance, no
reason to special-case it per-directory.

Tree-wide dry-run census (`ansify --dry-run --dir src`, 2026-07-25)
baselines:

| Directory | Files | Helpers | DRIFT | Skipped |
|---|---|---|---|---|
| `overhead/mail/lib` | 33 | 180 | 0 | 0 |
| `overhead/mail/cmd` | 3 | 3 | 0 | 0 |
| `overhead/mail/testing` | 1 | 0 (already ANSI) | 0 | 0 |
| `overhead/eli/lib` | 25 | 388 | 0 | 0 |
| `overhead/eli/bglisp` | 1 | 3 | 0 | 0 |
| `overhead/bison` | 23 | 71 | 0 | **4** |

Five of six directories are expected to be clean, mechanical
conversions with no findings needing judgment. `overhead/bison` is
the one with real content — see below.

### `overhead/bison`'s 4 pre-diagnosed bailouts — already root-caused, apply directly

All 4 are in vendored GNU `getopt` (`getopt.c`, `getopt1.c` — bundled
with bison, not AUIS-native code, same "vendored third-party code has
its own K&R idioms" pattern O2 hit repeatedly). All 4 share one root
cause: every one of them takes `char *const *argv` — a pointer-to-
const-pointer-to-char, where `const` sits *between* the two `*`
tokens. `ansify`'s declaration-block parser only recognizes
qualifiers (`const`/`unsigned`/etc.) as a prefix before the base type
token, never embedded inside a multi-star declarator — bails on the
whole block for each. This is a different specific shape from O2's
`tif_fax3.c` postfix-const finding (`tableentry const *te`), but the
same general class ("the strict parser doesn't recognize every legal
C declarator shape") — worth documenting as its own sub-case if it
recurs (see the Report section below).

All 4 already have correct, existing typed prototypes in
`getopt.h` (already `#include`d by both files) — mirror them into the
definition headers, keeping each `.c` file's own K&R parameter names
(the header uses different names in a couple of places; use the
`.c`'s names, not the header's, so nothing else in the file needs to
change):

- **`getopt.c:340`** (`_getopt_internal`) — existing prototype
  (`getopt.h:113-116`): `extern int _getopt_internal (int argc, char
  *const *argv, const char *shortopts, const struct option *longopts,
  int *longind, int long_only);` → convert the definition (which has
  the return type `int` on its own line immediately above, per the
  K&R split-declaration style — fold it in) to: `int
  _getopt_internal(int argc, char *const *argv, const char *optstring,
  const struct option *longopts, int *longind, int long_only)`.
- **`getopt.c:674`** (`getopt`) — existing prototype (`getopt.h:102`):
  `extern int getopt (int argc, char *const *argv, const char
  *shortopts);` → `int getopt(int argc, char *const *argv, const char
  *optstring)` (same split-return-type-line fold).
- **`getopt1.c:64`** (`getopt_long`) — existing prototype
  (`getopt.h:106-107`): `extern int getopt_long (int argc, char *const
  *argv, const char *shortopts, const struct option *longopts, int
  *longind);` → `int getopt_long(int argc, char *const *argv, const
  char *options, const struct option *long_options, int *opt_index)`
  (again, keep the `.c`'s own parameter names: `options`,
  `long_options`, `opt_index`, not the header's `shortopts`/
  `longopts`/`longind`).
- **`getopt1.c:80`** (`getopt_long_only`) — existing prototype
  (`getopt.h:108-110`): same shape → `int getopt_long_only(int argc,
  char *const *argv, const char *options, const struct option
  *long_options, int *opt_index)`.

Verify each with a direct `make <base>.o` before running `ansify` for
real on the whole directory, same sequencing O1/O2 used: hand-fix
first, confirm it compiles, then let `ansify --dir` convert everything
else normally (it will report "no K&R definitions converted" for
these 4 functions specifically, having already been hand-converted).

## Directory-specific context worth knowing before you start

- **`overhead/eli/bglisp`** (1 file, `bglisp.c`) is ELI's own test
  REPL — noted in `porting-assessment.md` as hanging *uninterruptibly*
  at startup in an earlier, unrelated verification pass (2026-07-07),
  cause not confirmed. This is exactly the shape of AUIS terminal app
  that can go unkillable (`UE` state) under a sandboxed/harness shell
  — **do not attempt to run `bglisp` interactively for any reason**,
  compile-only verification is sufficient and matches
  `sonnet-playbook.md`'s standing rule anyway.
- **`overhead/eli/lib`** (ELI, the Lisp-like engine behind AMS's
  FLAMES mail-filtering language) is linked into `ms`, `cui`, `vui`,
  `nns`, and `messages` (`amsn.do`) — wide fan-out, but M3's `.eh`-
  locality argument (`m3-rollout-runbook.md` "Gate scope") means this
  doesn't change the gate scope, just worth knowing for the runtime-
  check section of your report.
- **`overhead/mail/lib`** is the general mail-parsing/formatting
  library (`libmail.a`) — wide consumer fan-out per M2's own session
  on this directory (~25 directories); same gate-scope note applies.

## Task

1. Confirm no `.ch` files anywhere in this batch (see above — should
   already hold, but verify).
2. Apply the 4 hand-fixes in `overhead/bison/getopt.c`/`getopt1.c`
   exactly as specified above. Verify each with a direct `make
   getopt.o`/`make getopt1.o` before proceeding.
3. `ansify --dir` each of the six directories for real, one at a time:
   `overhead/mail/lib`, `overhead/mail/cmd`, `overhead/mail/testing`,
   `overhead/eli/lib`, `overhead/eli/bglisp`, `overhead/bison`.
4. Confirm the 4 previously-diagnosed `getopt`/`getopt1` bailouts no
   longer appear in `overhead/bison`'s report.
5. If any directory's real run surfaces DRIFT or a skipped/bailout
   finding not already covered above, **stop and report before doing
   anything else** — don't try to resolve a genuinely new finding
   yourself.
6. Fix any ordinary `COMPILE FAILED — restoring original` fallout,
   same spirit as O1/O2 (missing includes, the `(void)`-as-bare-
   parameter or `DECLARE<N>`-macro misparses from O2 if either recurs
   here, etc.) — yours to resolve, not escalate, unless it looks like
   a genuine semantic disagreement. Note: the O2 `ansify` safety-gate
   gap (compile gate skipped when only `fix-missing-static-decl`
   changes a file) is **already fixed** in this tree (commit
   `948a6569`) — you don't need to work around it, but stay alert for
   its two still-open sibling parser bugs (`m3-rollout-runbook.md` O2
   section, items 1-2) since vendored code (bison, getopt) is exactly
   where they're likely to recur.
7. Subtree-local gate for **each** of the six directories separately:
   `make -C <absolute-path> clean`, then `depend`, then `-k install`
   — separate calls, absolute paths, not chained. Rebuild each clean
   twice to confirm determinism.
8. Do not run the tree-wide gate — none of these six is
   `atkams/messages/lib` or `contrib/zip/lib`.

## Gate

**Do NOT commit. Do NOT run any AUIS GUI/terminal binary
interactively — this explicitly includes `bglisp` (see above).**
Identify real consumers via `nm -g`/Imakefile `LIBS` lines for each
directory and give wdc the exact runtime-check command(s) in your
report. Stop after all six gates are green (twice each, for
determinism) and say you have stopped.

## Report

Write `m3-o3-mail-eli-bison-REPORT.md` per
`sonnet-playbook.md`'s standard format, plus:
- Real instance counts vs. the dry-run baselines above for all six
  directories, and confirmation the 4 pre-diagnosed bailouts are
  resolved.
- Each hand-fix and each compile-gate fallout fix: file:line, what was
  wrong, the fix, compile confirmation.
- Exact runtime-check command(s) for wdc, with consumers identified,
  per directory.
- `fossil status` output confirming exactly which files changed
  (expect files only inside these six directories), no commit made.
- Any new `ansify` fallout pattern not already described in
  `porting-assessment.md` §14/§17 or `m3-rollout-runbook.md`'s
  findings section (O1, O2) — in particular, whether the `char *const
  *`-embedded-qualifier shape found in `getopt` is worth recording as
  its own named pattern alongside O2's postfix-const finding, since
  both are "const in an unexpected position" but structurally
  different.
