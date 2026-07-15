# AUIS Revival: What Was Done and Why

*Last distilled: 2026-07-14.* A narrative account of reviving Carnegie
Mellon's Andrew User Interface System (AUIS) on a modern Mac, for readers
who already know ATK and readers encountering it for the first time. For
the complete technical record this document summarizes, see
`porting-changelog.md` (chronological fix log), `porting-assessment.md`
(bug-class analysis), and `roadmap.md` (current status and open items).

## What AUIS is

AUIS is the software behind `ez` — a word processor capable of embedding
spreadsheets, equations, drawings, and animations directly inside a
document — `messages`, a mail and bulletin-board client with the same rich
embedding, and a dozen smaller applications, all built on a shared toolkit
called ATK (the Andrew Toolkit). The version being revived, 6.3.1
(August 1994), is the last release CMU shipped as plain C before the
project moved to C++. (See `version-comparison.md` for the reasoning
behind reviving 6.3.1 rather than the later, never-finished C++ line.) The
target platform is macOS on Apple Silicon, using XQuartz to provide the X11
display server this software was originally written against.

For readers who haven't encountered it before, ATK is worth a moment's
context, because its central idea was genuinely ahead of its time. That
idea is the *inset*: a self-contained, interactive object — a spreadsheet
cell, an equation, a drawing, an animation, a footnote, a scripted
widget — that can be embedded inside a document and can, in turn, embed
further insets inside itself, recursively and without a fixed limit. A
single `ez` document can contain a spreadsheet nested inside a drawing
nested inside a footnote. Each inset knows how to read and write its own
portion of the file format, draw itself, and handle its own input,
independent of what it happens to be embedded in. General, recursive object
embedding across applications became a mainstream industry goal only later
in the 1990s — Microsoft's OLE and Apple's OpenDoc both pursued versions of
it, with OpenDoc discontinued in 1997 without wide adoption — and ATK had a
working, recursively general implementation of the idea years earlier,
built in portable C on top of a small, purpose-built object system (called,
simply, "Class") that is contemporary with Objective-C. The application
framework itself — windows, menus, dialogs, scrolling, printing — is shared
by every ATK-based program, so `ez`, `messages`, and `help` are less a set
of separate applications than different entry points onto the same
document-and-inset substrate. That shared substrate, and the "any object
can live inside any other object" model it enables, is ATK's most
distinctive and still-interesting contribution.

Nobody had compiled this code in roughly thirty years. Restoring it meant
addressing two different kinds of decay at once: the environment changed
around the code (new compilers, a new processor architecture, deprecated
operating-system interfaces), and the code itself contained real defects
that had simply never been exercised in three decades of use. The rest of
this document covers both, along with the strategy adopted to manage them.

## Modernizing

Not everything changed during the revival was about recovering old,
dormant behavior. Some changes were required simply to make 1994 code
function in a 2026 environment that behaves differently in specific,
load-bearing ways — without them, the software does not run at all.
Separately, a smaller amount of work was undertaken by choice, to bring
one part of the software's appearance up to a standard users now expect,
even though the original approach was still technically functional. Both
count as modernization, but they carry different justifications and were
undertaken for different reasons.

### Required to run at all

- **Deleting a character from the middle of a string was commonly done by
  copying the remainder a few bytes to the left, within the same
  buffer** — technically undefined behavior by the letter of the C
  standard, but a pattern that produced correct results on essentially
  every C library for decades. Apple's runtime-hardened library now
  detects that overlapping copy and aborts the program outright. Fifteen
  call sites across nine files used this idiom, and it crashed three
  applications (a file browser, an outliner, and an HTML viewer) under
  ordinary use. Each site needed only a one-line fix — a function
  explicitly defined for overlapping ranges — but locating all fifteen
  required a deliberate, tree-wide search, and none of them were
  optional: without the fix, those applications do not start.
- **`dlopen()`/`dlsym()`**, the standard POSIX dynamic-library interface,
  replaced AUIS's own hand-built, per-platform dynamic code loader — the
  component responsible for loading an inset's code on demand. The
  original loader had to understand the executable file format of every
  target platform individually and had no notion of the current one; the
  standard library call accomplishes the same task in roughly seventy
  lines of code rather than the original's five hundred, and is what
  actually loads insets in the revived system.
- **A compiler-leniency strategy**, as the default response to the volume
  of errors modern, stricter compilers raise against 1990s C. Rather than
  rewriting the source wholesale, the build disables a small, specific set
  of modern strictness checks corresponding to conventions this code
  relied on, reserving actual rewriting for places that genuinely require
  it. Without this, the code does not compile at all under a current
  compiler. (Discussed further below.)

### Bison and flex

