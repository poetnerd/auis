# calc inset text rendering bugs — UNRESOLVED, handoff notes (2026-07-12)

## Status at handoff

`contrib/calc` is now in the active build (`MK_CALC`) and functionally
works: buttons compute correctly, the class loads, no crashes. Two
real bugs found along the way are fixed and confirmed. Two more remain
**open** — this doc is the state to pick up from.

**Fixed and confirmed (uncommitted as of writing — see `fossil status`):**

1. **`calc.c`'s `Writer` — real crash, unrelated to rendering.**
   `field.content = (char *)sprintf(value, "%f", Value);` — `sprintf`
   returns an `int` char-count, not a pointer. The cast turned that
   small int into a bogus near-NULL pointer; `apt_WriteObjectField`'s
   `fprintf(file, "%s", field.content)` then dereferenced it, crashing
   in `strlen` on every document checkpoint/save. Any doc containing a
   calc inset was guaranteed to crash within ~30s (the checkpoint
   timer). Fixed: `sprintf(value, "%f", Value); field.content = value;`
   Confirmed via lldb backtrace (`_platform_strlen` → `vfprintf` →
   `apt__WriteObjectField` → `Writer` → `apt__WriteObject` →
   `calc__Write`) and confirmed gone after the fix (checkpoint no
   longer crashes).

