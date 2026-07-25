# M2 rollout point 4b (bucket 4, session 2): `overhead/mail/metamail/metamail`

## 1. Status

Stopped at the (only) gate — subtree-local `make clean && make depend
&& make -k install`, zero real errors, confirmed twice more for
determinism, per the runbook's "Gate scope" ruling (tree-wide gate NOT
required for this directory). No commits made. Working tree: 8 files
edited (`Imakefile` + 7 `.c` files). Session diff at tree root:
`m2-metamail-session.diff` (265 lines). Full gate log at the canonical
path `~/src/AUIS/andrew-6.4/dependInstall.log` (overwritten across
this session's several build passes; the final two passes are both
clean, zero `error:` lines).

## 2. What I did, in order

1. Read `sonnet-playbook.md`, `rollout-procedure.md` (including the
   "Command style" section and the "Logging" section's
   `malloc`/`realloc`/`free`/`calloc` clang-builtin note),
   `m2-rollout-runbook.md`, `m2-metamail-prompt.md`, and all five
   prior rollout reports (`m2-pilot-eq-REPORT.md`,
   `m2-batch2-REPORT.md`, `m2-batch3a-REPORT.md`,
   `m2-batch3b-REPORT.md`, `m2-utillib-REPORT.md`) in full before
   starting.
2. Checked `ToolSearch` for `Grep`/`Glob` before starting real work —
   neither resolved (same as the `overhead/util/lib` session). Used
   `Read` plus individual `Bash grep` calls throughout, one
   pattern/file per call, never a loop, never chained/piped beyond one
   `tee` slip early on (noted in §11).
3. Checked `src/overhead/mail/metamail/metamail/Imakefile` for
   `Parser()`/`LexFile` — none present, confirmed by reading the
   Imakefile directly. No generated-source gap to work around.
4. Flagged the Imakefile with the runbook's exact four-flag
   `COMPILERFLAGS` override, verified via `make Makefile` + `grep -n
   COMPILERFLAGS Makefile` (override at line 292 wins over
   `system.mcr`'s default at line 177).
5. Fix-surfacing pass: `make clean`, `make depend`, `make -k install
   CDEBUGFLAGS="-ferror-limit=0 -g -O0"` (raised limit used from the
   start per the prompt's instruction) — each a separate Bash call.
   Result: **338 real errors**, far above the stale census's 70 (see
   §3).
6. Fixed fallout in two rounds (see §5 for the process lesson): an
   initial pass covering standard-library headers and the small
   cross-file "no header anywhere" family, then a second and third
   correction round after rebuilds kept surfacing more errors — I had
   initially under-declared each file's own large same-file
   forward-reference set. Investigated every project-local function's
   real definition via `grep`/`Read` before writing any declaration.
7. Rebuilt repeatedly until 0 errors, then ran two full
   `make clean && make depend && make -k install` cycles back-to-back
   to confirm determinism (both exit clean, both install all four
   programs).
8. Checked gate-scope doubt via `nm -g` (one target, not
   piped/chained — see §6): confirmed none of this directory's own
   function names appear in `runapp`'s symbol table, and structurally
   this directory produces only `ProgramTarget`s (no `LibraryTarget`),
   so nothing here could be statically archived into another
   directory's link line in the first place.
9. Identified real runtime consumers via source-level `grep` for
   subprocess-invocation strings, not assumed (§9).
10. `fossil status` (§10) confirms exactly the 8 files touched. No
    commit made. `fossil diff > m2-metamail-session.diff` written to
    the tree root.

## 3. Real instance count: 338 (vs. the stale table's 70)

The stale census (70) was not close to the real count here — a volume
surprise similar in kind to batch B's `ams/libs/cui` (350 actual vs. a
much smaller stale bucket), though this directory was individually
listed in the table (not lumped into the "~60 more" bucket) and still
missed by nearly 5x. Per-file breakdown, confirmed via `grep -c
"^<file>.c:.*error:"` on the fix-surfacing pass's log (one call per
file, summing to 338 exactly):

| File | Real instances |
|---|---|
| `metamail.c` | 169 |
| `mailto.c` | 137 |
| `splitmail.c` | 10 |
| `codes.c` | 8 |
| `shared.c` | 6 |
| `mmencode.c` | 5 |
| `uue.c` | 3 |
| **Total** | **338** |

