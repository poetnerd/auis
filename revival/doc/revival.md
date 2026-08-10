# AUIS Revival: What Was Done and Why

Last updated: 2026-08-08

This document is a narrative account of reviving Carnegie Mellon's Andrew
User Interface System (AUIS) on a modern Mac, for readers who already know
ATK and readers encountering it for the first time.

For the complete technical record this document summarizes, see:

| Doc | Description |
|---|---|
| `roadmap.md` | Current status, active projects, and open issues |
| `porting-changelog.md` | Chronological log of every fix |
| `porting-assessment.md` | Bug-class analysis and the full ANSI C conversion plan |
| `version-comparison.md` | Why 6.3.1 (the last C release) was chosen over the later, unfinished C++ rewrite |
| `quickstart.md` | Build and run instructions |

## What AUIS is

AUIS is the latest name for the user-interface toolkit work that came out
of the IBM-funded Information Technology Center at Carnegie Mellon
University in the 1980s and 1990s. It was originally the "Andrew Base
Editor," then a second version, "Base Editor II" (BE2 for short), then the
"Andrew Toolkit" (ATK for short). As the project began to be shown outside
CMU and adopted by others, the more ambitious name "Andrew User Interface
System" (AUIS) was adopted.

The applications from AUIS of primary interest now are `ez` and
`messages`. `ez` is a word processor capable of embedding spreadsheets,
equations, drawings, and animations directly inside a document.
`messages` is a mail and bulletin-board client with the same rich
embedding. In addition to `ez` and `messages`, AUIS is the basis for a
dozen smaller applications, including `bush`, a filesystem browser.

The version being revived, 6.3.1 (August 1994), is the last release CMU
shipped as plain C before the project moved to C++. (See
`version-comparison.md` for the reasoning behind reviving 6.3.1 rather
than the later, never-finished C++ line.) The target platform is macOS on
Apple Silicon, using XQuartz to provide the X11 display server this
software was originally written against.

ATK's central idea is the *inset*: a self-contained, interactive object — a
spreadsheet cell, an equation, a drawing, an animation, a footnote, a
scripted widget — embedded inside a document, able to embed further insets
inside itself, recursively and without a fixed limit. A single `ez`
document can contain a spreadsheet nested inside a drawing nested inside a
footnote. Each inset reads and writes its own portion of the file format,
draws itself, and handles its own input, independent of what it's embedded
in. The model predates the industry's later attempts at the same idea —
Microsoft's OLE and Apple's OpenDoc, both later in the 1990s, OpenDoc
discontinued in 1997 — built in portable C on a small object system called
"Class," contemporary with Objective-C. The application framework —
windows, menus, dialogs, scrolling, printing — is shared across every
ATK program, so `ez`, `messages`, and `help` are entry points onto the same
document-and-inset substrate rather than separate applications.

This code hadn't been compiled in roughly thirty years. Restoring it
required addressing two things: the environment around the code had
changed (new compilers, a new processor architecture, deprecated
operating-system interfaces), and the code itself contained real defects
that had never been exercised in three decades of use. The rest of this
document covers both, along with the strategy used to manage them.

## Modernizing

Some of what changed was required just to make 1994 code run in a 2026
environment. A smaller
amount was optional: bringing part of the software's appearance up to a
standard users now expect, even though the original approach still worked.
Both count as modernization, for different reasons.

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
  required a deliberate, tree-wide search.
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
  it. (Discussed further below.)

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

## Finding ATK lessons in modern implementations

Not every observation from this project is about a defect. A few times,
working through thirty-year-old code turned up a design decision that
still holds up, validated later by a standard that didn't exist yet when
ATK was written.

