# M4 Wave 7, Batch C3 (former C3+C4) STRICT_COMPILERFLAGS — REPORT

## 1. Status

**Stopped at Gate 1** (the task's only/final gate — none of C3's 9
directories are one of the two elevated-risk batches `m4-batches.md`
names, `C1`/`contrib/zip/lib` and `AMS2`/`atkams/messages/lib`, so only
the subtree-local gate was required for each). All 9 directories build
clean under `$(STRICT_COMPILERFLAGS)` — zero real `error:` lines in
each, confirmed across two separate `clean && depend && install`
cycles per directory (one with `-ferror-limit=0 -g -O0` for full-census
fixing, one more from a fresh `make clean` at the directory's normal
flags for determinism). **No fossil commit made.**
`fossil diff > c3-session.diff` (tree root, 1003 lines) has the full
diff. 34 files touched (9 `Imakefile`s + 25 `.c` files).

## 2. What I did, in order

1. Read `revival/doc/sonnet-playbook.md` and
   `revival/doc/m4-rollout-runbook.md` in full, then
   `revival/doc/m4-batches.md`'s Wave 7 section for C3's directory list
   and census (67 total across 9 directories). Confirmed
   `STRICT_COMPILERFLAGS` is already defined in
   `src/config/darwin/system.mcr` (Batch 0 already landed) and that the
   working tree started clean (`fossil status` — only the parent
   checkout's docs commit, nothing pending).
2. For each of the 9 directories, in ascending-census order
   (`circlepi` → `srctext/html` → `srctext/ltext` → `mit/util` →
   `contrib/time` → `contrib/calc` → `eatmail` → `mit/annot` →
   `srctext/ptext`):
   - Checked the Imakefile for an existing `COMPILERFLAGS` line — none
     of the 9 had one (only `CLASSFLAGS = $(CLASSINCLUDES) -pe` in the
     ATK-inset directories, nothing at all in `eatmail`). Added
     `COMPILERFLAGS = $(STRICT_COMPILERFLAGS)` fresh in each case —
     nothing to replace, nothing stacked.
   - Regenerated the Makefile via `touch -m -t <far-future> Imakefile
     && make Makefile` (the tree's own `IMAKE_CMD` convention, not
     plain `xmkmf` — confirmed by grepping the regenerated Makefile for
     the last `COMPILERFLAGS =` assignment each time, always the
     `$(STRICT_COMPILERFLAGS)` line winning over `system.mcr`'s still-
     suppressed default).
   - `make clean && make depend` (benign `makedepend` "cannot find
     include file <system header>" noise only, every directory), then
     `make -k install CDEBUGFLAGS="-ferror-limit=0 -g -O0"` for the
     full-census dry run.
   - Fixed every real error per the fix policy in the task brief and
     `m4-rollout-runbook.md`'s "Fix policy for
     incompatible-function-pointer-types" section (detail in §3).
   - Re-ran `make -k install` with the debug flags until 0 errors, then
     a full `make clean && make depend && make -k install` cycle at
     normal flags to confirm the gate holds without the debug overrides
     (all 9 directories: 0 errors both times).
   - Grepped tree-wide for consumers of each directory's build products
     (`grep -rln` across all `Imakefile`s for the `.do`/`.a`/binary
     names) to determine downstream-relink need (§5).
3. Wrote this report and captured `fossil diff > c3-session.diff` at
   the tree root.

## 3. Findings — per-directory error counts and fixes

Real counts came in above census in 8 of 9 directories (same direction
as every prior M4 batch, expected per the runbook's `-Werror=format`
undercount warning), one landed exactly on census:

| Directory | Census | Real | Breakdown |
|---|---|---|---|
| `contrib/demos/circlepi` | 1 | 1 | 1 function-pointer |
| `contrib/srctext/html` | 1 | 2 | 1 format-security, 1 implicit-function-decl |
| `contrib/srctext/ltext` | 3 | 3 | 3 function-pointer (2 `procedure`-slot, 1 bind-table) |
| `contrib/mit/util` | 5 | 24 | 1 implicit-function-decl, 4 function-pointer (`procedure`-slot), 19 format |
| `contrib/time` | 8 | 10 | 8 function-pointer (`procedure`-slot), 2 format |
| `contrib/calc` | 8 | 8 | 4 implicit-int, 4 function-pointer (`procedure`-slot) |
| `contrib/eatmail` | 11 | 12 | 11 implicit-function-decl (+1 format that resolved itself once the real prototype landed) |
| `contrib/mit/annot` | 15 | 24 | 5 implicit-function-decl, 4 implicit-int, 6 function-pointer (`procedure`-slot), 9 format |
| `contrib/srctext/ptext` | 15 | 15 | 1 implicit-int, 1 implicit-function-decl, 1 function-pointer (`procedure`-slot), 12 function-pointer (bind-table) |
| **Total** | **67** | **99** | |

### `procedure`/`bind_Description.proc` function-pointer mismatches (the majority shape)

Every `incompatible-function-pointer-types` error in this batch was one
of two already-established tree-wide idioms — no new bare-cast
judgment calls, nothing escalated:

- **`proctable_DefineProc`/`im_EnqueueEvent`/`tree23int_Apply`-style
  calls**, whose real declared parameter type is `procedure` (`int
  (*)()`, `proctbl.ch`/`im.ch`/`tree23.ch`). Fixed with `(procedure)`
  casts at the call site — the same idiom used tree-wide (e.g.
  `atk/basics/common/bind.c`'s `(procedure) bl->proc`). Sites:
  `circpiv.c` (`LimitProc`), `ltext.c` (`DoFreeTree`, `addindent`),
  `vutils.c` (`helpDeath`), `popts.c` (`DestroyWindow`, x2),
  `headrtv.c` (`headrtv_MoveOn`), `timeoday.c`/`clock.c`
  (`UpdateTime`), `timeodayv.c`/`writestmpv.c` (`MenuSetFormat`),
  `clockv.c` (`MenuSetShape`/`MenuSetLabels`/`MenuSetTicks`/
  `MenuSetSeconds`), `calc.c`/`calcv.c`
  (`ReadObject`/`WriteObject`/`PrintObject`/`Stroke`), `noteview.c`/
  `stroffetv.c` (`insert`/`openall`/`closeall`), `ptext.c`
  (`DoFreeTree`).
- **`struct bind_Description.proc`** (`bind.ch`), declared
  `void (*proc)()` — genuinely polymorphic across a single table's
  entries (mixed arities/types by design: no-arg, `(self, char)`,
  `(self, int)`, `(self, long)` all coexist in the same array). Only
  the entries whose second parameter is a bare `char` actually error
  (C's function-type compatibility rule exempts a K&R/unspecified-arg
  function pointer from a prototyped one only when no parameter is
  subject to default argument promotion — `char` is, `int`/`long`
  aren't, which is exactly the split the compiler enforced and why
  `tab`/`newline`/`redo` never errored while `paren`/`space`/`brace`/
  `asterisk` did). Fixed with `(void (*)())` casts, **not a new
  judgment call** — this is the tree's own original-author idiom,
  found unmodified in 1988 code at `atk/frame/framecmd.c:1885` onward
  (`(void (*)())frame_cd` etc., predating this project entirely) and
  already reused by the C1 batch (`zipedit.ch`'s
  `Set_Keyboard_Processor` macro, cast to the field's own declared
  type). Sites: `ltextv.c` (`paren`), `ptextv.c` (`paren`, `space` x6,
  `brace`, `asterisk` x2 — 10 entries in one `bind_Description` array).

Two directories needed a real-type function-pointer fix instead of a
polymorphic-slot cast: `calc.c`'s `Reader`/`Writer` and `calcv.c`'s
`Printer` were missing their `int` return type (`static Reader(...)`,
implicit-int) even though a correctly-typed forward declaration
(`static int Reader();`) already sat above them in the same file —
fixed the definitions to match the existing declaration (same
duplicate-forward-decl-vs-definition shape the task brief called out,
except here the *declaration* was already correct and only the
*definition* was stale, the mirror image of the usual case). The
matching `apt_ReadObject`/`apt_WriteObject`/`aptv_PrintObject` call
sites then got a full-signature cast, `(void (*)(struct calc *))
Reader`, mirroring the identical, already-in-tree pattern at
`atk/chart/chart.c:380`/`:484` for the same `apt` package interface —
confirms this is an established precedent, not a new one.

### Implicit-int (missing return type) — all same-file forward-reference or stale-definition shape

- `contrib/mit/annot/icon.c`'s `check_for_title`, `ps.c`'s
  `check_for_title`, `contrib/mit/util`'s none, `contrib/calc/calcv.c`'s
  `Fill_Area`, `contrib/srctext/ptext/ptext.c`'s `domatch`,
  `contrib/mit/annot/iconview.c`'s `SlayChild`/`AdoptNewChild`: all had
  a correctly-typed `static int Foo();` forward declaration already
  present in the same file, with only the definition itself missing
  the `int` keyword (relying on K&R implicit-int). Fixed each
  definition to state the type its own forward declaration already
  promised — no behavior change, the compiler was already treating
  these as `int`-returning.

### Implicit-function-declaration

- Missing standard-library headers (`#include <string.h>` for
  `strcpy`/`strlen`/`strcmp`, `#include <stdlib.h>` for `exit`):
  `icon.c`, `noteview.c`, `stroffetv.c`, `ptextv.c`, `ez2ascii.c`,
  `eatmail.c`.
- Same-file forward references to a real tree function defined later
  in the same file: `iconview.c`'s `string_width` (used at line 91,
  defined at line 220) and `contrib/mit/util/ez2ascii.c`'s `exit`
  (stdlib). Added a proper prototype matching the real definition, not
  a cast.
- `contrib/eatmail/eatmail.c` is the standout case: 6 genuinely
  undeclared tree functions, all reachable because the file's own
  top-of-file comment (`#include <ms.h>` commented out) and a
  same-file forward-reference to its own later-defined
  `ConvertIncomingMail`. Added full prototypes matching each real
  definition (`CheckAMSConfiguration()` — matches the real definition's
  own K&R empty-parens signature, `overhead/mail/lib/mailconf.c:857`;
  `ConvertIncomingMail(char *, char *, int *)` — matches this file's
  own later definition; `vclose(int)` —
  `overhead/util/lib/vclose.c:154`; `writeall(int, char *, int)` —
  `overhead/util/lib/writeall.c:44`; `ams_genid(int)` —
  `overhead/mail/lib/genid.c:118`; `ParseAddressList(char *,
  PARSED_ADDRESS **)`/`FreeAddressList(PARSED_ADDRESS *)` —
  `overhead/mail/lib/parseadd.c`). One of `eatmail.c`'s format errors
  (`ams_genid(1)` passed to `%s`) resolved itself automatically once
  `ams_genid`'s real `char *` return type was declared — without a
  prototype the compiler had been defaulting it to `int`, which is what
  triggered the `%s`-vs-`int` mismatch in the first place.

### Format (`-Werror=format`)

All were `%d`/`%c` against a real `long` field, checked against the
declared type before fixing, same policy as every prior M4 batch:

- **Cosmetic, debug-flag-gated only** (`contrib/mit/util/popts.c`'s
  `ENTER`/`LEAVE` macros and two `DEBUG()` traces): fixed `%d`→`%ld`
  for `struct rectangle`'s `long` fields and `FullUpdate`'s `long`
  parameters, and cast pointers to `(unsigned long)` for `%lx` (the
  same idiom already used at `atk/raster/cmd/rastvaux.c`,
  `atk/basics/x/xfontd.c`, etc. — `self`/`printopts_GetDrawable(self)`
  are pointers, not integers, so `%lx` needed a cast, not just a wider
  specifier).
- **`contrib/srctext/html/html.c:461`**: `fprintf(stderr, buf)` where
  `buf` is a runtime-built string (via `sprintf` from a caller-supplied
  `msg`) — a real format-string-injection risk, not just a truncation
  bug (if `msg` ever contained a literal `%`, this crashes or leaks
  stack data). Fixed to `fprintf(stderr, "%s", buf)`.
- **`contrib/mit/util/headrtv.c:344`**: `fprintf(fp,"\\*(H%d\n",
  index)` where `index` is `long`, writing a troff macro reference into
  a generated document header/footer template. This is output text a
  downstream `troff`/`ez2ps` run re-parses as a macro invocation, so a
  truncated `index` would silently reference the wrong header-variable
  macro — fixed `%d`→`%ld`. In practice `index` never exceeds
  `hvarcount` (a small count of configured header variables), so this
  is unlikely to have manifested as a real corruption in the field, but
  it is the same class of bug as the datastream id-truncation sweep,
  not purely cosmetic.
- **`contrib/mit/util/timeoday.c`/`writestmp.c`**: `sprintf(buf,
  "%s%d%s", myfontname, myfontsize, type)` where `myfontsize` is
  `long` — builds a synthetic font-name string consumed by
  `fontdesc`/X font lookup. Fixed `%d`→`%ld`.
- **`contrib/mit/annot/psview.c:310` — the one prominent memory-
  corruption find this batch**: `sscanf(bbox_buf, "%%%%BoundingBox: %d
  %d %d %d", &llx, &lly, &urx, &ury)` where `llx`/`lly`/`urx`/`ury` are
  all declared `long`. This is LP64 bug class variant #4 exactly
  (`sonnet-playbook.md` bug-class list, item 2) — `%d` into a `long*`
  writes only the low 32 bits of each variable via `sscanf`, leaving
  the upper 32 bits as whatever garbage was already on the stack (these
  are uninitialized auto variables). The parsed values feed directly
  into `ps_SetPixelWidth`/`ps_SetPixelHeight` (the inset's displayed
  size) and get written back into the document's own text buffer via
  `sprintf(tr_buf, translate, -llx, -lly)` moments later — so a garbage
  upper half could corrupt both the on-screen PostScript inset size and
  the `%%BoundingBox`-derived translate command actually saved into the
  `.ez` file. Fixed `%d`→`%ld` on all four conversions (read side only
  — there is no corresponding write-side `%BoundingBox` emitter in this
  directory to check symmetrically; the bounding box text is itself
  parsed *from* an externally-supplied PostScript file, not generated
  by this code).
- **`contrib/mit/annot/psview.c:435/437`**: `%d` against `width`/
  `height` (`long`, from `ps_GetPixelWidth`/`ps_GetPixelHeight`)
  written into a troff `clip` path macro during `Print` — same
  serialization-path reasoning as `headrtv.c` above, fixed `%ld`.
- **`contrib/mit/annot/psview.c:453`**: `fprintf(file,"%c%s",c,
  prefix)` where `c` is declared `long` (a `text_GetChar` return used
  as a character). `%c` genuinely wants a (promoted) `int`-sized
  value, not a wider specifier, so this got a `(int)c` cast rather than
  a format-string change — the value is always a single character
  (0–255), so the cast is exact, not a truncation.

## 4. Files touched (all compile clean, both gate cycles)

- `src/contrib/demos/circlepi/Imakefile`, `circpiv.c`
- `src/contrib/srctext/html/Imakefile`, `html.c`
- `src/contrib/srctext/ltext/Imakefile`, `ltext.c`, `ltextv.c`
- `src/contrib/mit/util/Imakefile`, `ez2ascii.c`, `vutils.c`,
  `popts.c`, `headrtv.c`
- `src/contrib/time/Imakefile`, `timeoday.c`, `timeodayv.c`,
  `writestmp.c`, `writestmpv.c`, `clock.c`, `clockv.c`
- `src/contrib/calc/Imakefile`, `calc.c`, `calcv.c`
- `src/contrib/eatmail/Imakefile`, `eatmail.c`
- `src/contrib/mit/annot/Imakefile`, `icon.c`, `ps.c`, `noteview.c`,
  `iconview.c`, `stroffetv.c`, `psview.c`
- `src/contrib/srctext/ptext/Imakefile`, `ptext.c`, `ptextv.c`

## 5. Downstream-relink / consumer check

All 9 directories build **`.do` dynamic-load objects only**
(`DynamicObject`/`DynamicMultiObject`/`InstallClassFiles`), except
`contrib/mit/util` (also `ProgramTarget(ez2ascii, ...)`, an installed
standalone binary) and `contrib/eatmail` (`ProgramTarget(eatmail,
...)`, standalone). **None produce a static library** (`.a`,
`LibraryTarget`). Tree-wide grep for each directory's `.do`/binary
names against every other `Imakefile` in the tree found zero real
consumers (one false-positive substring match — `smpltext.do`/
`cltextv.do` in `atk/text/Imakefile` superficially contain
`ltext.do` as a substring but are unrelated files). **No downstream
relink needed for any of the 9 directories** — each is either loaded
dynamically at runtime by `ez`/`help` on demand, or is a standalone
installed program with no in-tree callers.

`contrib/mit/util` and `contrib/demos/circlepi` (whose `.do` outputs
are inset classes loaded on demand) and the `bind_Description`-table
directories (`ltext`, `ptext`, `contrib/time`, `contrib/mit/annot`)
are all loaded the same way — via `class_Load`/menu insertion at
runtime, not linked at build time — so this holds uniformly across the
batch.

## 6. Open questions / anything that surprised me

- **`contrib/calc`'s known ghost-text rendering bug was not touched.**
  Per the task brief, `calc.c`/`calcv.c`'s fixes were strictly the 8
  compiler-error-driven changes above (4 implicit-int, 4
  function-pointer); nothing in this batch's diff touches drawing,
  erase, or anti-aliasing code. No new observations relevant to that
  bug surfaced during this pass.
- **Real counts ran 0–92% over census** across the 9 directories (most
  extreme: `contrib/mit/util` 5→24, `contrib/mit/annot` 15→24), all in
  the expected direction given the `-Werror=format` ruling postdates
  the original census. `contrib/srctext/ptext` landed exactly on its
  census of 15 — the only directory in this batch (or, per the prior
  reports read, in Wave 7 so far) to do so.
- **`popts.c`'s `ENTER`/`LEAVE` macros have a separate, pre-existing,
  non-compiler-error bug** noticed while fixing the `%lx` format issue:
  `#define ENTER(r) DEBUG(("Enter %s(0x%lx)\n", "r", self))` passes the
  literal two-character string `"r"` instead of stringifying the macro
  parameter (`#r`), so every `ENTER`/`LEAVE` trace prints the literal
  text "r" instead of the actual function name. This does not affect
  compilation or produce any warning under the new flags (it's a valid
  `%s` against a string literal) and only ever shows up with `debug`
  hand-set to `TRUE` in a debugger, so it was left untouched as out of
  scope — noting it here per the task's "if you happen to notice
  anything... mention it" guidance.
- **No new `.ch`-vs-implementation DRIFT or lifecycle-method
  (`InitializeClass`/`FinalizeObject`) issues found** in any of the 9
  directories — this batch's fallout was cleanly implicit-int/format/
  function-pointer, no interface-signature surprises.
- **No new bare/unjustified `void(*)()` casts introduced.** Every
  function-pointer cast in this batch matches an established tree-wide
  precedent (`procedure` for `proctable_DefineProc`/`im_EnqueueEvent`/
  `tree23int_Apply`, or the `bind_Description.proc` field's own
  declared type, itself found unmodified in 1988 code) — nothing here
  needed orchestrator escalation.

## Suggested runtime checks for wdc

None of these directories are wired into `runapp`'s static link (`nm
-g runapp` — confirmed none of `circpiv`, `htmlview`, `html`, `ltextv`,
`ltext`, `popts`, `headrtv`, `vutils`, `compat`, `timeodayv`,
`writestmpv`, `clockv`, `calcv`, `noteview`, `iconview`, `psview`,
`stroffetv`, `ptextv` appear there); they load dynamically from
`build/dlib/atk/*.do` when their inset type is invoked, or (for
`eatmail`/`ez2ascii`) run as standalone programs from `build/bin`.
Suggested spot checks, each from a scratch `.ez` file (never save
against a versioned fixture):

- **Clock/time insets**: `DISPLAY=:0; runapp -d eza` then insert a
  clock inset and a time-of-day inset (`<ESC><TAB>clock`,
  `<ESC><TAB>timeoday`) — exercises `clock.do`/`clockv.do`/
  `timeoday.do`/`timeodayv.do`'s `procedure`-cast fixes (the
  `UpdateTime`/`MenuSet*` proc-table bindings) and confirms the clock
  inset (flagged reopened-working in memory) still displays and
  updates.
- **Calc inset**: insert a calc inset, type a few digits/operators,
  save and reopen the file — exercises `calc.c`'s `Reader`/`Writer`
  datastream round-trip fix and `calcv.c`'s keystroke/print dispatch
  fix. Do **not** judge anti-aliased-text rendering quality here (known
  separate ghost-text issue, out of scope).
- **Annotation insets (note/PS)**: insert a note inset and a
  PostScript inset with a real `.ps` file that has a
  `%%BoundingBox` line — this is the one prominent LP64 fix in the
  batch (`psview.c`'s `sscanf`); worth confirming the inset displays at
  the correct size (not wildly oversized/zero-sized, which is what
  garbage-upper-32-bits would look like) rather than just that it
  compiles.
- **`eatmail`**: `build/bin/eatmail -h` (or with a scratch spool file,
  never a real mailbox) to confirm the binary runs at all post-rebuild
  — this directory had the most implicit-function-declaration fallout
  in the batch (6 genuinely undeclared tree functions) and is the only
  directory here that links directly against `libmail.a`/
  `libcparser.a`, so it's worth a basic smoke test that linking still
  resolves correctly.
- **`ptext`/`ltext` (Pascal/Lisp source-text modes)**: insert a
  `ptext`/`ltext` inset, type some Pascal/Lisp-like text including a
  `)`/`}`/`*` character (exercises the `char`-key bind-table entries
  that needed the `(void (*)())` cast) and confirm paren-balancing/
  brace-indent still fires.
