# M3 Wave 1 batch O3: `overhead/mail/lib`, `overhead/mail/cmd`, `overhead/mail/testing`, `overhead/eli/lib`, `overhead/eli/bglisp`, `overhead/bison`

## 1. Status

Stopped at the gate for all six directories — subtree-local `make
clean && make depend && make -k install`, zero real `error:` lines,
each run twice for determinism. `overhead/eli/lib`'s one held-back
file, `prmtives.c`, was **resolved 2026-07-26 per wdc's ruling** (see
the addendum at the end of §5) — all 25 files in that directory are
now converted, no exceptions remain. No commits made by this session
(the `prmtives.c` follow-up fix was applied directly afterward, also
uncommitted at report-writing time). Working tree: 76 files edited —
75 inside the six batch directories, plus **one file outside the
batch**, `src/overhead/mail/hdrs/mail.h` (§4.1, a one-line, low-risk,
verified-safe cross-directory fix; flagged prominently, not hidden).
Session diff at tree root: `m3-o3-mail-eli-bison-session.diff` (8,399
lines, 75 file sections — predates the `prmtives.c` follow-up, see the
addendum). This report lives in `revival/doc/claude-history/` per the
standard convention.

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (including its
   "Command style" section), `m3-rollout-runbook.md` (including its
   full "Findings from real sessions" — O1 and O2), `m3-batches.md`,
   and `porting-assessment.md` §14 and §17 in full, plus skimmed
   `m3-o1-utillib-REPORT.md` and
   `m3-o2-imagecodecs-REPORT.md` — all before starting,
   per the prompt.
2. Confirmed no `.ch` files in any of the six directories (`find ...
   -maxdepth 1 -name '*.ch'`, all empty) — matches the prompt's note,
   no `-pe`/`.eh`-regen step applies anywhere in this batch.
3. Applied the 4 pre-diagnosed `getopt`/`getopt1` hand-fixes exactly
   as specified in the prompt (§3 below). Verified each with a direct
   `make getopt.o`/`make getopt1.o` before proceeding — both compiled
   clean (getopt.o: 4 benign `-Wdeprecated-non-prototype` warnings
   from the file's own `#if __STDC__`/`#else` empty-parens branch,
   which this build's `__GNU_LIBRARY__` undefined path takes — not
   touched, out of scope; getopt1.o: 0 warnings).
4. `ansify --dir` each of the six directories for real, one at a time,
   investigating every compile failure with a scratch diagnostic
   script (importing `ansify.py`'s own `run_fix_tools`/`convert_file`/
   `compile_gate` directly, always restoring from a backup) to see the
   *full* compiler output rather than the tool's own 12-line-truncated
   report — same technique O1/O2 used. Full breakdown in §4/§5 below.
5. Confirmed the 4 pre-diagnosed `getopt`/`getopt1` bailouts no longer
   appear in `overhead/bison`'s real run (0 skipped/bailout, 0 DRIFT).
6. Subtree-local gate for each of the six directories, twice each, as
   separate `make -C <absolute-path> clean`/`depend`/`-k install`
   calls (never chained) — all six green both times. `overhead/eli/
   bglisp`'s Imakefile confirmed to have no execute-during-install
   step (`ProgramTarget`/`InstallProgram`/`InstallDocs` only) before
   running its gate, per the prompt's explicit ban on running
   `bglisp` interactively.
7. Identified real runtime consumers via `nm -g` (where the harness
   permission system allowed it — see §6) and Imakefile `LIBS` lines.
8. `fossil status`/`fossil extras` (§7) — 75 files edited exactly as
   expected, no stray backup files, no commit made.
   `m3-o3-mail-eli-bison-session.diff` written to the tree root.

## 3. The 4 pre-diagnosed `getopt`/`getopt1` hand-fixes (prompt's own spec, applied verbatim)

- `getopt.c:340` (`_getopt_internal`) → `int
  _getopt_internal(int argc, char *const *argv, const char *optstring,
  const struct option *longopts, int *longind, int long_only)`.
- `getopt.c:674` (`getopt`) → `int getopt(int argc, char *const *argv,
  const char *optstring)`.
- `getopt1.c:64` (`getopt_long`) → `int getopt_long(int argc, char
  *const *argv, const char *options, const struct option
  *long_options, int *opt_index)`.
