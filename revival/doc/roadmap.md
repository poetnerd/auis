# AUIS Revival Roadmap

Current as of 2026-07-11. See `porting-changelog.md` for the detailed
history behind each completed item.

---

## Current action plan

### Objective: messages demo a.k.a.amsdemo

- ~~Prerequisite: cui~~ — done 2026-07-07, was a missing RESOLVER_LIB link flag, not sgtty
- ~~gendemo~~ — done 2026-07-07: cwd bug (needs to run from `src/ams/demo/`)
  plus a `cui recon` segfault (modern-flex init-flag polarity bug, new bug
  class, see `porting-assessment.md` §13); both fixed, demo folder populates
  and reconstructs cleanly
- ~~Then find the bugs in the demos~~ — done 2026-07-11. Chased through three
  stacked bugs to find the real one:
  1. Caption display: `tm_year` (years since 1900) printed via `%02d`
     instead of `%02d` of `tm_year % 100` — harmless before 2000, wrong
     ("7-Jul-126") once `tm_year` exceeds 99. Fixed in `bldcapt.c`/`shrkdate.c`.
  2. Message ordering: `recon.c`'s sort-by-time comparator had no tiebreak
     for messages sharing the same one-second-resolution `AMS_DATE`; fixed
     with an `AMS_ID` fallback (kept as a general hardening, checked in
     separately from the caption fix).
  3. **The real root cause of both the "second, unrelated, intermittent
     crash" below and, it turned out, most of the ordering symptom too:**
     `parsedateheader()`/`parsedate()` — the bison-generated date-header
     parser — failed on essentially *every* input, not just certain dates,
     silently falling back to file-mtime dating every time. Root-caused to
     a fixed-width-table assumption bug in the *shared* `mkparser`/
     `cparser.c` parser engine used by five grammars tree-wide, not
     anything specific to `prsdate.gra`. Full writeup: `porting-assessment.md`
     §15. `gendemo`'s demo folder now populates with correct captions and
     correct Part 1…23 ordering, verified interactively.
   - Inter-line spaceing in folders and message header panes is double wide.

### Objective: Reliable operation

- Let's get all the function prototypes live with ANSI — plan of record
  now at Medium-term → ANSI C conversion (M1–M4)

---

## Little Annoyances to clean up

### Keymap:

- The arrow keys don't work yet
- We don't have a "Meta" key active yet

### make Clean transiently deletes src/atk/adew/Arb:

- adew's `clean::` rule does `$(RM) arb`, which on macOS's
  case-insensitive filesystem deletes the version-controlled `Arb`
  directory entry's file `Arb` (observed 2026-07-10 during the
  batch-11 gate; dependInstall regenerates it and fossil is clean
  again post-gate, so it's self-healing in a full gate but would
  leave the tree dirty after a bare `make Clean`). Pre-existing;
  fix is renaming one of the two or making the rule case-exact.

### filetype.c DeleteEntry:

- `filetype__DeleteEntry` (atk/basics/common/filetype.c:216,218,
  observed 2026-07-09, logged *before* the basics/common -pi
  rollout — pre-existing, not a regression): passes
  `&defaultMapping.newAttributes` (a `struct attributes **`) to
  `FreeAttributes()`, which walks it as a list node — UB/bogus
  frees if that path ever runs. The enclosing
  `if (strcmp(extension, "*"))` also looks inverted (wipes the
  default mapping when the extension is NOT `"*"`). Compiler flags
  it via -Wincompatible-pointer-types.

---

## Applications to Repair

Applications that currently crash instead of running. All are
pre-existing failures surfaced by first-ever runtime tests during the
M1 rollout — none are `-pi` regressions.

### Overlapping-strcpy crash family — FIXED 2026-07-10, runtime-confirmed

A tree-wide audit (grep for the same-variable idiom `strcpy(x, x+n)`
plus manual read-through of every call site that derives its second
argument from a pointer computed off the first, e.g. via
`index()`/`strchr()`/`rindex()` in the enclosing function) found
**15 overlapping-strcpy call sites across 9 files** — the 3 already
logged here (bush, org, htmlview) plus 6 more never surfaced by a
runtime test. All are the same idiom: an in-place left-shift
(`strcpy(dst, dst+n)` or two differently-named pointers that alias
the same buffer) to delete characters from a string. Apple's
fortified libc's `strcpy` aborts (`EXC_BREAKPOINT` →
`__strcpy_chk` → `__chk_fail_overlap`) whenever src/dst ranges
overlap, even though the classic forward byte-copy this idiom relies
on is safe *precisely for this direction* (dst < src) and has clearly
worked for ~35 years on non-fortified libcs. Fix is mechanical and
semantics-preserving everywhere: `strcpy(dst, src)` → `memmove(dst,
src, strlen(src)+1)` — memmove is defined for overlapping ranges and
produces byte-identical output to what the (unfortified) strcpy
already produced. All 9 files compile clean (zero `error:` lines)
after the fix, checked individually per file, **except** the two
noted as dead-tree below. Full gate (`make Clean && make
dependInstall`) run 2026-07-10: zero real `error:` lines tree-wide
(one hit, the known `-Wdeprecated-non-prototype` false positive);
`bush.do`, `org.do`, `htmlview.do`, `strtbl.do`, `label.do`, and
`gentlex` all reinstalled with fresh timestamps.

