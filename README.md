# AUIS — Andrew User Interface System

The Andrew User Interface System (AUIS) is an X Window System application
framework and toolkit developed at Carnegie Mellon University in the late
1980s and early 1990s, originally as part of the Andrew Project. It includes
the Andrew Toolkit (ATK), a structured document editor (EZ), and a rich
collection of inset types and application frameworks.

This repository is a **read-only Git mirror** of the authoritative Fossil
repository. If you want to follow development, browse history, or contribute,
please use the Fossil repo:

> **https://poetnerd.com/wdc/auis/**

## About this revival

This codebase was imported from the AUIS 6.3.1 release and is being
modernized for current Unix/Linux/macOS systems. The `main` branch reflects
the 6.3.1 baseline tagged as `auis-6.3.1`.

**Active development — including all bug fixes and modernization work —
is on the `andrew-6.4` branch.** That is the branch to clone or track if
you want to follow or participate in the revival. At some point `andrew-6.4`
will be merged back into `main`/trunk, but for now it is where the action is.

## History

AUIS grew out of the Andrew Project, a joint venture between CMU and IBM
in the 1980s. It was one of the first systems to implement embedded multimedia
objects in a text editor — what would now be called a structured document
editor. The toolkit predates and influenced later frameworks, and remains
of historical and technical interest.

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