# Prompt: fix calc-inset ghost text (Xft erase-by-overdraw residue)

You are working in the AUIS revival tree at `/Users/wdc/src/AUIS/andrew-6.4/`
(Fossil checkout; the tree is currently clean — run `fossil status` to
confirm before you start). Follow the established check-in workflow from
memory: compile-verify every touched file, `fossil status` before/after,
ask the user for confirmation before any `fossil commit`, terse one-line
commit messages. Never run concurrent builds. Use `DISPLAY=:0;` (semicolon
form) before any X11 command. Install binaries with `install -m 755`,
never chmod+cp.

## The bug you are fixing

Symptom (see `revival/doc/calc-text-rendering-investigation.md`, open
bug #1): in the calc inset, typing `123+4=` leaves a faint gray "ghost"
of the erased expression (`123 + 4`) behind the final answer `127` in
the display area. Ghosts accumulate and are never cleaned up.

## Root cause — already identified, do not re-investigate

ATK's idiom for erasing a text label is *redraw the same string in
`graphic_WHITE` transfer mode* (see `aptv__ClearBoundedString` in
`src/atk/apt/apt/aptv.c:1421`, used by `Replace_String` in
`src/contrib/calc/calcv.c:365`). That worked on core X11 bitmap fonts
because a glyph paints exactly its coverage bitmap: overdraw in
background color restores every pixel it touched.

Under the Xft path in `xgraphic_DrawChars`
(`src/atk/basics/x/xgraphic.c:615`), glyphs are **anti-aliased**: edge
pixels are alpha-composited. Drawing black text on white paints an edge
pixel to `1-α` gray. "Erasing" by re-drawing the same glyph in white
composites `v' = v·(1-α) + α`, which equals 1 (white) only where α=1.
Every partially-covered edge pixel retains up to 25% gray — a permanent
outline ghost of the erased string. Repeated draw/erase cycles at the
same spot can accumulate to ~33% gray. Nothing ever repaints the
display-area background (ATK partial updates never clear), so residue
persists indefinitely — including across the FullUpdate cascade that
fires after `=`.

Prior lldb tracing (logs `lldb_calc_trace*.log` in the tree root) proved
everything above the pixel level is correct: per keystroke, exactly one
Clear(old)/Draw(new) pair fires with correct strings, colors alternate
white/black correctly (the `GetXftForeColor` WHITE-mode fix is already
committed), and glyph coordinates for a string's erase are byte-identical
to its original draw. The defect is purely that erase-by-AA-overdraw
cannot mathematically restore the background.

## The fix

In `xgraphic_DrawChars` (`src/atk/basics/x/xgraphic.c`), inside the
`HAVE_XFT` painting block: when `self->header.graphic.transferMode ==
graphic_WHITE` (i.e. this is an erase), **fill the glyph advance cells
with the background color via `XftDrawRect` instead of drawing glyph
shapes**. A solid rect fill has no anti-aliasing and erases completely.

Key facts you can rely on:

- `GetXftForeColor(self)` already returns the **background** color when
  `transferMode == graphic_WHITE` (that fix is committed), so the
  existing `fgc` variable is already the correct erase color.
- The per-glyph loop already computes each glyph's advance cell —
  `clip.x = cx`, `clip.y = gy - xftfont->ascent`, `clip.width = adv`,
  `clip.height = xftfont->ascent + xftfont->descent` — and intersects it
  with `paneClip`. That intersected cell rect is exactly the region the
  corresponding draw could have painted (draws are clipped to these same
  cells), so filling it is a complete, minimal erase.

Sketch (adapt to the existing code style, K&R C, tabs as in the file):

```c
/* inside the widths loop, after the existing cell/paneClip
   intersection has produced `clip` */
if (self->header.graphic.transferMode == graphic_WHITE) {
    /* Erase: AA overdraw can't restore edge pixels; fill the
       advance cell with the background color instead. */
    if (clip.width > 0 && clip.height > 0)
        XftDrawRect(xftd, &fgc, clip.x, clip.y, clip.width, clip.height);
} else {
    XftDrawSetClipRectangles(xftd, 0, 0, &clip, 1);
    XftChar8 c8 = (XftChar8)ch;
    XftDrawString8(xftd, &fgc, xftfont, cx, gy, &c8, 1);
}
cx += adv;
```

Note the erase branch needs no `XftDrawSetClipRectangles` call at all —
the fill rect *is* the intersected cell. Keep the zero-advance
(`adv <= 0`) behavior for the draw branch as it is today. For the
`widths == NULL` fallback path (rare), do the analogous thing: in WHITE
mode fill one band `[gx, gy - xftfont->ascent] × [W, ascent+descent]`
where `W` comes from `XftTextExtents8`'s `xOff`, clipped to `paneClip`;
otherwise keep the existing `XftDrawString8` call.

Do NOT touch the core-X11 (non-Xft) path — bitmap-font overdraw erase is
exact and correct. Do NOT touch vector drawing (lines/rects in WHITE
mode) — those go through the GC, which `xgraphic_LocalSetTransferFunction`
already handles exactly.

### Safety audit (already done — recorded here so you don't repeat it)

`graphic_WHITE` + text drawing on **color** displays is used exclusively
as an erase idiom in this tree. The one "draw white glyphs on dark
background" usage (`suiteev_ItemDrawTitle`/`ItemDrawCaption` in
`src/atk/apt/suite/suiteev.c`) is gated on `graphicIsMono` and never
runs on our 24-bit XQuartz TrueColor display. `fadv.c`, `chartv.c`,
`chartobj.c` WHITE-mode text uses are erases and benefit from this fix.
No gating on display depth is required, but mention this audit in your
final report.

## Build and install