- `getopt1.c:80` (`getopt_long_only`) → `int getopt_long_only(int
  argc, char *const *argv, const char *options, const struct option
  *long_options, int *opt_index)`.

All 4 verified with a direct `make getopt.o`/`make getopt1.o` before
running `ansify` for real, per the prompt. `overhead/bison --dir`'s
real run then reported 0 skipped/bailout, confirming the 4 are gone.

## 4. Real instance counts vs. dry-run baselines, and each fallout fix

| Directory | Dry-run (files/helpers/DRIFT/skipped) | Real run |
|---|---|---|
| `overhead/mail/lib` | 33/180/0/0 | 33 files; 0 DRIFT; 0 skipped; **4 compile-gate fallout fixed** (§4.2) |
| `overhead/mail/cmd` | 3/3/0/0 | 3 files; 0 DRIFT; 0 skipped; 0 fallout — clean |
| `overhead/mail/testing` | 1/0/0/0 | 1 file (`utest.c`, already ANSI); 0 fallout — clean |
| `overhead/eli/lib` | 25/388/0/0 | 25 files; 0 DRIFT; 0 skipped; **1 file (`prmtives.c`) held back — not ordinary fallout, see §5** |
| `overhead/eli/bglisp` | 1/3/0/0 | 1 file; 0 fallout — clean |
| `overhead/bison` | 23/71/0/4 (pre-diagnosed) | 23 files; 0 DRIFT; **0 skipped/bailout — the 4 pre-diagnosed bailouts confirmed resolved** by the hand-fixes in §3; 0 further fallout |

Every dry-run-predictable number matches exactly; the only
discrepancies are the compile-gate fallout counts, which the dry-run
structurally cannot predict (same framing as O1/O2).

### 4.1 `overhead/mail/lib`: 4 compile-gate fallout fixes

All 4 are variants of the same underlying mechanism the O1/O2 reports
already named — a pre-existing or `fix-missing-static-decl`-inserted
**empty-parens forward declaration becomes incompatible once the real
definition gets full ANSI parameter types**, either because a
parameter type is narrow (promotable) or because the arg count itself
was never actually correct. One of the four also required touching a
file **outside** the six-directory batch (`overhead/mail/hdrs/
mail.h`) — flagged explicitly, verified low-risk, and covered in its
own subsection.

