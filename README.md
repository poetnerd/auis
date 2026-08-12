# Andrew User Interface System — Revival

A project to bring the [Andrew User Interface System](https://en.wikipedia.org/wiki/Andrew_User_Interface_System)
back to life on modern POSIX platforms.

AUIS is a compound document environment from Carnegie Mellon University (circa 1986–1997).
Its editor, **ez**, lets users create documents with embedded objects — drawings,
spreadsheets, equations, images — that nest recursively. The toolkit is extensible:
new objects plug in without modifying existing applications.

This repository is a **read-only Git mirror** of the authoritative Fossil
repository. If you want to follow development, browse history, or contribute,
please use the Fossil repo:

> **https://poetnerd.com/wdc/auis/**

## Start here

[**revival/doc/revival.md**](revival/doc/revival.md) is a narrative account of
what's been done and why: old bugs the port uncovered that had sat undetected
for decades, the 32-bit-to-64-bit issues behind most of them, the deliberate
modernizations undertaken along the way (anti-aliased text rendering, a modern
dynamic loader, current bison/flex), and the move to full ANSI C. Start there
for the story — the documents below are the detailed record it's drawn from.

Want to build it yourself? [**revival/doc/quickstart.md**](revival/doc/quickstart.md)
covers getting the source, building, and running `ez` on macOS/XQuartz.

## This project

We are reviving the **6.3.1 C codeline** (the last public release, August 1994)
rather than the later C++ port. The initial target platform is macOS/Darwin with
XQuartz, building toward broader POSIX support.

See [revival/doc/roadmap.md](revival/doc/roadmap.md) for current status and plans.

## What's here

  *  `src/` — AUIS 6.3.1 source (ATK, AMS, overhead, config, contrib)
  *  `revival/doc/` — project documentation:
     [revival.md](revival/doc/revival.md) (narrative summary),
     [overview](revival/doc/auis-overview.md),
     [ATK format spec](revival/doc/atk-datastream-format.md),
     [version comparison](revival/doc/version-comparison.md),
     [quickstart](revival/doc/quickstart.md),
     [mail quickstart](revival/doc/mail-quickstart.md),
     [porting assessment](revival/doc/porting-assessment.md),
     [changelog](revival/doc/porting-changelog.md),
     [roadmap](revival/doc/roadmap.md)
  *  `revival/tools/` —
     * [ez2md](revival/tools/ez2md) converts ATK/ez documents to Markdown;
     * [ansify](revival/tools/ansify) converts K&R function definitions to ANSI C, guided by class-definition signatures — the tool that carried the ANSI C conversion;
     * [fix-static-methods](revival/tools/fix-static-methods) removes 'static' from ATK class method definitions;
     * [modernize](revival/tools/modernize) mechanical C modernization tool — retired as the primary K&R→ANSI conversion path (see the porting assessment), kept for its narrower fixups
  *  `patches/` — official and contributed patches from the original archive
  *  `bison/` — AUIS's modified GNU bison

## Getting the source

This repo is mirrored to both Fossil (canonical) and GitHub. See
[revival/doc/quickstart.md](revival/doc/quickstart.md#getting-the-source)
for clone instructions for either — the short version:

```bash
git clone https://github.com/poetnerd/auis.git
```

## History

AUIS grew out of the Andrew Project, a joint venture between CMU and IBM
in the 1980s. It was one of the first systems to implement embedded multimedia
objects in a text editor — what would now be called a structured document
editor. The toolkit predates and influenced later frameworks, and remains
of historical and technical interest.

The original archive was mirrored from CMU's FTP server in June 2026.

## License

The core AUIS codebase is distributed under a BSD-style permissive license:
use, copy, modify, and distribute freely for any purpose, with attribution.
The copyright holders are IBM, Carnegie Mellon University, and other
contributors. See `src/DISCLAIMER` for the full terms.

GNU-derived components in `src/overhead/bison/` and
`src/contrib/gestures/gestsrc/` carry their own GPL notices; see the
`COPYING` files in those directories.

## Contributing

This Git mirror exists for read-only access. Pull requests here will not
be monitored. If you'd like to contribute or get in touch, please open an
issue in this GitHub repository.
