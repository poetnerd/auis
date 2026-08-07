# M4 rollout: batch plan

Companion to `m4-rollout-runbook.md`. Built 2026-08-01 from **real,
measured Phase 1 census data** (`dependInstall.log`, the corrected
full-tree run with `CDEBUGFLAGS=-ferror-limit=0` after both `system.mcr`
and the 29 M2-era per-directory overrides were fixed) — unlike
`m3-batches.md`'s first draft, this is not a proxy count built before
checking real scope. **Consolidated 2026-08-02** from the original 24
batches to 18 sessions (plus Batch 0): the small/mid mechanical batches
were merged within their waves, since per-session fixed overhead
(briefing, gates, report, review) dominated exactly there, and M2/M3's
own late-stage throughput (single sessions clearing 300–500+ mechanical
instances) proves the merged sizes are conservative. The 8 outlier
directories keep their own sessions — they're alone for risk, not
arithmetic. Non-outlier target: ~120–180 errors/session; the Gate-0
dry-run shape (runbook Phase 2) is the split-back-apart hedge if a
merged batch's error mix turns out judgment-heavy.

**Numbers**: 1,778 census errors tree-wide (`implicit-int`: 1,079;
`incompatible-function-pointer-types`: 515;
`implicit-function-declaration`: 183; `int-conversion`: 0, clean).
Directory accounting (corrected 2026-08-02 against the real tree): the
census showed errors in 83 *build* directories, but one of those,
`atk/basics/lib`, has **no `.c` files of its own** (verified: only
Imakefile + the built `libbasics.a`) — its Imakefile recompiles
`atk/basics/x`'s sources (`../x/xim.c` etc.) into a combined library,
so its 35 errors are exact duplicates of `atk/basics/x`'s and close
automatically when that directory is fixed. Of the 91 source
directories with their own `.c` files: **82 are fix targets (1,743
errors, matching the wave totals below exactly) and 9 are clean**
(82 + 9 = 91; the earlier "83 of 91" phrasing mixed the build-dir and
source-dir counts).

**Not counted in the 1,743 but in scope**: ~715 unique `-Wformat`
warning sites tree-wide (measured 2026-08-02 from the current baseline
build log — clang reports them by default even under today's
suppressed flags). **All are in-batch must-fix**, enforced as
`-Werror=format` — wdc's ruling, from a real data-loss incident: a
datastream writer's `%d` on a `long` object/view id truncated the ids,
the reader saw mismatched ids, and the file silently lost the object
and view. Printf is a serialization path here, not display. See the
runbook's "`-Wformat` scope" section. Per-directory format counts were
never censused, so each batch's Gate-0 dry run reveals its real total;
the known skew: the top three files (`rastvaux.c` 201, `dispbox.c` 84,
`rasterv.c` 66) are all in `atk/raster/cmd` (batch I1), and the fourth
(`figorrec.c` 58) is in `atk/figure` (batch I3) — everywhere else the
per-directory residue is small.