**`imap_prot.c:106` `imap_seterr`** — `fix-missing-static-decl`
inserted a fresh empty-parens stub (`static void imap_seterr();`)
because it saw no declaration matching its narrow empty-parens
recognizer (same non-idempotency class O1's `unscribe.c`/`WriteFrag`
found), but `imap_seterr`'s real definition is **variadic** (`static
void imap_seterr(struct imapconn *conn, const char *fmt, ...)`) — an
empty-parens (K&R "unspecified arguments") declaration is
*structurally* incompatible with a variadic definition in C, not just
narrow-type-promotable-incompatible like the encode.c/eq__WriteFILE
species; this is a **new sub-case of the fix-missing-static-decl
non-idempotency pattern**, not previously documented: it doesn't just
fail to recognize a full prototype as "already declared," it can
insert a stub that's fundamentally impossible to reconcile with a
variadic definition. Fixed by running `fix-missing-static-decl` once
for real, then hand-retyping the one inserted declaration to the full
variadic prototype (O1's `unscribe.c` sequence). Verified with a
direct `make imap_prot.o`: 0 errors, 38 benign
`-Wdeprecated-non-prototype` warnings for the file's other
(pointer/int-typed, non-narrow) helpers.

**`parseadd.c`: `UnparseAddress`, `UnparseSimpleAddress`,
`UnparseGroupAddress`, `Fold`** — all four take `bool` (=`unsigned
char`, confirmed via `cc -E`) by-value parameters, the exact
narrow-type-promotion conflict species (Pilot A `eq__WriteFILE`,
O1 `encode.c`) — but against **pre-existing** empty-parens forward
declarations already in the file (`parseadd.c:51-57`), not
tool-inserted ones. Hand-retyping those pre-existing declarations and
then running the **full** `ansify` driver reproduces O1's non-
idempotency bug on top (fix-missing-static-decl doesn't recognize the
now-full-prototype declarations as "already declared," inserts a
second empty-parens stub for the same 4 names, new conflict at a new
line). Working sequence, extending O1's playbook to the
"pre-existing-declaration" trigger (not just the "tool-inserted-stub"
trigger): revert to pristine, run `fix-missing-static-decl` once for
real (it inserts a **second block** of decls for names it didn't
recognize — `Fold` and 3 unrelated-but-also-affected names, `PrintWith0Hosts`
et al., stayed unduplicated because their existing decls happened to
already match its recognizer), hand-retype only the 4 problem names
(deleting the one truly-redundant duplicate for `Fold`, which would
otherwise sit before the file's own `bool` typedef and fail to
compile for an unrelated ordering reason), then run only `convert_file`
directly (skipping the top-level driver's second `fix-missing-static-
decl` invocation). Verified with a direct `make parseadd.o`: 0 errors,
57 benign warnings, 32 helpers converted.

**`scan822.c:94` `AddTok`** — narrow `char ch` parameter, tool-
inserted stub (the ordinary O1 `unscribe.c`-shaped case, no header
involved). Fixed the same way: `fix-missing-static-decl` once for
real, hand-retype the one inserted declaration, then
`convert_file`-only. Verified: 0 errors.

**`scan822.c:102` `IsOK822Atom`** (bundled with the `AddTok` fix, same
file, same session pass) — this one conflicts not with a file-local
declaration but with **`overhead/mail/hdrs/mail.h:124`**:
`extern int IsOK822Atom();` (empty parens) — a public, non-static,
cross-directory header declaration, the one part of this fix that
reaches outside the six batch directories. `IsOK822Atom` takes a
narrow `char ch` parameter, same promotion conflict. Checked blast
radius before touching it: `grep -rln IsOK822Atom src` finds exactly
2 consumers tree-wide — `scan822.c` itself (the definition) and
`overhead/wputil/brisk.c`, which is dead code (`overhead/Imakefile`
gates `WPUTIL = wputil wpi` behind `#ifdef WHITEPAGES_ENV`, undefined
in this build — same dead-code family O1's session found for
`overhead/util/lib`'s bt/wp files; confirmed via absence of any
`wputil` descent and the `#ifdef` gate directly). Retyped
`mail.h:124` to `extern int IsOK822Atom(char ch);` and reinstalled
the header via `make -C src/overhead/mail/hdrs install` (proper
`install`, not `cp`, per the standing rule). Verified with a direct
`make scan822.o`: 0 errors, 5 helpers converted
(`BracketField`/`AddTok`/`IsOK822Atom`/`Quote822LPart`/`Quote822Phrase`).
This is a genuinely new fallout shape for the M3 taxonomy (see §8):
unlike the `.eh`/classpp mechanism, an ordinary public C header is
**not** locality-guaranteed to the flagged directory — a narrow-typed,
non-static file-local function's ANSI conversion can surface a stale
declaration in a totally different, unbatched directory's hand-
maintained header.

**`stats.c` `warning`** — a `/*VARARGS1*/`-annotated "poor man's
varargs" idiom: a **fixed 6-parameter** K&R function
(`static void warning(format, a0, a1, a2, a3, a4) char *format, *a0,
*a1, *a2, *a3, *a4;`), historically called with **fewer than 6**
arguments at every one of its 9 call sites, relying on K&R's lenient
calling convention (unsupplied trailing args are simply never read,
since the format string never references them). Once the definition
gets a real ANSI prototype, every call site must supply exactly 6
arguments — an `error: too few arguments to function call, expected
6, have N` at 8 of the 9 call sites (the 9th is inside `#ifdef NOTDEF`,
permanently dead, not compiled, hence not flagged, but padded anyway
for internal consistency). This is a distinct fallout shape from
DRIFT/skipped/bailout/narrow-type-promotion — not a `.ch` question,
not a tool bug, just the direct, expected consequence of an
under-specified varargs-style caller convention now getting arity-
checked. Fixed by padding every call site with `NIL` (the file's own
`#define NIL 0`) for the missing trailing arguments, in the pristine
K&R source, before running `ansify` for real:
`stats.c:119,179,186,205,245,253,288,323,410`. Verified pristine
compiled clean before conversion, then `ansify src/overhead/mail/
lib/stats.c` converted cleanly: 0 compile failures, 5 helpers
(`warning`/`Format`/`InitStats`/`Logstat`/`SetTransaction`).