- **bush.c:269** (`bush__InitTree`) — root cause confirmed: not the
  originally-guessed `strcpy(p, p+n)` shape. `GivenDirName` is
  `self->given_dir_name`; `bush__Create` calls
  `bush_InitTree(self, GivenDirName)`, so `root_path` and
  `GivenDirName` are the *same pointer* inside `InitTree` — the
  `strcpy(GivenDirName, root_path)` at line 269 is a full self-copy,
  which fortify treats as total overlap. **Runtime-confirmed
  2026-07-10: bush launches** (was: instant crash on every launch).
  New pre-existing bug noticed during this check, NOT related to the
  strcpy fix (bush.c's rendering code was untouched): bush is
  confused about foreground/background colors, and leaf nodes draw
  completely wrong. Needs its own dedicated debugging session —
  logged here so it isn't mistaken for a side effect of this fix.
- **org.c:396** (`Strip()`, called from `Read_Body` at line 192) —
  root cause corrected from what was logged here previously. The
  earlier note named `strcpy(fName, tmpnam(seed))` (org.c:231) as the
  site; re-derivation found that call copies between two independent
  stack buffers (not overlapping) and doesn't match the crash's
  actual call path as convincingly as `Strip()`'s
  `strcpy(string, ptr)` where `ptr = string + (leading whitespace
  count)` — an exact instance of the same aliasing idiom as every
  other confirmed site, sitting directly in `Read_Body`'s control
  flow, and triggered by any node name with leading whitespace
  (routine in org's indented tree format). Fixed at line 396.
  **Runtime-confirmed 2026-07-10: all 3 example `.org` files load
  and work 100%** (was: crash on load). **Separately noted, not
  fixed:** `org.c:231`'s `strcpy(fName, tmpnam(seed))` is still a
  real bug — `seed` is sprintf'd as if it were a naming template,
  but `tmpnam()`'s buffer-argument form just overwrites it, so the
  sprintf'd content is silently discarded. Not an overlap (doesn't
  crash), but wasted work and a misleading read. Left alone pending
  a deliberate decision on temp-file strategy
  (`tempnam()`/`mkstemp()`) — out of scope for a mechanical overlap
  fix.
- **html.c** (`html__ReadSubString`/entity parser) — all 4 sites
  fixed: line 992 (`posStart`/`posEnd+1` alias the same `buf`) and
  the three `strcpy(buf, buf+pos)` sites (now ~1414, 1424, 1462).
  **Runtime-confirmed 2026-07-10: htmlview launches without
  crashing** (was: instant crash on any HTML content). New
  pre-existing issue noticed during this check, NOT related to the
  strcpy fix: no HTML file on hand actually rendered visible text —
  real-world HTML has likely diverged too far from what this ~1994
  parser understands. Separate task, needs its own fixture/triage
  (does it choke on DOCTYPE/charset, on modern tag soup, or
  something else); logged here so it isn't mistaken for a
  regression.
- **atk/supportviews/strtbl.c** (3 sites) and **label.c** (3 sites)
  — identical escape-stripping idiom (`while (t = index(t, '\\'))
  strcpy(t, t+1);` for `\`, `{`, `}`) in `stringtbl__AddString` and
  `label__SetText`. Never hit a coredump — found only by the
  tree-wide grep, and no fixture exists to exercise a label/string-
  table entry containing `\`, `{`, or `}`. **Accepted gate-only**
  (compiles clean, installed, mechanically identical to the 3
  runtime-confirmed sites above) — same precedent as `ptext`/
  `ltext`/`circlepi`/`mit-util` in point-10 batch 11.
- **atk/syntax/tlex/readtlx.c:260** — quote-stripping in the tlex
  grammar reader (`seq` line handling). `atk/syntax/tlex` is a
  build-time code generator (not a runtime app); found only by grep.
  Runtime check: rebuild whatever `.tlx` grammar exercises a quoted
  `seq` argument.
- **contrib/calc/calcv.c:502** — leading-zero digit strip in the
  calculator inset. **Verified 2026-07-11**: `contrib/calc` is now in
  the active build (`MK_CALC`, see "calc inset" under Completed) and
  this fix compiles clean.
- **contrib/mit/fxlib/server/commands.c:230** — same idiom (`.@` realm
  stripping in the MIT `fx` course server). **Still outside the active
  build**: fails to compile for an unrelated reason, a missing
  generated `fxserver_err.h` from a Kerberos code-gen step that's
  never been run in this checkout. Fixed by the same mechanical edit
  for correctness/consistency, but **compile-unverified** — no way to
  build the file right now. Verify whenever `fxlib/server` is ever
  brought into the active tree (same precedent as `wpedita.ch` in
  point-10 batch 11).

### typescript — crashes on launch (PTY failure + missing NULL check)

- First-ever launch of the `typescript` app (2026-07-10, point-10
  batch-6 runtime checks; not a -pi regression: atk/typescript was
  zero-fallout, no `.ch`/`.c` file in the directory was touched).
  Prints `Can't connect subchannel` (from `GetPtyandName` failing —
  suspect PTY allocation doesn't work the way this code expects
  under the current terminal/sandbox), then `EXC_BAD_ACCESS` inside
  `typescript__Create` at the `typescript_SetDataObject(self, ...)`
  call: `self = typescript_New()` came back NULL because
  `InitializeObject` returned FALSE (the pty failure above), and
  `Create` never checks for that before dispatching through `self`.
  Two bugs really — the underlying PTY/subchannel failure (macOS
  PTY compat, not LP64), and the missing NULL check that turns any
  such failure into a crash instead of a clean error return.

---

## Insets to Repair

Insets with known breakage, or not buildable/enabled at all. Each is
its own task; none block M1.

### calc — builds, loads, computes correctly; two open rendering bugs, UNRESOLVED (handoff 2026-07-12)

Brought into the active build 2026-07-11 (see Completed → `contrib/calc`
inset). First real interactive exercise (2026-07-11/12) found one real
crash (fixed) and one real Xft rendering bug (fixed, confirmed via lldb
trace), plus two more rendering bugs still open. Full trail, reproduction
steps, what's been proven/disproven, and ready-to-adapt lldb tracing
scripts: `revival/doc/calc-text-rendering-investigation.md`. Summary:

- **Fixed, confirmed:** `calc.c Writer` NULL-pointer crash on every
  document checkpoint (`sprintf`'s `int` return cast to `char *`).
- **Fixed, confirmed via lldb trace:** `xgraphic.c GetXftForeColor` used
  the stale `foregroundpixel` even in `graphic_WHITE` (erase) mode,
  since Xft never consults the core-X GC's color swap — erase-by-redraw
  was drawing in black, not white.
- **Root-caused and fixed 2026-07-12:** the ghost was AA
  erase-by-overdraw residue in `xgraphic_DrawChars`'s Xft path —
  "erasing" a string by redrawing the same glyphs in background color
  only exactly restores pixels where a glyph's alpha is 1; anti-aliased
  edge pixels stay partially gray forever, accumulating with each
  draw/erase cycle. Fixed by filling the glyph's advance-cell rectangle
  with the background color (`XftDrawRect`) instead of redrawing glyph
  shapes, whenever `transferMode == graphic_WHITE`. User-confirmed: no
  more gray residue behind the final answer.
- **New open bug, surfaced by the above fix:** during incremental
  multi-keystroke redraws, the display now shows only a *suffix* of the
  correct string — leading characters go missing. Typing `123+4=`
  shows `1`, `2`, `23`, `3+`, `23+4` instead of `1`, `12`, `123`,
  `123+`, `123+4`; the final `=` result (`127`) always draws correctly,
  and a window focus-loss/regain forced repaint shows the correct full
  string (proving calc's underlying value tracking is fine — this is
  purely an incremental Clear/Draw redraw defect). Not yet root-caused;
  see `calc-ghost-fix-prompt.md`'s "Outcome" section for the untested
  hypothesis and next diagnostic step.
- **Open:** the "=" button stays in reverse video indefinitely after
  being pressed. Not investigated; may be original 1988 "last button
  pressed" UX rather than a bug — needs a clean-build check to establish
  whether it's pre-existing.
- **Tried and reverted:** a hypothesis that `xgraphic_DrawChars`'s
  alignment math should use the Xft-resolved font's metrics instead of
  the core "dummy" font's metrics (since Xft's fontconfig substitute for
  a custom Andy font could plausibly have different ascent/descent).
  Implemented, tested live, found to make **zero observable difference**
  anywhere (not just calc) — reverted back to original core-only
  metrics, then re-confirmed inert via a side-by-side Frame-animation
  comparison in both states. Not the cause of anything seen this
  session, in either direction.

### zip — builds and loads now; solid-black render bug at `-O`, RESOLVED 2026-07-11

- **Progress 2026-07-11:** `MK_ZIP` defined in `config/site.h`,
  Makefiles regenerated down `contrib/zip/{lib,symbols,samples,utility}`.
  `contrib/zip/lib`'s ~21 `.ch` files were **100% untyped 1990s K&R
  style** (never touched by the M1 rollout since the directory was
  inert) — typing them against real implementations took ~479 compile
  errors down to 0 (Sonnet-delegated mechanical pass + 6 real `.c`-side
  bug fixes: missing K&R param declarations in `zipdf00.c`/`zipdi00.c`/
  `zipve02.c`, a pointer-through-`long`-rock laundering cast in
  `zipedit.c`, a transposed-argument live bug in `zipve00.c`'s
  `DrawString` call, and 13 files' `Build_Object` stub `peer` params
  retyped from `long`/`int` to `zip_type_figure`). All installed and
  indexed (`build/dlib/atk/index`), fonts installed, `zipview` class
  loads — the "not supported" placeholder is gone for good. Full
  details, including a Makefile `install.time`/`$?` staleness gotcha
  that silently skips reinstalling files after a `-k` build error, in
  memory `project_zip_inset_status`.
- **Also fixed 2026-07-11, general core-ATK bug, not zip-specific:**
  `atk/basics/x/xgraphic.c`'s Xft (anti-aliased text) drawing path
  never applied the pane-level GC clip to its `XftDraw` — zoomed-in
  text in any Xft-rendered, clip-relying view could bleed outside its
  own bounds. Fixed with a new `xgraphic_GetClipBoundingRect` helper
  mirroring `xgraphic_LocalSetClippingRect`'s clip computation;
  required relinking `libbasics.a`/`runapp` (statically linked).
  Runtime-confirmed: zip zoom no longer escapes its box.
- **RESOLVED 2026-07-11 — root cause found and fixed.** With
  `contrib/zip/lib` built at the default `-O`, the whole zip inset
  rendered as a **solid black rectangle**; the identical source at
  `-O0` rendered correctly. Root-caused via per-file, then
  per-function, `-O0`/`optnone` bisection (whole-directory → single
  file `zipd000.c` → single function
  `zip__Contextual_Figure_Line_Width`) plus disassembly comparison —
  full trail in `zip-black-render-investigation.md`. **Actual bug:**
  `zip.ch` declared `Superior_Image_Line_Width(...) returns char;`
  (signed), but its real implementation in `zipdf01.c` is explicitly
  `unsigned char`-returning and uses `255` as its "no width configured
  anywhere in the superior-image chain" sentinel. The signed
  declaration made classpp's typed dispatch macro cast the vtable slot
  to a *signed* `char` return, so at default `-O` the caller
  (`zip__Contextual_Figure_Line_Width`, `zipd000.c`) optimized its
  `!= 255` check into `cmn w0, #1` (`== -1`, the sign-extended form of
  255-as-signed-char) instead of the plain `cmp w0, #0xff` used for the
  figure's/image's own direct field reads a few lines earlier. The
  callee actually zero-extends its `unsigned char` 255 to
  `0x000000FF`, which never equals `0xFFFFFFFF`, so the caller always
  concluded "found a real width" and returned **255** even when
  nothing was configured — which flows straight into
  `zipview_SetLineWidth(self, 255)` and paints every stroke as a
  255-pixel-wide line, filling the whole figure black. `-O0` "fixed"
  it only by accident (naive truncate-then-compare instead of the
  sign-extension shortcut). **Fix:** one-line correction in `zip.ch`
  (`returns char` → `returns unsigned char`) + classpp regeneration of
  `zip.eh`/`zip.ih`; confirmed working end-to-end at normal default
  optimization, no special flags anywhere. Tree-wide scan of all 566
  `.ch` files for the same declared-vs-implemented narrow-return-type
  mismatch (both signed/unsigned directions) found no other live
  instances. Two incidental real bugs fixed along the way in
  `zipd000.c` (kept, neither was *the* bug): an LP64 pointer-truncated
  NULL check in `symtab_add` (`(int) entry <= 0` → `entry == NULL`),
  and an uninitialized-output path in
  `zip_Contextual_Figure_Line_Dash` when no dash pattern is set
  anywhere in the chain. Test fixture:
  `src/doc/papers/atk/Cattey.turnin` (also re-confirmed against
  `contrib/zip/samples/dragon.zip`). Committed — see fossil log for
  commit id. `contrib/zip/utility` (`lt`/`sched`) not started; same
  untyped-`.ch` treatment will likely be needed there too.
- **Confirmed 2026-07-11 (found while gating an unrelated `MK_CALC`
  change):** `contrib/zip/utility/ltapp.c:115,123` —
  `lt_Set_Debug(self->lt, debug)` / `ltv_Set_Debug(self->ltview,
  debug)` pass a `boolean` (int) `debug` through an untyped `Set_Debug`
  class method whose `.ch`-declared parameter is `void *` — same
  untyped-K&R-`.ch` gap as the rest of `contrib/zip/lib` before its
  M1-style typing pass, not a regression from anything touched this
  session. Blocks a full top-to-bottom `make dependInstall` gate
  (`SUBDIRS` order puts `zip` after `calc` in `contrib/Imakefile`).
  Untouched — out of scope for the `MK_CALC` work; needs the same
  `.ch`-typing treatment `contrib/zip/lib` already got.

### ness — bison grammar extension blocker

- `atk/ness/objects/ness.gra` uses a multi-character string token
  extension specific to the Andrew bison fork. Ness scripting is
  unavailable until this is resolved. Options: implement the
  extension in a bison `%skeleton` or rewrite the affected grammar
  rules. `atk/ness/{objects,type}` are inert (not in the default
  build). Related loose end: `celv`'s only callers live in
  `ness/objects`, and `nevent.c` was edited in point-10 batch 1 —
  compile-unverified until ness builds.

### htmlview — crashed reading any HTML file (ReadSubString overlapping strcpy) — FIXED 2026-07-10, runtime-confirmed

- First-ever real engagement of htmlview (2026-07-10, point-10
  batch-11 runtime checks; needs a `~/.ezinit` to map `.html` at
  all, per the help instructions — without it ez reads HTML as
  plain text, which is why this never surfaced before). See
  "Overlapping-strcpy crash family" under Applications to Repair for
  the fix (all 4 sites in `html.c` now use `memmove`, compiles clean,
  installed via the full gate). Runtime-confirmed 2026-07-10:
  htmlview launches without crashing (was: instant crash). A
  `.ezinit` in the home directory does NOT affect regular non-HTML
  ez startup (verified). New follow-on, separate from this fix: no
  HTML fixture on hand actually rendered visible text — likely
  real-world HTML has diverged too far from this ~1994 parser (see
  the html.c bullet under "Overlapping-strcpy crash family" above).

### layout — excess whitespace in complex layout (Sherman.Alloc)

- Noticed 2026-07-10 during point-10 batch-5 runtime checks: the
  complex layout inset near the end of `Sherman.Alloc` renders with
  much more whitespace margin around its contents than expected.
  Presumed pre-existing, not a -pi regression — zero files in
  atk/layout were touched in that batch (census found no pair
  macros, rocks, or typeless declarations; the gate was zero-fallout,
  meaning classpp needed no interface changes at all). First close
  look at this inset's rendering in the revival; needs its own
  investigation.

### figure — menu commands ignored until inset regains input focus

- Figure inset menus post but commands are ignored until the inset
  regains input focus (observed 2026-07-09 in `95Summer.ez`;
  first-time test, so presumed pre-existing rather than a -pi
  regression). Workaround that proves the diagnosis: scroll the
  object off screen and back — the inset then takes focus and obeys
  commands. Research: menu posting appears not to route/claim input
  focus for the posting view.

### eq — integral symbol missing (suspect font pipeline, not eq)

- Integral symbol missing in eq insets (observed 2026-07-08 in
  `Sherman.Alloc`, pre-existing — not a -pi regression). Suspects:
  symba PCF generation, or the `xset fp+` font-path setup. Other
  symbol glyphs render fine.

### raster — convertraster RF read-back hang

- `convertraster intype=RF` hangs reading back the `.ras` file the
  same binary just wrote (observed 2026-07-09, logged *before* the
  atk/raster/lib -pi rollout — pre-existing, not a regression).
  Writer output also looks LP64-suspicious: header fields appear
  8 bytes wide. Suspect `oldrf.c` raw struct I/O. Other codecs fine:
  raster identity rewrite, PostScript, MacPaint, Xbitmap write and
  Xbitmap→raster round-trip (byte-identical to identity output) all
  pass; baseline captured in `~/src/AUIS/test-baselines/raster-pi/`.
  The raster inset itself is proven working; this is the converter
  CLI only.

### clock — face never draws; pre-existing, not a regression (confirmed 2026-07-12)

- Clock inset instantiates but its face/hands never render (observed
  2026-07-12 in `build/testing.ez`). Manually bisected across the
  entire M1 rollout plus all recent zip/calc/Xft work — 9 checkpoints
  from `6338ade7de` (2026-07-07, before M1 point 1) through HEAD, each
  a full from-scratch `make Clean && make World` rebuild — blank at
  every single one, including the oldest. Confirmed pre-existing; no
  commit in that range explains it. `clockview__Redraw`
  (`contrib/time/clockv.c`) draws hands/face via `graphic_XOR`/
  `graphic_SOURCE` transfer modes and line/arc primitives
  (`DrawLineTo`, `DrawArc`, `EraseVisualRect`) — a different path from
  the Xft text-drawing code the recent calc/zip fixes touched. Not yet
  root-caused; next step is an lldb trace of `clockview__FullUpdate` →
  `Redraw` to confirm it's even being called and with what
  `clockview_GetLogicalBounds` rect.

---

## Questions

- How do I get a right click with my magic pad?
- How do I get drag scroll working without having to click and drag the elevator?

---

## Major milestones

- **2026-07-10**: **M1 complete** — classpp emits fully typed dispatch
  casts by default across the whole active tree (11 rollout points,
  ~50 directories, ~35 years of `.ch` signature drift found and
  fixed; five live LP64/caller bugs caught by the typed casts along
  the way: suite unsigned rocks, htmlview DisplayString transposition,
  clockv NewString, lexan ParseNumber int*/long*, noteview/stroffetv
  ICONSTYLE string literal). The compiler now type-checks every
  method call site tree-wide. Next: M2 prototype sweep.

- **2026-07-05**: `messages` application running with local mail store —
  "mail (Private BB; 0 new of 0)" confirmed in the folder panel. All three
  prerequisite streams closed simultaneously: AMS local backend alive,
  atkams/ interface working, ATK insets (lset, value, pushbutton/link)
  sufficient for the messages UI to render.

---

## Completed

- `ez2md` converter (`revival/tools/ez2md.py`): converts `.ez` files to
  Markdown; handles styles, footnotes, page breaks, nested insets; tested
  against 51 archive documents
- Full build: 278 `.do` files, 0 errors, `make dependInstall` exit 0
- `ez` launches, renders documents, scrollbar/menus/keyboard input all work
- `help` application: multiple frames, topic navigation working
- Dialog boxes: visible, correctly positioned, text readable
- Raster insets: rendering correctly
- Table insets: cell text visible (LP64 `update.c` fix)
- Eq insets: complex equations render correctly
- Fad insets: fully working (LP64 fix + Xft XOR ghost fix + 30ms timing floor)
- Fnote insets: marker glyph centered correctly (Xft metrics fix, 2026-07-04); **proven working** — footnote marker and popup text display correctly in `Cattey.Writing`
- `help` app: fully functional — Programs list panel scroll fixed (LP64 fix, 2026-07-04); regression tests below
- Bp (page break) insets: **proven working** — visible as page-break rule in `Cattey.Writing`
- Srctext insets: **proven working** — indentation and syntax coloring render correctly (LP64 audit)
- Figure insets: **proven working** — `95Summer.ez`'s figure renders correctly end to end (see Chronological log 2026-07-04). Two independent bugs stacked on top of each other: (1) parser-desync — official CMU `patch.633` (figattr tolerates unknown attributes from later format versions) plus new `smpltext.c` hardening (failed inset reads fall back to a raw `unknown` object instead of corrupting the rest of the parse); (2) LP64 — `figure__Read`'s `$origin` line was parsed with `sscanf(buf, "$origin %d %d", &val1, &val2)` into `long val1, val2`, the same %d-into-long pattern as the other LP64 audits, leaving stack garbage in the upper 32 bits of `originx`. That corrupted `originx` flows straight into `figview`'s `panx` (`SetDataObject` does `panx = originx`), pushing the entire figure's rendering ~4 billion pixels off-screen — content was being drawn, just nowhere near the visible clip region. Confirmed live via `lldb`: found `originx == 0x100000000` in the running `figview`'s memory. Fixed both `figure__Read` and the analogous `figure__ReadPartial`.
- `Sherman.Alloc` integration test: text, eq, fad, cel/arbiter spreadsheet
  all render; zip unsupported (expected). Calc is a separate contrib
  inset, not part of this document — see `contrib/calc` under Completed.
- Xft phase 1: body text rendering via client-side Xft (anti-aliased)
- Andy symbol fonts (`symba*.pcf`) built and installed in `build/X11fonts/`
- `overhead/malloc/malloc.ci` `addarena` arena-size pointer-arithmetic bug fixed (`patches/contrib/malloc.ci.auis6.3.diff`, 2026-07-04) — source-correctness only, see Historical patches audit below for why it has no runtime effect here
- `messages` application: **running** (2026-07-05) — local mail store (`Private BB`) visible in folder panel; three-pane layout, menus, and help text all rendering; AMS local backend + atkams/ interface + ATK insets all working
- `chart` application: launched and runtime-verified interactively (2026-07-10, point-10 batch 5) — startup, chart creation, format switching, palette labels; exercises the `SetChartAttribute`/`SetItemAttribute` rewrite
- `htmlview` DisplayString transposition fixed (2026-07-09) — three `message_DisplayString` calls had priority/string transposed since the 1990s; those HTML-editing status messages display for the first time
- `mkparser`/`cparser.c` fixed-width table bug (2026-07-11) — the shared parser engine used by all five AUIS grammars (`prsdate`, `eliy`, `eqparse`, `num`, `parsey`) assumed every bison table was a `short`; modern bison narrows some to 1 byte per grammar, corrupting every lookup into a narrowed table. Root-caused via `amsdemo`'s caption dates/ordering; fixed generically in the shared engine, not per-grammar. See `porting-assessment.md` §15 — a close cousin of the LP64 bug family (§12): different mechanism (generator-chosen storage width vs. ABI sign/zero-extension), same shape (1990s code assumed a fixed width; a modernized tool in the chain silently chose otherwise decades later).
- `contrib/calc` inset — brought into the active build 2026-07-11:
  `#define MK_CALC` in `config/site.h`, Makefiles regenerated down
  `contrib/calc` (`make Makefiles` from `contrib/`). Unlike `zip`,
  `calc.ch`/`calcv.ch` were already fully ANSI-typed (not 1990s K&R) —
  no classpp typing pass needed. `calcv.c` had 11 K&R functions
  forward-referenced (called before their `static` definition) via a
  stale non-`static` forward declaration, which clang now rejects as a
  linkage conflict (`static declaration ... follows non-static
  declaration`); fixed by replacing the one non-static forward decl
  with proper `static` ANSI prototypes for all 11 internal helpers.
  Also fixed: three `graphic_BLACK` (0xFF int constant) arguments
  passed directly to `void *` `Tile` parameters in `Fill_Area`,
  needing the same `(struct graphic *)` cast already used at
  `chart/charthst.c:238` for the identical idiom; and 5 sites
  (`calc.c:155`, `calcv.c:526,548,557,558`) using `sscanf(...,"%F",
  &x)` where `x` is `double` — `%F`/`%f` only fill 4 bytes, silently
  leaving the upper 4 bytes of each `double` as stack garbage, the
  same wrong-width-scanf shape as the documented LP64 Variant 4 class
  (just float/double instead of int/long) — fixed to `%lf`. The
  already-present `calcv.c:502` overlapping-strcpy fix (see
  Overlapping-strcpy crash family above) compiles clean now that the
  directory builds. `calc.do`/`calcv.do` build warning-clean of errors
  and install (`make install` in `contrib/calc`); full top-level
  `make dependInstall` gate confirms `contrib/calc` itself builds
  clean, though the gate doesn't reach past `contrib/zip/utility`
  (pre-existing, unrelated break — see Insets to Repair → zip).
  **Compiling/installing is done; runtime rendering is not** — see
  Insets to Repair → calc for the open bugs found once this got its
  first real interactive exercise.

**LP64 bug classes identified and swept:**
- Variant 1: Missing prototypes / pointer return truncation (23 sites)
- Variant 2: >8-arg untyped dispatch stack spill (classpp fix)
- Variant 3: `int` constant zero-extended through untyped dispatch
  (`observable_OBJECTDESTROYED`, `value_OBJECTDESTROYED`,
  `class_VERSIONNOTKNOWN`)
- Variant 4: `%d` with `long *` in scanf family (full tree audit done; one straggler found later and fixed — `figure.c`'s `$origin` parsing, see Completed and Chronological log 2026-07-04)
- Variant 5: `long`/`int` mismatch in display positioning through untyped
  dispatch (lpair, panel, dialog, table, fad, srctext, eq, metax, toez,
  typescript margins; style__ReadAttr operand; figure_NULLREF sentinel; full audit committed)

**Variant 3 follow-up audit (2026-07-04) — bare `-1` literals at call sites,
not just `#define`d sentinels:**

Found via `help`'s list-panel scroll bug: `textv.c:455`,
`self->frameDot = text_CreateMark((struct text *) dataObject, -1, 0)`.
`text_CreateMark` dispatches through the untyped `(struct mark *(*)())`
class-method macro, so the bare `int` literal `-1` isn't sign-extended to
the `long pos` field on LP64 — `frameDot->pos` ends up as garbage (observed:
`4294967295`, `4294967335`, `8589934591` in lldb — all the "low 32 bits look
like -1, upper 32 bits are register garbage" signature). `DoUpdate`'s
`mark_GetPos(self->frameDot) != -1` check then spuriously fires on every
first redraw. **Fixed**: `(long)-1` cast at the call site.

This is the same root mechanism as Variant 3 (`observable_OBJECTDESTROYED`
etc.) but the `-1` is a bare call-site literal rather than a named
`#define`d sentinel, so grepping for the constant's name doesn't find it —
it only shows up by grepping call sites directly. A full sweep (see
methodology below) turned up **6 more confirmed instances**, all fixed with
the same `(long)-1` cast, all compile-clean and rebuilt/reinstalled:

- `src/atk/text/content.c:649`, `src/atk/textaux/contentv.c:134,186` —
  `content_Enumerate`/`content_Denumerate`'s `opos`/`pos` (`long`) is
  checked `< 0` as an "enumerate everything" sentinel in
  `src/atk/text/content.c` (`content__Enumerate`/`content__Denumerate`);
  the corrupted value would read as a huge positive number instead,
  silently skipping the enumerate-all path.
- `src/atk/figure/figv.c:129-130` — `ChangeZoomProc`'s `rock` (`long`) is
  checked `rock<0`/`rock>0` to decide zoom in vs. zoom out; the "Zoom Out"
  menu item and `Esc-z` keybinding both passed a bare `-1` through
  `menulist_AddToML`/`keymap_BindToKey` (both untyped dispatch). If
  corrupted, **Zoom Out would zoom in instead**.
- `src/atk/raster/cmd/rasterv.c:1634-1635` — `ModifyCommand`'s `rock`
  (`long`) is checked `rock == -1` (exact equality) for "invert
  selection"; the "Negative" menu item and `Esc-n` keybinding both passed
  a bare `-1` through the same two untyped-dispatch macros. If corrupted,
  **Negative would silently do nothing** (falls through all the `==`
  branches).

**Audit methodology** (repeatable for future sweeps):
```sh
# class-dispatch-style calls (lower_Upper(...)) with a bare -1 argument
grep -rEn '\b[a-z][a-zA-Z0-9]*_[A-Z][a-zA-Z0-9]*\([^;()]*(,|\() *-1 *(,|\))[^;]*\)' \
  src/ --include=*.c | grep -v "(long)-1\|(long) -1\|== *-1\|!= *-1"
```
This narrowed ~925 raw `-1`-near-parens hits down to 22 candidates. Each
candidate needs manual triage: (1) find the macro's dispatch — untyped
`(TYPE (*)())` cast is the risky pattern, a plain field-assignment macro
(e.g. `mark_SetPos`, `rectangle_SetRectSize`) or a normal prototyped C
function is safe; (2) find the receiver and check whether it actually
*consumes* the value in a way sensitive to its exact bit pattern (a sign
check `< 0`/`== -1`, or arithmetic) vs. ignoring the parameter entirely
(several `view_FullUpdate(...,-1,-1)` width/height args in `figv.c` and
`rastvaux*.c` are ignored by both `figview__FullUpdate` and
`rasterview__FullUpdate`, which recompute geometry from the view instead —
confirmed harmless despite passing through the same untyped mechanism).

**Deferred / not yet triaged** (lower priority, left as future audit
targets, not confirmed either way):
- `src/atk/basics/common/rect.c` / `figv.c:905` — `rectangle_InsetRect`
  is a plain function but its header prototype
  (`rect.h:73: void rectangle_InsetRect(/*LHS, DeltaX, DeltaY*/);`) has no
  parameter types, so call sites don't widen `-1` to the real `long
  DeltaX, DeltaY`. Unlike the confirmed bugs above, the receiver does
  arithmetic (`+=`/`-=`) rather than a sign check, so a corrupted value
  would grossly mis-size a rectangle rather than silently no-op. (Not the
  cause of the `95Summer.ez` "messy screen" case — that turned out to be
  a figure-attribute version mismatch, see Completed; this remains an
  untriaged latent risk.)
- `environ_GetProfileInt(...,-1)` (messages/atkams, several sites) and
  `cwp_Search(...,-1,...)` (ams/delivery) — likely safe (looks like a
  plain `int`-returning function, not virtual dispatch) but unverified;
  deprioritized since `messages`/AMS revival is long-term, not part of
  the active inset sweep.
- `tlex_RecentPosition(...,-1 or -2,...)` (ness) — moot until the `ness`
  bison grammar extension blocker is resolved; the code doesn't run yet.

---

## Historical patches audit (`patches/`) — complete, 2026-07-04

CMU's official 6.3.x point-release patches plus community/site contrib
patches from the 1990s live in `patches/official/` and `patches/contrib/`
(the same set is duplicated verbatim under `andrew-6.4/patches/`,
`trunk/patches/`, and the top-level `AUIS/patches/` — they're identical,
no need to check more than one copy). This was triggered by finding that
CMU had already patched the exact figure-attribute-version bug hit while
fixing `95Summer.ez` (see Completed, above) — worth checking here *before*
deep-diving into a new bug, since CMU or a site admin may have already
found and fixed it decades ago. Every file every patch touches was
diffed against current source (not just the patch descriptions) to
determine actual relevance.

**Applied:**
- `official/patch.633` — "make figure accept figures created with later
  versions, including C++ 7.2+." Makes `figattr__Read` tolerate unknown
  attribute names instead of returning `dataobject_BADFORMAT`. Applied
  2026-07-04 as part of the `95Summer.ez` figure fix (see Completed).
- `contrib/malloc.ci.auis6.3.diff` — `overhead/malloc/malloc.ci`
  `addarena`: `A.arenaend - A.arenastart` is `struct freehdr *`
  subtraction, which the C standard defines in units of
  `sizeof(struct freehdr)`, not bytes — undercounts the arena-growth
  heuristic by ~20-24x. Fixed by casting both to `char *` before
  subtracting. **No observable runtime effect**: `ANDREW_MALLOC_ENV` is
  `#undef`'d in `config/site.h` and there is no `malloc.o`/`libmalloc.a`
  anywhere in `build/` — this codebase runs on system `malloc` via libc,
  Andrew's custom allocator is dead code here. Fixed anyway for source
  correctness (submitted originally by the current user, `wdc@mit.edu`,
  in 1995) in case `ANDREW_MALLOC_ENV` is ever revisited. `malloc.ci` is
  `#include`d into `malloc.c`/`pmalloc.c`, which currently fail to
  compile for unrelated pre-existing reasons (`AbortFullMessage`
  static/non-static prototype conflict) — out of scope, this directory
  isn't part of the active build either way.

**Investigated, found already fixed in our 6.3.1 baseline** (no action
needed — but worth knowing these bug patterns were already closed before
this project started, so don't waste time rediscovering them):
- `official/patch.631`, `atk/figure/figospli.c` hunk — guards
  `ctemp[last+1].t4 /= ctemp[last+1].t2` division (spline math for
  polyline figures) with `if (last >= 0)` to avoid an uninitialized-value
  divide on degenerate (too-few-point) curves. Already present at both
  call sites (lines ~148, ~210).
- `official/patch.631`, `atk/textobjects/unknown.c` (the exact file our
  new `smpltext.c` hardening depends on) — all 3 hunks already applied:
  `self->odata==NULL` typo (comparison instead of assignment, would have
  left a dangling pointer after `text_Destroy`) is correctly `=`; `static
  int tungetc` is correctly `static void`; `unknown__Read` already
  `return`s `ret` at the end.
- `official/patch.631`, `atk/support/hash.c` hunk — use-after-free fix
  (`return egg->value` after `free(egg)`) already applied; value is
  copied to a local before the free.
- `official/patch.631`, `atk/text/tabs.c` hunk — `PrevTab >= 0` bounds
  check already extended to `PrevTab >= 0 && PrevTab < self->number`.

**Not applicable:**
- `official/patch.631`, `atk/basics/common/im.c` hunk — `#ifdef
  hpux`-only, doesn't affect Darwin.
- `official/patch.632` — AMS `parseadd.c` fix, `official/patch.634` —
  AMS/`eatmail` coredump fix. Both out of scope; AMS/Messages revival is
  long-term (see Medium-term below), not part of the active inset sweep.
- `contrib/symlink.patch` — makes a shared-lib install symlink relative
  instead of absolute in `overhead/class/lib/Imakefile`. Confirmed dead
  code path for us: it's inside `#else /* LIBDL_ENV */`, and Darwin's
  config never defines `LIBDL_ENV` — we build a static `libclass.a`, not
  the shared `libclass.so` this patch targets.
- All SGI/NetBSD/Solaris/HP-UX/RS6000 platform ports (`SGI-port.6.3.3.*`,
  `NetBSD*.README`/`.diffs`, `Solaris*.diffs`/`.README`,
  `patch.631-hp-only`, `dvi2disp.patch`, `mit.patch`) — assembler flags,
  `stty`/`gtty` variants, `mode_t` sizing for Irix, `sys_errlist` typing,
  etc. for platforms unrelated to macOS/arm64.

---

## Subsystem dependency lattice

Indentation shows inheritance / dependency. `[PROVEN]` means confirmed
working through runtime testing. Unlabelled leaves are untested.

```
overhead/class  [PROVEN — loader, everything depends on this]

atk/basics  [PROVEN — view, graphic, scroll, lpair, panel, sbuttonv, xgraphic, xfontd, xim]

dataobject [PROVEN]          view [PROVEN]
    |                            |
    +-- bp / bpv  [PROVEN]       +-- scroll / sbuttonv  [PROVEN]
    +-- raster    [PROVEN]       +-- lpair              [PROVEN]
    +-- text ---- textview  [PROVEN]
    |        +-- fnote         [PROVEN — Cattey.Writing: footnote marker + popup]
    |        +-- textref / texttag  [PROVEN — ex14/ex14.doc cross-ref page numbers]
    |        +-- rofftext             (extends text)
    |        +-- srctext      [PROVEN — indentation + syntax coloring confirmed]
    |
    +-- eq -- eqview  [PROVEN — complex equation rendered from ia-archive/dec.91]
    +-- table         [PROVEN — cell text visible after LP64 fix]
    +-- fad -- fadview  [PROVEN]
    +-- pushbutton -- pushbuttonview
    |        +-- link -- linkview     (hyplink)
    +-- lookz -- lookzview
    +-- value -- valueview
    |        +-- sliderv
    |        |      +-- bargraphV
    |        +-- buttonV
    |               +-- controlV, enterintV, enterstrV, fourwayV
    |        +-- stringV -- clklistV
    +-- apt -- aptv
    |        +-- org -- orgv          (outliner)
    |        +-- chart                (charts)
    +-- cel                           (ADEW spreadsheet cell; needs value+text)
    |        +-- arbiter              (ADEW application builder)
    +-- lset                          (scrollable list view)
    +-- figure -- figview             [PROVEN — 95Summer.ez renders correctly]
               +-- figobj hierarchy
                      (figorect, figoplin, figoell, figogrp, figotext...)

application [PROVEN via ez, help]
    +-- ez          [PROVEN]
    +-- help        [PROVEN — multiple frames, topic nav, frame size correct]
    +-- fad         [PROVEN]
    +-- typescript / pipescript       (terminal emulator)
    +-- bush                          (shell)
    +-- org (orga)                    (outliner as standalone app)
    +-- chart (chartapp)
    +-- launchapp

contrib
    +-- writestamp [PROVEN]
    +-- calc [builds and installs clean, 2026-07-11 — runtime untested]
```

Note: `help` does not use `lset` — confirmed by source audit. `lset` is
unproven and requires its own test.

---

## Inset testing sequence

Ordered by dependency depth; each step proves a layer the next relies on.
`[PROVEN]` items are already confirmed; start from the first unconfirmed entry.

| # | Inset / App | Test document | What it proves | Search string |
|---|-------------|--------------|----------------|---------------|
| 1 | **fnote** | **[PROVEN]** `PAPERS/atk/Cattey.Writing` | inline text-in-text insets | look for superscript footnote markers in body text; click to expand |
| 2 | **textref / texttag** | **[PROVEN]** `src/atk/examples/ex14/ex14.doc` + `ex15/ex15.doc` | cross-ref insets; page-number references that update dynamically | in ex14: search "Program Listing for Example 14 at the end of this section on p." — the number after "p." is a live textref pointing to the texttag at the listing |
| 3 | **eq** | `ia-archive/dec.91` | **[PROVEN]** equation editor; text marks confirmed | look for rendered equations with fractions and subscripts |
| 4 | **table** | `ia-archive/aug.90` | **[PROVEN]** cell text visible | spreadsheet cells with numbers and formulas |
| 5 | value (slider/button) | `ia-archive/sep.90` or `ia-archive/jan.90` | value views; valueview dispatch chain | slider or button widgets embedded in text |
| 6 | lset | `ia-archive/nov.91` or `ia-archive/jan.90` | scrollable list widget | scrollable selection list inset |
| 7 | pushbutton / link | `PAPERS/conf/1995/widgets.ez` | hyplink chain: pushbutton→link→linkview | clickable button insets |
| 8 | **fad** | `src/atk/adew/Title.doc` | **[PROVEN]** LP64 fix complete | animated/fading text title inset |
| 9 | org | `src/atk/org/example1.org` | outliner; proves apt→aptv base | outline nodes with expand/collapse |
| 10 | rofftext | `bin/rofftext -d <manpage>` | roff formatter on top of text | formatted man page output |
| 11 | chart | `build/doc/atk/classes.org` or synthesize | proves apt branch independently of org | bar or line chart inset |
| 12 | **cel / adew** | `src/atk/adew/vallist` | **[PROVEN via Sherman.Alloc]** ADEW stack: value+text+cel+arbiter renders | spreadsheet cells with live calculation |
| 13 | typescript | `bin/typescript -d` | terminal emulator; **crashes "Can't connect subchannel"** — likely macOS PTY compat issue, not LP64 | terminal window inset |
| 14 | bush | `bin/bush -d` | shell application | interactive shell |
| 15 | **figure** | `NEWSLETTERS/EZ/95Summer.ez` | **[PROVEN]** two stacked bugs fixed: parser desync (patch.633 + smpltext.c) and LP64 `$origin` scanf corruption (figure.c); renders correctly end to end | drawing/diagram insets in newsletter |
| 16 | **Sherman.Alloc** | `PAPERS/atk/Sherman.Alloc` | **[PROVEN]** text+eq+fad+cel/arbiter all render; zip unsupported (expected) | multi-inset compound document |
| 18 | calc | Esc-Tab, type `calc`, Enter in any `ez` doc (see `contrib/calc/calc.help`) | builds/installs clean 2026-07-11, not yet runtime-tested | calculator button-grid inset |
| 17 | **Cattey.Writing** | `PAPERS/atk/Cattey.Writing` | **[PROVEN]** writestamp, fnote, raster, |

**No good test document exists for:** `lookz`, `launchapp`, `prefed`
— these need synthetic test files or targeted app launches.
`srctext` is now [PROVEN].

**Known non-starters:** `ness` (bison extension blocker, still inert)
— detailed under **Insets to Repair**. `zip` now builds and loads
(see Insets to Repair → zip) though `zip/utility` is still broken.
`clock`/`timeoday` (contrib, lower priority, still inert). `calc` now
builds (see row 18 above; row kept lower-priority pending runtime test).

---

## Heisenbugs (intermittent, not currently reproducible)

### Display mess

**Observed:** On rare occasions, text will draw into the menu area. 
But I can't reproduce it reliably.

### ^V scroll hang on fresh window (spoon host)
**Observed:** on host `spoon`, repeatedly: opening `help` or `ez` and
pressing `^V` to scroll the default window caused the process to die
(confirmed dead in debugger) while the X window persisted until XQuartz
was restarted. `help` would still believe a server process was available.
Triggered even on the default help window with no special document.
Stopped reproducing spontaneously once debugging attempts began.

**Not reproduced on:** Mac-mini. Not triggered by VS Code terminal (was
running from native Terminal.app).

**Possible cause (superseded, see below):** the checkpoint timer UAF
(`observable_OBJECTDESTROYED` zero-extension bug, fixed 2026-06-30) was a
plausible match — scrolling a fresh window can trigger a checkpoint, and
the UAF produced a crash rather than a hang.

### Xlib display-lock self-deadlock — **deferred** (reproduced 2026-07-04, root cause identified)

Reproduced by accident during the figure-inset LP64 audit: scrolling in
both `ez` (viewing `NEWSLETTERS/EZ/95Summer.ez`) and, independently, a
`help` window hung the same session. **Not a crash this time** — attaching
lldb to the stuck `help` process (no relaunch, no interrupt needed — it
was already wedged) showed:

```
frame #0: libsystem_kernel.dylib`__psynch_mutexwait
frame #1: libsystem_pthread.dylib`_pthread_mutex_firstfit_lock_wait
frame #2: libsystem_pthread.dylib`_pthread_mutex_firstfit_lock_slow
frame #3: libX11.6.dylib`_XLockDisplay
frame #4: libX11.6.dylib`XkbGetUpdatedMap
frame #5: libX11.6.dylib`XkbKeysymToModifiers
frame #6: libX11.6.dylib`XRefreshKeyboardMapping
frame #7: runapp`HandleWindowEvent
frame #8: runapp`xim__HandleFiles
frame #9: runapp`im__Interact
frame #10: runapp`im__KeyboardProcessor
frame #11: runapp`application__Run
frame #12: helpa.do`helpapp__Run
frame #13: runapp`main
```

Only **one thread exists** in the process, and it's blocked forever trying
to acquire Xlib's own display-connection mutex (`_XLockDisplay`) — a
self-deadlock, not a cross-thread one. `HandleWindowEvent` is responding
to an X `MappingNotify` (keyboard mapping changed) by calling
`XRefreshKeyboardMapping`, which tries to lock the display — but something
earlier in the *same* call chain already holds that lock (almost
certainly Xlib's own event-dispatch machinery calling back into
`xim__HandleFiles`/`HandleWindowEvent` while still holding it internally),
and the lock isn't held recursively. This matches the "intermittent,
input-related, seems tied to fresh windows" character of the original
report far better than the checkpoint-timer theory, and is **not** fixed
by any patch committed so far — this is a live, distinct bug.

**Trigger appears to be keyboard-mapping churn** (`MappingNotify`), not
scrolling logic itself — consistent with why `^V` specifically was the
original trigger (a modifier-involving key combo) and why it's
intermittent (depends on X server-side keymap-change timing, not app
state).

**Not yet investigated:** which AUIS/Xlib call site re-enters
`_XLockDisplay` while already holding it; whether this is triggerable
deliberately (vs. needing to wait for an incidental `MappingNotify`);
whether it's an AUIS-side bug (calling into Xlib from inside a callback
that already holds the lock) or an XQuartz/libX11 packaging issue specific
to this environment. Needs a dedicated debugging session — see
`revival/doc/runtime-debugging-guide.md` for the general lldb debugging
process/cookbook developed for this project.

---

## Regression test checklists

### help application
Run: `DISPLAY=:0; build/bin/runapp helpa -d`

1. **Startup**: window opens showing "A Guided Tour of Andrew" in the main panel
2. **Overviews pane**: right panel top section shows entries (Andrew Tour, Multimedia, Mail, Programming); text is readable
3. **Programs pane**: right panel bottom section shows programs list starting from the **top** of the list (first entry visible, scrollbar at top) — regression for LP64 frameDot bug
4. **Overviews link**: click "Andrew Tour" in Overviews → main panel changes to that document
5. **Programs link**: click any entry in Programs → main panel changes to that help topic
6. **In-text links**: click a cross-reference link in the main panel text → navigates to linked topic
7. **Expand/Shrink Programs pane**: use Panels menu → "Expand Programs" / "Shrink Programs" → pane resizes correctly
8. **Show History**: use Panels menu → "Show History" → history pane appears listing previously visited topics
9. **History links**: click an entry in the History pane → main panel navigates to that topic

### ez application (menus)
Run: `DISPLAY=:0; build/bin/ez build/testing.ez`

**Before testing, check `~/.ezinit`.** If it exists, it must start
with `include /Users/wdc/src/AUIS/andrew-6.4/build/lib/global.ezinit`
or every global menu/keybinding — including "Media" — silently
disappears. This is original 1988 `atk/basics/common/init.c` cascade
design (`addmenu`/`addkey` docstring at line 76), not a bug: `app.c`
loads `~/.ezinit` first, and if it loads successfully, **returns
immediately** without ever reaching `global.ezinit` (where the
`addmenu` lines for Media live — see `atk/ez/ezinit`). A personal
init file replaces the global one unless it explicitly `include`s it.
This looks exactly like a code regression in menu construction and
cost a full manual bisection (`6338ade7de` through HEAD, 9 checkpoints,
each a clean from-scratch rebuild, all showing Media present) before
being traced to a stray one-line `~/.ezinit` (added earlier for
htmlview testing, forgotten about) on 2026-07-12.

1. **Media menu present**: textview "Insert"-area menu bar has a
   "Media" submenu (Equation, Header/Footer, PostScript, Raster,
   Spreadsheet, Animation, Hyperlink, Layout, Ness, Note, Writestamp,
   By name...)
2. **Clock inset**: currently fails — see Insets to Repair → clock
   (pre-existing, unrelated to the above)

---

## Active (instances running)

- **Xft phase 2: menu rendering** — deprioritized; menus are working
  acceptably with the current rendering path

---

## Near-term

### ~~LP64 positioning sweep~~ — complete
All five LP64 variant classes identified, swept, and committed. `Sherman.Alloc`
and `95Summer.ez` used as integration tests; both render correctly.

### ~~Messages application prerequisites~~ — **DONE (2026-07-05)**

Goal: get `messages` running with a local mail store. Three streams of work:

**Stream 1 — remaining ATK inset prerequisites (unproven):**
These insets appear in the messages UI and/or in rendered mail:
- `lset` (scrollable list) — mail folder/message list display; test with `ia-archive/nov.91` or `jan.90`
- `value` / `valueview` (slider, button) — UI controls; test with `ia-archive/sep.90` or `jan.90`
- `pushbutton` / `link` / `linkview` — hyplink navigation; test with `PAPERS/conf/1995/widgets.ez`

**Stream 2 — AMS local mail store: found, 2026-07-04.** The build already
separates the message store/UI from the AMDS delivery daemon via independent
Imake flags:

| Flag | Gates | Needed for local-store `messages`? |
|---|---|---|
| `AMS_ENV` | whether `ams/`/`atkams/` build at all (`Imakefile:37-39`), plus `overhead/mail` (`libmail.a`, `libcparser.a`) | **yes** — currently `#undef`'d in `config/site.h`, overriding `allsys.h`'s default of 1 |
| `AMS_DELIVERY_ENV` | `ams/delivery/` (sendmail/vicemail/queuem/trymail) and `ams/utils/` (nntp/muserver/purge/reauth/undigest) — the actual AMDS transport | no — leave off |
| `SNAP_ENV` | networked "remote message server" variant (`ams/ms`, `libcuis.a`) vs. the local `nosnap` path (`libcuin.a`) | no — leave off, local path is the default |
| `WHITEPAGES_ENV` | `overhead/wpi`/`wputil`; auto-forced only by `AFS_ENV` or `AMS_DELIVERY_ENV` | no — stays off since neither of those is on |

With only `AMS_ENV` on, `atkams/messages/lib`'s Imakefile builds `amsn.do`
against `NLIBS` (`libcui.a`, `libcuin.a`, `libmssrv.a`, `libeli.a`,
`librxp.a`) — the local, non-networked message store — and never touches
delivery or white-pages code.

The local-mailbox backend the user remembers is real and already in the
source: `ams/libs/ms/newmail.c`'s `ProcessNewMail(..., PROCESSNEW_MBOX, ...)`
imports from a plain mailbox file returned by `GetPersonalMailbox()` in
`ams/libs/ms/findmbox.c` — a standard Unix mbox (`~/mailbox`, or the
`mailboxdir` profile override), with no AFS/AMDS/white-pages involved. This
is almost certainly the exact seam the user's fetchmail fork fed into at
MIT Athena. `AFS_ENV`/`WHITEPAGES_ENV` references in `ams/libs/ms/mswp.c`,
`init.c`, and `atkams/messages/lib/{ams,stubs}.c` are all `#ifdef`-optional
(`mswp.c:980` even has an explicit `#ifndef WHITEPAGES_ENV` fallback path),
confirming this is a first-class supported configuration, not a hack.

**Next action:** ~~flip `#undef AMS_ENV` → `#define AMS_ENV 1` in
`config/site.h`~~ — **done, 2026-07-05.** `AMS_ENV`/`CONTRIB_ENV` are on in
`config/site.h`. All newly-exposed subtrees (`atkams/messages/lib`,
`overhead/mail`, `overhead/eli`, `ams/libs/cui`, `ams/libs/ms`,
`contrib/srctext`/`eatmail`/`time`) fixed and verified compiling/linking
clean per-directory. User's first full top-level `make dependInstall`
(no `-k`) surfaced exactly 2 more link-time errors (`nns`'s `getla()`
needing `getloadavg()` instead of dead `/dev/kmem`+`nlist()`, and a
missing `${RESOLVER_LIB}` on `nns`'s link line) — both fixed same-day,
full details in `porting-changelog.md`'s 2026-07-05 entry. `ams/msclients/vui`
and `contrib/tm` — curses terminal clients on the removed BSD `sgtty` API —
remain conditionalized out of the build (`MK_VUI`/`MK_TM`) rather than fixed;
not needed for the GUI `messages` path. Rationale in `porting-assessment.md`
§7a. `ams/msclients/cui` was originally grouped with them but didn't actually
share that dependency — see the 2026-07-07 fix below.

First runtime test of `messages` (2026-07-05) segfaulted: `EXC_BAD_ACCESS`
in `_platform_strlen` via `mailconf.c`'s `CkAMSCellConfig` (`AndrewDir`/
`LocalDir` called with no prototype in scope — same LP64 #1 pattern fixed
at 23 sites on 2026-06-30, recurring because `overhead/mail` was never
built/audited before `AMS_ENV` went on). Fixed `mailconf.c` plus a sweep
of 5 more active files with the same bare-call pattern (`ams/libs/ms/init.c`,
`hdlnew.c`; `atkams/messages/lib/stubs.c`; `overhead/mail/metamail/metamail/{metamail,mailto}.c`;
`overhead/eli/lib/prims1.c`); all rebuilt clean. Full details in
`porting-changelog.md`. Separately, `contrib/bdffont` turned out to be
unbuildable (missing `bdfparse.act`, no generator, no fossil history) and
was conditionalized out (`MK_BDFFONT`); see `porting-assessment.md` §7b.
**✓ MILESTONE (2026-07-05):** `messages` is running. "mail (Private BB; 0 new
of 0)" confirmed in the folder panel. Three-pane layout, menu bar, and help
text all rendering correctly.

**Follow-up (2026-07-05): same-tip rebuild on host `spoon` segfaulted immediately.**
Real `SIGSEGV` (confirmed via `lldb`, not a codesigning/kernel-kill artifact),
in `CUI_GetHeaders`'s header-scanning loop. Two `long`/`int` mismatches in
`CUI_GetHeaders`'s `startbyte`/`nbytes`/`status` params, both invisible at
compile time (no prototype in scope at the mismatched boundaries) — same
LP64 Variant 5 family as `lpair__Init`/`style__SetNewIndentation`, but this
time crossing a plain unprototyped C call, not just the `void (*)()` vtable
macros. `ms/libs/ms/headers.c`'s `MS_HeadersSince` (`int`/`int *`) was the
outlier; `cuilib.c`'s `CUI_GetHeaders` and the dormant SNAP variant
(`cuisnap.c`) already agreed on `long`/`long *`. Fixed end-to-end
(`headers.c`, `ams.ch`/`amsn.ch`/`amss.ch`, `ams.c`/`amsn.c`/`amss.c`,
`capaux.c`, `foldaux.c`, dormant `ams/ms/ms.c`); rebuilt clean. Full detail
in `porting-assessment.md` §12 and `porting-changelog.md`. **Next up:**
retest on `spoon`.

**Stream 3 — atkams/ interface audit: resolved by the Stream 2 survey.**
`ams/Imakefile` and `atkams/messages/lib/Imakefile` show the boundary is
exactly the Imake flags above — `messages` links against the local
`libmssrv.a`/`libcuin.a` regardless of whether AMDS is present. No IMAP
adapter is needed for the local-store path; that fallback remains available
later if the mbox approach hits a wall.

**Contrib objects:** `CONTRIB_ENV` on brings in `calc demos gestures wpedit
time eatmail mit srctext` (see `contrib/Imakefile`); `tm` and `bdffont`
deferred (§ above). Still TBD whether any of these besides `srctext`
(already a proven inset, see Completed) matter for the messages path
specifically.

### gendemo — done; demo folder populates and reconstructs cleanly

**✓ (2026-07-07)** `cui` builds/links/installs — the blocker was a missing
`${RESOLVER_LIB}` link flag on its Imakefile (same bug class as `nns`'s
2026-07-05 fix), not the BSD `sgtty` API as originally assumed. `cui` doesn't
use curses at all, and its one sgtty reference was already dead code
(`POSIX_ENV` is unconditionally on for darwin). Full detail in
`porting-changelog.md`'s 2026-07-07 entry.

**✓ (2026-07-07)** `gendemo` itself has two independent bugs, both fixed:

1. It reads its 23 demo posts (`d1`/`d1.heads` ... `d23`/`d23.heads`)
   relative to the current directory, not `$ANDREWDIR` — those files live
   only in `src/ams/demo/` and are never installed. Must `cd src/ams/demo`
   before invoking it (`ANDREWDIR` is only used to find `cui`/`arpadate`).
2. With cwd fixed, `cui`'s final `recon` step segfaulted on the very first
   address caption it tried to build. Root cause: a **new bug class** — flex
   regenerates `overhead/mail/lib/parsel.c` at build time (no fossil
   history), and modern flex inverted the meaning of an internal init flag
   that a hand-written `pareset_lexer()` was poking directly instead of
   using flex's real public API. Fixed (`yyrestart(yyin)`); swept the whole
   tree and found/fixed one sibling instance (`overhead/eli/lib/elil.flex`,
   the ELI/FLAMES filter-language lexer — pre-emptive, no confirmed crash
   yet). Full writeup in `porting-assessment.md` §13, session detail in
   `porting-changelog.md`'s 2026-07-07 entry. Verified: `recon` completes
   ("Reconstructed folder ~/.MESSAGES/amsdemo with twenty-three entries")
   repeatably, no crash.

**Next up — two threads:**

1. ~~New, unrelated intermittent crash found while re-verifying `recon`~~ —
   **root-caused and fixed 2026-07-11.** The `memmove` heap overrun and
   unkillable-`UE`-state hang (escalated 2026-07-09) were both symptoms of
   the same bug: `mkparser`/`cparser.c` — the shared, hand-written parser
   engine used by *every* AUIS grammar, not just `prsdate` — assumes every
   LALR table is a `short`, but modern bison narrows several tables to
   1-byte types when a grammar's value range allows it. Reading a 1-byte
   array through a `(short *)` cast merges pairs of entries into garbage,
   which manifested as `parsedateheader()` failing on *every* input (not
   just certain years), sometimes cleanly (fast syntax error), sometimes by
   a runaway state-machine loop that grew the parser stack without bound
   (the `memmove` overrun) badly enough to occasionally take unbounded time
   (the `UE` hang). Full root-cause writeup: `porting-assessment.md` §15.
   Since the crash is gone, `gendemo`'s auto-invocation from
   `src/ams/demo/Imakefile`'s `install.time::` target (disabled 2026-07-09,
   `#if 0`/`#endif`) can likely be re-enabled — not done as part of this
   fix; left as a follow-up decision since it changes `dependInstall`'s
   default behavior.
2. Once `recon` is fully stable, verify `messages` can actually browse and
   read the populated `amsdemo` folder end-to-end (captions, dates, bodies).
   **Partially done 2026-07-11:** captions and Part 1…23 ordering verified
   correct interactively. Body content for each message not separately
   re-verified this session.

### IMAP / AMS backend investigation (week of 2026-07-14)

With `messages` running against the local mbox backend, investigate whether
an IMAP adapter behind `atkams/` is viable for connecting to a live mail
server. The `atkams/`–`ams/` boundary is already audited (see Stream 3 above);
this is the next architectural step toward real-world mail use.

### printf/fprintf %d/%ld audit
2,597 printf-family hits with `long` values and `%d` format specifiers
logged during the scanf audit. These produce wrong output for large values
but do not corrupt memory. Address as a batch; not blocking messages work.

### Xft phase 2 (deferred)
Menu text rendering via Xft. Menus are currently acceptable without it.
Only remaining X core font path dependency is Andy symbol and cursor fonts.

### Update quickstart.md
Remove resolved known-issues entries as each fix lands.

### Not current focus
- `typescript`, `bush`, `org` crashes — details consolidated under
  **Applications to Repair**; defer until after messages + M1 close
- `chart` — runtime-verified 2026-07-10 (see Completed); `launchapp`
  — inert (`MK_BASIC_UTILS` off), defer

---

## Medium-term

### ANSI C conversion (M1–M4) — plan of record, assessed 2026-07-08

Absorbs the former "Prototype sweep" and "Classpp typed dispatch" entries
here plus the long-term "ANSI C modernization" entry into one ordered
plan. Analysis — June mass-conversion postmortem, keystone finding, tool
verdicts, delegation guardrails — in `porting-assessment.md` §14.

Keystone: `.ch` files already carry full ANSI method signatures; classpp
parses and discards them. Emitting them (M1) type-checks every method
call site and definition tree-wide *before* any mass file editing starts
— the compiler becomes the auditor instead of grep.

- **M1 — classpp emits types.** Typed prototypes in `.eh` (today:
  `long text__Read();`) and typed casts in all `.ih` dispatch macros
  (extends the 2026-06-30 ≥9-arg fix to every method). Kills LP64
  Variants 2/3/5 structurally and catches signature drift (the
  `CUI_GetHeaders` class of bug) at compile time. The machinery
  already exists in classpp (`usePrototypesImport`/`Export`, `-p`,
  `-D` signature/`.desc` files, `$(CLASSFLAGS)` hook in andrew.rls —
  see porting-assessment §14 "M1 mechanics"); the code change is just
  splitting `-p` into `-pi`/`-pe` and dropping the `>= 8` gates behind
  `-pi`, defaults untouched. The real work is the per-directory
  rollout — see "M1 rollout points" below.
- **M2 — Prototype sweep.** `-Werror=implicit-function-declaration`
  subtree-by-subtree (`src/config/darwin/system.mcr` COMPILERFLAGS);
  fix by adding `#include`s or `extern` declarations at call sites.
  Closes Variant 1 permanently — it has cost debugging time on every
  subtree activation so far.
- **M3 — Definition conversion.** `ansify` (`revival/tools/ansify`,
  built and validated 2026-07-08 — see porting-assessment §14):
  static-fix tools → class methods/classprocs by signature-DB lookup
  (`ansify --build-db`, 565 classes) → file-local helpers from their
  own K&R declarations, strict parser (cproto rejected: can't read
  macOS SDK headers) → per-file compile gate with auto-restore.
  Per-subtree step: add `-pe` to the directory's `CLASSFLAGS` (its
  `.eh` must be prototyped in the same step — narrow param types like
  `char` conflict with typeless `.eh` decls otherwise; proven on
  `eq__WriteFILE`), force regen, `ansify --dir`, clean build, triage
  DRIFT reports, commit. One subtree per commit, dependency order:
  overhead → atk/basics+support → atk/text → insets → apps →
  atkams/ams → contrib. Ratchet each completed subtree from `-Wno-*`
  to
  `-Werror=implicit-int,strict-prototypes,int-conversion,incompatible-function-pointer-types`.
- **M4 — Global strictness.** Tree-wide `-Werror` on the type-safety
  set; `-Wformat` then catches any remaining scanf `%d`/`%ld` (Variant
  4) automatically. Keep `-std=gnu89` until conversion completes;
  consider c99 after. Writable-strings stays deferred.

Scale: ~13,700 K&R definitions across ~1,301 of 1,544 `.c` files; ~5,100
are class methods converted by `.ch` lookup, not inference. M2/M3 runs
are delegable (Sonnet-class) under the §14 guardrails; M1 and
`.ch`-vs-`.c` signature disagreements stay top-level.

#### M1 rollout points (Import half: `CLASSFLAGS = -pi` per directory)

Rollout state lives only in committed Imakefiles; classpp defaults
never change until step 10. Per-step rhythm: set `CLASSFLAGS` → force
regen (delete the directory's generated `.ih`/`.eh` or touch its
`.ch`s) → `make Clean; make dependInstall` → fix consumer fallout →
runtime spot-check → commit. Clean build passing is the definition of
done. Ordering is by external-consumer count (survey 2026-07-08,
porting-assessment §14), not directory nesting: pilots on
zero-consumer leaves, then the core, largest last.

1. [x] classpp: `-pi`/`-pe` split, `>= 8` gates dropped under `-pi`
       (done 2026-07-08; verified byte-identical default output,
       binary installed)
2. [x] Pilot A — `atk/eq` (done 2026-07-08; clean-build gate green,
       eq inset visually verified in `Sherman.Alloc`. Findings — see
       porting-assessment §14 "Pilot A findings": macro-parameter
       capture bug class, fixed structurally in classpp; DoScript
       stray-`*` DRIFT in `eq.ch`, a ~35-year-old typo caught by the
       first typed rebuild; two process wrinkles for the runbook)
3. [x] Pilot B — `atk/figure` (done 2026-07-09; clean-build gate
       green, figure inset visually verified in `95Summer.ez` —
       behavior unchanged; new pre-existing menu-focus bug logged
       under Little Annoyances. Findings — see
       porting-assessment §14 "Pilot B findings": typeless `.ch`
       declarations (`MoveHandle`), rock-idiom `long`→`void *`, and
       a six-file `Build(action, v)` vs runtime `(v, action)`
       transposition — the `.ch` was wrong for ~35 years; all fixes
       interface-side only)
4. [x] First cross-directory step — `atk/raster/lib` (done 2026-07-09;
       ZERO fallout — the seven codec `.ch`s were accurate; consumers
       `raster/cmd` + `raster/convert` rebuilt clean against typed
       `.ih`; gate green. First before/after test protocol:
       `convertraster` battery byte-identical to pre-rollout baseline
       (`~/src/AUIS/test-baselines/raster-pi/`, incl. Xbitmap
       round-trip == identity invariant); user visually verified
       92Sep.ez raster inset and face.raster negate/flip-lr/flip-ud
       before and after. Pre-existing RF read hang logged under
       Little Annoyances pre-flip)
5. [x] `atk/frame` (5 classes, 95 external; done 2026-07-09; gate
       green, frame chrome (windows, menus, scrollbars) visually
       verified. Findings — see porting-assessment §14: `Enumerate`'s
       `long functionData` rock retyped to `void *`; six call sites
       across five files (`framecmd.c` x4, `atk/textaux/contentv.c`,
       `atk/extensions/{compile,tags,deskey}.c`) carried a redundant
       `(long)` cast on the pointer they passed — a one-caller-in-five
       omission (`framecmd.c:768`, bare pointer, no cast) is what
       first exposed the pattern as a live disagreement, which in
       turn prompted a runbook revision pre-authorizing deletion of
       these casts as part of the rock-idiom interface fix, since
       they launder a pointer through `long` rather than mean
       anything. First rollout point with import fallout in
       directories other than the flagged one, confirming the
       blast-radius-asymmetry prediction in §14)
6. [x] `atk/supportviews` (17 classes, 178 external; done
       2026-07-09; gate green, `help` app scrollbars/panel
       expand-shrink/matte chrome and `ez` visually verified. One
       fallout: `sbutton.ch Enumerate`'s `long rock` retyped to
       `void *` — same rock-idiom pattern as `atk/frame`/`atk/figure`,
       all three tree-wide callers already passed bare pointers, no
       `(long)` casts to delete)
7. [x] `atk/text` (21 classes, 321 external; done 2026-07-09; gate
       green, `help` regression checklist and `ez` (`Cattey.Writing`
       fnote, `ex14.doc` textref) visually verified. Two rock-idiom
       fallouts, same pattern as points 5/6: `pcompch.ch`
       `ATKToASCII`/`ASCIIToATK` and `text.ch` `EnumerateEnvironments`
       `long rock` retyped to `void *`; `EnumerateEnvironments` again
       showed the bare-vs-laundered-cast split (one caller in
       `atk/lookz/lookzv.c` passed the pointer bare, five callers in
       `atk/text` laundered via `(long)` — all five casts deleted per
       the point-5 pre-authorized exception). No new fallout pattern)
8. [x] `atk/support` (19 classes, 450 external; done 2026-07-09; gate
       green, `help` app, `ia-archive/jan.90`'s `lset`/`buttonV`
       widget (first proof of both, previously unproven insets), and
       a `File > Save All` buffer command all visually verified. Two
       rock-idiom fallouts: `buffer.ch` `Enumerate`/`EnumerateViews`
       — same pattern as points 5-7 (8 of 9 callers laundered a
       pointer via `(long)`, one bare-pointer omission at
       `framecmd.c:552` exposed it). `list.ch` `Enumerate` — a
       genuine hard stop, escalated mid-session: rock declared
       `char *` but two callers (`dired.c` `FindPosProc`, `buttonv.c`
       `findkey`) pass real `long` integers compared numerically,
       disagreeing with ~40 pointer-passing callers elsewhere.
       Resolved as a new **dual-use rock** pattern (ruling added to
       the runbook's rock-idiom bullet): retype to `void *`; the
       integer call sites get an explicit `(void *)` cast
       (`dired.c:348`, `buttonv.c:489`, plus a third found while
       sweeping, `prefs.c:513` in `atk/prefed`, which isn't part of
       the default build — `MK_PREFS`/`MK_AUX_UTILS` are off — so
       harmless but unverified locally); pointer call sites drop
       their now-redundant casts; callbacks (`FindPosProc`,
       `findkey`) untouched, since they're invoked through typeless
       `procedure` pointers outside `-pi` checking. Also: the gate
       surfaced an unrelated pre-existing hang — see `gendemo` below
       — worked around, not a rollout fallout.)
9. [x] `atk/basics/common` (41 classes, 2,351 external; done
       2026-07-09; gate green after four cycles, `help`/`ez` runtime
       battery visually verified. The directory's own `.ch`s had ZERO
       local fallout; all fallout was consumer-side rock collisions.
       16 rocks retyped `void *` across 10 `.ch`s (menulist
       AddToML/Chain*/Unchain/GetChained, im HandleMenu/
       AddZombieHandler/EnqueueEvent/SetInteractionEvent/
       SetDeleteWindowCallback, keystate SetOverride, init Load,
       view PostResource, namespace/proctable Enumerate, message
       AskForStringCompleted); `keymap_BindToKey` stays `long` under
       the new integer-majority ruling. ~100 call-site cast edits in
       ~50 consumer files, driven by static censuses, not the gate
       log (censuses + mechanical edits delegated to cheaper-model
       agents — see §14 "Point 9 findings" and the runbook's new
       methodology notes). Real bugs caught: clockv.c NewString
       missing prototype (LP64 pointer truncation), suite.c laundered
       out-params, htmlview.c DisplayString arg transposition (fixed
       as separate commit per ruling), filetype.c DeleteEntry
       attributes** misuse (logged, untouched))
10. [x] Breadth: remaining atk (`value`, `adew`, `apt`, `basics/wm`,
       `basics/x`, `hyplink`, `syntax/parse`, ...), then `atkams`/
       `ams`, `contrib` (`zip/lib` first), `examples` — delegable
       batches (one session + one gate per batch, ruled 2026-07-09;
       exhaustive batch list + per-session prompts:
       revival/doc/m1-point10-batches.md)
       - Batch 1 (2026-07-09): `atk/value`, `atk/adew`,
         `atk/apt/{apt,suite,tree}`, `atk/controllers` (inert — not
         in default build). Two live-LP64-bug classes fixed (suite
         unsigned rocks feeding every handler callback; LinkTree
         missing param decl), the suite+treev attribute-pair
         convention expanded at 95 dispatch sites across 12 files,
         and two long-dormant caller bugs caught (bushv title-as-code
         no-op; chartv `*X`-for-`&X` CaptureString corruption) — see
         porting-assessment §14 "Point 10 batch 1 findings" and the
         runbook's new unsigned-rock and variadic-by-macro bullets.
       - Batch 2 (2026-07-10): `atk/basics/x`, `atk/basics/wm`
         (inert — `WM_ENV` off). ZERO fallout: census clean (no pair
         macros, no rocks, overrides match the point-9-typed
         defining classes), gate green first pass, full ez + help
         regression verified. Imakefile-only, like raster/lib.
         Census note: classpp shares comma types like C
         (`GrayPattern(short a, b)` casts as `(short, short)`) —
         verified empirically, not drift. [Correction, batch 3:
         classpp does NOT comma-share — override macros take the
         defining class's typed decl, which is what GrayPattern
         showed; see batch 3 findings.]
       - Batch 3 (2026-07-10): `atkams/messages/lib` alone. Gate
         green first pass, all fallout local (19 errors, one ring);
         messages runtime fixture verified. Seven `.ch` drift fixes
         (cvEng typeless, DisplayNewBody bare params, PostMenus
         by-value struct, AlterSubscriptionStatus 35-year arg
         transposition, ReadFromFile file-private `Boolean`,
         SetCUIRock rock→`void *` + one pre-authorized `(char *)`
         cast at ams.c:120). New pattern for the runbook: an
         unknown type token in a `.ch` (`proc`, a file-private
         typedef) emits an implicit-`int` cast param under gnu89 —
         a typed cast that lies, truncating function pointers on
         LP64; fixed `proc`→`procedure` in ams/amsn/amss.ch and
         cross-directory in `orgv.ch` (fldtreev inherits it;
         classpp reads the INSTALLED parent `.ch`, so the fix
         needed `make install` in atk/org). See porting-assessment
         §14 "Point 10 batch 3 findings".
       - Batch 4 (2026-07-10): `atk/image`, `atk/hyplink`,
         `atk/console/lib` + `atk/console/cmd` (both inert —
         `MK_CONSOLE`/`MK_BASIC_UTILS` off, no generated Makefile, no
         `console` binary), `atk/raster/cmd`. Gate green first pass.
         Two known-taxonomy fallout fixes, no new patterns: `image`'s
         `sliderv.ch SetCallback` rock (`long`→`void *`, sole caller
         `cmapv.c` passes a bare pointer); `hyplink`'s `pshbttn.ch
         ParseRGB` signature drift (`unsigned char rgb_vect` declared
         by value, impl + all four callers use it as an array,
         matching the already-correct `GetFGColor`/`GetBGColor`
         siblings). `raster/cmd`'s own four `.ch`s were zero-fallout
         (fully typed already). `convertraster` battery run for
         due-diligence but doesn't actually verify `raster/cmd` —
         `convrast.c` only includes `raster/lib` headers, never
         `raster/cmd`'s; byte-identical regardless. Runtime: hyplink
         verified via `PAPERS/conf/1995/widgets.ez`
         (pushbutton→link→linkview), raster/cmd verified via
         `NEWSLETTERS/EZ/92Sep.ez`'s raster inset; `image` accepted
         gate-only (no known fixture for its picture-format codecs,
         zero-caller local fix only).
       - Batch 5 (2026-07-10): `atk/chart`, `atk/org`, `atk/bush`,
         `atk/fad`, `atk/layout`, `atk/table`. Gate green first pass
         (all fallout caught and fixed during chart's local
         `make -k install`, before the tree-wide gate ran). chart
         carried the suite-identical variadic-by-macro attribute
         family across two classes (`chart.ch` Chart/Item Attribute,
         `chartv.ch` Chart/ChangeChart Attribute) — true arity
         declared, ~45 dispatch call sites mechanically rewritten
         (all local to atk/chart, zero external consumers), pair
         macros fenced for `*_Specification`-table-only use, per the
         ruling already in hand. `chartobj.ch` also had a ~35-year
         signature-drift typo (`SetDataObject(struct char *)` →
         `struct chart *`) and four typeless declarations
         (`WhichItem`, `SetChartOptions`, `HitChart`, `ObserveChart`);
         `HitChart`'s typeless override repeated across five
         subclasses (chartcsn, charthst, chartmap, chartpie,
         chartstk). Two dual-use-attribute-value call sites
         (`chartobj.c`, `chartpie.c`, `PrintString` argument) were
         missing the `(char *)` cast their siblings already had —
         found only once the tree-wide gate walked past the local
         rebuild's stopping point. `org.ch NodeName(node)` was fully
         typeless (zero callers tree-wide, so zero fallout risk);
         typed from the impl. `bush`, `fad`, `layout`, `table` were
         all zero-fallout — census clean, gate green, no `.ch`/`.c`
         edits needed. No new patterns for porting-assessment §14.
         Runtime: chart verified interactively (create/format/label a
         chart); fad+table verified via `Sherman.Alloc`. Two
         pre-existing bugs surfaced by first-ever runtime tests, not
         regressions (both logged under Little Annoyances): org
         crashes loading a file (`Read_Body`'s `tmpnam`/`strcpy`
         misuse, same overlapping-strcpy-under-fortify class as
         bush's already-logged InitTree crash); Sherman.Alloc's
         complex layout inset renders with excess whitespace margin
         (zero atk/layout files touched this batch, so presumed
         pre-existing). bush's pre-existing startup crash confirmed
         unchanged.
       - Batch 6 (2026-07-10): `atk/textobjects`, `atk/textaux`,
         `atk/rofftext`, `atk/srctext`, `atk/typescript`, `atk/lookz`.
         Gate green first pass. No attribute-pair macros anywhere in
         this batch. Three genuine drift fixes, no new patterns:
         `rofftext/rofftxta.ch` had two typeless declarations
         (`ParseArgs(argc,argv)`, `InitializeObject(self)`) typed to
         match every sibling app's `ParseArgs(int argc, char **argv)`
         and the impl's `struct rofftextapp *self`; `srctext/hlptext.ch`
         and `srctext/rawtextv.ch` each had a signature-drift
         `InitializeObject`/`FinalizeObject` typed to the WRONG
         sibling struct (`struct srctext *`/`struct srctextview *`
         instead of their own class), caught immediately by the
         local rebuild since both are cast-incompatible pointer
         types. `textobjects/dired.ch`'s `EnumerateAll`/
         `EnumerateMarked` rock retyped `long`→`void *` (all three
         tree-wide callers in `diredv.c` already pass pointers, same
         rock-idiom precedent as frame/figure/supportviews).
         `textobjects/chlist.ch`'s `AddItemAtIndex` had a ~35-year
         transposed-parameter signature drift (`.ch` declared
         `(str, index, ...)`, impl and its sole caller use
         `(index, str, ...)`) — fixed to match. `srctext.ch`'s
         `Lookup` classprocedure was missing the `**`/`[]` on its
         hash-table parameter (declared `Dict hashTable` by value;
         impl and all 8 tree-wide callers use `Dict *hashTable[]`,
         and its two sibling classprocs `BuildTable`/`HashInsert`
         already had it right) — an isolated typo, not a pattern.
         `textaux`, `typescript` were zero-fallout. Runtime: lookz
         verified via `PAPERS/atk/Hansen.Algebra`; textaux's
         `contentv` (Table of Contents) verified via
         `PAPERS/atk/Cattey.Writing` (Hansen.Algebra has no section
         headings, so ToC has nothing to discover — Cattey.Writing is
         the fixture to reuse); help app confirmed no regression
         (textobjects' only live consumer, `panel`). srctext and
         textobjects' `dired`/`chlist`/`unknown` accepted gate-only —
         no srctext/ctext document exists anywhere in ia-archive,
         PAPERS, or NEWSLETTERS, and `dired`/`chlist`/`unknown` have
         no live consumer in the default build (chlist's only callers
         are contrib/wpedit and contrib/bdffont, neither built; dired
         and unknown have zero call sites anywhere, presumably
         reflective/by-name loading for unknown). typescript crashes
         on launch (new pre-existing bug, logged under Little
         Annoyances): `typescript__Create` doesn't check `typescript_New()`
         for NULL before calling `SetDataObject` on it, and `New()`
         returns NULL because `InitializeObject` fails at
         `GetPtyandName` ("Can't connect subchannel") — zero
         atk/typescript files were touched this batch (fully
         zero-fallout), so this cannot be caused by the diff.
       - Batch 7 (2026-07-10, live subset only): pre-flag census found
         8 of the planned 12 directories (`ezprint`, `preview`, `toez`,
         `datacat`, `launchapp`, `createinset/null`, `music`, `prefed`)
         are currently inert — `MK_BASIC_UTILS`/`MK_AUTHORING`/
         `MK_AUX_UTILS` are all off in `allsys.h` and no per-app
         override (`MK_EZPRINT`, `MK_PREVIEW`, etc.) is defined, so none
         are in `atk/Imakefile`'s `SUBDIRS` and none have a generated
         Makefile; deferred to a future batch (user decision: split
         rather than flip the macros on). The `ez2ascii`/`ez2ps`
         binaries already in `build/bin` are leftovers from
         `contrib/mit/util` (batch 11, gated by `CONTRIB_ENV`, also
         off) plus a csh wrapper — not built from `atk/ezprint` at all,
         so the planned CLI byte-diff battery had no live target and
         was skipped along with the rest of the deferred 8.
         [Correction, 2026-07-10 active-tree census: `CONTRIB_ENV`
         is ON in `config/site.h` (since 2026-07-05) and
         `contrib/mit/util` IS in the default build — the gate log
         shows `building (dependInstall)` descents into it, and
         `ez2ascii`/`ez2ps` are rebuilt live by every gate, not
         leftovers. The "not built from atk/ezprint" half of the
         finding stands; the "CONTRIB_ENV off" half was a
         mis-census — see the Active tree section below for the
         reliable liveness check.] Ran the
         full runbook on the 4 live directories instead: `atk/ez`,
         `atk/utils`, `atk/help/src`, `atk/extensions` (all
         unconditionally in `BASICS`). Gate green first pass. Six
         genuine drift fixes caught by census before any build, no new
         patterns: `utils/dialog.ch` and `utils/dialogv.ch` each had
         `InitializeObject`/`FinalizeObject` typed to the wrong
         sibling struct (`struct sbutton *self` instead of their own
         class) — same pattern as batch 6's hlptext/rawtextv;
         `help/src/hlptextv.ch` had the identical wrong-sibling-struct
         drift (`struct srctextview *self` instead of
         `struct hlptextview *self`) — a different file from batch 6's
         srctext/hlptext.ch, just a confusingly similar name.
         `help/src/help.ch` and `help/src/helpdb.ch` each declared
         `InitializeClass(struct help(db) *self)` with a bogus extra
         `self` param the implementation doesn't take (impls take only
         `classID`, matching the universal zero-param
         `InitializeClass()` convention every other class uses).
         `extensions/ezdiff.ch` had the opposite arity drift:
         `FinalizeObject()` was missing its `self` param entirely
         (impl is `ezdiff__FinalizeObject(classID, self)`).
         `utils/dialogv.ch`'s `PostInput` `choicerock` was a rock-idiom
         retype (`long`→`void *`; its one tree-wide caller,
         `frame.c:1746`, already passes a bare pointer). Traced how
         `InitializeObject`/`FinalizeObject`/`InitializeClass` arity
         actually matters under `-pi` despite the user-facing
         convenience macros having zero external callers tree-wide:
         classpp's auto-generated `Destroy`/`Finalize` wrapper code
         (baked into the `.eh`) calls the raw `classname__FinalizeObject`
         function by the fixed `(classID, self)` convention regardless
         of what the `.ch` declares, so a `.ch` arity mismatch becomes a
         real prototype conflict once `-pi` is on — not dormant.
         Confirmed `struct thisobject *self` (used pervasively for
         `InitializeObject`/`FinalizeObject`/`ObservedChanged` self
         params across dozens of files, including several already
         flagged in batch 1) is a real, working classpp idiom that
         resolves to `void *` even under `-pi` — not a bug, left alone
         everywhere it appears (`strinput.ch` included). Runtime:
         `ez` launch confirmed (including an Extensions-menu command);
         a Quit-with-unsaved-changes confirmation dialog exercised the
         `dialog.ch`/`dialogv.ch` fix and the `PostInput` rock retype
         directly; `help` launch confirmed, including a
         `hlptextview`-rendered topic with working hyperlinks. All
         three user-verified, no regressions. Checkins: bug fixes
         105b96414a, rollout 165e3862b6.
       - Batch 9 (2026-07-10, live subset only, batch 8 skipped ahead
         of per user request): pre-flag census found 21 of the 22
         planned directories inert — all 19 `atk/examples/ex*` dirs
         (`MK_EXAMPLES` off in `allsys.h`, no per-app override) and
         both `rdemo` dirs (`rdemo` isn't referenced anywhere in
         `src/Imakefile`'s `SUBDIRS` at all — a standalone package
         with its own `config.csh`/`config.h` generation, never part
         of `make dependInstall`). Only `overhead/class/testing` is
         live. Same user ruling as batch 7: split, flag/verify the
         live dir now, defer the rest as Batch 9b. Both `.ch` files
         (testobj.ch, testobj2.ch) were already clean — no
         `InitializeObject`/`FinalizeObject`, no pair macros, no
         typeless params — zero fixes needed, pure flag-and-gate.
         Gate green first pass; confirmed real typed casts in the
         local `.ih` files directly (this directory has no
         `InstallClassFiles`, so nothing copies to `build/include`).
         Runtime check skipped by user choice: the only artifact,
         `testmain`, is a class-loader self-test ending in
         `while(1);`, not part of the normal install path — matches
         the batch's own "gate is the whole verification" guidance.
         Checkin: rollout-only, no bug-fix commit needed.
       - Batch 8 (2026-07-10, live subset — first Sonnet-delegated
         batch): `atk/syntax/{parse,tlex,sym}`; ness dropped (inert,
         bison blocker). Gate green first pass. Six known-taxonomy
         drift fixes (wrong-sibling structs, int/long index params,
         unsigned name params, FindAll rock to impl's `long *`,
         Create's error handler to `void (*)()`) — details in
         m1-point10-batches.md. One hard stop escalated and ruled:
         `lexan.c ParseNumber` passed a `long *` where
         `TransEscape` takes `int *` — live LP64 bug, fixed with an
         `int` temporary as its own commit. Runtime: ctext syntax
         coloring/indent user-verified on a scratch `.c` in ez.
         Checkins: d3386126d5 (.ch), 7ad519b869 (lexan.c),
         6b1564ec89 (rollout).
       - Batch 11 (2026-07-10, live subset — Sonnet-delegated):
         `contrib/{mit/annot, mit/util, srctext/html, srctext/ptext,
         srctext/ltext, time, wpedit, demos/circlepi}`. Gate green,
         ez2ascii battery byte-identical before/after (new baseline
         `~/src/AUIS/test-baselines/ez2-pi/`; ez2ps excluded — it
         execs inert ezprint + eqn/ditroff). Six .ch drift fixes
         (typeless SetDesired/DecidedSize/RecommendSize; wrong-struct
         FinalizeObject ×2 incl. wpedita.ch borrowing AMS `struct
         folders *`; AddImage missing `*`; ReindentLine `struct
         mark *`→`long pos` matching sibling ptext.ch) + one
         pre-authorized dual-use rock cast (html.c). One hard stop
         escalated and ruled: noteview.c/stroffetv.c defined
         ICONSTYLE/TITLESTYLE as the STRING LITERAL
         `"fontdesc_Plain"` — a ~35-year copy/paste bug truncating a
         pointer into every note/troff inset's font-style `int`;
         fixed to the bare symbol + the missing `<fontdesc.ih>`
         includes, own commit. Census correction: wpedit is inert
         one level deeper than the gate log shows — descent happens
         but its Imakefile body is entirely
         `#ifdef AMS_DELIVERY_ENV`-gated (flag committed inside the
         guards, compile-unverified; runbook liveness rule refined:
         descent ≠ compilation). ptext/time/circlepi/mit-util
         zero-fallout, typed casts verified in all 27 installed
         `.ih`s. Runtime: note inset (exercises the ICONSTYLE fix
         path) and clock inset user-verified; htmlview surfaced a
         NEW PRE-EXISTING crash (ReadSubString overlapping strcpy,
         logged under Insets to Repair — crash precedes the batch's
         only html.c edit in execution order, so mechanically not a
         regression); ptext/ltext/circlepi/mit-util gate-only by
         user sign-off. Checkins: 7eaec122fd (live-bug fix),
         f46de124ed (rollout).
       - **Point 10 is COMPLETE (2026-07-10):** every live `.ch`
         directory in the active tree now builds under `-pi`. Only
         point 11 remains for M1.
11. [x] Default flip (done 2026-07-10): classpp emits typed import
       casts (`-pi` behavior) by default (`class.c`
       `usePrototypesImportAll = TRUE`; `-pi` accepted as a no-op);
       all 50 per-directory `-pi` flags deleted. Hybrid execution:
       classpp edit + unit proof top-level (flagless regeneration of
       testobj.ch byte-identical to flag-era output), mechanical
       remainder Sonnet-delegated. Gate green. **Decisive proof: all
       341 installed `.ih`/`.eh` headers byte-identical to the
       pre-flip baseline** (`~/src/AUIS/test-baselines/
       point11-headers/before/`) — the default is bit-for-bit
       equivalent to the flags it replaces. ez/help/messages
       regression battery user-verified. Checkins: ff35ac3904
       (classpp flip), 4f6c344e44 (flag deletions).
       **M1 IS COMPLETE.**
12. [ ] Export (`-pe`) is *not* sequenced here — it rides with each
       subtree's M3 conversion, since its blast radius is only the
       implementing directory

Steps 2–4 are top-level work (learning the fix patterns); 5–10 are
increasingly delegable once the patterns are documented.

#### Active tree — census 2026-07-10

M1's scope is the **active tree**: directories the default build
actually descends into. Liveness ground truth is the gate log —
`grep '^building (dependInstall)' dependInstall.log` — NOT Makefile
presence: stale Makefiles from before subtrees were conditionalized
out survive in `atkbook`, `tm`, `bdffont`, and `prefed`, and a
mis-census around exactly this fooled the batch-7 session into
recording `CONTRIB_ENV` as off (it is on, and contrib builds — see
the correction in batch 7 above).

Census result: **108 directories contain `.ch` files.** 46 are live
(35 flagged + the 11-directory gap above); 62 are inert (4 carry
courtesy flags: `basics/wm`, `console/lib`, `console/cmd`,
`controllers`). The inert 62 break down by gate:

- Off in `allsys.h`: `MK_EXAMPLES` (ex1–ex19); `MK_BASIC_UTILS`/
  `MK_AUTHORING`/`MK_AUX_UTILS` (ezprint, preview, toez, datacat,
  launchapp, createinset/null, music, prefed); `WM_ENV`
  (basics/wm); `MK_CONSOLE` (console/lib, console/cmd)
- Off in `contrib/Imakefile`: `MK_ZIP` (zip/lib, zip/utility —
  see Insets to Repair), `MK_CALC`, `MK_CHAMP`, `MK_GESTURES`
  (gtext), `MK_TM`, `MK_BDFFONT`; `alink` is SunOS-only; `atkbook`
  (18 dirs), `mit/neos`, `pobbconf`, `snap2` aren't in `SUBDIRS`
  at all
- Never wired into `src/Imakefile`: `rdemo/{hide,rdemosh}`

**Consequence of point 11:** once the classpp default flips, inert
directories need no Imakefile flag ever — any inert subtree enabled
later gets typed casts automatically, and the runbook's census/fix
work simply happens at enable time as part of turning it on.
Batches 7b, 9b, 10, and 11's inert remainder are therefore obsolete
as flagging exercises; each survives only as a "run the runbook
census when enabling" note attached to its gate. **M1 ends at point
11 with typed dispatch across the whole active tree.** Full ANSI C —
prototypes everywhere (M2), K&R definition conversion (M3), global
`-Werror` (M4) — continues from that foundation.

### ~~Integration test: `Sherman.Alloc`~~ — proven
All insets in `Sherman.Alloc` render correctly (fad, cel, arbiter, eq, table);
zip unsupported as expected. Multi-inset compound documents confirmed working.

### ~~zip inset~~ — root-caused, moved
Moved to **Insets to Repair → zip**: it isn't broken, it was never
built (`MK_ZIP` never defined anywhere). Repair path documented there.

### ~~ness.gra bison extension~~ — moved
Moved to **Insets to Repair → ness** (same content).

### Andy font path automation
`xset fp+ build/X11fonts && xset fp rehash` is currently a manual step
required each XQuartz session. Automate via a wrapper script or by
installing the PCF files into XQuartz's default font path
(`/opt/X11/share/fonts/`).

### ~~fad view "wrong icon" bug~~ — root-caused, not a regression (2026-07-12)
Suspected `fad` (animation) drawing bug — `ams/demo/d10`'s diagram
showed a literal "M" instead of an icon for its "Client Program" node.
Traced to `con10` (a console-app icon font the diagram happens to
reference) never being built, because `MK_CONSOLE` gates out all of
`atk/console` including `console/fonts` — not a `fad` defect at all.
Full root cause and permanent-fix options: `porting-assessment.md` →
"`MK_CONSOLE` being off silently breaks `con10`/`con12`...". Fixed and
confirmed 2026-07-12: once `con10` resolves, the animation renders and
plays correctly. Codified as `revival/tools/install-console-fonts`
(builds only `console/fonts`, never touches the rest of `console`) —
re-run it any time a full clean rebuild wipes `build/X11fonts/`; not a
true upstream fix (that needs `console/fonts` carved out of the
`MK_CONSOLE` gate, or `MK_CONSOLE` itself enabled), but no longer a
manual multi-step recipe either.

### ~~Frame size reporting in help~~ — fixed

---

## Long-term / architectural

### ANSI C modernization (full K&R conversion)

**Elevated to medium-term** — see Medium-term → ANSI C conversion
(M1–M4). The `modernize` tool is no longer the starting point; see
`porting-assessment.md` §14 tool verdicts.

### Messages application

**Elevated to near-term focus** — see Near-term section for the active
work plan. Moved here for architectural notes only.

`messages` is the AUIS mail/bulletin-board client. Full AMS revival is
off the table — the AFS/shared-filesystem delivery model is a dead end.
See Near-term → Messages prerequisites for the two viable backend paths
(local store vs. IMAP adapter).

---

## Stretch goals

### Pie menus
AUIS's menu architecture is well-suited to Don Hopkins' pie menu design.
The menu system is clean and the attachment points are known. A
collaborative implementation project once the core system is stable.

### Additional applications
`bush` (shell), `typescript` (terminal emulator), `org` (outliner),
`chart`, `layout` -- each is a symlink to `runapp` and built; exercise
and fix as interest warrants after core insets are stable.
