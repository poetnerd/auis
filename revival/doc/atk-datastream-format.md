# ATK Datastream Format

The ATK datastream format (used in .ez files) is a text-based compound document format. Related to [AUIS Project Overview](auis-overview.md).

## Structure

```
\begindata{text,UNIQUEID}
\textdsversion{12}
\template{templatename}
\define{stylename
  menu:[MenuPath~Priority,Label~MenuId]
  attr:[AttrName AttrId Type Value]}
[content with inline formatting]
\enddata{text,UNIQUEID}
```

## Inline Formatting Commands

`\bold{}`, `\italic{}`, `\typewriter{}`, `\bigger{}`, `\smaller{}`, `\majorheading{}`, `\chapter{}`, `\heading{}`, `\section{}`, `\subsection{}`, `\paragraph{}`, `\center{}`, `\indent{}`, `\leftindent{}`, `\flushright{}`, `\flushleft{}`, `\formatnote{}`

Nesting supported: `\bold{\italic{text}}`

## Style Definition Attributes

FontFace (Bold, Italic, FixedFace), FontSize, FontFamily (Andy, AndyType), LeftMargin, RightMargin (Inch/Cm), Justification, Flags (Underline, PassThru, NoFill, KeepPriorNL, OverBar, ChangeBar, DottedBox), Script (super/subscript).

## Embedded Objects

```
\begindata{objecttype,UNIQUEID}
[object-specific data]
\enddata{objecttype,UNIQUEID}
\view{viewclass,UNIQUEID,priority,param,param}
```

Object types encountered: text (recursive), bp (bullet point), fnote (footnote), raster (bitmap — run-length encoded), table, eq, figure, image, link, org, ness, fad.

Raster data is run-length encoded bitmap lines between `bits ID W H` and `\enddata`.

## Key Details

- Unique IDs are numeric, must match between begindata/enddata/view triplets
- IDs change on every write by ez
- `\textdsversion{12}` is the common version
- Templates reference installed style sheets
- The `\` before `\n` at end of line appears to be a line continuation mark in some contexts

## Existing Converters in Source

`atk/toez/` — converters to ATK format
`atk/ezprint/` — print ATK documents
File format converters: ATK to/from RTF, ASCII, PostScript, troff, ppm, Scribe

**Why:** This is the format we need to parse for the Markdown converter.

**How to apply:** The converter must handle: header/metadata lines, style definitions (skip initially), inline formatting commands (map to Markdown), and recursive begindata/enddata blocks for embedded insets (start with placeholders, evolve per type).
