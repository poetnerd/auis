# AUIS Version Comparison: 6.3.1 (C) vs 7.4/8.0 (C++)

An analysis of what the C++ versions added beyond the last public C release,
to inform the decision of which codeline to revive.

## Release timeline

- **5.2** (early 1992) — CDrom release
- **6.1** (late 1993) — beta for X11R6 contribution
- **6.2** (March 1994) — first public release since 5.1; major POSIX work
- **6.3** (late 1994) — X11R6 contrib release; "last release of AUIS in C"
- **6.3.1** (August 1994) — bug fixes to 6.3
- **7.4** (August 1996) — first public C++ release (binary only, free)
- **7.5** — binary-only public release
- **8.0** (January 1997) — C++ source release, members-only, "not extensively tested"

## New features in the C++ versions (7.4 / 8.0)

### Direct-to-PostScript printing

The entire print pipeline was replaced. No more dependency on troff/psdit.
Faster formatting, smaller output files. The `preview/` directory was
removed from ATK, presumably folded into the new printing path.

### Color manager

Dynamic color allocation and dithering for images. Replaces the crude
5x5x5 color cube from v6.x. Colors are reallocated across all Andrew
windows as images are added. Dithering option for better appearance on
limited-color displays.

### Recursive search (recsearch)

Searching across ALL content including text inside embedded insets.
In 6.3.1, search only finds text in the top-level text object.

### ATK base class

All objects now derive from a common `ATK` class, enabling runtime type
testing and dynamic code loading by type. A fundamental object-model change.

### Widgets framework

New `awidgetview` and attribute-driven dataobject system for building UI
controls (buttons, checkboxes, radio buttons). The `widgets/` directory
in v8.0 has no counterpart in v6.3.

### Web browser / improved HTML

`htmltext` substantially rewritten. HTML forms support via the new widgets.
`ez2html` converter can convert arbitrary insets to GIFs for web publishing.

### Improved srctext

New language modes: IDL, Perl, REXX, Scribe (added to the existing set of
assembler, C++, C, Lisp, Modula-2, Modula-3, Pascal, man pages, HTML).

### gofig

Specialized inset for Go game diagrams (Fred Hansen's example of rapid
inset development in C++).

### Bug fixes

The v7.4 announcement says "literally thousands" of bug fixes. The C++
port likely caught many latent bugs through stronger type checking.

## Directory structure changes (6.3 vs 8.0)

### ATK directories added in v8.0

- `typescript/` — terminal emulator (moved or promoted to standalone)
- `utils/` — utility code
- `widgets/` — new widget framework
- `web/` — web-related code
- `mit/` — replaces `music/`
- `pkgs.mcr` — package configuration

### ATK directories removed in v8.0

- `preview/` — folded into new PostScript printing
- `music/` — replaced by `mit/`

### Overhead directories added in v8.0

- `c++conv/` — C-to-C++ conversion tools
- `dynlink/` — new dynamic linking (replaces old platform-specific loaders)
- `external/` — external interface support
- `genstatl/` — static linking (concept existed in 6.3, promoted here)
- `grefs/` — global references
- `relativize/` — path relativization

### Overhead directories removed in v8.0

- `conv/` — replaced by `c++conv/`
- `image/` — moved elsewhere
- `malloc/` — presumably using system malloc

## Key improvements already present in 6.3.1

These were accomplished in the 5.2 → 6.2 → 6.3 cycle, before the C++ port:

- **POSIX standardization** — code revised to use primarily POSIX calls
  with some BSD extensions; tested under gcc
- **No assembler dependency** — CLASS_CTRAMPOLINE_ENV eliminates the need
  for platform-specific assembler in the class system trampoline
- **Static loading option** — genstatl allows building without dynamic loading
- **Figure editor** — replaced zip with a faster, more reliable drawing editor
- **Image inset** — multibit pixel support (JPEG, GIF, TIFF, xwd, PostScript,
  PBM, PPM, MAC Pict)
- **Srctext framework** — generalized source editing with language subclasses
- **Prefed** — preferences editor
- **Launchapp** — application launcher
- **Parsing toolkit** — enhanced bison, parse object, symbol table, lexeme scanner
- **RTF converters** — bidirectional RTF translation
- **Keyboard macro to Ness** — define operations by demonstration, then edit
- **Menu keystroke display** — menus show equivalent key combinations
- **HTML editing** — basic HTML mode (in contrib)

## Assessment for revival

Most features added in the C++ versions either:

1. **Solve problems that no longer exist** — The color manager dealt with
   8-bit displays and limited colormaps. Modern systems use 24-bit TrueColor.
   The PostScript printing pipeline replaced troff, but a modern revival
   would target PDF output anyway.

2. **Would be done differently today** — The ATK base class and widgets
   framework are tightly coupled to C++ idioms. A C revival could achieve
   similar goals through the existing Class system.

3. **Are irrelevant in a modern context** — 1996 HTML/web support is
   not useful in 2026.

The 6.3.1 codebase has significant advantages for a C-based revival:

- The "Class" object system is coherent and was designed for C, not a
  transitional artifact of the C++ migration
- POSIX standardization was already done
- No assembler code required
- The dynamic loader can be bypassed via static loading, or replaced
  with modern dlopen()/dlsym()
- The codebase is familiar to the revival team

The main loss from choosing 6.3.1 over 8.0 is the "thousands of bug fixes."
However, many of those were likely related to the C++ conversion itself
rather than fixes to pre-existing C bugs. Bugs discovered during the port
can be selectively examined and backported as concepts to the C codebase
if the original source is available.

**Recommendation:** Revive the 6.3.1 codeline. Selectively backport
*concepts* (not code) from the C++ versions where valuable. The modern
dlopen() API is a natural replacement for the old dynamic loader, and
the POSIX foundation is already in place.

## Sources

- `ANNOUNCE/ANNOUNCE.C++.prospects` — Hansen's 1992 analysis of conversion costs
- `ANNOUNCE/ANNOUNCE.6.2.changes` — detailed changelog 5.2 → 6.1 → 6.2
- `ANNOUNCE/ANNOUNCE.6.3.ez` — v6.3 release announcement
- `ANNOUNCE/ANNOUNCE.7.4.binaries` — v7.4 release announcement
- `ANNOUNCE/press.aug96` — press release for first C++ version
- `NEWSLETTERS/EZ/96Spring.ez` — details on color manager, widgets, web browser
- `andrew-8.0/README` — v8.0 component list and platform support
- `auis-6.3/Changes.62-63` — RCS changelog between releases
