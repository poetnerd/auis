# AUIS Revival: What Was Done and Why

*Last distilled: 2026-07-25.* A narrative account of reviving Carnegie
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

## Finding ATK lessons in modern implementations

Not every observation from this project has been about repairing decay.
Occasionally, working through thirty-year-old code turns up a design
decision that still holds up — quietly validated by a standard that didn't
exist yet when ATK was written.

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
  on the same underlying representation for a scanline bitmap, because it's
  simply the obvious correct one.

- **The Andrew Message System's 1988 client-server interface reads like a
  preview of the modern IMAP extension stack.** AMS and IMAP were
  contemporaries — Crispin's first IMAP drafts and Borenstein's message
  server were designed in the same few years, for the same problem, at
  different institutions — and the convergences run deep. Both settled on
  durable per-message identifiers that survive reconnection (AMS's 18-
  character unique ids; IMAP's UIDVALIDITY plus UID). Both retrieve
  message bodies by offset with a bytes-remaining count — AMS's
  `MS_GetPartialBody` has essentially the signature of IMAP's partial
  `FETCH BODY[]<offset.size>`. Both use a two-phase delete: mark, then
  purge (`AMS_ATT_DELETED` and `MS_PurgeDeletedMessages`; `\Deleted` and
  `EXPUNGE`). Both maintain a per-user subscription list over a shared
  folder space. Both send fixed-size summary records in place of messages
  (AMS snapshots; IMAP's ENVELOPE/FLAGS/INTERNALDATE items). And the AMS
  spec's engineering doctrine — every call idempotent, a server that "can
  die at any moment, with the client automatically reconnecting and
  continuing its work without any loss of context" — is precisely the
  discipline that makes a modern IMAP client's reconnect logic workable.
  Where the parallels reach past core IMAP is the striking part: AMS
  computed reply-chain threading server-side in 1988 (the snapshot's
  chain field), which IMAP added as the THREAD extension years later; its
  "master update file" answering *what changed since I last looked* cheaply
  is CONDSTORE's job description; its 168 user-defined per-folder
  attributes are IMAP keywords. In one respect AMS remains ahead: the
  server computed each message's one-line caption once, for every client
  — a summary IMAP clients still each rebuild for themselves from
  ENVELOPE, four decades on. The revival's IMAP work leaned on this
  kinship directly: mapping the store's operations onto IMAP was mostly
  transcription, because the two designs had already agreed on what a
  mail store is.

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

- **A leak-tracking macro that could poison the C library's own
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

- **A bet on the direction of filesystem history, quietly lost.** The
  message store names each message's body file after its unique id — an
  18-character string over a base-64 alphabet in which uppercase and
  lowercase letters are distinct values. That scheme silently assumes
  filenames are case-sensitive. In 1988 this looked less like an
  assumption than an observation about progress: the case-insensitive
  filesystems were the *older* systems — TOPS-10, VMS, CP/M, DOS — and
  UNIX, the future, distinguished case. History went the other way.
  The filesystems that won the desktop — HFS+, then APFS, and NTFS as
  Windows uses it — are case-insensitive (case-*preserving*, which makes
  the trap quieter still: names display exactly as written, they just
  refuse to be distinct). On a Mac, two AMS ids differing only in letter
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

- **A 35-year memory leak on the duplicate-message path.** The message
  store's append routine reads an entire message into memory, then — if
  it discovers the message is already present — returns success without
  freeing it. Every other exit from that function frees the buffer; the
  duplicate path leaked the whole parsed message. Nobody ever saw it
  because duplicate appends were rare, one-at-a-time events in
  short-lived processes. The IMAP mirror made the path hot: a sync
  recovering from lost state re-appends thousands of already-present
  messages in one process, which would have transiently leaked
  approximately the whole mailbox. Found by code review during the
  mirror work, fixed with one `FreeMessage` call.

- **A cast that used to be true.** The font toolkit's bounding-box
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

- **A blocking dialog that outlived its answer.** Clicking a folder in the
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

- **The cleanup that destroyed what it collided with.** When the store
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
  One deleted line fixed it — the rare bug you repair by removing code.

- **A header parser that assumed local mail's line endings, forever.**
  The mail reader's rule for finding where headers end and the body
  begins — read lines until a blank one — tested only for a bare LF
  (`'\n'`). RFC 822's wire format specifies CRLF, and Andrew's own local
  mail delivery had evidently always normalized incoming mail to
  bare-LF line endings before this parser ever saw a message, because
  the check had gone unexercised against genuine CRLF for the code's
  whole life. Fetching raw messages directly from a live IMAP server,
  CRLF intact, was new. Against such a message the blank-line check
  never fires: the parser reads straight through the header/body
  boundary and keeps going, consuming the entire body as if it were
  more headers — each line displayed as an unrecognized header in tiny
  type, undecoded, with a colon-triggered bolding rule occasionally
  highlighting ordinary prose. The visible symptom (a screenful of raw
  header lines, then unreadable body text with a literal `=20` where a
  quoted-printable space belonged) took real mail, not local mail, to
  produce — and was found the same way most of this list was: a user
  looked at real output and described exactly what was wrong. Fixed by
  recognizing CRLF as well as LF at both header/body boundary checks,
  plus the same LF-only blindness in the quoted-printable soft-line-
  break decoder one call downstream of it.

- **A dynamic-loading convention that only ever worked by coincidence.**
  Most of the mail reader's dynamically-loaded `.do` modules link with
  no library list of their own at all — every external symbol resolves
  at load time against whatever else the process happens to have
  already loaded (`-undefined dynamic_lookup`, which lets a missing
  library pass silently at build time and fail only at the first actual
  call). One module in particular had never once, in its whole life,
  called a symbol that wasn't already provided by something else loaded
  earlier in the process — so the gap in its own link line was
  invisible for as long as that held. The first new library call it
  ever made jumped straight to address zero. Fixed by giving that one
  module an explicit link line, and confirmed with `nm -m` that the
  symbols it actually calls now resolve inside its own binary rather
  than by chance.

- **A four-function type mismatch that outlived the language it was
  written for.** The mail server's directory-info, new-message-count,
  subscription-entry, and changed-subscriptions calls were declared in
  the class layer as returning their results through pointers to a
  64-bit-wide integer, while every one of their actual C
  implementations wrote through pointers to a plain 32-bit integer — a
  disagreement present in the very first commit that ever brought this
  source into version control, and certainly older than that. On the
  machines this code was written for, the two integer sizes were the
  same, so no data was ever lost no matter which one a caller believed;
  the mismatch was invisible by coincidence, not by correctness. On a
  modern 64-bit machine the two sizes differ, and coincidence stops
  covering for the bug — except this one stayed hidden even here,
  because the calling code had *also*, independently, been written
  expecting the narrower size, restoring the coincidence one layer up.
  It took a routine sweep fixing exactly the kind of caller/declaration
  disagreement this bug produces — widening a caller to agree with the
  class layer's declared type — to remove that second coincidence and
  let the original one through: a folder's subscription-status line
  began reporting a nonsensical negative count instead of a real one,
  the first time in the software's history it had ever actually done
  what its own interface claimed it did. Corrected by tracing every
  affected function to its real C body and matching the class layer to
  *that*, not the other way around — the class declaration, not the
  implementation, was the thirty-year-old mistake.

- **A misspelled function call that silently linked to the wrong library
  for thirty-five years.** Three mail-filtering primitives — regular-
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

- **A misspelled preprocessor guard that quietly deleted a header's typed
  half for over thirty years.** A menu library's header offered two
  versions of its own function declarations, selected by `#ifdef`: a
  fully typed set for a standards-conforming compiler, and an older,
  untyped fallback set for one that predates function prototypes. The
  guard tested `_STDC_` — one underscore short of `__STDC__`, the name
  every C compiler that defines this macro at all has actually defined
  since the 1989 standard. No compiler, then or since, has ever defined
  the misspelled name, so the typed half of the header was dead on
  arrival: every build silently took the untyped fallback, and one
  function the fallback branch omitted entirely went undeclared
  wherever a caller didn't supply its own local declaration. It surfaced
  only when the ANSI C conversion effort's own `-pe` typed-prototype
  mechanism, applied to a neighboring directory, made the header's
  guard load-bearing for the first time — every caller that had been
  quietly relying on the fallback branch's absence of type-checking now
  needed the guard to actually pick the typed branch. Flipping the
  fallback branch on for real then exposed a second, independent latent
  mistake in that same never-before-compiled typed branch: one
  declaration's return type had been omitted (silently defaulting to
  `int`) while the real function was `void`, a mismatch nothing had ever
  checked because the branch had never been live long enough to check
  it against anything. Corrected both: the guard now reads `__STDC__`,
  and the return type now matches the definition.

- **A stray dereference that only became visible once a call's argument
  types were finally checked.** A function managing a window's color
  table took a pointer to a color-table structure and, in one of its
  three call sites, passed not the pointer itself but the structure
  it pointed to — dereferencing it — to two operations that both
  expect the pointer. Nothing caught this for the entirety of the
  program's life: the era's compiler performed no argument-type
  checking across these particular calls at all, so a whole structure
  handed to a function expecting a pointer to one was simply
  accepted and misinterpreted. The mistake surfaced only once these
  calls were finally typed as part of the ANSI C conversion effort,
  and only then because something forced the file to actually
  recompile — for over two weeks after the typed calling convention
  went into effect tree-wide, this file's own object code had never
  been rebuilt, so the now-real type mismatch had nowhere to raise an
  error until this session's work finally did. Confirmed against a
  neighboring, correct call in the very same function, which passed
  the pointer directly with no dereference — the same argument, spelled
  two different ways four lines apart, only one of them right.
  Corrected by removing the stray dereference at both incorrect call
  sites; the third call in the function, structurally different (a
  pointer to a pointer, correctly dereferenced once), was already
  right and left untouched.

- **Six copy/paste typos in class interface files, each substituting
  a plausible-looking wrong type name for the right one.** A class's
  interface file declares, for its own constructor and destructor
  methods, which structure type the object being built or torn down
  actually is. In six places across two unrelated subsystems (form
  widgets and a diagram-editing framework), that declared type was
  wrong — not garbled, but a real, existing, *different* type: a
  sibling class one character away in the alphabet, the class's own
  parent, the short internal filename a class is stored under rather
  than the class's real name, and in one case a type that does not
  exist anywhere in the codebase at all. Every implementation was
  already correct — these were declaration-only mistakes, and the
  declared type for a constructor happens to be pure documentation to
  the compiler that built this software originally, checked against
  nothing, so a wrong entry there had no way to ever be caught. One of
  the six was not a constructor or destructor but an ordinary method
  used throughout its own file — its wrong declared type meant every
  one of that method's own field accesses was, technically, accessing
  the wrong structure's memory layout by name, silently correct only
  because the two structures happened to be laid out compatibly by
  coincidence. Found by systematically checking every such declaration
  in two directories against the real class each belonged to, once the
  ANSI C conversion effort's own type-checking made the first of the
  six impossible to ignore. Corrected all six to name the class they
  actually belong to.

- **A code-generation tool that silently contradicted its own
  generated code, for one particular kind of destructor — invisible
  until its own typed-declaration option was actually switched on for
  the first time.** This software's class-generation tool
  automatically writes, for every class, both the declaration of that
  class's cleanup method and the internal code that calls it. For one
  specific method name, the tool's two halves disagreed: the code it
  generated to *call* the method always passed two pieces of
  information, but the declaration it generated for that method —
  whenever a class's own interface didn't spell out the second one
  explicitly — only promised one. Nothing ever caught this, because
  the tool's optional feature for emitting these fully spelled-out
  declarations was essentially never turned on for real, anywhere,
  until the current compiler-modernization effort began switching it
  on one part of the software at a time, decades after the tool itself
  was written. The first time it was turned on for a class using the
  plain, undecorated form of this declaration, the tool's own generated
  file contradicted itself and failed to compile. A related
  complication made the obvious fix wrong: a handful of classes
  legitimately override this same method with a different kind of
  return value than the rest, and a fix assuming one universal shape
  for it would have broken exactly those classes, which had been
  working correctly the whole time. Corrected by making the tool
  recognize only the specific shape that was actually broken, leaving
  every other class's generated code exactly as it already was —
  verified by regenerating every affected class's generated file and
  confirming it came out byte-for-byte identical to before, except for
  the one shape being fixed.

- **A destructor that never woke up, twice.** Two unrelated classes in
  two unrelated subsystems — one a diagram-editing element, the other
  part of the text-content machinery — each carry a second cleanup
  method alongside their real one, its name misspelled by one
  transposed pair of letters: "Finialize" for "Finalize." Both
  misspelled versions have empty bodies and were never called, in
  either subsystem, in the decades since they were written — the
  class machinery dispatches destructors by exact name, so a
  misspelled one is simply invisible to it, never wired to anything,
  never missed. Unlike every entry above, this one has no "corrected
  by": there is nothing to fix, because nothing was ever broken —
  the real, correctly-spelled destructor in each class already does
  the actual cleanup work and always has. It's recorded here anyway
  because it's the same shape of mistake as the six copy/paste typos
  above — a plausible-looking wrong spelling that the era's tools had
  no way to ever flag — just caught this time before it could do any
  damage, by the same systematic method-by-method reading that the
  ANSI C conversion effort required everywhere else.

- **An interface that never mentioned the one argument its own
  implementation always needed.** A tree-widget class declares two
  measurement methods — one for a subtree's width, one for its
  height — and describes both, in the file that's supposed to be their
  authoritative interface, as taking no arguments beyond the object
  itself. Every real implementation of both methods, from the day they
  were written, took a second argument: which node in the tree to
  measure. Nothing before this project ever compared the declared
  interface against the working code, so a function that could not
  possibly have worked with the interface as written ran, correctly,
  for decades — because nothing ever called it through that interface
  in the first place. Neither method has a single caller anywhere in
  the source tree; whatever originally needed a subtree's dimensions
  either used another path or was never finished. Corrected by adding
  the always-present, never-declared second argument to the interface,
  matching the implementation that was right all along.

- **A view-scrolling method whose interface named the wrong structure
  for its own parameter, from the day it was written.** A text view's
  interface file declares a method that repositions one on-screen line
  during scrolling, taking a pointer to the line being moved. The
  declared type was the structure used elsewhere in the same interface
  for a position within a document — but the method's own
  implementation, and every one of its seven call sites, always passed
  a pointer to an unrelated structure: the type used for an on-screen
  line's own layout bookkeeping (its height, character count, screen
  position). The two types share no relationship; a correct call
  compiled and ran fine under K&R only because pre-standard C never
  checked a call's arguments against the interface it declared. Every
  real caller agreed with the implementation and always had — only the
  interface was wrong, invisible until the ANSI C conversion effort's
  typed-prototype mechanism finally checked it. Corrected by changing
  the interface's declared type to match the implementation and every
  caller.

