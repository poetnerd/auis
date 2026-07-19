# zip inset black-render bug — RESOLVED (2026-07-11)

## Root cause (confirmed)
`zip.ch` declared `Superior_Image_Line_Width(zip_type_image image) returns
char;` (plain `char`, signed on this platform), but the real implementation
`zip__Superior_Image_Line_Width` (`zipdf01.c`) is explicitly
`unsigned char`-returning and uses `255` as its "no width set anywhere in
the superior-image chain" sentinel.

Because the class-spec said `char`, the generated typed dispatch macro in
`zip.eh` cast the vtable slot to **signed** `char (*)(...)`, so at the call
site in `zip__Contextual_Figure_Line_Width` (`zipd000.c`), the optimizer (at default
`-O`, not `-O0`) generated `cmn w0, #0x1` (checks return value == **-1**)
instead of the plain `cmp w0, #0xff` (checks == **255**) used for the
figure's/image's own direct field reads a few lines earlier. The callee
zero-extends its `unsigned char` sentinel `255` into `0x000000FF`; the
caller's signed-shortcut check was looking for `0xFFFFFFFF`. These never
match, so the caller always concluded "found a real width" and returned
**255** — even when nothing was actually configured anywhere in the
figure/image/superior-image/stream chain. `255` then flows straight into
`zipview_SetLineWidth(self, 255)` in `zipv000.c`'s `Ensure_Line_Attributes`,
producing a 255-pixel-wide stroke that fills the entire figure with solid
black.

Confirmed via disassembly comparison of `zip__Contextual_Figure_Line_Width`
compiled at default `-O`: the two direct-field sentinel checks use
`cmp w0,#0xff`/`b.eq`, but the `zip_Superior_Image_Line_Width(...)` call
result is checked with `cmn w0,#0x1`/`b.ne` (i.e. `== -1`, not `== 255`) —
a genuine caller/callee ABI mismatch from the `.ch`/implementation
declared-type disagreement, not a compiler bug. `-O0` "fixed" it only by
accident (it emits a naive truncate-then-compare instead of the
sign-extension-based shortcut, which happens to still work).

## The fix
One-line type correction in `zip.ch`:
```
-  Superior_Image_Line_Width( zip_type_image image )     returns char;
+  Superior_Image_Line_Width( zip_type_image image )     returns unsigned char;
```
followed by regenerating `zip.eh`/`zip.ih` via `make zip.eh zip.ih` (classpp)
and a normal full rebuild+install (`make install`, no special
`CDEBUGFLAGS` — the whole directory builds and runs correctly at the
default optimization level now). Confirmed working end-to-end.

`Superior_Image_Pattern`/`Superior_Image_Shade` share the same
`returns char` declaration in `zip.ch` but use `NULL`(0) as their sentinel,
which sign-extends and zero-extends identically — not affected by this bug
class, left as-is.

## Two incidental fixes made along the way (kept, both are real bugs, neither
was *the* bug, confirmed via live lldb tracing that one is dead code for
this test case and the other simply didn't move the needle)
1. `symtab_add()`: `(int) entry <= 0` pointer-truncation-to-32-bit bug on
   LP64 fixed to `entry == NULL`. `symtab_add`/`symtab_create`/`palloc` are
   never called during ordinary figure rendering (confirmed via lldb
   breakpoint trace, 0 hits) — this only matters for the symbol-table path
   (figure/image/stream cross-references by name), if that's ever
   exercised.
2. `zip_Contextual_Figure_Line_Dash()`: on the "no custom dash pattern set
   anywhere in the chain" path, `*pattern`/`*offset`/`*type` were left
   completely unwritten, and the caller (`zipview__Ensure_Line_Attributes`)
   read `*type` uninitialized. Fixed by unconditionally initializing all
   three outputs at function entry.

## Bisection methodology notes (kept for future reference)


## Symptom
Zip figure insets (e.g. a circle in `src/doc/papers/atk/Cattey.turnin`, also
reproduced with `contrib/zip/samples/dragon.zip`) render as a solid black
rectangle instead of the correct figure content, when `contrib/zip/lib` is
built at the default optimization level (`CDEBUGFLAGS = -O`, i.e. `-O1`).
Building the same source tree at `-O0` fixes it. This is confirmed
build-config-sensitive, not data/file-sensitive — same `.zip`/document,
different `CDEBUGFLAGS`.

