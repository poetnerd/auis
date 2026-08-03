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

- [ ] **I1**: `atk/raster/cmd` (112) — alone, largest in this wave and
      second-largest overall after `contrib/zip/lib`. Also carries 351
      of the tree's ~715 `-Wformat` sites (`rastvaux.c` 201,
      `dispbox.c` 84, `rasterv.c` 66) — all must-fix under the format
      ruling, so this session's real size is ~460 items. Mostly
      mechanical `%d`→`%ld` (well inside M2's proven single-session
      throughput), but raster is a datastream-heavy inset — treat its
      writer/reader format pairs with the id-truncation incident in
      mind.
- [ ] **I2**: `atk/table` (81) — alone. Historically the directory
      with the `AUXMODULE` classpp sub-case (M2) — expect similar
      class-internal-dispatch findings here.
- [ ] **I3**: `atk/figure` (48), `atk/chart` (38) — 86. `atk/figure`
      carries known LP64/DRIFT history (M1 Pilot B, M2 batch3a, M3
      i2) — expect findings, not a routine batch. Kept unmerged for
      that reason. `figorrec.c` is the tree's #4 `-Wformat` file (58
      sites, all must-fix — and figure's datastream `$origin %d→%ld`
      history makes it the poster child for the format ruling).
- [ ] **I4** (former I4+I5): `atk/image` (31), `atk/fad` (29),
      `atk/srctext` (22), `atk/org` (19), `atk/bush` (16),
      `atk/rofftext` (15), `atk/raster/lib` (9), `atk/hyplink` (8),
      `atk/eq` (8, the M3 pilot directory — clean under M3's own
      flags, but has real M4 fallout like everywhere else),
      `atk/layout` (7), `atk/raster/scan` (2), `atk/raster/convert`
      (2) — 168, small/leaf grab-bag, 12 directories.

**Checkpoint** after this wave: full `make Clean; make World` +
runtime pass.

## Wave 5 — apps (8 directories, 59 errors, 1 session)

- [ ] **A1**: `ams/msclients/imapsync` (18), `ams/msclients/cui` (14),
      `atk/typescript` (13), `atk/help/src` (6), `doc/mkbrowse` (3),
      `atk/help/maint` (2), `atk/ez` (2), `ams/msclients/nns` (1) — 59,
      one batch (`atkams/messages/cmd` already clean, not included;
      the original draft's "9 directories" header miscounted — the
      list is and was 8).

## Wave 6 — atkams/ams (5 directories, 185 errors, 3 sessions)

- [ ] **AMS1**: `ams/libs/ms` (74) — alone, own dedicated session,
      same fdplumb-history briefing M2/M3 both required.
- [ ] **AMS2**: `atkams/messages/lib` (65) — alone. Tree-wide gate
      required (the `messages` GUI app's actual backend, same rule
      M2/M3 both applied).
- [ ] **AMS3**: `ams/libs/cui` (35), `ams/libs/nosnap` (10), `ams/libs/
      shr` (1) — 46.

**Checkpoint** after this wave: full `make Clean; make World` +
runtime pass.

## Wave 7 — contrib (11 directories, 348 errors, 3 sessions)

- [ ] **C1**: `contrib/zip/lib` (214) — alone. Largest single
      directory in the entire M4 census by a wide margin; tree-wide
      gate required (tree's known highest-defect-density directory,
      same rule M2/M3 both applied).
- [ ] **C2**: `contrib/zip/utility` (67) — alone. Shares lifecycle-
      method/lt/sched classes with `zip/lib`'s history (M3 C1/C2's
      `FinalizeObject`/`InitializeObject` findings) — do after C1,
      not before, so any shared pattern is already known.
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
