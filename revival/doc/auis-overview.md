# AUIS Project Overview

The Andrew User Interface System (AUIS) is a compound document environment from Carnegie Mellon University's Andrew Consortium, directed by Wilfred J. Hansen.

## Three Principal Components

1. **Andrew Toolkit (ATK)** — Portable, dynamically-loadable, object-oriented UI toolkit. Objects ("insets") embed recursively in one another. Open for programmer-created insets. Source in `atk/`.

2. **Andrew User Environment (AUE)** — Applications built on ATK: ez (word processor/editor), figure (drawing), messages (mail/bboards), bush (directory browser), help (docs), console/typescript (shell), bdffont (fonts), prefed (preferences), plus ezprint, preview, datacat, nessrun.

3. **Andrew Message System (AMS)** — Multi-media mail and bulletin boards with authentication, return receipts, auto-sorting, vote collection, enclosures, audit trails. Source in `ams/` and `atkams/`.

## Embeddable Insets

Interactive: eq (equations), fad (animation), figure (drawing), layout, lset (split view), ness (scripting), org (hierarchies), page (multi-page), raster (bitmap), table (spreadsheet), text.

Non-interactive: clock, header/footer, image (JPEG/GIF/TIFF), link (hypertext), month (calendar), note (annotation), timeoday, writestamp.

## Architecture

- Built on **"Class"** — a dynamic C object model, contemporary of Objective C (`overhead/class/`)
- Dynamic object loader evolved through SunOS 4 / Solaris 2 dynamic loading, then C++ port
- **v6.3** (Dec 1996): free public release in C
- **v7.x**: members-only, C++ transition
- **v8.0** (Jan 1997): C++ version ("not extensively tested"); adds `dynlink`, `c++conv`, `genstatl` in overhead

## Source Tree Layout

- `atk/` — toolkit (insets and apps)
- `overhead/` — infrastructure (class system, bison, fonts, cmenu, mail, regex, utils)
- `ams/` — message system
- `atkams/` — ATK/AMS integration
- `config/` — build config (Imakefile/imake)
- `contrib/` — contributed code
- `doc/` — docs (ADMINISTRATOR, DEVELOPER, PROGRAMMER, HELP)
- `ossupport/` — OS portability

## File Format

ATK datastream: text-based, LaTeX-like markup. `\begindata{type,id}` / `\enddata{type,id}` framing for embedded objects. `\view{viewclass,id,...}` for rendering hints. See [ATK Datastream Format](atk-datastream-format.md).

Converters exist: ATK to/from RTF, ASCII, PostScript, troff, ppm, Scribe, X window dump.

## Platforms

IBM RS/6000 (AIX), Sun 3/4 (SunOS, Solaris), DEC VAX/MIPS (Ultrix, BSD), HP 300/900, Linux i386, NetBSD, BSDI, Mac II (MacMach), Apollo (DomainOS), SGI (IRIX), SCO.

## This Archive

Contains: v6.3 source (`auis-6.3/`, `auis-6.3.1/`), v8.0 source (`andrew-8.0/`), binary distributions (`bin-dist/`), compressed tars (`dist-6.3/`), patches, FAQ, newsletters, papers, and the `ia-archive` of info-andrew mailing list postings.

**Why:** Understanding the full scope of AUIS is essential for navigating this archive and planning work like document format conversion.

**How to apply:** When working in this project, know that the core abstraction is the "inset" — a self-contained object that reads/writes its own datastream segment and can be embedded anywhere. The format converter we're building needs to handle this recursive inset structure.