The project's vendored bison fork hangs outright on current processor
architecture, so the build now uses the system's installed bison instead;
flex was already the system tool, but 1990s wrapper code around both
generators still assumed specific behavior of the versions in use at the
time. An early fix — passing bison's output filename explicitly, since
modern bison's default naming convention had changed — flagged that risk
directly, since the original code already carried its own accommodations
for 1990s bison behavior. That prompted a closer look for other places
where old assumptions about generator output might no longer hold, and
turned up two:

- A shared code generator assumed every parser lookup table was a fixed
  storage width, matching 1994 bison's output. Modern bison narrows some
  tables to a smaller type when the data range allows it; reading a
  narrowed table through the old, fixed-width assumption merged adjacent
  entries into garbage — surfacing as a date parser that failed on every
  input, for reasons unrelated to dates.
- A hand-written reset routine poked an internal flex flag whose meaning
  had reversed between old and new flex — nonzero used to mean
  "reinitialize on next call," modern flex treats it as "already
  initialized." The first call in a process skipped its own setup and
  wrote through an unset pointer, crashing a mail-address parser on the
  first address it ever processed.

### Adopted by choice: anti-aliased text

X11's original design placed font rendering on the server: the X server
itself rasterized glyphs from bitmap font files and returned pre-rendered
pixels on request, a scheme known as the core font protocol. This was the
normal way to draw text throughout the 1990s, and it is what AUIS was
built against — every text-rendering call in the codebase assumes it.
Modern desktop environments instead render text on the client side, using
libraries such as Xft and fontconfig, which support anti-aliasing, scalable
outline fonts, and subpixel hinting — the smooth, high-resolution text
users now consider normal.

The old, server-side bitmap path still works: XQuartz, the X server used
here, still implements the core font protocol, so nothing required this
change. It was made anyway, because 1990s bitmap fonts, while entirely
serviceable on 1990s monitors, look noticeably coarse next to the
anti-aliased text every other current application renders, and that
difference was judged worth correcting. The revival adopted a hybrid:
ordinary document text now renders through Xft, while the small set of
custom CMU symbol and cursor glyphs (bullets, math marks, cursor shapes)
that have no modern equivalent still render through the original
bitmap path — replacing only the parts that had a clean modern substitute.

This was a comparatively small amount of new code, and the visual
improvement was judged worth the effort — but, being new code rather than
old code regaining exercise, it introduced its own new defects rather than
exposing dormant ones. The calculator inset's display, once it received
real interactive testing under the new rendering path, showed two: text
was being "erased" by redrawing it in the background color, which exactly
cancels only the fully opaque center of each anti-aliased character, so the
partially transparent edge pixels accumulated into a visible ghost after
repeated use; and, separately, the X display server sometimes wrote the
correct pixels into a window's buffer without promptly making them
visible, requiring an incidental redraw — a window focus change, for
instance — before the correct text actually appeared. Both were resolved,
but both took debugging effort disproportionate to the size of the
feature, which is the general cost of this kind of optional modernization:
low volume of new code, comparatively high difficulty per defect.

The project's own infrastructure was modernized in a smaller way as well:
source control runs under Fossil, a self-contained modern system, in place
of whatever distribution mechanism carried the source in the 1990s.

## Old bugs never found till now

Bringing 1994 code onto a 2026 machine did more than require accommodating
a changed environment — it also surfaced defects that had existed in the
source for decades without ever manifesting. Two changes in particular
turned latent mistakes into visible ones: stricter runtime checks in
Apple's C library, which refuses certain unsafe patterns outright rather
than silently permitting them, and, later in the project, a build
configuration that type-checks function calls the original toolchain never
checked. Neither change introduced a defect; both exposed defects that had
been present, and untriggered, since the code was written. A representative
sample:

- **A core interface had its arguments in the wrong order for roughly 35
  years.** The class-definition file for the figure-drawing inset declared
  one method as `Build(action, view, ...)`; every implementation, and every
  call site, used `(view, action, ...)`. This had no runtime effect for the
  entire life of the code, because nothing had ever cross-checked the
  declaration against actual usage — until the ANSI C conversion effort,
  described below, began doing exactly that.
- **A class of status message had never once displayed.** A single call
  site in the HTML-editing view passed its arguments in the wrong
  order — message text and priority transposed — so that category of
  informational message had never appeared, since the code was first
  written.
- **A parser that never checked whether an embedded inset loaded.** `ez`'s
  plain-text container read each inset via a success-or-failure code it
  never checked. When a figure's reader failed partway through — on a
  document with an attribute this older reader didn't recognize — the
  container kept parsing from wherever the failed read had abandoned the
  file: the figure's raw internal data appeared on screen as literal text,
  and nothing after it in the document rendered at all. Fixed by falling
  back to a generic reader that resynchronizes by counting nested begin/end
  markers instead of assuming every read succeeds.