2. **`xgraphic.c`'s `GetXftForeColor` — real erase-color bug, confirmed
   fixed.** `graphic_WHITE` transfer mode ("erase by redrawing in
   background color") only swaps colors via `XSetForeground()` on the
   core X GC (`xgraphic_LocalSetTransferFunction`, ~line 408-417) —
   Xft rendering never consults the GC's foreground pixel, it reads
   `self->foregroundpixel` directly. That field is never updated by
   the WHITE-mode swap, so `ClearBoundedString`'s "erase by redrawing
   the old string in white" was actually redrawing it in **black**
   via Xft — re-drawing instead of erasing, then the new string drew
   on top. Fixed by checking `transferMode == graphic_WHITE` and using
   `backgroundpixel` instead. **Confirmed via live lldb trace**:
   breakpointed `XQueryColor` (the only place the pixel value used for
   Xft color resolution gets queried) across a full `123+4=` sequence
   — every `ClearBoundedString` call now requests `0xFFFFFF`, every
   `DrawBoundedString` call requests `0x000000`, perfectly alternating.
   This part of the pipeline is provably correct now.

**Reverted (tried, disproven, backed out):**

An earlier hypothesis was that the *alignment offset math* in
`xgraphic_DrawChars` (the `ATTOP`/`BETWEENTOPANDBOTTOM`/
`BETWEENTOPANDBASELINE`/horizontal-centering code, ~line 649-697) was
wrong because it computes `y`/`x` using the **core "dummy" XFontStruct's**
`maxChar->ascent`/`descent`/`XTextWidth`, while the glyph actually
painted comes from a **different, independently-resolved Xft font**
(`xfontdesc_GetXftFont`'s fontconfig substitute for the XLFD) — if
those two fonts' metrics disagree, position and paint disagree too.

This was implemented (resolve `xftfont` early, use its `ascent`/
`descent`/`XftTextExtents8` instead of the core metrics) and tested
live. **Result: zero observable difference anywhere** — not just
calc, but other previously-tested apps too. Then, after reverting it
back to core-only metrics and rebuilding, a side-by-side comparison of
a Frame animation (unrelated app, exercises the same `xgraphic_DrawChars`
alignment path) also showed **no difference** between the two states.
This is internally consistent: the change was inert both when added
and when removed, meaning it was never the cause of anything observed
in this session (neither a fix nor a regression). Current code is
back to 100% original core-metrics behavior for this part — see
`fossil diff src/atk/basics/x/xgraphic.c` for the small residual diff
(just the `GetXftForeColor` fix, plus a harmless restructuring that
resolves `xftfont` once up front and reuses it in the actual Xft
painting block instead of calling `xfontdesc_GetXftFont` twice).

**Note for whoever picks this up:** an early, unverified user
observation suggested a Frame animation might be rendering "a few
pixels too high" after the (since-reverted) metrics change. The
side-by-side re-test after reverting showed the animation identical
in both states, which argues against that change being the cause —
but this was never independently confirmed as a real discrepancy in
the first place (vs. an artifact of comparing across separate
sessions). If pixel-position issues resurface in *any* Xft-rendered
view, don't assume they're new; verify against a totally clean
checkout first.

**Also changed, still in place (separate from the above):** in
`contrib/calc/calcv.c`, the `Balanced`/`RightMiddle` alignment macros
were changed from `view_BETWEENTOPANDBASELINE` to
`view_BETWEENTOPANDBOTTOM` (matching the precedent in `fnotev.c`'s
2026-07-04 marker-centering fix, see `porting-changelog.md`). This
was the *original* ask (button/display label vertical centering) and
has **not been re-verified** against the current (reverted) xgraphic.c
state — do this first if picking up the centering question. It's a
plausible, low-risk change in isolation (matches established
precedent for exactly this symptom class) but wasn't isolated/tested
independently of the other changes in this session.

## Open bug #1: "ghost" text — old digits visible behind new ones

**Symptom:** typing a multi-step calculation (e.g. `123+4=`) shows only
the final answer clearly (`127`), with the previous expression (`123`)
visible as a fainter "ghost" underneath/behind it. Screenshots in this
session's transcript show this clearly — search for "ghost of 123".

**What's been proven NOT the cause**, via live lldb tracing
(breakpoints on `aptv__DrawBoundedString`/`aptv__ClearBoundedString`/
`XQueryColor`/`XftDrawString8`, correlated across a full keystroke
sequence):

- **Content is correct at every step.** Every keystroke (`1`,`2`,`3`,
  `+`,`4`,`=`) triggers exactly one `calcv__Update` → one
  `ClearBoundedString(old)` → one `DrawBoundedString(new)` cycle, with
  the string contents progressing correctly: `0→1→12→123→"123 + "→
  "123 + 4"→127`. No missed updates, no stale content, no double-fires.
- **Color is correct at every step** (see fix #2 above) — proven via
  the `XQueryColor` trace.
- **Position is self-consistent.** For a *given* string (e.g. "123"),
  the exact per-glyph pixel coordinates (`XftDrawString8`'s `x`,`y`
  args) captured during its original `DrawBoundedString` call are
  **byte-identical** to the coordinates captured later when that same
  string is passed to `ClearBoundedString`. If a string is drawn at
  position P, it is later erased at exactly position P. No drift.

Given all three of content/color/position check out at the API level
traced, the residue must be happening **below** what these breakpoints
can see — candidates, untried as of this handoff:

1. **Xft's actual pixel compositing / anti-aliasing.** Alpha-blending
   white-over-white should mathematically cancel to exactly white
   regardless of coverage, but hasn't been verified with an actual
   framebuffer pixel diff (e.g. `XGetImage` before/after an erase,
   diffed programmatically) — only inferred from "the API-level
   argument values are correct." A real pixel-level readback would be
   the definitive next step.
2. **The unexplained second `FullUpdate`.** The trace showed that
   right after the `=` press's `DrawBoundedString("127")` call
   completes, a **second full redraw of all 18 button labels** fires
   (identical glyph positions to the very first initial render) —
   something neither the "1", "2", "3", "+", or "4" presses
   triggered. This is very likely benign (calc is embedded in a `cel`
   spreadsheet cell in the `Sherman.Alloc` test document; the value
   change may cascade through the cel dependency graph and force a
   `calcv__FullUpdate`), but it was **not traced through to
   completion** — the log capture ended partway through this second
   render, before reaching (or confirming it doesn't reach) the
   display area again. If this second FullUpdate's `Draw_Calc` call
   does anything to the display area's clip/background without going
   through the traced `Clear`/`Draw` path, that's a candidate. Trace
   this fully before ruling it out.
3. Test whether the ghost reproduces with **no** `cel`/table
   embedding — insert calc directly into a plain `ez` document (not
   via `Sherman.Alloc`'s spreadsheet) and repeat the `123+4=` sequence.
   If the ghost disappears, the `cel` dependency cascade (point 2) is
   implicated directly.

**Reproduction:** insert a fresh calc inset (Esc-Tab, type `calc`,
Enter, or via `Sherman.Alloc`'s existing cel), press "AC", type
`123+4=`. Ghost of `123` appears behind `127`.

**lldb tracing scripts used this session** (adapt as needed — all
non-invasive, attach/detach without disturbing the target):
```
breakpoint set -n aptv__DrawBoundedString
breakpoint set -n aptv__ClearBoundedString
breakpoint set -n XQueryColor       # or XftDrawString8 for per-glyph coords
breakpoint command add <N>
  script print("=== LABEL ===")
  x/s $x1          # string arg (DrawBoundedString/ClearBoundedString)
  # or: register read x3 x4 ; x/c $x5   (XftDrawString8: x,y,char)
  # or: memory read -f x -c 1 -s 8 $x2  (XQueryColor: pixel value)
  continue
DONE
continue
```
Attach via: `script -q logfile lldb -p <pid> -s scriptfile` (run in
background). **Important**: always explicitly `kill` both the `lldb`
and `script` processes when done with a tracer — letting the script
file run to completion without an explicit `detach`/`quit` at the end
appears to leave lldb in an idle-but-still-attached state that doesn't
harm the target (confirmed safe to `kill` cleanly), but don't rely on
it self-detaching.

## Open bug #2: "=" button stuck in reverse video

**Symptom:** after pressing "=", the equals button remains visually
inverted (highlighted) indefinitely, rather than returning to normal
video. Visible in the same screenshots as the ghost bug.

**Not investigated at all this session** beyond reading the relevant
code. `Highlight_Area`/`Normalize_Area`/`Normalize_Other_Areas` in
`calcv.c` implement "only one button highighted at a time" — pressing
any button un-highlights whatever was previously highlighted (via
`Normalize_Other_Areas`, called from inside `Highlight_Area`), but
**nothing un-highlights the most-recently-pressed button** — it stays
inverted until a *different* button is pressed. This might be:

- **Original 1988 intended behavior** (a "last button pressed"
  indicator, common in real calculators) — in which case this isn't a
  bug at all, just unfamiliar/undocumented UX.
- **A real bug** if the original design expected some other event
  (mouse-up, a timer, `LoseInputFocus`) to clear the highlight and
  that mechanism is broken in the revival.

None of this session's code changes touched `Highlight_Area`/
`Normalize_Area`/`Normalize_Other_Areas` at all, so if it's a
regression, it's not from anything in this session — check whether it
reproduces on a completely clean build with none of this session's
`calcv.c`/`xgraphic.c` changes applied, to establish whether it's
pre-existing.

## Where things are in fossil

**Not committed as of this handoff.** `fossil status` shows edits to:
`revival/doc/roadmap.md`, `src/atk/basics/x/xgraphic.c`,
`src/config/site.h`, `src/contrib/calc/calc.c`,
`src/contrib/calc/calcv.c`. All compile clean and are installed into
`build/` (rebuilt+relinked `runapp` for the `xgraphic.c` change,
`calc.do`/`calcv.do` reinstalled for the calc changes). The two
*proven* fixes (Writer NULL pointer, GetXftForeColor) are worth
committing on their own regardless of how the remaining two bugs are
resolved — they're independently correct and confirmed working.

## Full context

This investigation started from the simpler task of enabling
`MK_CALC` in the active build (see `roadmap.md`'s "calc inset" entry
under Completed, and the "contrib/zip/utility ltapp.c" note under
Insets to Repair for an unrelated pre-existing break the full gate
surfaced along the way). The rendering bugs here only became visible
once calc actually got real runtime exercise for the first time in
the revival.