**9 directories are already clean**, no batch needed: `atkams/
messages/cmd` (dead source, never compiled — see `m4-rollout-runbook.md`
Phase 0), `contrib/wpedit`/`overhead/malloc`/`inst` (confirmed inert,
same as M3), `ossupport` (1 file, compiles clean), and
`overhead/class/lib`/`overhead/class/pp`/`overhead/class/machdep/darwin`/
`overhead/class/testing` — the last four **only after Batch 0 below is
reapplied**: the classpp fix that made them clean during Phase 1 was
part of the 2026-08-02 revert and is NOT currently in the tree.
**Correction 2026-08-02 (post Batch 0)**: the Phase 1 record's
pathopen-only fix was incomplete — under the promoted
`-Werror=format`, both `class.c` files needed additional fixes beyond
what Phase 1 found (see Batch 0's own entry below); reapplying only
the originally-recorded fix does not reach zero errors.

Total: **Batch 0 + 18 sessions across 7 waves** (same dependency-order
wave grouping as M3: overhead → atk/basics+support → atk/text → insets
→ apps → atkams/ams → contrib). Note the waves are convention in M4,
not dependency: `COMPILERFLAGS` doesn't propagate to consumers, so any
batch can run in any order — if budget risk makes front-loading
discovery attractive, C1/AMS1 can legally be pulled earlier. No
separate pilot needed — Phase 0's classpp fix already serves as the
proof-of-mechanics step M3's `atk/eq` pilot provided.

## Batch 0 — classpp prerequisite (do first, commit once)

- [x] **Batch 0** — DONE 2026-08-02, committed f6bad47b273b: reapplied
      the Phase 1 classpp fix to `overhead/class/pp/class.c`
      (`#include <stdlib.h>`, removed the now-conflicting manual
      `char *malloc(),*realloc();` decl, `extern` forward declarations
      for `PushFile`/`PopFile`) and `overhead/class/lib/class.c`
      (`static pathopen()` → `static int pathopen()` at both the
      forward decl and definition). **Beyond the Phase 1 record**:
      `-Werror=format` surfaced real bugs neither file's original fix
      addressed — `pp/class.c`'s `outstr0()` passed a non-literal to
      `fprintf` as the format string (fixed to `"%s", str`) and three
      call sites passed an unused extra vararg (dropped, ~30-year-old
      copy/paste leftovers); `lib/class.c` had three LP64
      pointer/id-truncation sites (`%x`/`%d` on `unsigned long`
      fields/pointer-casts, fixed to `%lx`/`%lu`, confirmed against
      `class.h`'s field types) plus one message referencing `error %d`
      with no `errno` argument supplied (added). Verified: both files
      compile clean under the full strict flag set standalone, `class`
      builds/installs from a full clean rebuild, `overhead/class/
      testing`'s cascade failure is gone (clean rebuild confirmed
      independently). Added `STRICT_COMPILERFLAGS` to `system.mcr`
      exactly as specified (separate `-Werror=<name>` flags, not
      comma-joined) and verified propagation + genuine firing on
      `overhead/class/cmd` (temporary override reproduced its 3 known
      census errors verbatim, then reverted, directory rebuilt green).
      Two fossil-tracked generated machdep Makefiles
      (`overhead/class/machdep/{,darwin/}Makefile`) picked up the
      propagated macro on rebuild and were committed alongside, same
      as Phase 1's own record of touching them.

## Wave 1 — overhead (22 directories, 266 errors, 3 sessions)

- [x] **O1** — DONE 2026-08-02, committed e473083e3078: `overhead/mail/
      metamail/richmail` (81 census) — 82 raw errors (81 implicit-int +
      1 `-Werror=format`), 31 unique fix sites, matching the census
      almost exactly, no surprise fallout. All mechanical implicit-int
      → explicit `int`/`void` return-type fixes (verified per-site via
      grep for real `return <expr>` and address-of use, not blind
      `int`-stamping), plus one genuine bug: `richlex.c`'s ISO-2022
      charset-designator handler called `sprintf(token,
      ISO2022_CHARSET, newc)` where `ISO2022_CHARSET` is
      `"x-iso-charset-"` (no `%` specifier) — a real `-Werror=format`
      violation, the extra arg was silently dropped. Fixed to
      `strcpy`, verified by feeding a real `ESC $ ) C` KSC-5601
      designator through the rebuilt `richtoatk` and confirming the
      output token is exactly `x-iso-charset-` with no truncation/
      garbage. No function-pointer casts needed (0
      incompatible-function-pointer-types errors in this directory).
      Gate clean (`make clean && make depend && make -k install`,
      exit 0), independently reproduced by the orchestrator. Not
      reachable from the live `messages` app (which has its own inline
      `text822.c` richtext handling, doesn't exec these binaries), so
      no further runtime test required before check-in.
- [x] **O2** — DONE 2026-08-02, committed 61fda88897d6: `overhead/eli/lib`
      (53), `overhead/mail/metamail/metamail` (32), `overhead/mail/lib`
      (11), `overhead/eli/bglisp` (10), `overhead/mail/cmd` (9),
      `overhead/mail/testing` (5) — 120 census, 6 directories, 25 files
      touched (24 in-batch + `overhead/eli/hdrs/elifns.h`, a shared
      header dependency of `eli/lib` rebuilt via its own tiny
      header-install directory to propagate). Built in dependency order
      (`mail/lib` → `mail/cmd`/`mail/testing` → `eli/lib` → `eli/bglisp`
      → `metamail`, since `bglisp`/`cmd`/`testing` link the libs). Mostly
      mechanical implicit-int/void return-type fixes verified against
      real call sites, plus three genuine bugs: `mail/lib/qmail.c`
      printed `UnixError()` (returns `char *`, confirmed against ~40
      other correct call sites) with `%d` instead of `%s`; two format-
      string injection sites (CWE-134) in `metamail.c`'s
      `SaveSquirrelFile` and RFC-2047 encoded-word display, where
      attacker-controlled MIME header text (a `Content-Type` parameter,
      an encoded-word payload) was passed directly as a `printf`/
      `fprintf` format string — fixed to `"%s"`. One function-pointer
      fix (`eli/lib/prims1.c` signal handlers: real `void handler(int)`
      signatures + `SIG_ERR` comparison, replacing the old `(int(*)())`
      cast-and-compare-to-`-1` hack); no bare casts, nothing escalated.
      All 6 directories gate clean (`make clean && make depend && make
      -k install`, exit 0), independently reproduced by the orchestrator
      along with every diff and format-width fix (checked against real
      field types: `time_t`, `long int`, `unsigned long int`).
      `overhead/mail/metamail/metamail` is genuinely on the live
      `messages` app's runtime path (`popen("metamail ...")` in
      `atkams/messages/lib/mailobj.c`, which via metamail's own mailcap
      dispatches `text/richtext` to O1's `richtext` binary) — orchestrator
      ran the rebuilt `metamail` directly against a synthetic multipart
      MIME message (including the `%s%s%s%n` injection shape) with no
      crash/garbage output. wdc confirmed the `messages` app's own
      multipart-parsing rewrite has since disabled the UI hook that
      invokes metamail, so no live end-to-end app test was required
      before check-in.
- [x] **O3** — DONE 2026-08-02, committed 4c5c0272c887: (former O4+O5)
      `overhead/bison` (17), `overhead/util/lib` (11), `overhead/index`
      (7), `overhead/mkparser` (4), `overhead/addalias` (4),
      `overhead/util/cmd` (3), `overhead/sys` (3), `overhead/rxp` (3),
      `overhead/class/cmd` (3), `overhead/image/tiff` (2),
      `overhead/fonts/cmd` (2), `overhead/errors` (2), `overhead/cmenu`
      (2), `overhead/util/hdrs` (1), `overhead/image/jpeg` (1) — 65
      census, 15 directories, 64 files touched (15 Imakefiles + 49
      source/header files). Real counts matched census almost exactly
      everywhere except one outlier: `overhead/image/tiff` (census 2,
      real 109) — this vendored libtiff hadn't actually been recompiled
      since before the strict flags existed, so forcing it clean
      exposed a library-wide "typed forward-decl already said `int`,
      the definition just never caught up" idiom repeated across ~20
      files; fixed via two library-internal header additions
      (`tiffcompat.h`, `tiffioP.h`: missing includes, cross-file
      declarations for `TIFFInitCCITTFax3/4`/`TIFFFlushData1`/
      `TIFFSetCompressionScheme`) plus per-file `static int` retypes,
      rather than touching all 20 files' logic individually. Genuine
      bugs found: `overhead/bison/files.c`'s `AndrewDir()` (a
      pointer-returning function) was invoked via the `XPFILE`/
      `XPFILE1` macros with no declaration anywhere in scope — the
      same LP64 pointer-truncation class fixed tree-wide elsewhere,
      fixed with `extern char *AndrewDir();`; `overhead/index`'s
      on-disk hash-bucket (`H%d`) and version (`V%d.%d`) filenames were
      built/parsed with `%d`/`sscanf %d` against `long` fields (hash
      index, version, record ids) — fixed to `%ld`, verified against
      `struct recordID`/`struct indexBucket` in `index.h`; `tif_print.c`
      had 11 `%u`-vs-`u_long` mismatches in `TIFFPrintDirectory`
      (display/debug path only). One function-pointer fix
      (`overhead/addalias/addalias.c`'s `qsLineCompare` rewritten to a
      real `(const void*, const void*)` qsort comparator, not a bare
      cast); nothing escalated. No CWE-134 injection sites found in
      this batch. `overhead/bison` confirmed first-party-maintained
      (Andrew Consortium's own fork, not upstream-vendored per the
      Imakefile's own comment) — fixed like any other directory;
      `lex.c`/`gram.c` (bootstrap-generated parser tables) needed zero
      changes. All 15 directories gate clean (`make clean && make
      depend && make -k install`, exit 0), independently reproduced by
      the orchestrator from a fresh rebuild of all 15, along with every
      diff and format-width fix checked against real declarations. None
      of these 15 directories sit on a live GUI-app runtime path (build
      tools/libraries, not exec'd by `ez`/`messages`), so no additional
      runtime test was applicable beyond the gate rebuilds. **Closes
      Wave 1** — no full `Clean; make World` checkpoint due yet (that's
      after Waves 2/4/6).

## Wave 2 — atk/basics+support (19 directories, 381 errors, 3 sessions)

- [x] **B1 COMPLETE 2026-08-02, fossil `d45d25635594`**: `atk/apt/tree`
      (census 87, real 94 — clang's default `-ferror-limit=20` was
      truncating the initial dry-run count to 23, hence the "biggest
      surprise" framing). Orchestrator pre-diagnosis found the real
      cause before briefing the delegate: `treev.c` (2833 lines)
      carried two forward-declaration blocks for the same ~40 static
      helpers — a correctly-typed one and a stale untyped duplicate
      further down — plus each function's actual K&R-style definition
      still lacking a return type. Fix: deleted the dead block, folded
      its 6 non-`int` entries (`void`/`long`/struct-pointer returns)
      into the correct block, added `int` to the 42 definitions that
      needed it (verified each against real `return` behavior, not
      just the pre-existing block's say-so). Plus 7 LP64 `%d`→`%ld`
      fixes verified against `tree.ch`'s real `long`-typed fields
      (display/outline-index strings, not an on-disk format — unlike
      O3's `index` bug) and 3 function-pointer casts (`tree_Apply`,
      `treev__SetHitHandler`'s `HitHandler`, `treev_PrintObject`'s
      `Printer` arg) checked byte-for-byte against the real classpp-
      generated `.ih`/`.eh` dispatch-macro slot types — same genuinely-
      polymorphic-table exception already used elsewhere, not a banned
      bare cast. No CWE-134 sites. Gate independently reproduced clean
      from a fresh `make clean && make depend && make -k install`.
- [x] **B2 COMPLETE 2026-08-02, fossil `2b2e570ca739`**: `atk/adew`,
      `atk/basics/common`, `atk/apt/suite`, `atk/basics/x` (+
      `atk/basics/lib`, verified separately — no `.c` files of its
      own, closed automatically), `atk/utils`, `atk/apps`,
      `atk/syntax/{tlex,parse,sym}` — 9 directories. Census (113) badly
      undercounted; real total was 259, almost entirely from
      `atk/basics/x` (89 real vs. 6 census — Xlib-typedef `%d`/`%X`
      format mismatches on `Window`/`Cursor`/`Time`/`Atom`, all
      `unsigned long` on LP64, concentrated in `xim.c`/`xcursor.c`/
      `xfontd.c`/`xgraphic.c`). All mechanical, no architectural
      surprise. Five genuine bugs found and fixed: LP64 `sscanf %d`
      truncation in three on-disk save/cache formats (`basics/common`'s
      path-truncation cache, `adew`'s cel and lset document formats —
      reader+writer fixed symmetrically in each); the same class
      cross-process instead of on-disk in `basics/x`'s `atoms.c`/
      `xgraphic.c` (X root-window properties `ATK_ATOMS`/`ATK_SHADES`
      written with `%d`, read with `atol()`); and three signal handlers
      in `basics/common/im.c` silently compiling with wrong K&R
      signatures instead of POSIX `void(*)(int)` (the correct `#if`
      branch's guard macro was never defined under any build this tree
      actually uses). One out-of-batch prototype fix required
      (`class_PrependClassPath` added to `overhead/class/lib/class.h`
      to unblock `atk/apps/runapp.c`). All function-pointer casts
      verified against real `.ch`/`.ih` slot types per policy; two
      sites retyped to true signatures instead of cast
      (`basics/x/menubar.c`'s `mcomp`, `basics/common/path.c`'s
      `CompareFileNames` — both real qsort comparators, not class
      dispatch slots). Orchestrator independently re-verified all 50
      changed files against source headers, re-ran all 10 directory
      gates clean from scratch. Given this batch's wide blast radius
      through `basics/common`+`basics/x` (input dispatch, signal
      handling, X11, image codecs — statically linked into every app),
      wdc required a manual runtime smoke test before check-in (not
      just the compile gate) — ez relinked and hand-tested (input,
      scrollbar, menus, text) before commit; passed. Confirmed
      pre-existing (not a regression): GIF/JPEG-into-`image`-inset
      import still renders blank, same as before this batch.
- [x] **B3 COMPLETE 2026-08-02, fossil `2f65c929c095`**: `atk/value`,
      `atk/apt/apt`, `atk/textaux`, `atk/supportviews`, `atk/support`,
      `atk/lookz`, `atk/textobjects`, `atk/extensions`, `atk/frame` — 9
      directories. Census (181) undercounted only mildly this time
      (real 218, +20%, 0%–60% per-directory spread, no B2-style hidden
      outlier) — Gate-0 dry run confirmed the function-pointer share
      (74/218, 34%) tracked the tree-wide census ratio and was
      overwhelmingly one already-precedented shape (concrete function →
      generic `procedure`/`void(*)()` dispatch slot), so proceeded
      without splitting back to the original B3/B4 halves. Eleven
      genuine bugs found and fixed: LP64 on-disk format bugs in
      `value.c` (button/slider widget save format), `supportviews/
      label.c` (font style/size write/read asymmetry), and
      `supportviews/strtbl.c` (`%x`-on-`unsigned long` highlight
      bitmask — the exact scanf-into-long memory-corruption variant);
      two stale duplicate untyped forward-declaration blocks in
      `apt/apt/apt.c` and `aptv.c` (same shape as B1's `treev.c`
      finding — checked each function's real return behavior rather
      than trusting the "correct" block, which was itself wrong for 8
      of the functions); a dead-branch signal-handler bug in
      `frame/framecmd.c` (`_ANSI_C_SOURCE`/`_NO_PROTO` never defined,
      identical root cause to B2's `basics/common/im.c` finding); a
      real writable-string-literal crash bug in `extensions/filter.c`
      (`sprintf("literal", buf)` — format string and destination
      buffer swapped); a format-extra-args copy/paste bug in
      `support/print.c`; and assorted mechanical `%d`→`%ld` LP64
      fixes. ~30 function-pointer casts, all verified against real
      `.ch`/`.eh`/`.ih` declared types; one pre-existing **bare**
      `(void(*)())` cast in `lookz/lookzv.c` was removed outright
      (became unnecessary once the callee's implicit-int was fixed to
      its real `void` type) rather than re-justified. One out-of-batch
      prerequisite fix: `overhead/util/hdrs/util.h` gained a missing
      `lc_strcmp` prototype (same shape as B2's
      `class_PrependClassPath` fix). Orchestrator independently
      re-verified the highest-risk claims against real header
      declarations (`value.ch`, `strtbl.ch`, `panel.ch`, `im.ch`,
      `keystate.ch`, `dialogv.ch`, `framemsg.ch`, `proctbl.ch`) and
      re-ran all 9 directory gates clean from scratch. `atk/
      supportviews`, `atk/support`, and `atk/frame` are statically
      linked into `runapp` (every app); wdc ran the same manual
      smoke-test tier B2 required (scrollbar auto-repeat, style
      editing, a button/slider dialog, a message-line prompt) — all
      passed, before this wave's checkpoint.

**Checkpoint after Wave 2, 2026-08-02**: full `make Clean; make World`
from the tree root — clean, zero real errors (only the known
`"Internal error: unknown recognizer type"` string-literal false
positive). wdc then ran a broader runtime pass across `ez`, `help`,
`messages`, and `cuin` (all share the freshly-relinked `runapp` binary)
— all behave as before. **Closes Wave 2.**

## Wave 3 — atk/text (1 directory, 57 errors, 1 session)

- [x] **T1 COMPLETE 2026-08-03, fossil `623f242eba34`**: `atk/text` —
      alone. Census (57) badly undercounted (real 102, +79%, the
      largest gap-to-floor ratio of any Wave 1–3 batch), almost
      entirely because the census predates the `-Werror=format` ruling:
      45 of the 102 real errors were format-related (37 width, 7
      format-security non-literal strings, 1 insufficient-args),
      concentrated in `txttroff.c` (44 errors alone, a ~2000-line
      troff-stream writer that's essentially one long chain of
      `fprintf`s against `long` statevector fields). Two genuine
      standalone bugs found and fixed: `content.c:54`'s dead forward
      declaration `static int erestingstyle();` — a ~30-year-old typo
      that never matched the real function `interestingstyle`, silently
      unused since whenever it was introduced (no compiler warning
      exists for an unused `static` prototype); and two broken
      diagnostics in `be1be2a.c` (the standalone BE1→BE2 CLI tool) —
      `%d` used for a `char *` filename, and an `fprintf` missing its
      `progName` argument entirely, both ~30-year-old copy/paste bugs
      with no data-loss implication (stderr-only). Also: a dead
      duplicate untyped K&R forward-declaration block in `readscr.c`
      (same shape as B1's `treev.c`/B3's `apt.c`/`aptv.c` findings —
      folded its 2 unique entries into the correct block, added `int`
      to all 7 real definitions after checking each against its actual
      return behavior). Two `incompatible-function-pointer-types` sites
      (`pcompch.c`'s `lenorder`, `indexpro.c`'s `cmp`, both `qsort`
      comparators) fixed via true-signature retyping to
      `int(*)(const void*,const void*)` per fix-policy rule 2 — neither
      is a class-dispatch slot, so the cast/polymorphic-table exception
      didn't apply and wasn't used; **zero function-pointer casts
      landed in this batch**, nothing to escalate under rule 4. All
      remaining ~50 fixes were mechanical `%d`→`%ld` (verified against
      real `long`-typed statevector fields in `txtstvec.h` and similar)
      or missing-`int`-on-definition (each already correctly
      forward-declared elsewhere in the same file). Swept for the
      directory-specific malloc/stdlib.h blind spot flagged in
      `rollout-procedure.md` (106 sites found there in an earlier
      session) — confirmed already closed, all 19 malloc-calling files
      already include `<stdlib.h>`. Orchestrator independently
      re-verified the two genuine bugs and both qsort retypes against
      the diff, confirmed the dead-decl-block cleanup, checked the
      `txttroff.c` field types directly against `txtstvec.h`, and
      re-ran the directory gate clean from scratch. `atk/text` is
      statically linked into `runapp` (every app) per this project's
      own prior `nm -g runapp` finding; `runapp`/`ez`/`be1be2`/
      `typescript`/`pipescript` relinked before commit. wdc ran a
      broader-than-usual manual pass given this directory's size and
      centrality: text insertion/deletion, style set/clear, multi-page
      scrolling, a Contents inset, a footnote, and an index-term mark —
      all passed (the Contents and index-term paths hadn't been
      exercised in a long time and both worked correctly). Troff/print
      output byte-diffing and the `be1be2` CLI were left unexercised —
      no printing pipeline currently wired up (separate future project,
      either troff or a direct-to-PostScript backport from the C++
      codeline) and no BE1 sample file exists anywhere in the tree (an
      ancient pre-`ez` format) — judged not worth hand-crafting one for
      two stderr-message fixes on a standalone converter tool; both
      deferred without blocking check-in.

**Checkpoint after Wave 3**: none due — Wave 3 is a single one-directory
session, covered by the next full checkpoint after Wave 4 per the
consolidated gate schedule (waves 1/3/5 ride the checkpoint that
follows them). **Closes Wave 3.**

## Wave 4 — insets (16 directories, 447 errors, 4 sessions)

- [x] **I1 COMPLETE 2026-08-05, fossil `6a5fbc4c9c02`**: `atk/raster/cmd`
      alone. Census (112) + the ~351 estimated `-Wformat` sites landed
      close: real total 493 (7% over ~460, the closest census match of
      any M4 batch so far) — 379 format, 107 function-pointer, 5
      implicit-int, 2 format-extra-args. All 107 function-pointer sites
      were one uniform shape (`proctable_DefineProc`/`im_EnqueueEvent`
      callback registrations against the toolkit's `procedure` generic-
      dispatch type, already the established tree-wide idiom) — fixed
      with `(procedure)` casts, zero bare/escalated casts. Genuine bugs:
      `raster__WriteShare` silently dropped the `height` field from its
      datastream write (7 `%ld` conversions for 8 args) — real
      corruption on the "share" (same-machine shared-memory) write path,
      exactly the class the format ruling exists to catch; 4 scanf-
      direction LP64 half-writes (`options`/`depth` fields, the Scale
      and Gray dialogs' numeric entry); an id-truncation write bug; a
      shared `ENTER`/`LEAVE` debug-macro pointer-format bug in
      `dispbox.h` fixed once for 31 call sites. Also flagged, not fixed
      (out of compiler-error scope): the same macros' `"r"`-vs-`#r`
      literal bug, which has printed the wrong function name in every
      raster debug trace for ~35 years — cosmetic, left for a future
      sweep. `rastimg.c`/`rastervt.c` were already clean, untouched.
      Orchestrator independently verified the `procedure`-cast precedent
      against `class.h` and sibling files, the `WriteShare` bug against
      both writer and reader, the macro fix, and the Imakefile override,
      then re-ran the gate clean from scratch. `atk/raster/cmd` is
      dynamically loaded (`.do`, not statically linked into `runapp`) —
      no relink needed. wdc ran a raster-specific runtime pass (inset
      display/save/reload, Scale and Gray dialogs, several Raster menu
      commands) given the genuine read/write bugs found — all passed.
- [x] **I2 COMPLETE 2026-08-06, fossil `b2d41d7242e0`**: `atk/table`
      alone. Real total 137 vs. 81 census (69% over — this batch's
      census predated the `-Werror=format` ruling entirely, same root
      cause as T1's gap) — 56 format, 55 function-pointer, 26
      implicit-int. Function-pointer share 40%, above the "~30-35%
      typical" flag — consistent with the batch map's own advance
      warning about this directory's `AUXMODULE`/M2-classpp history —
      but all 55 sites resolved to two already-precedented shapes with
      zero escalations: the tree-wide `bind_Description.proc` K&R
      dispatch-table cast (52 sites, `menutable`/`keytable`, matching
      `atk/frame`'s Wave 2 B3 fix) and the `procedure`/
      `proctable_DefineProc` cast (1 site, matching I1's precedent);
      the remaining 3 were really a signal-handler dead-`#if`-branch
      bug (`eval.c`'s `Exception`/`oldsig`), the same root cause
      already found in B2/`im.c` and B3/`framecmd.c` — the one fix
      this batch with real behavioral surface (a K&R-typed `SIGFPE`
      handler under a dead guard, now ABI-correct). `#define AUXMODULE`
      confirmed present/load-bearing in `keyboard.c`/`hit.c` but
      produced no AUXMODULE-specific fallout beyond the two idioms
      above. Genuine bugs found, all debug-trace-only or type-only (no
      datastream read/write path affected, unlike I1's `WriteShare`
      bug): `table.c`'s `table_ReadASCII` trace had two stray `%d`s
      with no arguments; `tabio.c`'s `WriteCell` trace printed a
      `char **` pointer instead of the string it points at (`*buff`),
      a ~30-year-old always-wrong debug message; several pointer/width
      DEBUG-trace mismatches in `table.c`/`hit.c`/`spread.c` (24 of the
      137 in `spread.c` alone, all `if(debug)`-guarded). Orchestrator
      independently verified the `bind.ch:39` `proc` field type and
      the cast's struct-field position, the `framecmd.c`/I1 cast
      precedents, the `eval.c` signal-handler fix against both `#if`
      branches, the `tabio.c` `ENSURESIZE`/`*buff` reasoning, and the
      `table__Read`/`table__GetModified` field types, then re-ran the
      gate clean from scratch. `atk/table` is dynamically loaded
      (`DynamicMultiObject`, no `LibraryTarget`) — no relink needed.
      wdc ran a table-specific runtime pass (divide-by-zero formula
      error state, cell editing, several Table/Cells menu commands,
      save/reload round-trip) given the signal-handler fix's
      behavioral surface — all passed.
- [x] **I3 COMPLETE 2026-08-06, fossil `da6852bf98b2`**: `atk/figure`
      (census 48, real 191 — 298% over, the largest census/real gap of
      any I-wave batch) + `atk/chart` (census 38, real 58 — 53% over).
      Combined 249 vs. 86 census, gap fully explained the usual way
      (census predates the `-Werror=format` ruling); `figorrec.c`
      confirmed as exactly the tree's #4 `-Wformat` file (58 sites, all
      PostScript print-stream, no datastream exposure). This batch
      earned its "risk batch" label: **11 genuine on-disk datastream
      bugs found and fixed**, all the LP64 `%d`-for-`long`
      truncation/corruption class the format ruling exists to catch —
      8 in `atk/figure`, 3 in `atk/chart`. Highest-impact:
      `figobj.c:517`'s base-class `figobj__WriteBody` (inherited by
      nearly every figure object type) truncated the `x`/`y` position
      fields on every save; paired reader was already correct. Also:
      `figure.c:714`/`:742`'s `$origin` field — the *write* side
      (`figure__Write`/`figure__WritePartial`) was still `%d %d` even
      though the read side was already fixed in an earlier milestone
      (project memory's fix was read-only, not caught until this
      session re-checked both directions of the pair). `atk/chart`,
      despite no flagged prior history, turned out not clean: `chart.c`
      `Parse_Item_Field`'s `sscanf(extract, "%d", &value)` read into an
      **uninitialized `long`** (item Value/Position fields) — genuine
      memory corruption on read, not just truncation, with a symmetric
      write-side truncation in `Writer`. 6 more figure `WriteBody`
      truncations (`figorect.c`, `figotext.c`, `figoplin.c`,
      `figogrp.c`) and `figattr.c`'s 6-field write rounded out the
      figure findings. Function-pointer share was low in both
      directories (25%/7%, below the "~30-35%" escalation flag) and
      every site resolved cleanly: 47 `(procedure)` casts across two
      precedented idioms (`proctable_DefineProc` registration,
      `figview_EnumerateSelection`'s polymorphic callback slot per
      `figv.ch:113`), 2 `(void (*)(struct chart *))` casts on `apt.ch`'s
      deliberately-generic `ReadObject`/`WriteObject` reader/writer
      slot (full prototype stated, not bare), and two zero-cast
      end-to-end retypes — `figv.c`'s `EnumSelSplot` helper (matched to
      its caller's `procedure` type) and `chart.c`'s six `Sort_By_*`
      qsort comparators (retyped from a mismatched
      `long(*)(struct chart_item**,...)` to the real libc
      `int(*)(const void*,const void*)` contract). Zero bare/escalated
      casts. Orchestrator independently re-verified the `figobj.ch`/
      `figure.ch`/`chart.ch`/`apt.ch`/`figv.ch` field and slot types
      against the diff, confirmed the `$origin` half-fix history, the
      uninitialized-`long` scanf bug, the qsort retype's bounded
      -1/0/1 return values, and the `figoplin.c` near-miss (a
      structurally similar but genuinely-`int` field correctly left
      unchanged), then re-ran both directory gates clean from scratch.
      Both directories are `.do`-loaded (`DynamicObject`/
      `DynamicMultiObject`, no `LibraryTarget`) — no relink needed. wdc
      ran a figure+chart-specific runtime pass (insert/draw/save/
      reload round-trip for figure incl. a Cut/Copy exercising
      `WritePartial`'s separate `$origin` path; insert/save/reload plus
      ascending/descending sort for chart) given the genuine datastream
      findings — all passed.
- [x] **I4 COMPLETE 2026-08-06, fossil `35ed1bbdc5bf`**: `atk/image`,
      `atk/fad`, `atk/srctext`, `atk/org`, `atk/bush`, `atk/rofftext`,
      `atk/raster/lib`, `atk/hyplink`, `atk/eq`, `atk/layout`,
      `atk/raster/scan`, `atk/raster/convert` — 12 directories, closes
      Wave 4. Real total 281 vs. 168 census (67% over, the usual
      pre-`-Werror=format` gap), dominated by `-Wformat` (~60%) and
      `implicit-int`. `atk/layout` alone was 7 census vs. 64 real
      (+814%, the largest census/real gap of any M4 batch) — almost
      entirely one shape, parallel debug-trace `printf`/`fprintf`
      calls across `layoutv.c`/`boxview.c`/`layout.c`/`box.c` printing
      `long` coordinates via `%d` and pointers via `%x`, never
      censused. **4 genuine memory-corruption/datastream bugs**,
      smaller in count than I1/I3 but real: `atk/raster/lib/
      rasterio.c`'s `rasterio__ReadImage` read the `long options`
      field with `%u` (scanf-direction LP64 corruption, on the shared
      raster datastream reader used by every embedded/standalone
      raster), with a paired writer-side fix (7 literal args cast to
      `(long)` to match the already-correct `%ld` format); `atk/
      hyplink/link.c`'s `link__Write` wrote the `long pos`/`len`
      fields with `%d` (truncation-on-write, paired reader already
      long-safe via `atol()`); `atk/raster/convert/convrast.c`'s
      standalone crop-option parser (`-c(left,top,width,height)`)
      read all four `long` locals with `%d` (same scanf-corruption
      class, on `convertraster`'s CLI). Function-pointer share stayed
      low and resolved entirely to precedented shapes (23
      `(procedure)` sites, 17 `bind_Description.proc` `(void (*)())`
      sites, 1 full-prototype `tree_Apply` cast in `org.c`) plus two
      zero-cast end-to-end retypes matching real established contracts
      (`bush.c`'s `NodeFilter` to BSD `scandir()`'s real `const
      struct dirent *`; `bushv.c`'s 5 sort comparators + helper,
      `int`→`long` to match `suite.ch`'s real `sort_handler` field) —
      zero bare/escalated casts. Other real findings: 4 duplicate
      stale K&R forward-declaration blocks removed (`tif.c`, `org.c`,
      `orgv.c`, `bushv.c`); `atk/image/fbm.c`'s `fbmin_img_aspect`
      was `int`+`atoi()` for a field documented as fractional,
      retyped to `double`/`atof` (the validation against `<0.01`/
      `>100.0` could never fire correctly as `int`); `atk/raster/lib/
      heximage.c`'s 5 non-literal `fprintf(file, *hx++)` calls fixed
      with an explicit `"%s"` (real `-Wformat-security`/CWE-134 shape,
      template array not attacker input but still worth the fix);
      `atk/layout/layout.c` had two independent genuine debug-trace
      bugs found incidentally (a stray unmatched `%c` conversion, and
      a copy-paste `y` printed in place of `h`). Orchestrator
      independently re-verified all 4 datastream bugs against real
      `.ch` field types, several cast/retype shapes against their real
      target signatures (`procedure` typedef, `tree.ch`'s `Apply`,
      `suite.ch`'s `sort_handler`, `orgv.ch`'s `hit_handler`), the
      `tif.c` duplicate-block removal and `heximage.c`'s format-
      security fix directly against the diff, then re-ran the gate
      clean from scratch for all 12 directories in dependency-safe
      order (`atk/raster/lib` before `atk/raster/scan`/`atk/raster/
      convert`, which statically link `libraster.a`). Linkage: 10
      directories `.do`-dynamic only; `atk/raster/lib` is mixed (a
      real `LibraryTarget(libraster.a,...)` plus 2 of its own objects
      also independently `.do`-loaded); `atk/raster/scan`/`atk/
      raster/convert` are standalone programs (`ProgramTarget`/
      `ClassProgramTarget`, not `runapp`-linked, not `.do`) — no
      relink needed anywhere. wdc ran `make Clean; make World` (the
      Wave-4-close checkpoint) himself, confirmed it built clean, and
      ran a basic smoke pass — no regressions, but found two new,
      pre-existing bugs unrelated to this batch's own fixes, logged in
      `roadmap.md`: `contentv` (Table of Contents) shows nothing for
      documents using enumerated heading styles; `convertraster`'s
      crop option (this batch's own fix target) parses correctly now
      but the actual crop output is blank, and the tool's real CLI
      syntax (`infile=`/`outfile=`, shell-quoted crop arg) differs
      from what the code alone suggests — both open, deferred.

**Checkpoint** after this wave: full `make Clean; make World` +
runtime pass. **DONE 2026-08-06** (wdc, after I4) — clean build,
smoke pass found no regressions; two new pre-existing bugs found and
logged in `roadmap.md` (see I4's entry above).

## Wave 5 — apps (8 directories, 59 errors, 1 session)

- [x] **A1 COMPLETE 2026-08-06, fossil `bd430e1d6112`**:
      `ams/msclients/imapsync`, `ams/msclients/cui`, `atk/typescript`,
      `atk/help/src`, `doc/mkbrowse`, `atk/help/maint`, `atk/ez`,
      `ams/msclients/nns` — 8 directories, closes Wave 5
      (`atkams/messages/cmd` already clean, not included; the original
      draft's "9 directories" header miscounted — the list is and was
      8). Real total 82 vs. 59 census (39% over, usual
      pre-`-Werror=format` gap), biggest single jump `ams/msclients/
      nns` (1→7, all 6 new sites the `AMS_ERRNO`/`AMS_ERRCAUSE`/
      `AMS_ERRVIA` macro family's `long`-masked expressions printed via
      `%d`). Recurring "no header declares this" pattern hit 3 more
      instances of the shape Batch 0's `classpp.l`/`PushFile`/
      `PopFile` fix already established: `imap_sync.c`'s
      `dbg_close`/`dbg_fclose`/`dbg_closedir` (the known `fdplumb.h`
      declaration gap already carried by `ams/libs/cui`/`overhead/
      mail/lib`), `tscript.c`'s `vclose`/`GetPtyandName`, `eza.c`'s
      `frame_VisitNamedFile`/`UnixError`, and `mkbrowse/browser.c`'s
      own `PushFile`/`PopFile` (a second, independent flex grammar
      with the identical gap). 3 `qsort` comparators retyped to the
      real `int(const void*, const void*)` contract (rule 2, not a
      cast — `fcomp.c`'s `mystrcmp`, `help.c`'s `panelCompare`,
      `browser.c`'s `compareclasses`); 4 `procedure`-typed dispatch
      casts in `atk/help/src`, the same tree-wide idiom I1 already
      documented. **5 genuine bugs**, all UI/diagnostic-text
      correctness rather than memory corruption: `cui.c`'s epoch
      delete-confirmation prompt dropped the seconds field (format
      string had 5 `%d`s for 6 args); `mkindex.c` had `code` and
      `lineNo[fsPtr]` swapped relative to their own diagnostic labels
      in two error messages (both real `long` fields, confirmed
      against their declarations); `helpdb.c`'s `sprintf(msg,
      "Running command: %0.230s", ...)` carried a meaningless/UB `0`
      flag on an `s` conversion; `helpa.c`'s duplicate-index error
      path dropped the actual offending switch text; `cui.c`'s
      `SubscriptionChangeHook` was a bare 0-arg stub despite every
      call site (`ams/libs/cui/cuilib.c`) passing 4 real arguments,
      silently tolerated by K&R's no-arity-check convention — given
      its real signature, body stays a no-op (`cui` has no
      subscription-change UI). Plus 3 `-Wformat-security` sites
      (non-literal format string, zero args) in `help.c`/`helpaux.c`/
      `helpdb.c`, fixed with `"%s"` templates. Orchestrator
      independently re-verified every bug against real source (the
      `code`/`lineNo` `long` declarations, the `fdplumb.c`/`fdplumb6.c`/
      `vclose.c`/`browserpp.l` real prototypes matching each new
      `extern`, the `ATKLIBS` static-link claim in `atk/apps/
      Imakefile`), then re-ran the gate clean from scratch for all 8
      directories. Linkage: `atk/typescript` and `atk/ez` both
      statically link into `runapp` via `ATKLIBS` (relinked); the
      other 6 are standalone binaries or `.do`-dynamic only, no relink
      needed. wdc's own smoke pass (cui, typescript-in-ez, help,
      mkbrowse) found no regressions. Also, during this smoke pass wdc
      reverted the hand-edited `revival/testing.ez` from the I4
      close-out's ToC bug report and the symptom didn't reproduce —
      that bug's `roadmap.md` entry is now marked "needs further
      testing" rather than "open" (inconclusive, not resolved).

## Wave 6 — atkams/ams (5 directories, 185 errors, 3 sessions)

- [x] **AMS1 COMPLETE 2026-08-06**: `ams/libs/ms` (74 census; 123
      real, the same pre-`-Werror=format` undercount seen elsewhere).
      Fixed with the directory's fdplumb/M2/M3 history in hand: 52
      `implicit-int` (all true-signature fixes, no casts — 20 in the
      hand-maintained `prsdate.gra` bison source), 21
      `incompatible-function-pointer-types` (10 `qsort` comparators
      retyped to the real `int(const void*, const void*)` contract; 11
      `signal()` sites all against one function, see below — zero
      casts, zero escalations), ~50 `-Wformat`/`-Wformat-security`/
      `-Wformat-extra-args` sites (mostly `%d`→`%ld`/`%lu`/`%lld`
      width fixes, several on the recurring `AMS_ERRNO`/`AMS_ERRCAUSE`/
      `AMS_ERRVIA` macro family). 3 genuine pre-existing bugs found and
      fixed (all independently re-verified by the orchestrator against
      real source, and now also narrated in `revival.md`'s "Old bugs
      never found till now"): `epoch.c`'s local `extern int
      DescribeTimeInterval()` disagreed with the real function's
      `char *` return (LP64 pattern #1, live in the single-old-message
      epoch/deletion path); `subs.c:264`'s `WriteSubs()` recovery
      branch had a duplicated `fprintf` argument misaligning every
      later conversion (pointer printed as `%d`, `int` dereferenced as
      `%s`, the real date field silently dropped — fixed to match the
      correct sibling line and `ReadSubs()`'s own parser); `init.c`'s
      `DieYouHeathenSwine`, installed via `signal()` at 11 call sites,
      was implicit-`int` when `signal()` requires `void(*)(int)` — a
      genuine ~30-year interface bug, retyped to `void` (confirmed no
      call site anywhere ever used a return value). Flagged, not
      fixed (out of this directory's scope, a note for AMS2):
      `MS_FastUpdateState`/`MS_UpdateState` are now `int` here,
      matching the real implementation and every live caller, but
      `atkams/messages/lib`'s `.ch` files still claim `returns long` —
      not yet a live bug, AMS2 should narrow the `.ch` files to `int`
      rather than widen this directory. Downstream relink confirmed
      clean by both the delegate and the orchestrator:
      `atkams/messages/lib` (`amsn.do`) and `ams/msclients/cui`
      (`cuin`, already strict-flagged since Wave 5 A1) both rebuild
      against the new `libmssrv.a` with zero errors. wdc's smoke test
      (cuin SIGINT checkpoint/exit, `messages` folder read) found no
      regressions; the `subs.c` fix was accepted on code read-through
      alone, given its narrow/hard-to-safely-reproduce trigger
      condition.
- [x] **AMS2 COMPLETE 2026-08-06**: `atkams/messages/lib` (65 census;
      74 real, a much smaller pre-`-Werror=format` undercount than
      AMS1's — 14% over vs. 66%). 59 `implicit-int` (all true-signature
      fixes sourced from an existing in-scope declaration — zero
      guesses; two `stubs.c` forward declarations were themselves wrong
      and corrected to `void` to match every real caller/body rather
      than propagated), 6 `incompatible-function-pointer-types` (all
      `(procedure)` casts against `atk/basics`' own genuinely
      polymorphic callback-slot typedef — `message_AskForStringCompleted`,
      `im_SetDeleteWindowCallback`, `im_AddFileHandler` — matching
      dozens of already-committed pre-M4 sites, no bare untyped casts),
      9 `-Wformat`/`-Wformat-insufficient-args` diagnostics across 6
      sites (5 mechanical width fixes, 1 genuine bug, see below). 1
      genuine pre-existing bug found and fixed (independently
      re-verified by the orchestrator against real source, now narrated
      in `revival.md`'s "Old bugs never found till now"): `stubs.c:554`,
      `SnarfFile`'s "not a regular file" diagnostic in the automatic
      bug-report path dropped its filename argument entirely, leaving a
      `mode_t` value read through `%s` and the real mode read from
      stack garbage — fixed by supplying the already-resolved filename
      every sibling message in the function already uses. Also resolved
      AMS1's flagged `.ch`-vs-implementation width-drift lead
      (re-verified against current source first, not trusted blindly):
      narrowed `ams.ch`/`amsn.ch`/`amss.ch`'s `MS_FastUpdateState`/
      `MS_UpdateState` from `returns long;` to `returns int;` and their
      six wrapper definitions in `ams.c`/`amsn.c`/`amss.c` to match,
      confirmed via the regenerated `.ih` dispatch macros and every live
      caller's truthiness-only usage. Note: `amss.c`/`amss.ch` are only
      compiled under `SNAP_ENV` (off in this build) — their half of the
      width-drift fix is consistency-only, unverified by compilation.
      This batch was flagged elevated-risk (same tier as C1) and given
      a mandatory tree-wide gate: both the delegate's `make
      dependInstall` and the orchestrator's independent from-scratch
      rerun (subtree + tree-wide) came back clean, 0 errors. Downstream
      linkage: this directory produces zero static libraries (all 14
      outputs are `.do` dynamic objects) — independently confirmed via
      `nm -g runapp` (zero matching symbols) and a tree-wide grep for
      static consumers of its outputs; no relink needed anywhere. wdc's
      smoke test covered the general `messages` GUI pass and the
      folder-into-completion/compose-window-close `(procedure)`-cast
      sites; the third cast site (`MetaOutput`, MIME-attachment
      metamail invocation) and the `SnarfFile` fix itself were not
      live-tested this pass (MIME attachments hard to test right now;
      `SnarfFile`'s narrow/rare trigger condition made a live repro
      impractical, same reasoning as AMS1's `subs.c` — accepted on code
      read-through alone). Both are open for a future opportunistic
      smoke pass, not blocking.
- [x] **AMS3 COMPLETE 2026-08-06**: `ams/libs/cui` (35 census; 43 real,
      23% over — same range as prior batches once `-Werror=format` is
      counted), `ams/libs/nosnap` (10), `ams/libs/shr` (1) — 54 real, 44
      unique fixes across 7 files. Unlike AMS1/AMS2, all three build
      products are static libraries (`libcui.a`, `libcuin.a`,
      `libmsshr.a`), not `.do` dynamic objects, so real downstream
      consumers needed relinking rather than just a flag-visibility
      check. `cui`'s pre-existing M2-era override
      (`-Wno-implicit-int -Werror=implicit-function-declaration
      -Wno-incompatible-function-pointer-types -Wno-return-type`) was
      replaced (not stacked) with `$(STRICT_COMPILERFLAGS)`; `nosnap`/
      `shr` got the line fresh. Fixes: 26 `implicit-int` (all true-
      signature, sourced from existing in-scope `extern` declarations —
      zero guesses), 10 `incompatible-function-pointer-types` collapsing
      to 2 real signature fixes (`AlarmSignalHandler`/
      `SnapifiedClientSignalHandler` retyped to the real `void(*)(int)`
      `sigaction`/`signal()` contract, zero casts needed), 7 format
      fixes (`%d`→`%p`/`%lu`/`%ld` on pointer/`unsigned long`/`long`
      arguments), plus `nosnap.c`'s missing `<string.h>` and 4 `extern`
      prototypes for functions genuinely defined elsewhere. 1 genuine
      pre-existing bug found and fixed (independently re-verified by the
      orchestrator against real source, now narrated in `revival.md`'s
      "Old bugs never found till now"): `cuilib.c:713`,
      `CUI_SetPrinter`'s catch-all failure path had a format string with
      zero conversions, silently dropping the printer name from the
      user-visible error message whenever printer setup failed for any
      reason other than "no such printer" — fixed to include `%s`.
      Checked for another `.ch`-vs-implementation width-drift instance
      (three `CUI_*` functions declared `returns long` in
      `atkams/messages/lib`'s `.ch` files but genuinely `int` here) and
      correctly determined it is *not* a live bug, unlike the earlier
      `MS_UpdateState` case: every real call site already has its own
      `extern int` declaration in scope before the `long`-returning
      class-dispatch wrapper's `return`, so the widening happens via
      ordinary safe C conversion, not raw-register reinterpretation —
      re-verified by the orchestrator directly against
      `atkams/messages/lib/amsn.c`'s extern block and all three `.ch`
      files; left alone (AMS2's directory, already closed, and not
      actually buggy). Not flagged elevated-risk, so only subtree-local
      gates were required (all 3 clean, independently rebuilt from
      scratch by both the delegate and the orchestrator); downstream
      relink instead covered the real risk surface: `ams/msclients/cui`
      (`cuin`), `atkams/messages/lib` (`amsn.do`), and
      `ams/msclients/imapsync` (`libmsshr.a` only) all relinked clean by
      both the delegate and the orchestrator independently.
      `ams/msclients/vui`, a real consumer in principle, could not be
      relinked or confirmed — confirmed pre-existing and unrelated (a
      termcap `CM`/`SO` undeclared-identifier failure in `andpnlm.c`, no
      `COMPILERFLAGS` override present, no `vui`/`vuin` binary has ever
      existed in `build/bin`), not a regression from this batch. wdc ran
      the wave's full `make Clean; make World` checkpoint themselves
      (completed clean) plus a basic smoke pass of `cuin` and `messages`
      with no observed regressions.

**Checkpoint** after this wave: full `make Clean; make World` +
runtime pass — done by wdc alongside the AMS3 check-in.

## Wave 7 — contrib (11 directories, 348 errors, 3 sessions)

- [x] **C1 COMPLETE 2026-08-06**: `contrib/zip/lib` — census gap
      241 real errors vs. 214 census (+13%, same direction/magnitude
      as every prior batch's undercount). 32 files touched: 209
      implicit-int (a very uniform untyped-duplicate-forward-decl
      pattern, all typed `int` from an existing typed sibling
      declaration in the same file), 26 format-width fixes, 5
      incompatible-function-pointer-types (collapsing to 2 real
      fixes: `ziposym.c`'s `Filter` got a true-signature `const` fix
      matching macOS's real `scandir(3)` prototype; `zipedit.ch`'s
      `Set_Keyboard_Processor` macro got one cast to the field's own
      declared type, the correct fix for a genuinely polymorphic
      callback slot with 3 differently-typed concrete assignees), 1
      format-extra-args. Zero bare/void casts; the one other cast in
      this batch is the tree's established `(procedure)` idiom at a
      `proctable_DefineProc` call site, same pattern AMS1/AMS2 used.
      One genuine ~30-year bug found: `zipds02.c:243`'s figure-mode
      attribute writer built three characters but only wrote two,
      silently dropping the "halo" flag on every save (reader side
      confirmed it expects three); fixed. wdc's own testing after the
      fix found halo mode is effectively vestigial — no menu/palette
      path in the editor ever sets the flag, so the write bug, now
      fixed, protects an attribute nothing can currently turn on;
      logged in `revival.md` with that caveat. No `.ch`-drift found
      this batch. This directory produces only `.do` dynamic-load
      objects (no static library); no downstream relink needed,
      confirmed by both the delegate and the orchestrator
      independently (no other Imakefile references its `.do`
      outputs). Flagged elevated-risk (tree's known
      highest-defect-density directory) — both subtree and the
      mandatory tree-wide `make dependInstall` gate run clean twice
      independently (delegate: 21,048-line log, 0 errors; orchestrator:
      identical count and result from a from-scratch rerun). wdc ran
      smoke testing (general figure draw/save/reload plus the halo
      round-trip) with no observed regressions.
- [x] **C2 COMPLETE 2026-08-06**: `contrib/zip/utility` — census said 67,
      real count 80 (+19%, same undercount direction as every prior
      batch). 5 files touched (`Imakefile`, `lt.c`, `ltv.c`, `sched.c`,
      `schedv.c`; `ltapp.c`/`schedapp.c` needed nothing): 64
      implicit-int (uniform typed-sibling-forward-declaration pattern,
      same shape as C1), 13 format (12 mechanical `%d`→`%ld`, 1
      genuine bug), 2 implicit-function-declaration (`strcpy` missing
      `#include <string.h>`), 1 incompatible-function-pointer-types
      (`suite_Apply`'s `Detect` callback, cast to `suite.ch`'s own
      declared genuinely-polymorphic slot type, confirmed against
      other tree-wide `suite_Apply` call sites — same idiom as C1's
      `keyboard_processor`). One genuine ~1988 bug found:
      `sched.c:75`'s "Unable to Open" diagnostic printed the wrong
      field (`self->stream`, a `struct zip_stream *`, instead of
      `self->stream_name`, the actual filename) — fixed; wdc's own
      smoke test confirmed the fix (message now shows the real
      filename). M3-lifecycle-pattern family explicitly checked per
      this batch's flag: nothing new — this is the same directory M3's
      own C2 batch already fixed for that exact family
      (`InitializeClass`/`FinalizeObject` shapes, named
      `InitializeObject` params), verified still intact, no `.ch` file
      touched this session. Separately, wdc's smoke test surfaced that
      `sched`/`schedapp`/`lt`/`ltapp` have no help document and were
      never wired into the help-installation machinery any sibling
      contrib app uses — logged in `revival.md` as undocumented
      functionality, not a regression. This directory produces only
      `.do` dynamic-load objects (no static library); no downstream
      relink needed, confirmed by both the delegate and the
      orchestrator independently (tree-wide grep, zero references to
      its `.do` outputs elsewhere). Not one of the two elevated-risk
      batches, so only the subtree-local gate was required — run clean
      twice by the delegate and once more independently by the
      orchestrator from a fresh `make clean`. wdc's smoke testing
      (schedapp launched successfully once run from a directory
      containing `itcCR.scd`) found no regressions.
- [ ] **C3** (former C3+C4): `contrib/mit/annot` (15),
      `contrib/srctext/ptext` (15), `contrib/eatmail` (11),
      `contrib/calc` (8), `contrib/time` (8), `contrib/mit/util` (5),
      `contrib/srctext/ltext` (3), `contrib/srctext/html` (1),
      `contrib/demos/circlepi` (1) — 67, 9 directories.

Wave 7 has no separate checkpoint — it flows directly into the Phase 3
global flip and the Phase 5 completion gate (`make Clean; make World`
plus the broad runtime pass), which cover it.

## Session-count summary

Batch 0 + 18 sessions across the 7 wave groupings, versus the original
draft's 24: O2+O3, O4+O5, B2+B5, B3+B4, I4+I5, and C3+C4 were merged
(each merged batch is 65–181 errors, inside M2/M3's proven mechanical
throughput); the 8 outlier singles (O1, B1, T1, I1, I2, AMS1, AMS2,
C1) plus the two deliberately-unmerged risk batches (I3, C2) are
unchanged. Full `Clean; make World` checkpoints run after waves 2, 4,
and 6 (three, not seven — waves 1/3/5 are covered by the checkpoint
that follows them, and wave 7 by the completion gate), per the runbook's
consolidated gate schedule; per-batch subtree gates are unchanged and
always required. Tick batches here as they complete, same convention as
`m1-point10-batches.md`/`m3-batches.md`.