### 4.2 `overhead/eli/lib`, `overhead/eli/bglisp`, `overhead/bison`, `overhead/mail/cmd`, `overhead/mail/testing`

No fallout beyond §5's `prmtives.c` finding. `overhead/eli/lib`'s
other 22 fossil-tracked files (`bucket.c` through `symtab.c`)
converted cleanly with 0 compile failures; `elil.c`/`eliy.c` (the
`FlexOrLexFileRule`/`Parser(-d)` generated lexer/parser, confirmed via
`fossil finfo` returning "no history for file") also converted
cleanly but are untracked build products, regenerated fresh by `make
depend` — not part of the 75-file fossil diff, same as `parsel.c`/
`parsey.c` in `overhead/mail/lib` (also generated, also untracked,
also not in the diff). `overhead/eli/bglisp/bglisp.c` (3 helpers),
`overhead/bison` (19 of 23 files; `derives.c`/`gram.c`/`nullable.c`/
`version.c` already ANSI, "no K&R definitions converted"),
`overhead/mail/cmd` (all 3 files), and `overhead/mail/testing`
(`utest.c`, already ANSI) all converted with zero compile-gate
fallout.

## 5. `overhead/eli/lib/prmtives.c` — held back, not ordinary fallout: a likely real ~35-year-old bug

This is the one finding in this batch that crosses the line the
prompt itself draws between "ordinary COMPILE FAILED fallout... yours
to resolve" and "a genuine semantic disagreement" that should be
reported, not resolved. Full untruncated compile output gathered via
the same scratch-diagnostic technique used throughout this report
(13 real errors; `-ferror-limit` was not a factor, well under the
default 20).

**Three safe, mechanical, low-judgment fixes that are NOT applied
yet** (blocked behind the fourth, see below):

