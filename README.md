# EPDF

A PDF viewer for the Atari TT and Falcon (full pages) and, as a text view, the ST, written in C for GEM.

*Work in progress: EPDF is at its measurement stage and has no release yet.*

**Highlights**

- **Pages as they were made (TT, Falcon):** fonts drawn by FreeType, pictures through the family's
  picture code (the Falcon's DSP does the JPEGs).
- **A text view on every machine:** reflowed text, search, copy and pictures, down to a 68000 ST.
- **Modern but Atari:** right-click menus, drag and drop, iconify, help in EHypView/ST-Guide.
- **Hebrew**, in the Atari character set.

## Getting started

There is no release yet. The first, 0.1, is made by the Release workflow from `main`.

## Building

```sh
sudo apt install gcc-m68k-linux-gnu python3 libfreetype-dev   # Debian/Ubuntu
make            # build/ (host tools) and binary/ (Atari programs)
make test       # unit tests on this computer
make measure    # the measurement run on this computer (needs the corpus: tools/fetch-corpus.py)
```

`binary/` and `dist/` are what make builds; they are never committed, and a build changes no tracked
file. A release: `tools/bump.py`, commit and push, then **Actions > Release > Run workflow** on `main`
with what is new.

## Source

```
src/        the PDF reader (portable C, also built for this computer in the tests)
atari/      the measurement programs for the Atari
tools/      the corpus fetch, the measurement run, the release bump
tests/      host unit tests and Hatari rigs
third_party/ FreeType, the free fonts (pinned, with their licences and patches)
common/     the shared AERA library (from AERA-Atari; never changed here)
```

## Tests

`make test` runs the host unit tests. The Hatari rigs use the family's machines (`ST`, `ST-1`, `Falcon`,
`Falcon-4`) through `common/tools/machines.py`.

## Licence

EPDF is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation, either version 2 of the License, or (at your option)
any later version. It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See [COPYING](COPYING).

Copyright (C) 2026 AERA | Erez Yaary.

It includes, under their own licences (compatible with the GPL):

- [FreeType](third_party/freetype), the FreeType Project Licence or the GPL version 2.

---

Part of **AERA** for Atari: [ECal](https://github.com/whomper/ECal) · [EContact](https://github.com/whomper/EContact) · [EMail](https://github.com/whomper/EMail) · [EFTP](https://github.com/whomper/EFTP) · [EHypView](https://github.com/whomper/EHypView) · [ClaudeST](https://github.com/whomper/ClaudeST) · [EWeb](https://github.com/whomper/EWeb) · [EUpdate](https://github.com/whomper/EUpdate) · 8-bit: [EDOS](https://github.com/whomper/EDOS) · [EFTP_XL](https://github.com/whomper/EFTP_XL) · Pi: [AERA-Gateway](https://github.com/whomper/AERA-Gateway) · [EPDF](https://github.com/whomper/EPDF)