None of these are new mistakes. Each was introduced once, decades ago, and
never triggered — because the exercising code path was never run, because
nothing had checked a declared interface against its actual usage, or
because an earlier C library was more permissive. Restoring old software to
working order on current tools amounts, in part, to finally running a test
suite that nobody knew existed.

## Word size issues

The largest, most systemic category of defect came from a single
architectural fact: this code was written when a C `int` and a C `long`
were the same size — 32 bits, on the machines of the early 1990s. On
today's 64-bit Apple Silicon, `long` is 64 bits while `int` remains 32.
Code that quietly assumed the two were interchangeable — a reasonable
assumption for decades — now loses or corrupts data at every boundary
where the two are confused.

This is most acute in AUIS's object system, which implements polymorphism
(one class of object overriding a method defined by another) in plain C,
using function pointers stored in a dispatch table. Historically, every
method call went through generated code that cast that function pointer to
a completely generic, typeless signature — meaning the compiler, seeing no
argument types at the call site, could not insert the instructions needed
to correctly widen a 32-bit value to 64 bits, or to preserve its sign. Five
distinct, recurring failure patterns followed from this one root cause,
each responsible for real, visible bugs during the revival:

1. **A function returning a pointer, called with no declaration in
   scope.** Pre-standard C assumes an undeclared function returns a plain
   32-bit `int`. If the function actually returns a pointer (64 bits), the
   upper half is silently discarded — typically producing a crash the
   moment the truncated pointer is used. This was the single most common
   defect in the codebase: more than twenty separate sites, all the same
   shape, once the pattern was recognized in one core function used to
   locate files on disk.
2. **More arguments than the processor's registers hold.** Apple Silicon
   passes the first eight integer/pointer arguments in registers and
   spills the remainder to the stack — but only if the compiler knows, at
   the call site, that there are more than eight. Through the generic,
   typeless dispatch above, it did not, and the ninth and later arguments
   were silently dropped.
3. **A sentinel value corrupted by zero-extension.** The value `-1` is
   commonly used as a special "unset" or "apply to everything" marker.
   Passed as a plain 32-bit constant through the same typeless dispatch, it
   is not sign-extended as it should be — the receiving 64-bit field ends
   up holding 4,294,967,295 rather than "negative one," and any code
   checking "is this negative?" or "does this equal -1?" silently takes the
   wrong branch. This is what made a help-browser list open scrolled to the
   bottom instead of the top: the "no position set yet" marker for a
   freshly opened list was corrupted into a large number crossing one of
   these dispatch calls, and the scroll position duly followed it.
4. **A 64-bit number read with a 32-bit text-parsing format.** Numbers
   stored as text within document files were read back with `%d` (32-bit)
   into variables that were actually 64 bits wide, leaving the upper half
   as whatever value happened to occupy the stack. The clearest instance:
   a drawing's on-page position was parsed this way and came out corrupted
   by exactly 4,294,967,296 — the figure was being drawn correctly, just
   roughly four billion pixels off-screen.
5. **A 64-bit parameter fed a corrupted 32-bit argument** — the same
   zero-extension mechanism as pattern 3, but for ordinary values rather
   than named sentinels, tripped by negative numbers used for scroll
   positions, indentation, and margins.

Two related but mechanically distinct defects showed up alongside these
five, corrupting data across a similar boundary disagreement without being,
strictly, width problems:

- **A signedness mismatch rather than a width mismatch.** One inset type's
  interface declared a method as returning a plain (signed) character
  where the implementation actually returned an *unsigned* one. At low
  compiler-optimization levels the discrepancy had no effect; at normal
  optimization, the compiler exploited the declared (incorrect) signedness
  and generated a comparison that could never succeed, so a "no line width
  configured" sentinel was silently read as "maximum width," and an entire
  figure was painted over in solid black. This belongs to the same family
  as the width bugs above — a value crossing a boundary where the two sides
  disagree about how to interpret its bits — differing only in that the
  disagreement is over sign rather than size.
- **A pointer stored in a field only wide enough for a plain number.** Two
  font-style fields in the note and troff-text insets had, since the
  1990s, been mistakenly assigned the literal text `"fontdesc_Plain"`
  rather than the symbolic constant of the same name — a copy error that
  was harmless on the original 32-bit machines, where a pointer and an
  `int` were the same size, and became live pointer corruption only once
  the same code ran on a 64-bit processor. A related instance: a widget
  class declared a parameter as a plain `unsigned` value where the actual,
  pointer-carrying implementation needed something wide enough to hold a
  full address, silently truncating a `self` pointer on every
  control-panel widget built from that class.

A mechanically unrelated defect, described above in "Modernizing," produced
a very similar-looking symptom: a code generator's own choice of table
storage width — not the processor's register width — changed underneath
old code. It does not belong to this family mechanically, but the
underlying lesson is the same: 1990s code that hard-assumed a fixed width
is fragile against any later link in the toolchain — compiler, processor,
or code generator — making a different, reasonable-at-the-time choice.