- `malloc`/`free`/`realloc`/`system` implicit-function-declaration
  errors (`prmtives.c:319,399,3166,3971`) — the file has no
  `#include <stdlib.h>` anywhere in its own chain
  (`prmtives.h`→`eli.h` doesn't pull it in either). Same class of
  invisible-to-census blind spot `rollout-procedure.md`'s Logging
  section already documents (clang builtins, zero warning under K&R).
  Fix: add `#include <stdlib.h>`.
- `dbg_fclose` implicit-declaration errors (4 call sites,
  `prmtives.c:1568,1574,1578,1583,2382`) — `eli.h` includes
  `<fdplumb.h>`, which `#define`s `fclose` → `dbg_fclose` but (per
  `fdplumb.h`'s own comment, and matching a **pre-existing hand-
  written note already in `stats.c`** from an earlier session:
  "fdplumb.h renames close()/fclose()/socket() via #define but only
  declares 6 of its 16 dbg_* wrapper names -- these three aren't among
  them") never declares `dbg_fclose` itself. Fix, matching `stats.c`'s
  own precedent exactly: add `extern int dbg_fclose();`.
- `setpgrp(childpid, childpid)` at `prmtives.c:3739` — old BSD 2-arg
  `setpgrp(pid, pgrp)`; macOS's `<unistd.h>` only provides the
  POSIX 0-arg `setpgrp(void)` (the 2-arg BSD form is `setpgid` on this
  and every modern platform). Standard, unambiguous POSIX-drift rename
  (`porting-assessment.md`'s class-8 "Misc POSIX drift" territory),
  not the ansify tool's concern at all. Fix: rename to
  `setpgid(childpid, childpid)`.

**The one finding held for a ruling — `regexec`/`regcomp` calls
(3 sites: `prmtives.c:2362,2366` / `2575,2579` / `2777,2781`, the
`Prim_RE_STRCONTAINS`/`Prim_RE_STRDECOMPOSE`/
`Prim_RE_STRDECOMPOSEPLUS` regex primitives)**:

- `prmtives.c` calls `regcomp(pat)` (1 arg, returns `regexp *`) and
  `regexec(rptr, ref)` (2 args) — but **no function named `regcomp`
  or `regexec` is declared or defined anywhere in this codebase.**
  `overhead/rxp/regexp.h` (the AUIS-local, non-POSIX regex engine
  `overhead/rxp/regexp.c` implements) declares `reg_comp`/`reg_exec`
  (with underscores) — one-arg/two-arg signatures that match the call
  sites' arities **exactly**. `overhead/rxp` is not in this batch, but
  it produces `librxp.a`, and **every real consumer of `libeli.a`**
  (`ams/ms`, `ams/msclients/{cui,vui,nns,imapsync}`,
  `atkams/messages/lib`) **also links `librxp.a`** — confirmed via
  `grep -rl librxp --include=Imakefile src`, exact same consumer set
  as `libeli.a` itself. This is strong, if not certain, evidence the
  original 1990s author meant to call `reg_comp`/`reg_exec` and simply
  mistyped the name (missing underscore) — a plausible and easy typo
  given how close the names are.
- Under old K&R/`-Wno-implicit-function-declaration` compilation, this
  call would implicitly declare `regcomp`/`regexec` as functions
  returning `int` and let the **linker** resolve the symbol names —
  and macOS's own libSystem *does* export POSIX `regcomp`/`regexec`
  (`regcomp(regex_t*, const char*, int)`, `regexec(const regex_t*,
  const char*, ...)` — totally different arity and ABI from what's
  actually being called here). So this call has, for the entire life
  of this port (and quite possibly for decades on other Unix
  platforms with their own libc `regcomp`/`regexec`), silently linked
  against **the wrong function with an incompatible ABI** — passing a
  `char *` pattern string where POSIX `regcomp` expects a `regex_t *`
  as its first argument. If any ELI/FLAMES mail filter ever actually
  exercises `RE-STRCONTAINS`/`RE-STRDECOMPOSE`/`RE-STRDECOMPOSEPLUS`,
  this is a plausible crash or memory-corruption bug, not just an
  unused code path — I have not attempted to determine whether these
  primitives are reachable from any live FLAMES filter today.
- I am **not** applying a fix. Renaming to `reg_comp`/`reg_exec` is a
  live runtime-behavior change (previously: silent wrong-function
  link; after: calls the codebase's own local regex engine) in a
  mail-filtering-relevant subsystem — exactly the class of finding
  `rollout-procedure.md`'s Hard-stop reporting section and this
  prompt's own item 6 ("genuine semantic disagreement... escalate")
  say needs a human ruling, not a Sonnet-class unilateral fix, even
  with high confidence in the diagnosis.
- Because the compile gate is file-level, I could not get the three
  safe fixes above (`stdlib.h`, `dbg_fclose`, `setpgrp`→`setpgid`)
  through `ansify`'s own gate without *also* resolving this one — so
  `prmtives.c` is left **entirely K&R**, exactly as `ansify`'s own
  auto-revert correctly left it. The three safe fixes are documented
  above so a future session (or wdc directly) can apply all four in
  one pass once the `regexec`/`regcomp` question is ruled on.
- This is the one file across all six directories not converted by
  this session; every other file in `overhead/eli/lib` is done.

**RESOLVED 2026-07-26 (wdc's ruling, applied directly, not by this
session)**: renamed all 3 sites' `regcomp`→`reg_comp`/
`regexec`→`reg_exec`, plus the three safe fixes (`#include
<stdlib.h>`, `extern int dbg_fclose();` matching `stats.c`'s
precedent, `setpgrp`→`setpgid`). Verified K&R-compiles clean before
conversion, then `ansify src/overhead/eli/lib/prmtives.c` converted
cleanly (77 helpers, 0 compile failures, 0 DRIFT). Full `overhead/eli/
lib` directory gate re-run twice, both clean, identical object sets —
`prmtives.c` is no longer an exception; all 25 files in this directory
are now converted. Bug written up in `revival.md`'s "Old bugs never
found till now" per wdc's request. `fossil status` now shows 76 files
for this batch (the 75 above plus `prmtives.c`).

## 6. Runtime consumers (identified via `nm -g`/Imakefile, where the harness allowed it)

The harness's permission system denies `nm`/`ls`/any Bash access
to `build/bin/{runapp,ez,messages,messagesn,cui,cuin}` outright (every
attempt, including a bare `ls -la`, was denied with no prompt) —
consistent with the standing rule against running these interactively,
just enforced more broadly than expected (blocks read-only inspection
too, not just execution). Non-GUI/non-symlinked binaries and dynamic
libraries (`imapsync`, `mailserver`, `ms`, `build/dlib/atk/amsn.do`)
were inspectable directly; Imakefile `LIBS` lines fill the gap for the
blocked binaries.

- **`overhead/mail/lib` (`libmail.a`)** — wide fan-out (23 Imakefiles
  reference it tree-wide). Two real consumers matter most for this
  session's fixes: `ams/msclients/cui/Imakefile:66`
  (`ProgramTarget(cuin, ..., ${NOSNAPLIBS} ...)` → `NOSNAPLIBS`
  includes `${LIBS}` → `${BASEDIR}/lib/libmail.a`) and
  `atkams/messages/lib/Imakefile:93`
  (`DynamicMultiObject(amsn.do, ..., $(NLIBS), ...)` → `NLIBS`
  includes `$(LIBS)` → `libmail.a`). Confirmed directly via `nm -g`
  on `build/dlib/atk/amsn.do`: `_IsOK822Atom` and `_ParseAddressList`
  both defined (`T`) in `amsn.do`'s own symbol table — `IsOK822Atom`
  specifically makes `messages` the load-bearing check for this
  session's cross-directory `mail.h` fix (§4.1). `cuin`'s own linkage
  confirmed via the Imakefile line above (direct `nm` blocked by the
  harness, not by any real ambiguity — `NOSNAPLIBS` unconditionally
  includes `libmail.a`).
- **`overhead/eli/lib` (`libeli.a`)** — linked into `ams/ms`,
  `ams/msclients/{cui,vui,nns,imapsync}`, and `atkams/messages/lib`
  (`amsn.do`/`amss.do`), the ELI/FLAMES mail-filtering engine.
  Confirmed via `nm -g build/dlib/atk/amsn.do`: `_eliEval` and 12
  sibling `_eliEval*`/`_eliEvalStk_*` symbols all defined (`T`)
  directly in `amsn.do`'s own symbol table.
- **`overhead/eli/bglisp`** — standalone `bglisp` binary, ELI's own
  test REPL, not linked into any other consumer (`ProgramTarget`
  produces a standalone executable). Compiled and linked successfully
  this session; **not run**, per the prompt's explicit ban.
- **`overhead/mail/cmd`, `overhead/mail/testing`** — standalone
  command-line utilities (`amsgenid`/`arpadate`/`decodeid`/`utest`),
  not linked into any GUI/terminal app.
- **`overhead/bison`** — per `porting-assessment.md`'s own "Vendored
  bison... disabled on Darwin/arm64" section: `overhead/bison/
  Imakefile` builds `bison` for reference only and does **not**
  install it (the system `bison` is used instead, `Parser()` in
  `config/andrew.rls` invokes it). No runtime consumer exists or
  should be checked — gate-only, matching the pattern
  `m3-batches.md` already flagged for `atk/srctext`/`dired`/`chlist`
  in M1.

### Exact runtime-check commands for wdc

Native Terminal.app, not an IDE terminal; `DISPLAY=:0;` as its own
statement first; use a **fresh** process for each check (`.do` files
cache for the life of a process).

1. **The load-bearing check for this session's cross-directory fix**
   (§4.1, `mail.h`/`IsOK822Atom`, and general mail-header-parsing path
   exercising most of `overhead/mail/lib`'s other converted files):
   ```
   DISPLAY=:0; messages
   ```
   Open and read any message normally (header field extraction via
   `BracketField`/`IsOK822Atom`/`AddTok` runs on every message
   display). Confirm no crash and headers/addresses render correctly.
2. **Address parsing/unparsing** (`parseadd.c` — `UnparseAddress`
   family, this session's biggest single-file fallout fix): compose a
   reply or forward to a message with multiple `To:`/`Cc:` addresses,
   or open the address book if `messages` exposes one, and confirm
   addresses display/re-format without truncation or garbling.
3. **ELI/FLAMES path** (`overhead/eli/lib`, 22 converted files): if
   `messages` or `cui` has any FLAMES-based mail-filtering rule
   configured, exercising it would test `eliEval`/the primitive
   dispatch table this session touched. I did not find or set up a
   FLAMES filter myself (out of scope, `AMS`-config-territory) — if
   none is configured, ordinary mail delivery/filing already loads and
   initializes ELI's runtime (`eliInitLibraries`), which is enough to
   confirm no load-time regression even without exercising a specific
   rule. **Do not exercise `RE-STRCONTAINS`/`RE-STRDECOMPOSE`/
   `RE-STRDECOMPOSEPLUS` specifically** — those are exactly the
   primitives implicated in §5's unresolved `regexec`/`regcomp`
   finding; `prmtives.c` is unconverted so their behavior is unchanged
   from before this session, but there's no reason to go looking for
   the pre-existing bug during a routine check.
4. **`overhead/mail/cmd`/`overhead/mail/testing`**: no GUI-visible
   check — these are standalone CLI utilities, not reached from
   `messages`/`ez`. Gate-only for this session's purposes, matching
   the pattern noted above for `overhead/bison`.
5. **`overhead/bison`, `overhead/eli/bglisp`**: no runtime check
   applicable/authorized — bison isn't installed/invoked at all;
   bglisp must not be run interactively per the prompt. Both are
   gate-only for this session.

## 7. `fossil status` — 75 files changed, all inside the six directories plus one justified cross-directory header

```
$ fossil status | grep -c "^EDITED"
75
```

Breakdown: `overhead/bison` 19, `overhead/eli/bglisp` 1,
`overhead/eli/lib` 22, `overhead/mail/cmd` 3, `overhead/mail/hdrs` 1
(**outside the batch** — `mail.h`, §4.1), `overhead/mail/lib` 29.
(`overhead/mail/testing` 0 — `utest.c` needed no change.) Matches the
real-run counts in §4 exactly once "no K&R converted" files and
untracked generated files (`parsel.c`/`parsey.c`, `elil.c`/`eliy.c`)
are subtracted. `prmtives.c` correctly absent (left K&R, §5).

No commit made. `m3-o3-mail-eli-bison-session.diff` (8,399 lines, 75
files) written to the tree root. `fossil extras` checked for stray
artifacts from the investigation scripts: only the expected build
byproducts (`Makefile`/`Makefile.BAK`/`*.o`/`*.a`/`install.time`/
`install.doc`/generated parser sources) and the pre-existing `build/`
tree — no `.ansify-orig`/`.diag-orig`/backup files left behind (my
scratch scripts lived entirely in the scratchpad directory or used
`/tmp`, never wrote stray files into the checkout).

## 8. New `ansify`/fallout patterns found this session

1. **`fix-missing-static-decl` non-idempotency, variadic sub-case**
   (§4.1, `imap_prot.c`/`imap_seterr`) — O1's `unscribe.c` finding
   already established that the tool's "already declared" recognizer
   only matches a literal empty-parens form, so it can insert a
   conflicting stub even when a correct typed declaration already
   exists. This session found a sharper variant: when the real
   definition is **variadic**, the inserted empty-parens stub isn't
   just narrow-type-promotion-incompatible (fixable by retyping to
   match) — it's *structurally* impossible to write a compatible
   empty-parens declaration at all once the definition has a `...`
   tail, so the only fix is a full variadic prototype, same sequence
   as the non-variadic case but worth naming as its own recognizable
   symptom (a variadic K&R definition, previously undeclared, hits
   this every time).
2. **`fix-missing-static-decl` non-idempotency, pre-existing-
   declaration trigger** (§4.1, `parseadd.c`) — O1's and O2's
   instances of this bug were both triggered by declarations the tool
   itself had inserted on a prior pass. This session found the same
   failure mode triggered by a **pre-existing, hand-written**
   empty-parens declaration already in the original K&R source: once
   I retype it to a full prototype (the correct fix for the narrow-
   type-promotion conflict it was itself failing on), a *second*,
   independent run of `fix-missing-static-decl` doesn't recognize my
   typed version either and inserts a fresh duplicate. Same underlying
   root cause as O1's finding (recognizer only matches empty parens),
   but confirms it's not limited to tool-inserted declarations —
   *any* full-prototype forward declaration for a name lacking a
   matching empty-parens block will be seen as "missing" and get a
   duplicate stub on the next `fix-missing-static-decl` pass,
   regardless of the declaration's origin.
3. **Ordinary public headers are not `.eh`-locality-guaranteed**
   (§4.1, `mail.h`/`IsOK822Atom`) — `m3-rollout-runbook.md`'s "Gate
   scope" section argues M3's blast radius is directory-local because
   `.eh` files are never cross-directory-visible except via classpp's
   own mechanism. That argument is specific to `.eh`; it does not
   extend to ordinary hand-maintained public C headers. A non-static,
   narrow-typed, file-local function's ANSI conversion can surface a
   stale empty-parens declaration in a **different, unbatched
   directory's** header (here, `overhead/mail/hdrs`, not itself part
   of any M3 batch yet). The fix was safe and narrow in this instance
   (single real consumer, confirmed dead-code-only elsewhere), but
   this is a real, previously-undocumented gap in the gate-scope
   argument worth flagging for `m3-rollout-runbook.md`: **grep the
   installed header tree for a matching empty-parens declaration of
   any non-static helper before considering a directory's Import-side
   blast radius fully accounted for**, the same spirit as M1's
   Import-fallout caution, now shown to apply to M3 too, just via a
   different mechanism (ordinary headers, not `.ih`).
4. **Poor-man's-varargs (`/*VARARGS1*/`) fixed-arity K&R functions**
   (§4.1, `stats.c`/`warning`) — a distinct, not-previously-catalogued
   fallout shape: a K&R function declared with N fixed parameters but
   conventionally called with fewer, relying on old-style calling
   convention to silently ignore the unsupplied trailing arguments.
   Not a tool bug, not DRIFT, not a narrow-type-promotion conflict —
   just the direct and correct consequence of ANSI arity-checking a
   function that was never really variadic, just casually
   under-called. Fix is mechanical and safe (pad every call site to
   the full arity with a zero/NULL sentinel already in scope, `NIL`
   here) once recognized; worth naming so a future session recognizes
   "too few arguments... expected N, have M" immediately as this
   shape rather than investigating from scratch. Likely to recur:
   this codebase has other `/*VARARGS1*/`-annotated functions.
5. **A likely real, ~35-year-old wrong-function-name bug surfaced by
   arity/type checking, unrelated to any `.ch`** (§5,
   `prmtives.c`/`regexec`↔`reg_exec`) — the DRIFT/hard-stop taxonomy
   in `porting-assessment.md` §14 and `m3-rollout-runbook.md` frames
   this class of finding ("real bug, needs a human ruling on which
   side is stale") specifically around `.ch`-vs-`.c` disagreements.
   This session found the same *species* of finding with no `.ch`
   involved at all — a plain call-site name mismatch against a
   similarly-named function in a different library, silently working
   (with the wrong ABI) only because K&R's implicit declaration let
   the linker paper over it for decades. Worth widening the
   DRIFT/hard-stop framing in `m3-rollout-runbook.md` to cover this
   non-`.ch` variant explicitly, since M3's own conversion mechanism
   (turning implicit declarations into real ones) is exactly what
   surfaces it, and it's reasonable to expect more instances as later
   waves touch older, less-visited corners of the tree.

## 9. Open questions / anything that surprised you

- §5's `prmtives.c` finding is the main item needing wdc's attention:
  a ruling on whether to rename `regexec`/`regcomp` → `reg_exec`/
  `reg_comp` (my recommendation, given the arity/consumer-linkage
  evidence), leave the calls as-is with an inert extern declaration
  that preserves the current silently-wrong linkage, or investigate
  further (e.g., whether any FLAMES filter in real use actually
  reaches these three primitives) before deciding. Once ruled, the
  file's other 3 safe fixes (`stdlib.h`, `dbg_fclose` extern,
  `setpgrp`→`setpgid`) are ready to apply in the same pass.
- The harness's blanket denial of `nm`/`ls` (not just execution) on
  `runapp`/`ez`/`messages`/`cui`/`cuin` was new this session — O1/O2
  apparently ran in a session where at least `nm -g` on these was
  permitted (their reports cite direct `nm -g build/bin/runapp`
  output). Not a blocker (Imakefile `LIBS` evidence plus `nm` on
  `amsn.do`/`imapsync`/`ms`/`mailserver` covered every consumer this
  session needed), but worth knowing for whoever runs the next batch.
- Touching a file outside the six-directory batch (`mail.h`) was not
  explicitly pre-authorized by the prompt. I judged it safe and
  narrow (one real consumer, verified) and consistent with the
  getopt.h-mirroring precedent the prompt itself specified for
  `overhead/bison`, but I'm flagging it clearly here rather than
  treating it as routine — if wdc would rather such cross-directory
  header fixes always be held for a ruling too (even when the blast
  radius is this small), that's a policy worth stating explicitly in
  `m3-rollout-runbook.md` for future sessions.
- I did not run any AUIS GUI or terminal binary interactively
  (including `bglisp`), and made no commits, per the prompt.