- **A helper function called with an argument it never had, for the
  entirety of its life.** A text view's internal helper for finalizing
  a selection takes exactly two arguments: the view itself and the
  selected length. Every one of the five places in the same file that
  called it, for as long as the file has existed, passed a third — a
  true/false flag left over from some earlier shape of the function
  that its actual body has never referenced. Pre-standard C's calling
  convention accepted extra arguments silently, so the mismatch cost
  nothing: the stray value landed nowhere the function ever looked, and
  the two real parameters arrived exactly where expected. It became a
  compile error, not a runtime mystery, only once the ANSI C conversion
  effort gave the function a fixed, checked argument count — the same
  conversion that exposes an interface's wrong argument *type* elsewhere
  in this list, here exposing a caller passing the wrong argument
  *count* instead. Corrected by dropping the stray argument at all five
  call sites, matching the function's own long-unchanging, correct
  behavior.

- **Three more copy/paste type typos, found later in a fourth
  subsystem.** The same mistake as the six above, this time in the
  mail reader's own view classes: the ones that display a message
  body, a mail object, and a decoded 822-format body each declare, in
  their own interface file, a constructor and destructor whose object
  parameter is typed as a different class entirely. Two of the three
  copied the exact same wrong type from an unrelated sibling class
  declared earlier in the same file; the third named two different
  wrong types for its two methods, neither one its own. Every real
  implementation, as always, used the correct type and always had. The
  constructor half of all three stayed pure documentation even after
  this project's type-checking arrived, for the same reason as
  before — the class-generation tool supplies the real type itself
  regardless of what a constructor's interface claims. The destructor
  half was more exposed: for one of the three it stayed a silent
  disagreement, because the file containing the real destructor never
  happens to look at the file containing the wrong declaration in the
  same compile; for the other two, declaration and destructor share a
  file, the two disagreed inside a single compile, and the build
  failed until this was fixed. Corrected by retyping all three
  declarations to the class they actually belong to.

