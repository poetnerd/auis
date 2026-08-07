# `-Wstrict-prototypes` census (2026-08-07)

Sizing pass only — **no fixes made**, per wdc's explicit scope request.
Triggered by the zip figure-drag X-axis-lock bug (`revival.md`, "A
forward declaration that outlived its own honesty"): that bug's root
cause — a static helper's forward declaration left in the old
argument-less style (`static int Foo();`) even though its real
definition is fully ANSI-typed, so the compiler has no prototype to
check call sites against — is invisible to every M1-M4 diagnostic.
`STRICT_COMPILERFLAGS`'s `-Werror=int-conversion` and friends only
fire when a real prototype is in scope to compare against; an
argument-less declaration isn't one. `-Wstrict-prototypes` is the
correct flag to surface this whole class.

## Method

For each of the 91 active directories (the same set M3 enumerated —
`grep -c "building (dependInstall)"` against a fresh
`dependInstall.log`, filtered to directories with `.c` files at their
own level): `make clean && make depend && make -k install
CDEBUGFLAGS="-Wstrict-prototypes -ferror-limit=0"`, diagnostic-only
(not `-Werror`, nothing blocked, nothing committed). Working tree
restored to normal (`make clean && make depend && make -k install`,
no extra flags) in every directory afterward.

Each hit was classed by where Clang anchors it:

- **Own-file**: the diagnosed line is in the directory's own `.c`
  source (a real, hand-written forward declaration in project code).
- **Header/generated**: everything else — classpp-generated `.eh`/
  `.ih`, or shared framework headers (`class.h`, `bind.ih`,
  `proctbl.ih`, `keystate.ih`, `scroll.ih`, `point.h`, `rect.h`,
  `shadows.h`). These are overwhelmingly the tree's own intentional
  generic-dispatch idiom (`procedure`/`void (*)()` polymorphic slots),
  not bugs — already handled correctly, with casts, everywhere M1-M4
  touched them. Excluded from the counts below.

**This is a sizing proxy, not a bug count**, the same caveat M1-M4's
own census documents carried: a hit means "this declaration doesn't
tell the compiler its real argument types," not "this is definitely
broken." The zip fix needed real reading — checking every call site
against the real definition — to confirm an actual mismatch; most
hits are plausibly harmless old-style declarations whose callers
already happen to agree by construction. Treat the numbers below as
where to look, not a finished list of defects.

## Totals

**6,715 own-file hits across 79 of 91 directories** (52,484 raw hits
tree-wide including header/generated noise, excluded above). 12
directories are entirely clean.

## Per-directory breakdown (own-file hits, descending)

| Directory | Own-file hits |
|---|---:|
| `ams/libs/ms` | 913 |
| `atkams/messages/lib` | 529 |
| `contrib/zip/lib` | 368 |
| `atk/text` | 359 |
| `overhead/bison` | 239 |
| `atk/basics/common` | 238 |
| `atk/table` | 215 |
| `atk/apps` | 209 |
| `overhead/mail/lib` | 185 |
| `atk/image` | 183 |
| `atk/figure` | 168 |
| `atk/supportviews` | 162 |
| `atk/basics/x` | 151 |
| `overhead/mail/metamail/metamail` | 146 |
| `ams/libs/cui` | 145 |
| `overhead/util/lib` | 129 |
| `atk/extensions` | 128 |
| `atk/help/src` | 125 |
| `atk/raster/cmd` | 124 |
| `atk/apt/suite` | 117 |
| `atk/frame` | 103 |
| `atk/adew` | 102 |
| `ams/msclients/cui` | 99 |
| `atk/srctext` | 95 |
| `contrib/zip/utility` | 94 |
| `atk/bush` | 86 |
| `overhead/image/tiff` | 83 |
| `atk/typescript` | 80 |
| `atk/support` | 74 |
| `atk/apt/tree` | 72 |
| `atk/chart` | 70 |
| `atk/textobjects` | 59 |
| `atk/value` | 49 |
| `overhead/mail/metamail/richmail` | 48 |
| `ams/msclients/nns` | 44 |
| `contrib/mit/util` | 42 |
| `contrib/srctext/html` | 41 |
| `atk/textaux` | 41 |
| `atk/rofftext` | 37 |
| `atk/lookz` | 36 |
| `contrib/srctext/ptext` | 34 |
| `atk/hyplink` | 31 |
| `atk/fad` | 31 |
| `overhead/cmenu` | 27 |
| `contrib/mit/annot` | 25 |
| `overhead/eli/lib` | 24 |
| `contrib/time` | 24 |
| `atk/layout` | 24 |
| `atk/org` | 23 |
| `overhead/class/lib` | 22 |
| `doc/mkbrowse` | 21 |
| `overhead/rxp` | 20 |
| `overhead/class/pp` | 18 |
| `atk/eq` | 18 |
| `atk/apt/apt` | 18 |
| `ams/libs/shr` | 17 |
| `overhead/class/cmd` | 16 |
| `atk/syntax/tlex` | 14 |
| `atk/raster/convert` | 12 |
| `contrib/eatmail` | 11 |
| `atk/raster/lib` | 11 |
| `overhead/index` | 10 |
| `contrib/srctext/ltext` | 10 |
| `contrib/calc` | 10 |
| `overhead/mkparser` | 8 |
| `overhead/eli/bglisp` | 7 |
| `atk/syntax/sym` | 7 |
| `atk/syntax/parse` | 7 |
| `atk/ez` | 7 |
| `atk/utils` | 4 |
| `overhead/fonts/cmd` | 3 |
| `atk/help/maint` | 3 |
| `ams/libs/nosnap` | 3 |
| `contrib/demos/circlepi` | 2 |
| `overhead/mail/testing` | 1 |
| `overhead/mail/cmd` | 1 |
| `overhead/errors` | 1 |
| `overhead/addalias` | 1 |
| `atk/raster/scan` | 1 |

**Clean (0 own-file hits)**: `ams/msclients/imapsync`,
`atkams/messages/cmd`, `contrib/wpedit`, `inst`, `ossupport`,
`overhead/class/machdep/darwin`, `overhead/class/testing`,
`overhead/image/jpeg`, `overhead/malloc`, `overhead/sys`,
`overhead/util/cmd`, `overhead/util/hdrs`.

## Observations

- The top of the list tracks M4's own worst-defect-density directories
  closely (`ams/libs/ms`, `atkams/messages/lib`, `contrib/zip/lib`,
  `atk/text` were flagged risk/elevated-risk batches for exactly that
  reason) — consistent with this being the same underlying authoring
  era/style producing both defect families, not a coincidence.
- Spot-checked `ams/libs/ms`'s 913 hits are spread across 70+ distinct
  files (`recon.c` 71, `mswp.c` 49, `rawdb.c` 47, `subs.c` 44, ...),
  not concentrated in one skewed outlier — the volume is real, not a
  counting artifact.
- No attempt made to distinguish "real mismatch, like the zip bug"
  from "old style, always called consistently" — that requires the
  same per-site reading the zip fix needed. Given the volume, that
  triage is a milestone-scale effort (comparable to M1-M4), not a
  quick follow-up, if the goal is full coverage.

## Not done, by request

No fixing, no severity triage beyond the observations above, no
Imakefile changes, nothing committed to `STRICT_COMPILERFLAGS`. This
document exists to make an informed decision about whether/when to
scope that as a dedicated effort.
