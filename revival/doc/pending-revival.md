# Pending revival.md additions — convertraster bug-fixing session

Staged here, not applied directly, because `revival.md` is under active
editorial rewrite in a separate session as of 2026-08-08. Whoever merges
this should feel free to cut, reword, or fold these into existing bullets
rather than pasting them verbatim — the point is the content and the
target section, not this exact phrasing. Source material: this session's
full test pass of `convertraster` (three bugs found and fixed), recorded
in detail in `porting-changelog.md`'s 2026-08-08 entry and `roadmap.md`.
Delete this file once its content has been placed (or explicitly
rejected).

---

## Candidate addition to "Word size issues"

Fits after the two existing "related but mechanically distinct" bullets
(signedness mismatch; pointer-in-narrow-field), as a third — this one is
also distinct from the five untyped-dispatch patterns, since the code in
question predates the class dispatch system's involvement entirely.

> **A file format hand-rolled around a 4-byte `long`, nowhere near the
> object-dispatch system.** A raster-format command-line converter —
> untouched by every earlier width-bug sweep, because nothing about it
> looks like the other patterns above — had a file-header struct
> declaring three fields `long`, matching its format's actual on-disk
> layout (a fixed 14-byte header) only by 1990s accident. On a 64-bit
> build the struct is more than twice that size, so every field after
> the first lands at the wrong offset; reading such a file back fed
> uninitialized stack memory straight into an image-resize call,
> observed consuming multiple gigabytes of memory before being killed —
> not a wrong pixel, a runaway process. The same file's color-inversion
> routine carried a second, independent case of the identical
> assumption: a hand-optimized "invert 4 bytes at a time" loop walked a
> raw pointer cast to `long *`, with address arithmetic (`& ~3`, `-4`)
> sized for a 4-byte word. On this LP64 build each step silently
> inverted 8 bytes instead, missing a row's first few bytes entirely and
> overrunning its last few into whatever memory followed. Both defects
> had sat, unexercised, since the format was written; nobody had run
> this program's own read-back of its own output on this port before it
> was finally tested directly, byte-for-byte, against the original
> image.

Possible closing sentence, tying back to the section's own theme
("1990s code that hard-assumed a fixed width is fragile against any
later link in the toolchain"): by this point in the project the LP64
variant catalog was distinctive enough to pattern-match against on
sight — both defects above were root-caused in minutes from their
symptoms (a runaway allocation; a corrupted byte range at fixed,
predictable offsets) rather than hours of fresh investigation, on a
tool nobody had looked at before that day.

## Candidate addition to "Old bugs never found till now"

A different family — not a width mismatch, an input that was simply
never validated — but it fits this section's theme (thirty-year-old
code, never exercised, found only once someone finally ran the path):

> **A crop rectangle nobody ever checked against the image it was
> cropping.** The same raster-format converter's `-c(x,y,w,h)` crop
> option never validated its rectangle against the source image's actual
> dimensions before reading it — the underlying bitmap routines don't
> bounds-check either, by design, on the documented assumption that
> callers already have. A crop rectangle larger than the source image
> read straight past the end of the allocated buffer and kept going,
> producing an 18-megabyte file of heap garbage instead of an error, for
> an operation that should have been rejected outright. Fixed with an
> explicit bounds check ahead of the crop.

## New since the last answer: a methodology note worth folding in somewhere

Not from the bug list above — from reflecting on how these three bugs
were actually found, prompted by a question about whether the same
approach would help with the (separate, still-open) image-inset
blank-import bug. This project's `revival.md` already has one section
making a version of this point (the "one exception, caught the same day
it was made" aside under "Old bugs never found till now": "manual
round-trip testing... not by any compiler warning"). This is a second,
independent data point for that same claim, worth a sentence near it or
in "Where things stand today":

Both `convertraster` defects above compiled clean, with zero warnings,
under the same strict flags that caught hundreds of other bugs in this
project — nothing about either one is visible to a compiler, because
both are wrong at the level of *values*, not types. Neither would have
been caught by looking at the rendered output, either: the corrupted
byte range was a handful of columns per row, subtle enough that a
side-by-side glance at two drawings of a dragon could easily miss it.
What actually found it was mechanical, not visual — decoding the
raster format independently (a small standalone script, not the
program under test) and diffing the result against the original
byte-for-byte, which turned "the picture looks a little off" into "rows
differ at exactly byte columns 0–3 and 38–40 of 41, every time" — a
shape specific enough to point straight at a 4-byte-vs-8-byte pointer
bug. The same tactic — an independent decoder plus an exact diff, not a
rendered-image comparison — is the planned starting point for the
image-inset investigation next, where the current symptom description
("renders solid black" / "renders solid white") is really just what a
human eye reduces a completely uninspected buffer down to.