## Bisection result (confirmed via alternating whole-directory rebuilds,
cross-checked both directions each round)
- Whole `contrib/zip/lib` (41 `.c` files) at `-O0` → renders correctly.
- Whole directory at default `-O` → black.
- **Isolated culprit: `zipd000.c` alone.** Building only this one file at
  `-O0` (all other 40 files at default `-O`, including `zipv.c`, `zipv000.c`,
  all `zipo*.c` figure-type files, `zipedit.c`, etc.) fixes the render.
  Building everything else at `-O0` except `zipd000.c` (left at default `-O`)
  reproduces the black box. This was cross-checked in both directions, so
  it's solid: **the bug is 100% confined to `zipd000.c`'s own compiled code**,
  not a cross-file interaction, not xgraphic.c/fontdesc.c/xfontd.c (those
  stayed at default `-O` throughout every test in this investigation).

## Sub-bisection within zipd000.c
- `zip__Define_Font` (lines 382-439, the stateful realloc-grown font cache,
  keyed by name, used for every figure's shading/pattern font lookup e.g.
  `"zipshd16"`) forced to `__attribute__((optnone))` alone (rest of the file
  at default `-O`) → **still black**. So it is NOT the sole cause, contrary
  to the top initial suspicion (both this instance and another concurrent
  instance independently flagged it as prime suspect from static reading —
  static reading was wrong here, confirmed by this runtime test).
- NOTE: use `__attribute__((optnone))` on the function definition, NOT
  `#pragma clang optimize off/on` — `zip.h`/`apt.h` `#define on 1` /
  `#define off 0` (legacy boolean macros), which collides with and breaks
  the pragma's token parsing (`error: unexpected argument '0' to '#pragma
  clang optimize'`).