`metamail.c` and `mailto.c` are each large (~2600-line), single-file,
K&R-era standalone programs — most of their fallout (a majority of
both files' counts) is **same-file forward reference**: dozens of
static-linkage-style helper functions defined after `main()`, called
throughout the file before their own definition, with the file never
having declared any of them. This is a real, if unsurprising,
explanation for the volume gap: the stale 70 estimate presumably came
from an early, non-`-k` or truncated pass (the same undercounting
mechanism `rollout-procedure.md`'s Logging section already documents)
that caught only the standard-library misses and missed the much
larger same-file population.

## 4. Per-file taxonomy summary

No new taxonomy category. All three established categories are
represented; see §7 for one shape worth a closer look (a same-named,
unrelated duplicate function) and §8 for the volume/distribution
observation about same-file forward reference. Full per-call-site
tables are omitted for space (338 rows) per the precedent set by
batch B's `ams/libs/cui` report — grouped by taxonomy and file below,
every unique function name resolved to a real definition before any
declaration was written.

### `metamail.c` — 169

- **Missing standard-library header** (majority-share): `strlen`,
  `strcpy`, `strcmp`, `strncmp`, `strcat`, `strncpy` (`<string.h>`);
  `free`, `exit`, `atoi`, `system`, `putenv`, `mktemp` (`<stdlib.h>`;
  `mktemp` confirmed via direct SDK-header grep to live in
  `<unistd.h>` on this platform, not `<stdlib.h>` — see §4 note
  below); `getpid`, `getuid`, `geteuid`, `close`, `dup`, `dup2`,
  `unlink`, `access`, `isatty` (`<unistd.h>`).
- **Same-file forward reference** (largest single category by
  instance count in this file): ~34 distinct helper functions defined
  later in the file, called earlier — `ExitWithError`,
  `RestoreTtyState`, `ProcessArguments`, `HandleMessage`,
  `PauseForUser`, `Read822Prefix`, `PrepareMessage`,
  `ProcessMailcapFiles`, `SaveSquirrelFile`, `MkTmpFileName`,
  `TryBuiltIns`, `TranslateInputToOutput`, `usage`,
  `StripTrailingSpace`, `RunInNewWindow`, `CreateNewWindowPrefix`,
  `SetUpEnvironment`, `ProcessMailcapFile`, `TryMailcapEntry`,
  `GetMailcapEntry`, `CtypeMatch`, `PassesTest`,
  `ExecuteMailcapEntry`, `BuildCommand`, `SaveTtyState`,
  `NeedToAskBeforeExecuting`, `OKToRun`, `strcatquoting`,
  `WriteTmpFile`, `ExecuteCommand`, `maybephead`, `phead`,
  `EliminateNastyChars`, `strcpynoquotes`, `StartRawStdin`. Fix: one
  file-scope `extern int Foo(), Bar(), ...;` block (untyped K&R,
  matching every real definition's implicit-int return, confirmed
  individually via `grep -B1` against each definition — none had an
  explicit return-type prefix).
- **Missing in-tree/project header, no header anywhere (sub-case 3)**:
  `from64`, `fromqp` (`codes.c`), `fromuue` (`uue.c`),
  `PendingBoundary` (`codes.c`), `ExceptionalNewline`,
  `DoesNeedPortableNewlines` (`shared.c`), `lc2strcmp`, `lc2strncmp`
  (`shared.c`) — all cross-file, same-directory, no header declares
  any of them anywhere in the tree. Fix: local `extern int` block,
  same style.

### `mailto.c` — 137

Same three categories, same shape:
- **Missing standard-library header**: `strcmp`, `strcat`, `strncmp`
  (`<string.h>`); `exit`, `atoi` (`<stdlib.h>`); `getuid`, `getpid`,
  `unlink`, `access` (`<unistd.h>`, previously gated behind
  `#ifdef SYSV` — see §6); `time` (`<time.h>`, previously only
  included under `#ifdef AMIGA`); `tgetent`, `tgetstr`, `tgetnum`,
  `tputs` (`<termcap.h>`, confirmed present on this SDK and already
  the exact pattern used by the sibling directory
  `overhead/mail/metamail/richmail`, per `m2-batch3b-REPORT.md`).
- **Same-file forward reference** (largest category again): ~33
  helper functions — `nomemabort`, `tfputs`, `cleanexit`,
  `ProcessInitFiles`, `InitSignals`, `FputsQuotingLT`,
  `TryClosingStyle`, `TryOpeningStyle`, `TempCloseStyles`,
  `ReopenStyles`, `ToggleStyle`, `TranslateInputToEncodedOutput`,
  `EditCurrentMessage`, `EndStyle`, `StartStyle`, `WriteOutMessage`,
  `RestoreCurrentStyles`, `WriteDeadLetter`, `SwitchToEuropean`,
  `EmitHeader`, `EmitHeaderWithAliases`, `WriteContentTypeAndEncoding`,
  `WriteCtypeNicely`, `finalize`, `WhichEncodingForFile`,
  `SetTextFlags`, `ContainsEightBitChar`, `ProcessOneMailRC`,
  `HandleAliasCommand`, `HandleSetCommand`, `HeaderFputs`, `PutQP`,
  `EmitAddresses`, `DeAlias`, `fputsquoting`. **Plus two names that
  look cross-file but are not**: `mailto.c` defines its *own* local
  `ProcessMailcapFiles`/`BuildCommand`, unrelated to `metamail.c`'s
  same-named functions (the two programs are never linked together —
  `MAILTO_OBJS` doesn't include `metamail.o`) — a second-round finding
  documented in §5.
- **Missing in-tree/project header, no header anywhere (sub-case 3)**:
  `to64`, `toqp`, `from64`, `fromqp` (`codes.c`),
  `DoesNeedPortableNewlines`, `ExceptionalNewline`, `lc2strcmp`,
  `lc2strncmp` (`shared.c`).

### `splitmail.c` — 10

- **Missing standard-library header**: `exit`, `malloc`, `atoi`,
  `free` (`<stdlib.h>`).
- **Same-file forward reference**: `ShareThisHeader`, `HandleOnePart`,
  `IllegalContentType` — and, after a correction (§7), `ULstrcmp`
  too.

### `codes.c` — 8

- **Missing standard-library header**: `strncmp`, `strlen`, `strcpy`,
  `strcat`, `strcmp` (`<string.h>`).
- **Same-file forward reference**: `output64chunk` (called at lines
  81/85/87, defined at line 100 in the same file).

### `shared.c` — 6

- **Missing standard-library header**: `realloc`, `strlen`, `strcpy`
  (`<string.h>`/`<stdlib.h>`); `gethostname` (`<unistd.h>`, previously
  gated behind `#ifdef SYSV` — same pattern as `mailto.c`, see §6).
- **Same-file forward reference**: `lc2strcmp`, `lc2strncmp` — used by
  `DoesNeedPortableNewlines` earlier in the same file, defined later
  in the same file. Missed on the first pass (§5).

### `mmencode.c` — 5

- **Missing standard-library header**: `exit` (`<stdlib.h>`).
- **Missing in-tree/project header, no header anywhere (sub-case 3)**:
  `to64`, `from64`, `toqp`, `fromqp` (all defined in the sibling file
  `codes.c`).

### `uue.c` — 3

- **Missing standard-library header**: `strncmp` (`<string.h>`).
- **Missing in-tree/project header, no header anywhere (sub-case 3)**:
  `PendingBoundary` (defined in the sibling file `codes.c`), called
  twice.

**"Possible genuine bug/typo" category: still empty.** Every
unfamiliar-looking name across all 338 instances resolved to a real,
correctly-spelled, `grep`-findable definition (see §7 for the one name
that needed a second look, `ULstrcmp` — real, not a typo, just a
same-named duplicate).

## 5. Process lesson: I initially under-declared the same-file forward-reference population, twice

This is the most consequential thing that happened this session and
belongs in the report even though it isn't a taxonomy finding. On the
first fix pass I added local `extern` blocks only for the small,
easy-to-spot cross-file "no header anywhere" family
(`from64`/`fromqp`/`PendingBoundary`/etc.) and missed that the
*majority* of both `metamail.c`'s and `mailto.c`'s fallout is each
file's own large population of same-file forward references — ~34 and
~33 helper functions respectively, all defined after `main()` and
called before their own definition. Rebuilding after the first "fix"
still showed 210 errors (down from 338, but nowhere near 0), all of
them exactly this same-file shape. A second pass added the bulk
declaration blocks and got the count down to 4, which turned out to be
two more of the same mistake in miniature: `mailto.c` has its own
locally-defined `ProcessMailcapFiles`/`BuildCommand` (same names as
`metamail.c`'s, but a different file, never linked together — each
program independently reimplements similar mailcap logic), and
`shared.c` itself calls `lc2strcmp`/`lc2strncmp` before their own
later definition in the same file. A third, small correction closed
those and reached 0.

**Why this happened**: in every prior M2 directory, same-file forward
reference was a minority category (a handful of instances at most —
`eqview_Format` in the pilot, a few functions in `atk/frame`/
`overhead/fonts/cmd`/`atk/raster/cmd`). Here it's the *dominant*
category by volume in the two largest files, because both are
old-style, single-file, top-down-then-helpers-below K&R programs (the
`main()`/argument-parsing logic sits at the top, dozens of small
helpers follow). Worth flagging as a generalizable lesson for any
future M2 directory containing a large single-file program: census the
same-file forward-reference population as thoroughly as the
cross-file and standard-library ones on the *first* pass, rather than
treating it as the smaller, later-pass category the way smaller
directories have trained the workflow to expect.

## 6. Two `#ifdef SYSV`-gated `<unistd.h>` misses (mechanical, not a taxonomy category)

Both `mailto.c` and `shared.c` already had `#include <unistd.h>`
gated behind `#ifdef SYSV` (with an explanatory comment in `mailto.c`:
"Different people say different things about whether unistd.h lives in
sys/"). `SYSV` is not defined on this Darwin build, so the guard
always failed and `unistd.h` was never actually included — the same
general shape as batch A/B's stale-declaration-becomes-a-conflict
findings, just "the right header is already there, just unreachable"
instead of "the right header is missing." Fixed by adding an
unconditional `#include <unistd.h>` in both files (leaving the
`SYSV`-gated block's own comment/structure alone, since it's
conditionally dead code for other platforms this build doesn't
target, not something to clean up as part of this fallout fix).

## 7. `ULstrcmp`: a same-named, unrelated duplicate function — not a typo, a new observation

`splitmail.c` calls `ULstrcmp` three times (case-insensitive string
compare, used to match header field names). `overhead/util/hdrs/
util.h` also declares an `ULstrcmp` (`extern int ULstrcmp();`,
defined in `overhead/util/lib/ulstrcmp.c`) — so my first fix added
`#include <util.h>` to `splitmail.c`, treating this as ordinary
sub-case 1 ("header exists, declares it, just isn't included"). The
build succeeded either way (both declarations are untyped K&R, so
there's no hard type conflict), but a closer look at the full build
log's deprecation-warning notes showed `splitmail.c` **itself defines
its own separate, explicitly-typed `int ULstrcmp(s1, s2)` at line
433** — a private, case-insensitive string-compare helper, unrelated
to `overhead/util/lib`'s function of the same name and behavior.
Confirmed this is the one that actually gets linked:
`splitmail`'s `ProgramTarget` doesn't link `$(UTILLIB)`
(`ProgramTarget(splitmail, $(SPLITMAIL_OBJS),,)` — no library
argument), so `overhead/util/lib`'s `ULstrcmp` was never reachable
here regardless of the header. Corrected the fix: removed the
`#include <util.h>`, added `ULstrcmp` to `splitmail.c`'s own
same-file forward-declare block instead, with a comment noting the
non-relationship to the tree-wide `ULstrcmp`. Rebuilt clean twice
after the correction (§1).

This isn't the "possible genuine bug/typo" hard-stop category (both
functions are real, correctly spelled, and do the same job by
design — this reads like two authors independently writing "uppercase
compare" helpers, not a mistake), but it's worth naming as its own
small finding: **a same-named function can exist in two unrelated
places in the tree, one of which is coincidentally declared by a
header that would otherwise look like the obvious fix** — check
whether a file actually links the library that would make the outside
header's declaration meaningful before assuming sub-case 1 applies,
not just whether the name and rough signature match.

## 8. `malloc`/`realloc`/`free`/`calloc` blind-spot check — explicitly performed, no hidden instances found

Per the prompt's instruction, grepped all 7 files individually for
bare `malloc(`/`free(`/`realloc(`/`calloc(` call sites before
finalizing (one `grep` call per file). Every call site found was
already present in the original 338-error census — no additional,
census-invisible instances turned up this session (unlike
`overhead/util/lib`'s `svcconf.c` near-miss). `metamail.c`,
`mailto.c`, and `shared.c` all had a pre-existing (wrong-typed)
`extern char *malloc();`/`extern char *realloc();` before this
session's edits, which is consistent with why `malloc` itself mostly
didn't appear as an *error* in those files (a wrong-typed declaration
already existed, so it wasn't "implicit") while `free`/`realloc` (no
matching pre-existing extern in some files) did appear as real errors
in the census — i.e., this directory's malloc-family calls were
already visible to the `-k` census either via a real error or via
their own stale extern, not via the invisible-clang-builtin path
`overhead/util/lib` found. All such stale externs were removed once
`<stdlib.h>` was added, per the established precedent (batch B,
`overhead/util/lib`).

## 9. Runtime consumers (prompt's explicit ask) — identified via source-level `grep`, not `nm -g`

This directory builds four `ProgramTarget`s, no `LibraryTarget` — so
`nm -g`/Imakefile `LIBS` lines (the tool named in the prompt) don't
apply the way they did for library directories; the equivalent
empirical check for standalone programs invoking each other as
subprocesses is grepping the tree for the command name inside
`sprintf(Cmd, ...)`/`system()`/`popen()` call sites, which is what I
did (one `grep` call per binary name, across `src/ams/`,
`src/atkams/`, and the metamail directory itself):

- **`metamail`**: standalone CLI **and** invoked as a subprocess by
  three real mail clients, confirmed by exact call sites:
  - `atkams/messages/lib/mailobj.c:262`:
    `sprintf(Cmd, "metamail -m messages -z -x -d -q %s 2>&1", ...)` —
    the GUI `messages` app's own MIME-attachment-viewing path.
  - `ams/msclients/cui/cui.c:1285`:
    `sprintf(Cmd, "metamail -m cui %s %s %s", ...)` — the terminal
    `cui` client's attachment-viewing path.
  - `ams/msclients/vui/vuipnl.c:2436`:
    `sprintf(Cmd, "reset; metamail -m vui -p %s %s", ...)` — the
    `vui` client's attachment-viewing path.
  This makes `metamail` the load-bearing check: it's the actual
  runtime attachment-viewer backend for every mail client in the tree
  that can view MIME attachments, not just a manually-invoked tool.
- **`mailto`**: standalone CLI, user-invoked directly (its own help
  doc describes it as a Berkeley-mail-alike composer; no other
  program in the tree invokes it as a subprocess, confirmed by `grep`
  finding zero hits outside its own directory/docs). It in turn
  invokes **`splitmail`** internally via `popen(CmdBuf, "w")` at
  `mailto.c:1038` (`CmdBuf` built from `sprintf(CmdBuf, "splitmail -d
  -s %d", SplitSize)` two lines above) to actually split and deliver
  the composed message.
- **`splitmail`**: has its own standalone CLI entry point (built and
  installed as a `ProgramTarget` like the others) but its real-world
  use is almost entirely as `mailto`'s delivery helper (§ above) — no
  other invoker found.
- **`mmencode`**: standalone CLI utility only (MIME base64/
  quoted-printable encode-decode, analogous to `uuencode`/`base64`).
  `grep`-confirmed zero invocations from anywhere else in the tree —
  `atkams/messages/doc/mime.help` mentions it only as prose in a help
  topic, not a code reference.

### Exact runtime-check commands for wdc

Run from native Terminal.app (not an IDE terminal). None of these are
GUI/X11 programs except via their callers (`messages`/`cui`/`vui`),
so most can be run directly:

1. **`metamail` (highest-value check — exercises the largest same-file
   forward-reference block, the biggest fix in this session)**:
   ```
   printf 'Content-Type: text/plain\n\nHello from metamail.\n' | /Users/wdc/src/AUIS/andrew-6.4/build/bin/metamail
   ```
   Confirm it exits 0 and prints "Hello from metamail." to the
   terminal (plain `text/plain` bodies print directly). For the real
   load-bearing path, start a **fresh** `messages` process (`.do`
   files/subprocess behavior aside, `metamail` itself is re-exec'd
   fresh every time, but use a fresh `messages` regardless to rule out
   any other stale state) and open any message with a non-text MIME
   attachment (an image or `application/octet-stream` part, if one is
   on hand in a test mailbox):
   ```
   DISPLAY=:0; messages
   ```
   Open the attachment; confirm `metamail` launches (its own
   configured viewer for that content type opens, or a prompt to save
   appears if no viewer is configured) with no crash and no stray
   error text. `ams/msclients/cui/cuin` (the terminal client) is a
   second, independent exercise of the same binary if convenient:
   ```
   /Users/wdc/src/AUIS/andrew-6.4/build/bin/cuin
   ```
   View a message with an attachment there too.

2. **`mailto`**:
   ```
   /Users/wdc/src/AUIS/andrew-6.4/build/bin/mailto -s "M2 metamail test" wdc@fastmail.com
   ```
   Type a short line of body text, then `~.` (or Ctrl-D) to send, per
   `sonnet-playbook.md`'s hard rule (test emails go only to
   `wdc@fastmail.com`). This exercises both the ~34-function same-file
   forward-reference block directly (the compose/send path) and the
   `splitmail` subprocess invocation (`mailto.c:1038`) as a real
   `popen` call — confirm the send completes without a "Can't write to
   splitmail" error.

3. **`mmencode`** (no dedicated GUI/subprocess path — direct CLI
   check):
   ```
   printf 'Hello from mmencode.\n' | /Users/wdc/src/AUIS/andrew-6.4/build/bin/mmencode
   ```
   Confirm it prints a base64 string, then round-trip it:
   ```
   printf 'Hello from mmencode.\n' | /Users/wdc/src/AUIS/andrew-6.4/build/bin/mmencode | /Users/wdc/src/AUIS/andrew-6.4/build/bin/mmencode -u
   ```
   Confirm the second command reproduces "Hello from mmencode."
   exactly — this exercises `to64`/`from64` (both touched this
   session, cross-file `codes.c` externs).

4. **`splitmail`** (already exercised indirectly via `mailto` above;
   direct standalone check):
   ```
   printf 'To: wdc@fastmail.com\nSubject: M2 splitmail test\n\nBody text.\n' | /Users/wdc/src/AUIS/andrew-6.4/build/bin/splitmail -d -s 10000
   ```
   Confirm it exits 0 with no error to stderr (a message well under
   the split size should pass through as a single part to its
   delivery step).

## 10. Files touched (compile status)

All 8 edited files compile clean (0 real
`-Werror=implicit-function-declaration` errors) in their final state,
confirmed by two full `make clean && make depend && make -k install`
passes back-to-back after the last correction (§1/§7), both installing
all four programs (`metamail`, `mmencode`, `mailto`, `splitmail`) to
`build/bin`.

- `src/overhead/mail/metamail/metamail/Imakefile` — `COMPILERFLAGS`
  override added.
- `metamail.c` — `<string.h>`/`<stdlib.h>`/`<unistd.h>` added; stale
  `extern char *malloc(); extern char *realloc();` removed; local
  externs added for the same-file forward-reference block (~34
  functions) and the cross-file no-header family (`from64`/`fromqp`/
  `fromuue`/`PendingBoundary`/`ExceptionalNewline`/
  `DoesNeedPortableNewlines`/`lc2strcmp`/`lc2strncmp`).
- `mailto.c` — `<string.h>`/`<stdlib.h>`/`<time.h>`/`<termcap.h>`
  added; the `#ifdef SYSV`-gated `<unistd.h>` supplemented with an
  unconditional include; stale `malloc`/`realloc` externs removed from
  the existing `extern char *malloc(), *realloc(), *index(),
  *getmyname();` line; local externs added for the same-file
  forward-reference block (~35 functions, including its own local
  `ProcessMailcapFiles`/`BuildCommand`) and the cross-file no-header
  family (`to64`/`toqp`/`from64`/`fromqp`/
  `DoesNeedPortableNewlines`/`ExceptionalNewline`/`lc2strcmp`/
  `lc2strncmp`).
- `splitmail.c` — `<stdlib.h>` added; local externs added for
  `ShareThisHeader`/`HandleOnePart`/`IllegalContentType`/`ULstrcmp`
  (same-file forward references — see §7 for the `ULstrcmp`
  correction).
- `codes.c` — `<string.h>` added; local extern added for
  `output64chunk` (same-file forward reference).
- `shared.c` — `<string.h>`/`<stdlib.h>` added; the `#ifdef
  SYSV`-gated `<unistd.h>` made unconditional; stale `extern char
  *malloc();` removed; local extern added for `lc2strcmp`/
  `lc2strncmp` (same-file forward reference, missed on the first
  pass — §5).
- `mmencode.c` — `<stdlib.h>` added; local externs added for `to64`/
  `from64`/`toqp`/`fromqp` (cross-file, `codes.c`).
- `uue.c` — `<string.h>` added; local extern added for
  `PendingBoundary` (cross-file, `codes.c`).

Regenerated `Makefile` is not fossil-tracked, not a source edit.

```
$ fossil status
repository:   /Users/wdc/museum/auis.fossil
local-root:   /Users/wdc/src/AUIS/andrew-6.4/
config-db:    /Users/wdc/.fossil
checkout:     1b8a12853b053b0a1ae1b9559de923d619f4e934 2026-07-24 23:52:11 UTC
parent:       ad896120561575c5890d162913ec695c0f0fce14 2026-07-24 23:48:48 UTC
tags:         andrew-6.4
comment:      docs: M2 rollout 4a complete (overhead/util/lib, 74 instances); malloc-builtin census blind spot; command-style guidance corrections (user: wdc)
EDITED     src/overhead/mail/metamail/metamail/Imakefile
EDITED     src/overhead/mail/metamail/metamail/codes.c
EDITED     src/overhead/mail/metamail/metamail/mailto.c
EDITED     src/overhead/mail/metamail/metamail/metamail.c
EDITED     src/overhead/mail/metamail/metamail/mmencode.c
EDITED     src/overhead/mail/metamail/metamail/shared.c
EDITED     src/overhead/mail/metamail/metamail/splitmail.c
EDITED     src/overhead/mail/metamail/metamail/uue.c
```

Exactly 8 files, matching this section's list above. No commit made.
`m2-metamail-session.diff` (265 lines: 78 added, 17 removed) written
to the tree root.

## 11. Gate scope — no doubt found, and a structurally stronger argument than usual

The prompt asked to flag anything that makes the "subtree-local gate
only" ruling doubtful for this directory. **Nothing does — if
anything, this directory's own structure makes the case more strongly
than any prior data point.** All four data points behind the existing
ruling (`overhead/cmenu`, `atk/basics/x`/`atk/basics/common`,
`ams/libs/cui`, `overhead/util/lib`) are `LibraryTarget`s (`.a`
archives) that *could* in principle have been linked into something
else, and were empirically confirmed not to be via `nm -g`. This
directory is different in kind: its Imakefile defines **only
`ProgramTarget`s** (`metamail`, `mmencode`, `mailto`, `splitmail`), no
`LibraryTarget` at all — confirmed by reading the Imakefile directly
(§2 step 3's grep already showed `ProgramTarget(...)` ×4, no
`LibraryTarget`). A `ProgramTarget`'s object files are linked directly
into a final executable, never archived into a `.a` another directory
could pull in, so it is **structurally impossible**, not just
empirically unconfirmed, for this directory's fallout to cross into
another directory's build the way a library's could. Confirmed via
`nm -g build/bin/runapp` (run as `cd
/Users/wdc/src/AUIS/andrew-6.4 && nm -g build/bin/runapp`, the
carve-out shape the `overhead/util/lib` session found avoids the
piped/chained-command denial — see §12) that none of this directory's
distinctive function names (`ExitWithError`, `ProcessMailcapFiles`,
`HandleOnePart`, `ShareThisHeader`) appear in `runapp`'s symbol table,
as expected. Did not run the tree-wide gate.

## 12. Command-style discipline — how it went this session

Followed the same discipline as prior sessions: every `make clean`/
`make depend`/`make -k install` issued as its own Bash call, one
`grep`/`Read` per file/pattern, no `for`/`while` loops, no
multi-command chains beyond one early mistake.

- **One self-inflicted slip, caught immediately**: the very first
  fix-surfacing build was issued as `make -k install ... | tee
  <logpath>` — a pipe, against the discipline's own explicit warning.
  It went through without a denial or prompt (unlike
  `overhead/util/lib`'s two denied `nm` pipes), so this may be
  build-output piping into `tee` specifically behaving differently
  from piping into `grep`/redirection-plus-chaining, or simply
  inconsistent per-session denial behavior — not reissued as a
  pipeline again this session, switched to plain `> logfile 2>&1`
  redirection (a single redirect, not a pipe or chain) for every
  subsequent build call, which never triggered a prompt or denial.
- **Two more denial data points, both `nm`**: `nm -g
  build/bin/runapp 2>/dev/null | grep -c "..."` (piped) and `nm -g
  ... > /tmp/x 2>&1` (redirected) were both denied outright with no
  prompt — consistent with `overhead/util/lib`'s finding that an
  unmatched compound `nm` command gets silently denied in this
  non-interactive mode rather than prompted. Reissued as `cd
  /Users/wdc/src/AUIS/andrew-6.4 && nm -g build/bin/runapp` (the exact
  carve-out shape that session found) with no redirection at all,
  which went through immediately — confirming that specific shape (`cd
  <tree-root> && nm -g build/bin/<target>`, no pipe, no redirect) is
  reliably allow-listed, while anything piped or redirected after it
  is not, regardless of how simple the rest of the command is.
- No other prompts or denials across the rest of the session (~140
  tool calls: ~45 `Read`, ~55 `Bash grep`, ~15 `Edit`, ~20 build-step
  `Bash` calls, plus `fossil status`/`diff`, one `nm`, `ls`, `wc`).
  Net: **3 denials this session** (the `tee`-piped build did *not*
  deny, only the two `nm` compounds did), all self-corrected on the
  next attempt, consistent with — and slightly refining — the
  `overhead/util/lib` session's findings: it isn't that all piping
  denies uniformly; `tee` after a long-running build command went
  through, while `nm`'s pipe/redirect did not. Not enough data to
  generalize further than "when a compound command is denied, drop the
  pipe/redirect and reissue as the simplest possible single command";
  flagging the `tee` exception in case it's useful signal for a future
  session.

## 13. Open questions / anything that surprised you

- §5's process lesson (initially under-declaring the same-file
  forward-reference population, twice) is the main thing worth
  carrying forward to any future M2 directory containing a large
  single-file K&R program: census and declare that category as
  thoroughly as the cross-file/standard-library ones on the *first*
  pass, don't let smaller directories' "same-file forward reference is
  a minority category" pattern set an expectation that turns into a
  blind spot.
- §7's `ULstrcmp` finding (two unrelated, same-named functions, one of
  which is coincidentally declared by an outside header that looks
  like the obvious fix) is a new wrinkle on sub-case 1 worth folding
  into the runbook's taxonomy text if it recurs: check whether the
  file actually links the library backing the outside header's
  declaration before assuming sub-case 1 applies.
- The volume gap (338 real vs. 70 stale) is large enough to be worth a
  runbook correction if `overhead/mail/metamail/metamail`'s stale
  count is referenced anywhere else — it was individually listed in
  the table (not a "~60 more directories" bucket entry), so unlike
  batch B's `ams/libs/cui`/`ams/msclients/nns` surprises, this one
  wasn't flagged in advance as an estimate to distrust.
- No hard stops triggered: no called-but-undefined function anywhere
  (every name resolved to a real, correctly-spelled definition — see
  §7 for the one that needed a second look), no header-vs-usage type
  disagreement requiring escalation, and every fix stayed inside this
  directory's own `.c`/`.h` files (no `.h` files exist in this
  directory at all — confirmed by `ls *.h` returning nothing — so
  every project-local fix was necessarily a local `extern`, never a
  header edit).
- I did not run any AUIS GUI or binary interactively, and made no
  commits, per the prompt.