`xgraphic.c` is statically linked into `runapp` via `libbasics.a`.
Use the incremental recipe (from memory `feedback_incremental_rebuild`):

```sh
cd src/atk/basics/x && make xgraphic.o
cd ../lib && rm libbasics.a && make libbasics.a
cp libbasics.a /Users/wdc/src/AUIS/andrew-6.4/build/lib/atk/libbasics.a
ranlib /Users/wdc/src/AUIS/andrew-6.4/build/lib/atk/libbasics.a
cd ../../apps && make runapp
install -m 755 runapp /Users/wdc/src/AUIS/andrew-6.4/build/bin/runapp
```

(`ez` is a symlink to `runapp`, so this covers both.)

## Verification protocol

There is a known problem launching runapp/ez from the VS Code terminal —
**ask the user to launch the app** rather than launching it yourself,
unless they tell you otherwise.

1. Fresh calc in a plain ez document (no cel/table embedding): user
   inserts a calc inset, presses `AC`, clicks `1`, `2`, `3`, `+`, `4`,
   `=`. Expected after the fix: the display cleanly shows `1`, `12`,
   `123`, `123 + `, `123 + 4`, then `127` — each keystroke fully
   replacing the previous string, **no gray ghost residue at any step**.
2. Repeat inside `Sherman.Alloc`'s spreadsheet cel (the original repro)
   — same expectations; the post-`=` FullUpdate cascade must not
   resurrect ghosts.
3. Regression smoke tests, because this changes the tree-wide Xft text
   path: normal ez text editing (type, delete, scroll), helpa, and the
   Frame animation document (it exercises `xgraphic_DrawChars` erase
   cycles heavily — this fix should visibly *improve* any text erasing
   it does). Screenshots for before/after comparison are ideal.

Note on the user-reported symptom "intermediate numbers never display at
all while typing": the lldb traces contradict pure invisibility (correct
black draws at correct positions were issued every keystroke), so the
expectation is that this fix resolves the whole visible symptom. If
after the fix intermediates still fail to appear, STOP and report — the
next diagnostic step in that case is an `XGetImage` framebuffer readback
before/after a keystroke (see the investigation doc's candidate #1), not
more breakpoint tracing.

## Explicitly out of scope

- The "`=` button stays reverse-video" report (open bug #2 in the
  investigation doc) is original 1988 behavior — the last-pressed button
  stays highlighted until another is pressed (`Highlight_Area`/
  `Normalize_Other_Areas` in `calcv.c`). Do not change it without the
  user asking.
- Do not revisit the alignment/metrics hypothesis — it was implemented,
  tested, disproven, and reverted last session.

## When done

- Update `revival/doc/calc-text-rendering-investigation.md`: mark open
  bug #1 resolved with a short root-cause note (AA erase-by-overdraw
  residue; fixed by background rect-fill in WHITE-mode Xft path).
- Add a line to `revival/doc/porting-changelog.md` if that is the
  established pattern (check first).
- Propose the commit grouping (likely one commit: xgraphic.c + doc
  updates) and wait for user approval before `fossil commit`.

## Outcome (2026-07-12)

The fix was implemented exactly as specified above: `xgraphic_DrawChars`'s
Xft path now fills the glyph advance-cell rectangle with the background
color (`XftDrawRect`) instead of redrawing glyph shapes, whenever
`transferMode == graphic_WHITE`, in both the per-glyph (`widths` table)
loop and the `widths == NULL` string-extents fallback. Built
(`xgraphic.o`), relinked into `libbasics.a`/`runapp`, installed. Compiled
warning-clean aside from pre-existing tree-wide K&R-prototype
deprecation warnings unrelated to this change. `fossil status` showed
only `src/atk/basics/x/xgraphic.c` touched.

User-verified in `ez`: **the ghost is gone** — no gray residue behind
the final answer.

**But a new, distinct symptom appeared** during the same verification
pass: while typing a multi-keystroke expression, each incremental
redraw shows only a *suffix* of the correct string — leading
characters go missing. Observed sequence typing `123+4=`:

| keystroke | expected | observed |
|---|---|---|
| `1` | `1` | `1` |
| `2` | `12` | `2` |
| `3` | `123` | `23` |
| `+` | `123+` | `3+` |
| `4` | `123+4` | `23+4` |
| `=` | `127` | `127` (correct) |

The final `=` result always draws correctly, and switching window
focus away and back forces a full repaint that shows the correct
string — proving the underlying string state is right and this is
specifically a defect in the *incremental* Clear/Draw update path, not
in calc's value tracking.

Not root-caused this session (out of scope for this prompt — the user
is picking it up separately). Untested hypothesis for whoever
continues: the old AA-overdraw erase only ever touched pixels within
the *old* glyph's own coverage bitmap, so it could never clobber a
freshly-drawn *new* glyph occupying the same cell even if draw/erase
ordering were reversed. The new solid rect-fill erase has no such
protection — if `Draw(new)` and `Clear(old)` ever run in an order (or
against cell regions) where the old string's advance cells overlap
positions the new string was just painted into, the fill will silently
wipe the fresh glyphs there. This is consistent with "leading
characters vanish" if the erase of the shorter old string is landing
on the same leading cells the longer new string just used — but this
has **not** been confirmed via tracing (the prior lldb trace in
`calc-text-rendering-investigation.md` asserted `Clear(old)` then
`Draw(new)` order, which would rule this out, so that ordering claim
needs re-verification, not assumption). Next step: lldb trace
`XftDrawRect` vs `XftDrawString8` call order and arguments across a
`1`,`2`,`3` keystroke sequence.

Committed as-is (ghosting fix + this outcome note); the partial-erase
regression is left as a new open item for the next session.