## Two source-level "fixes" already tried and BOTH FAILED to resolve it
(applied together, whole directory rebuilt at default `-O`, confirmed still
black both times — these may still be worth keeping as independent
correctness fixes, but neither is *the* bug):
1. `symtab_add()` (line ~830ish): `if ((int) entry <= 0)` was a 64-bit
   pointer truncated to 32-bit int compared against 0 — classic LP64 bug
   shape. Fixed to `if (entry == NULL)`. **Confirmed via live lldb trace
   that `symtab_add`/`symtab_create`/`palloc`/`palloc_create_pool` are NEVER
   CALLED during this redraw at all (0 breakpoint hits)** — so this fix,
   while a legitimate bug fix, is provably irrelevant to this specific
   symptom/test case. Left in place (it's correct).
2. `zip__Contextual_Figure_Line_Dash()`: on the "no dash pattern found
   anywhere in the figure/image/stream chain" path (the common case), it
   left `*pattern`/`*offset`/`*type` completely unwritten, and the caller
   (`zipview__Ensure_Line_Attributes` in `zipv000.c`) reads them
   uninitialized (`short dashtype;` no initializer) on that path. Fixed by
   initializing all three outputs unconditionally at function entry
   (`*pattern=NULL; *offset=0; *type=graphic_LineSolid;`). Did not fix the
   symptom either, but is a legitimate defensive fix, left in place.

## Live lldb findings (both processes attached simultaneously and compared:
working=`-O0` zipd000.c build, broken=default-`-O` zipd000.c build, same
document open in both)
- `zip_Contextual_Figure_FillFG_Color` / `FillBG_Color`: **return status=1
  (zip_failure) in BOTH the working and broken builds, identically**, for
  the circle figure being drawn. Disassembly of both confirms this is a
  faithful, correct compilation of the source logic (walks
  figure→image→superior-image→stream looking for a fill color; genuinely
  none is set for this figure). This path is NOT the differentiator.
  IMPORTANT SEPARATE FINDING (not yet chased further): the fallback path in
  `zipview__Ensure_Fill_Attributes` on failure —
  `zipview_SetForegroundColor(self, def_fg=NULL, 0,0,0)` /
  `zipview_SetBackgroundColor(self, def_bg=NULL, 65535,65535,65535)` — is a
  **silent no-op** in `xgraphic__SetForegroundColor`/`SetBackgroundColor`
  (`src/atk/basics/x/xgraphic.c`) whenever `colorName==NULL` and the cached
  "old" RGB already matches, because on `graphic_Color` displays that
  function NEVER calls `super_SetForegroundColor`/`SetBackgroundColor`, so
  the base `graphic` struct's cached RGB fields are frozen at their
  class-init defaults (0,0,0 / 65535,65535,65535) forever and always
  "match" — meaning this fallback never actually touches the GC, and
  whatever foreground/background was left over from the previous draw
  operation silently persists. This is a real bug in `xgraphic.c`, always
  present regardless of `zipd000.c`'s optimization, so it can't by itself
  explain the `-O0`-vs-`-O` sensitivity, but it may be *amplifying* whatever
  the real zipd000.c bug is (turning "wrong color chosen" into "no color
  change happens at all, stale black GC state shows through"). Worth fixing
  regardless (`SetForegroundColor`/`SetBackgroundColor` should special-case
  `colorName==NULL` to always apply, or the base class fields should be kept
  in sync). Not yet attempted.
- `zip__Define_Font("zipshd16")` returns a stable, valid, non-NULL
  `fontdesc*` in the WORKING build. Did not get to check its return value in
  the BROKEN build (session was interrupted mid-check) before this instance
  independently ruled out `zip__Define_Font` via the `optnone` attribute
  test above, which is a stronger/cleaner result anyway.
- Disassembly of `zip__Define_Font` in BOTH the `-O0` and default-`-O`
  builds was hand-traced instruction-by-instruction against the C source;
  both appear to be faithful, correct translations (no compiler
  miscompilation found). This plus the optnone-negative-result strongly
  suggests `zip__Define_Font` itself is not buggy, in agreement.

## Remaining suspects in zipd000.c (not yet individually isolated)
- `zip__Contextual_Figure_Pattern`, `zip__Contextual_Figure_Shade`,
  `zip__Contextual_Figure_Line_Width`, `zip__Contextual_Figure_Line_Join`,
  `zip__Contextual_Figure_Line_Cap`, `zip__Contextual_Figure_Line_Color`,
  `zip__Allocate_Color_Values`, `zip__Allocate_Color` — none yet confirmed
  called-and-inspected live for this test case except Line_Dash (called 21x,
  `pattern` always NULL, consistent with the common "no custom dash" case —
  not yet compared working-vs-broken for its `*offset`/`*type` outputs
  specifically, only inferred safe from the source fix).
- Consider testing "everything in zipd000.c EXCEPT zip__Define_Font forced
  optnone, zip__Define_Font itself left at default -O" as the complementary
  test — if THAT fixes it, confirms bug is elsewhere in the file, not
  Define_Font, which optnone-per-function bisection can then narrow further
  (much faster than whole-file `-O0` rebuilds: ~1 compile + link per round).

## Reproduction / test setup notes
- App: `ez` opening `src/doc/papers/atk/Cattey.turnin` (has an embedded zip
  circle figure) or `contrib/zip/samples/dragon.zip`. Either reproduces it.
- Build: `cd src/contrib/zip/lib`, `touch <files>`, `make CDEBUGFLAGS="-O0"
  <targets>.o` or `CDEBUGFLAGS="-O"`, then plain `make install` to
  relink/reinstall the `.do` files (zip is dynamically loaded — no `runapp`
  relink needed, but **the already-running process does NOT pick up a
  reinstalled `.do`** — must quit and relaunch `ez` for a rebuild to take
  effect).
- User drives the app (launching/quitting/resizing to trigger redraws) per
  this project's established pattern — Claude does not launch GUI
  subprocesses directly (risk of hangs/forking, per
  `revival/doc/runtime-debugging-guide.md`). Claude attaches lldb via
  `process attach --pid <pid>` given by the user after they confirm the
  visual state (good/black), using `script -q <logfile> lldb -p <pid> -s
  <scriptfile>` run in background, one attach per round, killing the lldb
  process (not the target) between rounds.
- **ASLR note**: absolute addresses shift per process launch (confirmed:
  same function's disassembly had different base addresses across `ez`
  relaunches, though identical relative offsets/instruction bytes). Any
  address-based breakpoint (`breakpoint set --address 0x...`) must be
  re-derived via a fresh `disassemble --name <func>` after each relaunch —
  do not reuse addresses across process instances.
- `finish` inside a scripted breakpoint's `breakpoint command add` list
  silently aborts any commands queued AFTER it in that same list ("Aborting
  reading of commands after command #N: 'finish' continued the target") —
  the target ends up correctly stopped at the return site, but you must
  either make `finish` the last command, or use a second independent
  breakpoint set at the known return address instead (more reliable;
  get the address from `disassemble` of the caller).
- Sample figures actually exercised in the reproducing document: circle
  (`zipocirc.do`), drawn via `zipview__Draw_Stream` → `Show_Pane` →
  `zipview__Draw_Image` → `zipocirc__Draw_Object` → `Draw` →
  `zipview__Ensure_Line_Attributes` / (presumably) `Ensure_Fill_Attributes`.

## Current on-disk state (as of this writing)
- `zipd000.c` has 3 edits applied: the `symtab_add` NULL-check fix, the
  `zip_Contextual_Figure_Line_Dash` unconditional-init fix, and
  `__attribute__((optnone))` on `zip__Define_Font` (this last one is a
  *test* instrumentation, probably want to remove once the real bug is
  found, unless it turns out to matter after all).
- Currently installed build: `zipd000.c` compiled at default `-O` (with the
  optnone attribute active on `Define_Font` only), everything else in
  `contrib/zip/lib` at default `-O`. This is the config that just tested
  **black** (pid 97198, per user).
- Two other `ez` PIDs from earlier rounds have since been closed by the
  user; only rely on whatever PID the user most recently reports as live.