## Migrating to ANSI C

The oldest parts of this codebase predate the C language standard
itself — functions are defined in the pre-standard "K&R" style, with no
declared argument types anywhere. An early attempt to correct this
wholesale, by running an automated tool across the entire source tree in
one pass, made matters considerably worse: the tool's pattern-matching
approach to inferring argument types mishandled enough edge cases that a
single mass conversion took the build from roughly zero errors to over two
thousand, with no way to distinguish genuine bugs from the tool's own
mistakes. That attempt was reverted in full.

The approach that ultimately worked rests on one observation: this
codebase's class-definition files already describe every method's real
argument types in full, for an unrelated reason — they are the input to
the code generator that builds the object system's dispatch tables — and
that generator had simply been discarding the type information once it
parsed it. Teaching the generator to emit what it already knows turns the
compiler itself into the auditor: any place where a real implementation
disagrees with its own declared interface becomes an immediate, located
compile error, rather than a bug waiting to be found by accident at
runtime.

The resulting plan runs in four stages:

- **M1 — teach the class-definition compiler to emit typed code.**
  Completed tree-wide in July 2026. This alone eliminated the
  sentinel-corruption and too-many-arguments bug families by construction,
  and caught dozens of decades-old interface mistakes — including the
  backwards `Build()` arguments described above — simply by enabling real
  type checking at every method call site. The rollout itself proceeded
  one subdirectory at a time, verified by a clean full rebuild and a
  runtime check after every step — the discipline the failed mass-attempt
  had lacked.
- **M2 — sweep for missing function declarations** throughout the rest of
  the tree, not just class methods, closing the undeclared-function
  pointer-truncation pattern for good.
- **M3 — convert the function definitions themselves** from pre-standard
  to standard C, one subsystem at a time, using a purpose-built tool that
  looks up each function's real signature from the class-definition files
  rather than inferring it — the safer approach the original mass-
  conversion attempt should have taken.
- **M4 — enable full compiler strictness** tree-wide, once every subsystem
  has been converted, which closes the remaining scanf-format-code bug
  family as a side effect.

As of this writing, M1 is complete across the entire active codebase; M2
and M3 are in progress.

## Where things stand today

`ez` (the word processor), `help`, and `messages` (mail, running against a
local mailbox rather than the original 1990s shared-filesystem delivery
system) all run and render correctly, including most embedded inset types:
text, equations, tables, drawings, animations, footnotes, spreadsheets, and
more. The class-definition typing project (M1, above) is complete across
the entire active tree.

One deliberate scope decision from early in the project is worth noting:
the Console (terminal-emulator) subsystem is not part of this build.
Console's interprocess-communication layer would need a rewrite of its own,
and the project set that aside rather than take it on alongside everything
else. A small side effect: two icon fonts used by an unrelated animation
inset happen to live inside Console's own build directory, so they go
silently unbuilt too, unless Console — or just its font directory — is
built on its own.

A handful of smaller, known issues remain open, described below in "Open
issues" and tracked in full in `roadmap.md`.

## Open issues

A few problems remain unresolved, either because the root cause itself
isn't confirmed or because a workaround is in place without a real
upstream fix.

- **A live deadlock, cause unconfirmed.** While chasing an unrelated
  defect, a `help` window occasionally froze permanently. A debugger
  traced this to the process's single thread trying to reacquire a lock
  on the X display connection it already held indirectly, triggered by a
  keyboard-layout-change notification arriving mid-operation. Whether this
  is a genuine defect in how the toolkit calls back into the X client
  library, or a quirk specific to this environment's X server, hasn't been
  determined.
- **A completely empty build directory fails on the very first build.**
  The class-system directory everything else depends on builds its own
  pieces in a fixed order, and that order builds one piece needing
  `class.h` already in place before the piece that actually installs
  `class.h` runs. Any build directory that already has a copy sitting
  around from an earlier build never hits this; only a genuinely empty one
  does. The current fix is a manual step — copying the header into place
  before the first build — and a build-ordering problem is the leading
  explanation, though not a settled one: this step wasn't always necessary
  earlier in the project, and it isn't yet certain whether the analysis
  has found the real root cause or only a reliable workaround.

## Further reading

- `porting-changelog.md` — the complete, dated history of every fix, in
  the order it happened
- `porting-assessment.md` — the underlying bug-class analysis, strategy
  decisions, and the full ANSI C conversion plan
- `roadmap.md` — current status, open issues, and near-term plans
- `version-comparison.md` — why 6.3.1 (the last C release) was chosen over
  the later, unfinished C++ rewrite
- `quickstart.md` — how to build and run this on a Mac
