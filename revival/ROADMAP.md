# AUIS Revival Roadmap

Tracking the work to bring the Andrew User Interface System back to life.

## Repository

- **Remote:** https://poetnerd.com/wdc/auis/home
- **SCM:** Fossil
- **Codeline:** andrew-6.4, branching from the 6.3.1 C source (tagged `6.3.1`)
- **Trunk layout:**
  - `src/` — contents of auis-6.3.1
  - `revival/` — documentation and tools
  - `patches/` — official and contributed patches from the archive
  - `bison/` — AUIS's modified bison

## Completed

### ez2md converter — `revival/tools/ez2md`
*Completed 2026-06-23*

Converts ATK/ez format documents to Markdown. Handles the full document
structure: metadata, styles, headings, inline formatting, nested embedded
objects, footnotes, and page breaks.

    ez2md input.ez -o output.md
    cat input.ez | ez2md | less

Tested against all 51 `.ez` files in the archive without errors.

**Embedded object support:**

| Object type | Rendering | Status |
|-------------|-----------|--------|
| text | Recursive conversion | Done |
| bp (page break) | Horizontal rule (`---`) | Done |
| fnote (footnote) | GitHub-flavored footnotes (`[^n]`) | Done |
| raster | HTML comment placeholder | Placeholder |
| table | HTML comment placeholder | Placeholder |
| eq (equation) | HTML comment placeholder | Placeholder |
| figure | HTML comment placeholder | Placeholder |
| fad (animation) | HTML comment placeholder | Placeholder |
| image | HTML comment placeholder | Placeholder |
| link | HTML comment placeholder | Placeholder |

**Known limitations:**

- Word-wrap artifacts from the original `.ez` files carry through
  (e.g. bold text split across two `\bold{}` commands by the ez editor's
  line wrapper produces adjacent `****` markers in the output — these
  render correctly in Markdown but look odd in source)
- `\formatnote` content is intentionally dropped (typesetter directives)
- Indentation commands (`\indent`, `\leftindent`) pass content through
  without visual indentation in Markdown

### modernize — `revival/tools/modernize`
*Completed 2026-06-24*

Applies mechanical transformations to bring 1990s C source closer to
modern C standards. Idempotent — safe to run repeatedly.

    modernize file.c [file2.c ...]
    modernize --dir src/atk/text
    modernize --dry-run src/overhead/class/pp/class.c

Current transformations:
- Manual `malloc()`/`realloc()` declarations → `#include <stdlib.h>`
- `extern int errno;` → `#include <errno.h>`
- Missing `<stdlib.h>` when `exit`, `malloc`, `free`, `atoi`, `qsort`, etc. are called
- Missing `<string.h>` when `strlen`, `strcpy`, `strcmp`, etc. are called
  (skipped if `andrewos.h` is already included, since it provides these)
- Missing `<stdio.h>` when `printf`, `fprintf`, `fopen`, etc. are called
- K&R function definitions → ANSI prototypes

Known limitations (require manual fixes):
- Split-line definitions (return type on separate line from function name)
  are skipped to avoid name truncation

### Project documentation — `revival/doc/`
*Completed 2026-06-23*

- `auis-overview.md` — Summary of the three AUIS components (ATK, AUE, AMS),
  source tree layout, version history, platform list
- `atk-datastream-format.md` — ATK/ez file format specification: document
  structure, formatting commands, style attributes, embedded object framing
- `version-comparison.md` — Analysis of what v7.4/v8.0 (C++) added beyond
  v6.3.1 (C), with assessment for which codeline to revive
- `porting-assessment.md` — What it takes to build 6.3.1 on modern Linux:
  issues ranked by effort, and a phased build-up strategy

## Next steps

### Raster image extraction
Convert the run-length encoded bitmap data in `raster` objects to a modern
image format (PNG). This would let `ez2md` emit `![image](path.png)` instead
of placeholder comments.

### Table rendering
Parse the ATK table/spreadsheet data format and render as Markdown tables.

### Batch conversion of archive documents
Convert the `.ez` documents in the archive (FAQ, README, newsletters, papers)
to Markdown for easier browsing. Could live in `revival/converted/` or
alongside the originals.

### Build system investigation
Understand what it would take to compile AUIS on a modern platform.
The v8.0 C++ source is the likely starting point. Key areas:
- `overhead/class/` and `overhead/dynlink/` — the object system and dynamic loader
- `config/` — Imakefile-based build system
- `ossupport/` — OS portability layer
- Dependencies: X11, bison, flex, and whatever else the 1990s assumed

### Ness scripting language
Ness is AUIS's extension language (used for keybindings, macros, and
standalone scripts via `nessrun`). Understanding its syntax and capabilities
is relevant both for documentation and for eventual revival.

### Messages with IMAP backend
Investigate whether the messages UI can be cleanly separated from AMS's
storage layer and connected to an IMAP server instead. The `atkams/`
directory bridges ATK and AMS — the thickness of that interface determines
feasibility. The value proposition: a mail client that renders rich
compound documents inline, not as attachments.

### Pie menus
Add Don Hopkins' pie menus. AUIS's menu architecture (proctable bindings,
`menu:[]` declarations in style attributes) is well-suited — only the
menu rendering code needs to change, not every inset.
