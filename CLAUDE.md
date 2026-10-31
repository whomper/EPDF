# Notes for working on EPDF

EPDF is one of the AERA Atari apps (ECal, EContact, EMail, EFTP, EHypView, ClaudeST, EWeb, EUpdate). They
are meant to look and behave as one family.

## For every AERA app

These come from AERA-Atari's repo template and are the same in every app's CLAUDE.md.

- **Follow the AERA Atari app guidelines** ([ATARI-APP-GUIDELINES.md](https://github.com/whomper/AERA-Atari/blob/main/standards/ATARI-APP-GUIDELINES.md)). Pay most
  attention to:
  - About box
  - family shortcuts
  - PRG/ACC rules
  - context menus
  - VA_START, help, alerts
  - INF location

  When a change touches one of these, do it the guideline's way, even if the old code here did it
  differently.
- **Speed is set by the stock machines** (Erez, 26 Oct 2026; the guidelines' "Speed"): an 8 MHz ST
  and a stock Falcon (`ST`, `ST-1`, `Falcon-4`), not the DFB1. Design and code for them: draw only
  what changed, keep the 68000's slow operations out of inner loops, do work once. A change to
  drawing, a loop over the data or the start gives its `AESGAP` stretches on `ST` and `Falcon-4`,
  before and after, in its commit message. A GEM release passes the speed rig within the
  guidelines' times.
- **Shared code lives in [AERA-Atari](https://github.com/whomper/AERA-Atari).** Files under `common/` come from it. Change them
  there, not here, and pull them in with `tools/sync-common.sh`. A fix made only in this repo will drift
  again.
- **Build and test before every push:** `make` with no warnings and `make test`, plus this repo's
  own checks (below): `make itest` when network code changed, the Hatari rigs in `tests/hatari/` when
  the GEM side changed.
- **Versions and releases:** `VERSION` changes only in a release commit. A release: `tools/bump.py`,
  commit, push, then run the Release workflow on `main` with the notes (or push a tag `vX.Y` made with
  `git tag -a`); it tests, builds, tags vX.Y and publishes each file and the zip as a GitHub Release.
  A build must leave every tracked file unchanged and shows the version in `VERSION` as it is (there is
  no `-dev` label). Don't
  commit built programs, help files or certificates (`binary/`, `dist/`). Never reuse a number.
- **Only formal releases leave the build machine.** Erez gets vX.Y releases made by the Release workflow from
  `main`, nothing else: no test builds, no CI artifacts, no builds from a branch.
  Work lands on `main`; to try a fix on real hardware, release it. If a release doesn't work, roll back
  with a release: revert on `main`, release the next number, and run the Withdraw workflow
  (`.github/workflows/withdraw.yml`) on the bad one: it puts "Withdrawn: use vX.Y" at the top of its notes.
- **Releases go out without asking Erez** (his decision, 23 Oct 2026; guidelines 1.2). Release when
  work on `main` is ready:
  - this repo's checks pass;
  - the rigs on his TOS pass, where this file asks for them;
  - the hub's family checks pass.

  Then give Erez the link in one line, with anything he has to do, and send the hub the link. Roll
  back a bad release the same way. Decisions that are his (design, Pi packages, ports, a change to
  what a user sees that wasn't asked for) still go to him first.
- **Testing with another AERA program uses its latest release**, what Erez runs, never a build of its
  `main`: `common/tools/getrelease.py APP DIR FILE...` fetches it. Only for a change both programs are
  still making, test against the other's `main` at a named commit, say so, and test again against its
  release when it is out.
- **Every new source file starts with the SPDX line and copyright** (see any `.c` file); a generator
  writes them into the files it makes.
- **The name** reads "AERA | Erez Yaary" wherever it is given: copyright lines
  (`Copyright (C) 2026 AERA | Erez Yaary`), the About box, the help file's `@author`, the docs, and as
  the author of every commit: `git commit --author="AERA | Erez Yaary <yaary@me.com>"`, with Claude as
  the signing committer, so GitHub shows the commits as Verified (AERA-Atari,
  `standards/GITHUB-IDENTITY.md`).
- **Never commit:**
  - real addresses from the owner's network: use `192.168.1.x`, `PI_IP`, `ATARI_IP`
  - passwords, session keys, `.KEY` / `.INF` files from a test run
  - photos that still carry EXIF/GPS data
- **README:** keep the section order from the template (Highlights, Getting started, How it reaches the
  servers, Building, Source, Tests, Licence, family footer). Use British English.

## Talking with Erez

- Write to Erez only when he has a decision to make, with a short context and the decision: no progress
  or in-between messages.
- Findings and analysis: the information, your assessment of it and what he should do. Nothing more.
- Design and look are his decisions. So is a change to what a user sees that nobody asked for.
- Messages to the hub (session_01McWfUeeeAUvjhmDvN6UbnJ, `send_message`) are complete, with the evidence:
  commits, numbers, test output. Each message to another session ends with a blank line, `---`, and
  `_Generated by [Claude Code](https://claude.ai/code)_`.

## EPDF

A PDF viewer: full pages on the Falcon and TT (the stock 4 MB Falcon, `Falcon-4`, is the speed floor), a
text view (reflowed text, search, copy, pictures) that also runs on a 68000 ST. A program (PRG) only,
at first. This section is kept up to date: the build, the tests, what a release carries, the decisions.

- **Where it stands (31 Oct 2026):** stage 1, the text view, approved by Erez ("time stage 1 work to
  then", 29 Oct) and under way; stage 2 (page rendering) is not started and waits for his answer. Done:
  the window with every page one after another (`src/view/docview.c`), reflow (the extractor's soft
  break, `PTEXT.soft`), reading in slices on a second stack (`src/gem/coro.S`), the text store with
  `EPDF.TXC` (`src/view/tstore.c`), Find/^G/Shift+^G, selection, Copy, Select all, the context menu,
  `EPDF.INF` beside the program, the icon, iconify, Help, drops, the guide, README, NOTICES.TXT. To
  do: Hebrew (common's bidi), the Font dialog, pictures, the drawing cost on an ST, the interpreter's
  per-glyph cost (a 4000-glyph page reads in 1.6 s on an ST), columns, streaming content for ST-1;
  then a candidate to the hub for its TOS runs, then 0.1.
  Hub's rules from the measurement report: keep `-msoft-float` on every `-m68030` build (`make test`
  checks: `tools/nofpu.sh`); decode streams with `inflate_open`/`inflate_read` so a page needn't fit
  in memory; reuse `common/atari/dspjpeg.c` with `PIC_LIBJPEG` for JPEG on the Falcon; a progressive
  JPEG on a 68000 shows its 1/8 preview with a note, never 24 s without a turn; the Liberation (OFL
  1.1) licence is cleared before a release carries it; the first GEM release gets the speed rig and
  `acc_all` with EPDF in front.
- **Decisions in the code:** a program only, so `EPDF.INF` is beside it (6.1), not in the boot root.
  One window: ^U quits, ^W does nothing (3.2). Find next is ^G and Find previous Shift+^G (the Mac's
  keys, 3.2). The extractor's text is UTF-8; the view's is the Atari character set with the soft
  break as CR (`cs_to_atari` keeps only CR, LF, Tab). The page's text is read a page at a time on its
  own stack; the reader's hook (`PDOC.stop`, every 8 tokens and 32 glyphs) hands the machine back when
  the slice is up; nothing on that stack may call the AES.
- **Governance:** the hub owns AERA-Atari and `common/`. Never change `common/` here: pull it with
  `tools/sync-common.sh`; a needed change goes to the hub as a patch (`git format-patch`, plain text)
  with its reason and tests. Before every release send the hub the candidate commit and the checks
  (it runs `common/tests/family/acc_all.py` and the rest and answers "clear" or what failed); after
  the release send it the link. Ask the hub before copying anything from another app.
- **Licences:** everything must be GPL-2.0-or-later compatible. No MuPDF (AGPL), nothing else
  incompatible. FreeType (FTL/GPLv2) is fine and works in fixed point. Third-party code lives in
  `third_party/<name>/` at a pinned version with its README and any patches as patch files (EWeb's
  way). The standard 14 PDF fonts come from metric-compatible free fonts (Liberation, DejaVu…); check
  each licence. poppler/mupdf-tools may be installed on a build machine as a comparison oracle, but
  nothing of them is linked, copied or committed.
- **Never commit** (on top of the list above): Erez's TOS images and NVDI (`~/.aera/tos`,
  `~/.aera/nvdi`: never in git, never sent anywhere), built programs (`binary/`, `dist/`), and any PDF
  that isn't freely redistributable. The corpus is fetched into `build/pdfs` by `tools/fetch-corpus.py`
  and never committed.
- **Reuse before writing:** common's inflate (FlateDecode), `pic.c` (JPEG with the DSP on a Falcon, PNG,
  GIF) and the dither, text drawing, dialogs, pane bars, About box, help, notifications; TLS.OVL's crypto
  (RC4, AES, SHA) for encrypted PDFs.
- **Commits:** `git -c user.name=Claude -c user.email=noreply@anthropic.com commit
  --author="AERA | Erez Yaary <yaary@me.com>"`, ending with the session's Co-Authored-By and
  Claude-Session lines. No model name in a commit or a file.
- **Hatari rigs** boot the machines through `common/tools/machines.py` (ST, ST-1, TT, Falcon,
  Falcon-4), on Erez's TOS 1.04 and 4.04 where present (`~/.aera/tos`), else EmuTOS. A report names the
  machine and the TOS (`describe()`). The cloud session that made the measurement run had no TOS
  images: its numbers are on EmuTOS 1.4 (CPU-bound work is TOS-independent; file and GEM calls are not).
- **Build and test:** see the Makefile's header. `make` builds `binary/EPDF.PRG` (host tools under
  `build/`); `make test` runs the host tests (layout, store, view, INF, search, CCITT) and checks the
  programs (`tools/nofpu.sh`, `common/tools/oddaccess.py`). Before a push that touches the GEM side:
  `make gui-test` (`tests/hatari/gui.py`, ST:mono; also Falcon:16). A change to drawing, a loop over
  the data or the start: `make speed-test` (`tests/hatari/speed.py`, build/aprof with `gem.c`
  `-DAES_PROFILE=20` and `textwin.c` `-DEPDF_TIMES`), its stretches on ST and Falcon-4 in the commit.
  `make shots` takes docs/screenshot.png and docs/help/WINDOW.IMG again. `make help HCP=...`, `make
  dist`. The stage 0 tools: `make measure-atari`, `make measure`, `tests/hatari/measure.py`.
  `build/pdftext FILE PAGE [marks]` shows a page's reflowed text on the host.