- **Raster insets and PNG converged on the same bitmap encoding.** Adding
  image support to `ez2md` (the tool that converts `.ez` documents to
  Markdown) meant first decoding ATK's `raster` inset format: a
  one-bit-per-pixel bitmap, packed eight pixels to a byte, most-significant
  bit first, one bit value meaning "black." PNG's one-bit grayscale mode —
  standardized in 1996, years after this raster format was written
  (`rasterio.c` carries an IBM copyright of 1988) — packs pixels exactly
  the same way: MSB-first, one bit each, a bit value mapping straight to
  black or white. The only real work in the conversion was undoing ATK's
  own run-length encoding (a compact ASCII scheme that made sense when
  screen bitmaps had to move over modems and fit on small disks) back into
  raw packed bits, then flipping which bit value means black, since the two
  formats picked opposite conventions for that one detail. No resampling,
  no color-model translation, no coordinate reconciliation — two systems,
  designed for unrelated reasons the better part of a decade apart, landed
  on the same underlying representation for a scanline bitmap.

- **AMS's 1988 client-server interface anticipates several modern IMAP
  extensions.** AMS and IMAP were contemporaries — Crispin's first IMAP
  drafts and Borenstein's message server were designed in the same few
  years, for the same problem, at different institutions. Both settled on
  durable per-message identifiers that survive reconnection (AMS's
  18-character unique ids; IMAP's UIDVALIDITY plus UID). Both retrieve
  message bodies by offset with a bytes-remaining count — AMS's
  `MS_GetPartialBody` has essentially the signature of IMAP's partial
  `FETCH BODY[]<offset.size>`. Both use a two-phase delete: mark, then
  purge (`AMS_ATT_DELETED` and `MS_PurgeDeletedMessages`; `\Deleted` and
  `EXPUNGE`). Both maintain a per-user subscription list over a shared
  folder space. Both send fixed-size summary records in place of messages
  (AMS snapshots; IMAP's ENVELOPE/FLAGS/INTERNALDATE items). The AMS
  spec's engineering doctrine — every call idempotent, a server that "can
  die at any moment, with the client automatically reconnecting and
  continuing its work without any loss of context" — is the same
  discipline a modern IMAP client's reconnect logic depends on. The
  parallels reach past core IMAP, too: AMS computed reply-chain threading
  server-side in 1988 (the snapshot's chain field), which IMAP added as
  the THREAD extension years later; its "master update file" answering
  *what changed since I last looked* cheaply is CONDSTORE's job
  description; its 168 user-defined per-folder attributes are IMAP
  keywords. In one respect AMS is still ahead: the server computes each
  message's one-line caption once, for every client — a summary IMAP
  clients still each rebuild for themselves from ENVELOPE, four decades
  on. The revival's IMAP work relied on this directly: mapping the
  store's operations onto IMAP was mostly transcription, because the two
  designs had already agreed on what a mail store is.

## Old bugs never found till now

Bringing 1994 code onto a 2026 machine did more than require accommodating
a changed environment — it also surfaced defects that had existed in the
source for decades without ever manifesting. Two changes in particular
turned latent mistakes into visible ones: stricter runtime checks in
Apple's C library, which refuses certain unsafe patterns outright rather
than silently permitting them, and, later in the project, a build
configuration that type-checks function calls the original toolchain never
checked. Neither change introduced a defect; both exposed defects that had
been present, and untriggered, since the code was written. A sample of the
most illustrative follows. The fuller catalog — several dozen more of the
same species — is not yet consolidated in one place: some are in
`porting-assessment.md`'s bug-class writeups, some only in
`porting-changelog.md`'s dated log or in `roadmap-old.md`'s retired detail,
and a few live only in fossil's commit history. Bringing them all into
`porting-assessment.md` as the single technical reference is a natural
next step, not yet done:

- **A core interface's arguments were declared in the wrong order for
  roughly 35 years.** The class-definition file for the figure-drawing inset declared
  one method as `Build(action, view, ...)`; every implementation, and every
  call site, used `(view, action, ...)`. This had no runtime effect for the
  entire life of the code, because nothing had ever cross-checked the
  declaration against actual usage — until the ANSI C conversion effort,
  described below, began doing exactly that.

- **A parser that never checked whether an embedded inset loaded.** `ez`'s
  plain-text container read each inset via a success-or-failure code it
  never checked. When a figure's reader failed partway through — on a
  document with an attribute this older reader didn't recognize — the
  container kept parsing from wherever the failed read had abandoned the
  file: the figure's raw internal data appeared on screen as literal text,
  and nothing after it in the document rendered at all. Fixed by falling
  back to a generic reader that resynchronizes by counting nested begin/end
  markers instead of assuming every read succeeds.

- **A debug-tracing macro that could corrupt the C library's own
  declarations.** A debugging header renames `open` to `dbg_open` with a
  preprocessor macro, so that file-descriptor leaks can be traced through a
  wrapper. In source files that happened to include this header before the
  system's `fcntl.h`, the macro rewrote the C library's *own declaration*
  of `open()` — producing a declaration of `dbg_open` with `open`'s
  variadic signature, which the wrapper's fixed three-argument definition
  does not have. For the code's whole prior life this was harmless twice
  over: K&R compilation had no prototypes to mismatch, and the calling
  conventions of the era passed fixed and variadic arguments identically
  anyway. On Apple Silicon they differ — variadic arguments travel on the
  stack, fixed arguments in registers — so caller and wrapper disagreed
  about where the file-permission argument was. Files were silently
  created with garbage permission bits, and code that had just written a
  temporary file found itself unable to reopen it, an "impossible"
  permission error that traced back, through the disassembly, to a macro
  and an include order. Fixed at the root: the debugging header now parses
  `fcntl.h` itself before performing the rename, making the poisoning
  impossible in any include order.

- **The message store's id scheme assumed case-sensitive filenames.** The
  message store names each message's body file after its unique id — an
  18-character string over a base-64 alphabet in which uppercase and
  lowercase letters are distinct values. That scheme silently assumes
  filenames are case-sensitive. In 1988 this looked like a safe
  assumption: the case-insensitive filesystems were the *older*
  systems — TOPS-10, VMS, CP/M, DOS — and UNIX, the newer one,
  distinguished case. The filesystems that ended up dominant on the
  desktop — HFS+, then APFS, and NTFS as Windows uses it — are
  case-insensitive (case-*preserving*, which makes the trap quieter
  still: names display exactly as written, they just refuse to be
  distinct). On a Mac, two AMS ids differing only in letter
  case are different strings and the same file. For thirty-five years
  nothing noticed, because native ids derive from host and timestamp and
  differ in many character positions at once. The revival's IMAP mirror
  found it in an afternoon: it synthesized ids from dense sequential
  message numbers, adjacent ids collided as filenames every time the
  encoding walked the alphabet across a case pair, and a "new" message's
  body file turned out to already exist — containing a different
  message. The mirror's fix is an id alphabet with no case pairs; the
  store's native ids remain a documented, if astronomically unlikely,
  hazard on this platform.

- **Two independent header parsers, written decades apart, made the same
  assumption about where a message's headers end.** RFC822 says a blank
  line separates headers from body; both AMS's own mail reader
  (`text822.c`'s `GetHeader`) and the vendored `metamail` program
  (`Read822Prefix`) implemented that check by testing for a bare `'\n'`.
  That is correct for every message the code had ever seen — the Andrew
  mail system generated and stored its own messages internally with LF
  line endings — until this project's IMAP mirror started handing it real
  wire-format mail from Fastmail/Gmail/Outlook, whose blank line is
  `"\r\n\r\n"`. Neither check ever fires on that input, with two different
  failure shapes: `text822.c`'s header loop never terminates, so it reads
  straight through the "blank line" and treats the entire message body as
  a run of unrecognized headers (displayed in a tiny font, un-decoded,
  every colon making the text before it bold — see `porting-changelog.md`'s
  2026-07-21 entry for the full symptom list this produced); `metamail`'s
  version reads to EOF looking for a separator that never arrives and
  exits with "Could not find end of mail headers", which is what actually
  blocked `cui`'s `type` command on the very first message in a real
  mirrored inbox. Both are the same one-line fix — stop letting a bare
  `'\r'` reset the "did we just see a newline" state — applied
  independently in each file, since neither parser shares code with the
  other. A third site in the same `text822.c` function turned up a day
  later: finding the blank line correctly doesn't mean the lines
  *before* it are clean — `GetHeader` still handed back each header's
  raw `"...\r\n"` verbatim, and every one of those trailing `\r` bytes
  got inserted as a literal, visible character in the ATK `text`
  object, one per header line — "double spaced" was the visible result.
  Same root cause, one level deeper in code this project had already
  touched once.

- **An explicit cast was correct on 32-bit hosts and wrong on 64-bit
  ones.** The font toolkit's bounding-box
  routine measured a string's width by calling its own sibling method
  through an explicit `(long *) &w` cast, where `w` was a plain `int`
  local. On the 32-bit hosts this was written for, `int` and `long`
  were both four bytes, so the cast changed nothing — a stylistic
  choice, not a mistake. On this LP64 port `long` is eight bytes, and
  the same cast became a real four-byte stack overflow on every call,
  silently corrupting whatever local variable happened to sit next to
  it in the frame — every time a figure-inset text label recomputed
  its size. It produced no crash, just gradually garbled label text,
  and it evaded the ANSI conversion's own compiler-warning-driven
  audit for a specific reason: that audit worked by grepping for
  `-Wincompatible-pointer-types` warnings, and an explicit cast is
  exactly what that warning exists to suppress. A census built
  entirely from compiler diagnostics could not, by construction, see
  a mismatch the programmer had already cast into silence; only
  reading the function by hand, then checking the original 1990s
  source to confirm the cast predated this project, found it.

- **A folder-action dialog held pointers into a cache that could be freed
  while it waited.** Clicking a folder in the
  mail overview brings up a "What do you want to do with 'X'?" menu —
  implemented not as a native modal but as ordinary event dispatch, so
  background timers and callbacks kept running while the mouse waited over
  the choices. The function held onto the folder's name strings as raw
  pointers straight out of the directory cache; if anything freed that
  cache entry during the wait — another window's rename/remove
  notification, a periodic refresh — the strings went stale in place, and
  clicking "Subscribe" afterward walked the freed memory character-by-
  character and crashed. One sibling path in the very same function,
  reached by "see the messages," already defended against exactly this by
  duplicating the strings before doing anything that could invalidate
  them — a comment even says so — but the defense was never extended to
  subscribe/unsubscribe or "alter subscription status." For decades this
  raced against, at most, an occasional slow local change and rarely lost.
  The revival's IMAP mirror gave the directory cache far more frequent
  invalidation traffic than the original single-user model ever produced,
  turning a latent race from 1994 into a reproducible crash the first time
  someone tried to subscribe to INBOX. Fixed by giving the affected paths
  the same heap-copy discipline the "see the messages" path already had.

- **An error-cleanup path deleted the wrong file on a name collision.**
  When the store
  writes a message's body file, it opens with `O_CREAT|O_EXCL` — and if
  the open *fails*, the error path does `unlink(File)` before returning.
  That unlink is meant to remove a partially-written file, which is the
  right cleanup everywhere later in the function, after the open has
  succeeded. On the open-failure path there is no partial file — and in
  the one failure mode where the target *does* exist, `EEXIST`, the file
  belongs to an earlier message. So for thirty-five years, any append
  that collided with an existing message file quietly deleted the
  existing message. The author suspected something: the line above the
  open reads `/* Bogus -- what if not overwrite? */`. It never fired
  because the store's duplicate check (which keys on the RFC-822
  Message-ID header) almost always intercepted true duplicates before
  the filesystem could, and id collisions otherwise didn't happen. The
  IMAP mirror lined up the holes: a message with no Message-ID slipped
  the duplicate check, an RFC 3501 `n:*` range quirk re-presented an
  already-mirrored message as new, and the mirror watched a message
  file vanish from a directory nothing was supposed to delete from.
  Fixed by deleting the one erroneous line.

- **Misspelled function names linked silently to the wrong library for
  thirty-five years.** Three mail-filtering primitives — regular-
  expression search and decomposition operations available to the mail
  system's rule-based filtering language — called functions named
  `regcomp` and `regexec` to compile and run a pattern. No function by
  either name has ever existed anywhere in this codebase; the regular-
  expression engine actually built and shipped with the software, a few
  directories over, is named `reg_comp` and `reg_exec`, with an
  underscore neither call site had. Pre-standard C never checked that a
  called function actually existed before compiling the call, so the
  compiler raised no objection, and the linker simply resolved the
  misspelled names against whatever else provided them — in this case,
  the operating system's own built-in `regcomp`/`regexec`, a wholly
  different regular-expression implementation expecting a different
  kind of first argument than the plain string these call sites
  actually pass. Every call, for the software's entire life, therefore
  invoked the wrong function with the wrong argument shape — a defect
  invisible to any compiler or linker, on this platform or any other,
  because both misspelled names happened to already mean something.
  It came to light not through an automated check (nothing about it
  would trip one) but during a close reading of the file while fixing
  unrelated compile errors as part of the ANSI C conversion effort.
  Corrected to call the codebase's own, correctly named and typed,
  `reg_comp`/`reg_exec`.

- **The same declaration mismatch, independently, in five unrelated parts
  of the codebase.** An interface file declaring
  a wrong-but-plausible type for a constructor or destructor's own
  object parameter — invisible because K&R never checked it — turned up
  on its own in form widgets and a diagram-editing framework (six
  instances), the mail reader's own message-display view classes (three
  more), and the drawing library's arrow and polyline figure classes
  (two more). Every real implementation was already correct in all
  eleven cases; only the paperwork was wrong, apparently copy-pasted
  from a neighboring class's declaration each time. A related mistake
  in the same family ran the other direction — a drawing-library base
  class silently dropped an argument all fifteen of its real subclasses
  supplied — and a dead-end variant turned up too: two destructor
  methods misspelled by one transposed letter pair ("Finialize" for
  "Finalize"), never wired to anything and never missed, caught before
  they could do damage rather than after. None of it needed a runtime
  symptom to find; it surfaced by the same method every time, once the
  ANSI C conversion's typed interfaces finally gave the compiler
  something to check these old declarations against.

- **Attacker-controlled mail headers, used as a format string.** Two
  routines in the metamail viewer — one saving a MIME attachment under a
  name derived from its `Content-Type` parameters, one decoding an RFC
  2047 encoded word from a message header — each pass text taken
  directly from the *message itself* as the format argument to
  `printf`/`fprintf`, rather than as an ordinary data argument. Any
  percent-conversion sequence a message's author places in a filename or
  an encoded-word payload is interpreted as a format directive against
  whatever the stack happens to hold at that call, not displayed
  literally — a live format-string vulnerability sitting in
  header-parsing code that has processed arbitrary, untrusted mail for
  over thirty years. It surfaced not from a security review but from the
  same mechanical format-string sweep that found every other bug of this
  shape in this project — the compiler's own diagnostic for a
  non-literal format argument flags this pattern regardless of whether
  anyone was looking for a vulnerability. Corrected by passing each
  string as an ordinary `"%s"` argument at both sites.

- **A stale, argument-less forward declaration shadowed a correctly typed
  real definition.** Dragging a
  figure in the drawing editor worked vertically but froze horizontally
  after the first motion event. The function that computes each drag
  step's constrained coordinates was, at its real definition, already
  correctly typed to write a `long` through each of its two output
  pointers — but two duplicate copies of its forward declaration,
  sitting above every call site in the same file, still used the old
  argument-less style (`static int Set_Constraints();`), which is legal
  ANSI C and gives the compiler no parameter information at all. The one
  call site that mattered — the continuous drag-motion handler — held
  its output variables in ordinary 4-byte `int`s, so every motion event
  wrote 8 bytes through a pointer to a 4-byte stack slot, silently
  corrupting whatever sat next to it. Retyping the file's forward
  declarations to match their real definitions closed the hole and
  immediately surfaced a second bug of the same shape in the same file.
  Neither was reachable by any warning this project had enabled up to
  that point — `-Werror=int-conversion` and its siblings only fire when
  a real prototype is in scope to compare against, and an argument-less
  old-style declaration is, by definition, not one. That gap raised an
  obvious question — how many other stale, argument-less forward
  declarations were sitting above correctly-typed real definitions
  elsewhere in the tree? — and a tree-wide census answered it: roughly
  6,700 such declarations, across nearly every active directory. A
  mechanical pass retyped each one to match its real definition and let
  the compiler itself separate harmless old style from genuine bugs,
  turning up a short, specific list of further defects — an AMS API
  call whose byte-count out-parameter had been declared half the width
  every caller expected, a file handle typed as a plain integer where
  every caller and the function's own body treated it as `FILE *`,
  and nine ordinary argument-count/type drifts — all fixed, none of it
  needing its own rollout milestone the way M1–M4 did.

- **A crop rectangle was never checked against the image it was
  cropping.** A raster-format converter's crop option never validated its
  rectangle against the source image's actual dimensions before reading
  it — the underlying bitmap routines don't bounds-check either, by
  design, on the assumption that callers already have. A crop rectangle
  larger than the source image read straight past the end of the
  allocated buffer and kept going, producing an 18-megabyte file of heap
  garbage instead of an error, for an operation that should have been
  rejected outright. Fixed with an explicit bounds check ahead of the
  crop.

None of these are new mistakes. Each was introduced once, decades ago, and
never triggered — because the exercising code path was never run, because
nothing had checked a declared interface against its actual usage, or
because an earlier C library was more permissive. There was no test suite
to find them: what did was a stricter compiler, real use, and weeks of
manual, heuristic-driven work — grepping for a suspect pattern, rebuilding
with `make -k` to surface every hit at once, then reading each one by
hand. This development and repeated toil was done by Claude Code,
primarily with the Sonnet model managing delegated sessions following an
evolving `sonnet-playbook.md`, with the Fable model occasionally used for
reviews and for difficult problems.

**One exception, caught the same day it was made.** A drawing editor's
on-disk format for a figure's "mode" attributes packs three single-
character flags onto one line; the code that writes that line built all
three characters but only ever wrote two, silently dropping the third —
"halo" — on every save, a plain thirty-year-old bug fixed like the rest
above. Fixing it exposed a second, previously dormant bug in the very
same function: the three flag buffers were declared
`static char foo[2] = "?"`, scratch space seeded with a literal `?` and
only ever overwritten when their flag was on, never reset when it was
off. Because `halo` had never actually appeared in the output before,
its stale `?` sentinel was invisible; the moment the fix started
printing it, every figure with the halo flag off — effectively all of
them — got a literal `?` appended to its Mode line, desynchronizing the
reader for the rest of the document. Caught within hours by manual
round-trip testing (open, edit, save, reopen), not by any compiler
warning — nothing about a string that compiles cleanly but holds the
wrong content trips a diagnostic. Fixed by dropping `static` and letting
the buffers default to empty, which is arguably a second correction to
the same original mistake: true K&R C never allowed a stack variable to
carry an initializer at all, so the 1988 author most likely reached for
`static` only to make the `= "?"` syntax legal, not because persistence
across calls was ever wanted.

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

1. **A function returning a pointer — or any other 64-bit value — called
   with no declaration in scope.** Pre-standard C assumes an undeclared
   function returns a plain 32-bit `int`. If the function actually returns
   something wider, the upper half is silently discarded — typically
   producing a crash the moment a truncated pointer is used, or a
   silently wrong number when the return is an ordinary integer instead.
   This was the single most common defect in the codebase: more than
   twenty separate sites, all the same shape, once the pattern was
   recognized in one core function used to locate files on disk. The
   later, tree-wide sweep for exactly this class of gap (M2, below) found
   the same shape recurring by the hundreds across the rest of the tree —
   including several genuinely `long`-returning (not pointer-returning)
   library functions, such as a mail client's directory-lookup and
   session-initialization calls, each confirmed against its real
   definition, independently, before any declaration was written.
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

Three related but mechanically distinct defects showed up alongside these
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
- **A file-header struct declared its fields `long`, matching the on-disk
  format only by 32-bit coincidence.** A raster-format command-line
  converter, untouched by every earlier width-bug sweep because nothing
  about it resembled the dispatch-related patterns above, declared its
  file-header struct's three fields `long` — matching the format's actual
  on-disk layout, a fixed 14-byte header, only on the 32-bit machines this
  was written for. On a 64-bit build the struct is more than twice that
  size, so every field after the first lands at the wrong offset; reading
  such a file back fed uninitialized stack memory into an image-resize
  call that ran away, consuming multiple gigabytes of memory before being
  killed. The same file's color-inversion routine carried an independent
  case of the identical assumption: a hand-optimized loop inverting four
  bytes at a time walked a raw pointer cast to `long *`, with address
  arithmetic sized for a 4-byte word; on this LP64 build each step
  silently inverted eight bytes instead, missing a row's first few bytes
  and overrunning its last few into whatever memory followed. Both had
  sat unexercised since the format was written — nobody had tested this
  converter's own output against itself on this platform until it finally
  was, byte-for-byte, against the original image.

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
  pointer-truncation pattern for good. The sweep surfaced two
  complications of its own along the way: `malloc`/`free`/`realloc`/
  `calloc` are compiler built-ins, so a call to one with no declaration in
  scope anywhere never triggers the diagnostic being swept for — invisible
  to the compiler-driven pass, closed only by a second, deliberate search
  across every file regardless of how clean a directory's compile already
  looked; and one class's method-dispatch macro, an optimization that
  calls its internal implementation directly instead of through the usual
  indirect table, turned out to declare that implementation inside a
  header guard whose actual purpose was unrelated — avoiding duplicate
  method tables across a multi-file loadable module — which had the side
  effect of hiding a real, correctly-typed declaration from the one
  caller that needed it, harmlessly, for over thirty years.
- **M3 — convert the function definitions themselves** from pre-standard
  to standard C, one subsystem at a time, using a purpose-built tool that
  looks up each function's real signature from the class-definition files
  rather than inferring it — the safer approach the original mass-
  conversion attempt should have taken. Completed tree-wide August 2026,
  15 sessions across all 91 active directories. Consistent with M1's
  experience, real type/arity checking surfaced numerous decades-old
  interface bugs along the way — several are recounted below — and the
  conversion tool itself needed two rounds of fixes for K&R declaration
  shapes its own parser had missed.
- **M4 — enable full compiler strictness** tree-wide, once every subsystem
  has been converted, which closes the remaining scanf-format-code bug
  family as a side effect. Completed August 2026, directory by directory,
  seven waves plus an initial pilot batch.

As of this writing, M1 through M4 are complete across the entire active
codebase, each having turned up several of the decades-old defects
described above. A follow-on tree-wide census closed the one remaining blind
spot the four milestones couldn't see on their own — stale argument-less
forward declarations shadowing correctly-typed real definitions — and is
described in its own entry above.

## Where things stand today

`ez` (the word processor), `help`, and `messages` (mail, running against a
local mailbox and, now, a live IMAP mirror, rather than the original 1990s
shared-filesystem delivery system) all run and render correctly, including
most embedded inset types: text, equations, tables, drawings, animations,
footnotes, spreadsheets, and more. `bush`, `org`, `chart`, and `layout` all
run cleanly too. The ANSI C conversion described above (M1 through M4) is
complete across the entire active tree — every method call and function
definition in it is now compiler-checked. A live, itemized table of what's
fully working versus still rough is in `roadmap.md`, rather than repeated
here.

One deliberate scope decision from early in the project: the Console
(terminal-emulator) subsystem is not part of this build.
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
- **`metamail` has never successfully run in this environment.** Invoking
  it — directly, or indirectly through `messages`' MIME-attachment
  handling — raises `SIGTTOU` inside the routine that spawns a
  mailcap-entry viewer subprocess: ordinary 1980s terminal job-control
  code performing an `ioctl()` a foreground process doesn't expect.
  Confirmed unrelated to the ANSI C conversion work happening alongside
  it — the crash lives entirely in terminal/process-group handling,
  untouched by any declaration or typing fix — and predates this project;
  nobody has reported metamail working here at any point. Root cause
  identified; not yet fixed.

## Further reading

- `porting-changelog.md` — the complete, dated history of every fix, in
  the order it happened
- `porting-assessment.md` — the underlying bug-class analysis, strategy
  decisions, and the full ANSI C conversion plan
- `roadmap.md` — current status, open issues, and near-term plans
- `version-comparison.md` — why 6.3.1 (the last C release) was chosen over
  the later, unfinished C++ rewrite
- `quickstart.md` — how to build and run this on a Mac