- **A folder-tree class whose interface file redundantly named an
  argument its own code generator was already going to supply —
  wrong in two different ways for two of its three startup/teardown
  methods.** The class-generation tool automatically prefixes every
  startup and teardown method's generated declaration with a hidden
  first argument — the class itself — whether or not the interface
  file spells it out by name; every other class in the same directory
  leaves it out, as the tool expects. This one class's interface
  explicitly restated that argument anyway, for two of its three
  lifecycle methods, doubling it in the generated declaration each
  time: once for the plain, once-per-class initializer (declared with
  one named argument where the convention — and the working code —
  wanted none), and once for the per-object destructor (declared with
  its own restated first argument *plus* the required second one, for
  three total against the real function's two). A third restatement,
  on the one lifecycle method the tool always re-types from scratch
  regardless of what the interface says, looked like the identical
  mistake but never actually mattered. Neither of the two real
  mistakes was ever caught by the original compiler, which built a
  call from whatever the interface said and never checked it against
  the function actually being called. The destructor half surfaced
  first and most visibly: once this project's typed-header generation
  finally emitted this particular class's declarations, the generated
  file contradicted itself internally — its own generated call to the
  destructor, four lines above, still used the correct two-argument
  form. Corrected by removing the redundant restated argument from
  both declarations, restoring the interface every other class in the
  directory already followed — the same species of code-generator
  self-contradiction as the destructor described earlier in this list,
  just caught here for a second time, and this time affecting the
  class's constructor too.

- **A drawing library's abstract base class silently dropped an
  argument every one of its real subclasses used.** Five related
  methods — highlighting, normalizing, exposing, hiding, and printing
  a figure's selection points — are declared by the drawing library's
  interface file with two arguments: the figure and the pane it's
  drawn in. The base class's own versions, meant only as placeholders
  for figure types that don't override them, took just the figure,
  silently dropping the pane. Every one of the roughly fifteen real
  figure types in the library — arcs, rectangles, polygons, arrows,
  and the rest — override all five and correctly take both arguments;
  only the never-overridden base placeholders were short. Pre-standard
  C's calling convention let this go unnoticed for the same reason as
  the helper function above: a call always supplied both arguments,
  and the placeholder's body, which does nothing but return a fixed
  failure code, never looked for the one it lacked. Corrected by
  giving all five placeholder definitions the same two arguments as
  the interface and every real override already agreed on.

- **A status-message variant whose interface never matched the code
  it was calling, and nothing ever called either one.** The same
  drawing library's status-line class declares two message-issuing
  methods — one for a raw string, several more for typed variants
  (a figure, an image, a stream, a pane). The raw-string pair's
  interface entry took a single string argument, copied from a
  differently-named sibling method just above it in the same file.
  Its real implementation instead took two numeric codes and built the
  string internally, the same shape every one of the typed variants
  below it uses — not the string-based one it was declared to match.
  Neither name, under either signature, was ever called anywhere in
  the source tree: a rare case in this project where a `.ch`-vs-`.c`
  disagreement isn't a live bug at all, just two halves of a method
  that was apparently redesigned once, in code, and never updated in
  its own interface file. Corrected by retyping the interface to match
  the real, working implementation, the only side with any evidence of
  intent behind it.

- **Two more copy/paste type typos, in a fifth subsystem.** The same
  mistake as the six, then three, documented earlier in this list —
  this time in the drawing library's arrow and polyline figure
  classes, each declaring its own one-time setup method with its
  parent class's type instead of its own, both apparently copied from
  the same line in a shared ancestor file. Both real implementations
  used the correct type, and each reads a field that only its own
  class has — which is what confirmed the interface files were wrong
  rather than the code: neither placeholder type could have compiled
  against that field at all. Corrected by retyping both declarations
  to the class they actually belong to.

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
  conversion attempt should have taken.
- **M4 — enable full compiler strictness** tree-wide, once every subsystem
  has been converted, which closes the remaining scanf-format-code bug
  family as a side effect.

As of this writing, M1 and M2 are both complete across the entire
active codebase; M3 is underway, one subsystem at a time.

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
