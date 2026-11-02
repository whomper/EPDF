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
  back a bad release the same way.
- **Who decides what** (Erez, 31 Oct 2026; the guidelines' "Who decides what"). Erez's part, put
  to him before the work as a decision with a short context:
  - new programs and ideas, their core capabilities, and how they work with the other programs;
  - the user experience: the look, the layout, the way through a task, the words, the icons, the
    defaults;
  - phases: work stays inside the phase he approved; at its end, one phase report (what it set out
    to do, what was done with the release links, what is left, the next phase as a proposal), and
    the next phase starts when he says so;
  - priorities and conflicts between programs or sessions (through the hub, with its options and
    recommendation);
  - packages on the Pi and ports.

  How it is made is the team's, without asking: code, structure, shared code, tests, measurements,
  fixes, docs and releases inside an approved phase.
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
- Design and look are his decisions ("Who decides what" above). So is a change to what a user sees that nobody asked for.
- Messages to the hub (session_01McWfUeeeAUvjhmDvN6UbnJ, `send_message`) are complete, with the evidence:
  commits, numbers, test output. Each message to another session ends with a blank line, `---`, and
  `_Generated by [Claude Code](https://claude.ai/code)_`.

## EPDF

A PDF viewer: full pages on the Falcon and TT (the stock 4 MB Falcon, `Falcon-4`, is the speed floor), a
text view (reflowed text, search, copy, pictures) that also runs on a 68000 ST. A program (PRG) only,
at first. This section is kept up to date: the build, the tests, what a release carries, the decisions.

- **Where it stands (1 Nov 2026):** stage 1, the text view, approved by Erez ("time stage 1 work to
  then", 29 Oct) and under way; stage 2 (page rendering) is not started and waits for his answer. Done:
  the window with every page one after another (`src/view/docview.c`), reflow (the extractor's soft
  break, `PTEXT.soft`), reading in slices on a second stack (`src/gem/coro.S`), the text store with
  `EPDF.TXC` (`src/view/tstore.c`), Find/^G/Shift+^G, selection (XOR), Copy, Select all, the context
  menu, Hebrew (common's bidi), View > Font... (fontsel), pictures (`src/pdfimg.c`, `src/gem/pics.c`,
  decoded on a stack of their own), two columns read one after the other, `EPDF.INF` beside the
  program, the icon, iconify, Help, drops, the guide, README, NOTICES.TXT. The hub ran 7eff410 on
  Erez's TOS (acc_all clear; gui.py's Shift+^F, then fixed in the rig and in Copy's speed), cleared
  35b5f97, and 0.1 went out (v0.1 on deb68d9, 31 Oct). 0.2, the stock machines' speed, went out
  the same day (v0.2 on bcd2adc): text, scroll and selection drawn into TOS's screen through
  common's text_plain, scr_rows and scr_xor; the Falcon within every time but Select all (55); on
  TOS 1.04 a line 80, a page 145, next page 310, Select all 85, released so by Erez's choice
  ("option 3 now, then option 1"). Option 1 is on main since: rows wholly selected shown to the
  text's right edge, inverted as one box (win_sel_change: only rows whose shape changed are drawn).
  0.3, big documents on ST-1 (started 31 Oct), is on main as 8db7191b: content and scans read in
  pieces, pictures made a row at a time into the screen's bitmap (common's pic_dither_row), the
  reader's memory cut (pieces joined, big objects kept, a 4-byte object table), the text store's pool
  given back and the page or picture tried again when memory is refused. On ST-1 (big.py) 22 of the
  corpus's 25 pass; the 3 fail on JPEGs too big for a 68000 (0.4). The hub cleared cc798ec3 on Erez's
  TOS (gui and acc_all; ST 1.04: a line 80, next page 320, Select all 60, About 250) and 0.3 went out
  (v0.3 on b58255c5, 31 Oct); the hub's big.py on ST-1 with TOS 1.04 was still running (a loss there:
  0.3.x). Phase report sent through the hub. 0.4, pictures, started 1 Nov ("start 0.4"), went out
  the same day (v0.4 on 2a4abf21, cleared on c96aaef7): a JPEG streamed and made a row at a time (common's pic_jpeg_rows), every picture made a
  row at a time shown from the top down as its rows come (Erez: "rows top to bottom"; the plan's 1/8
  preview didn't pay on a 68000, 197 s against 257 s), the DSP on a Falcon through pic.c's own
  baseline header reader (PIC_TJPGD_DSP, dspjpeg_port_jpeg: novel 100 -> 37 s, scan-stinternals 219
  -> 47 s on Falcon-4), scr_fill where scr_cpu_wins(), -DDLG_CPU; then 5.8's one click (click.c:
  the selection's first cell on press, a double click's word on its release). On ST-1 the 0.3
  JPEG failures are gone (novel 257 s, scan-gfabasic 282 s); the two maps still read no page in
  300 s (the interpreter on a 68000). Then the hub's 5.8 note: the selection drawn on a press through
  press_update_begin/end (TOS holds BEG_UPDATE back while the button is held still; the speed rig's
  held-press shots check it); the resident size cut for acc_all under NVDI on Falcon-4 (465,180 ->
  440,772: EPDF's buffers and common's inflate and DSP tables taken when used; e4407c2a couldn't open
  a VDI workstation, 11d34604's menus didn't drop on the second boot); the rig's View menu click fixed
  (it clicked Edit's separator), which found View > The page's lines laying out 24 pages at once
  (dv_recount_view: ST 4.3 s -> 0.4 s). Resident 440,772 (0.3: 419,428). Still to do on the ST's
  speed (TOS 1.04, v0.4): a line 80, next page 315, a page while reading 160, Select all 65, View >
  The page's lines 415, About 260; the interpreter's per-glyph cost (the two maps and
  photo-nasa-sunda read no page in 200 s on ST-1). 0.4.1 went out the same day (v0.4.1 on
  85d8f4d2, cleared on bd825652): the menu bar built with the AES's cell (gl_wchar/gl_hchar), not the
  document font's (with a GDOS font its titles spread and its menus didn't open, 0.3 and 0.4; the
  speed rig compares a FONT= run's menu bar with the system font's), and text set a glyph at a time
  (Erez's EDOS_01.PDF from Calamus SL: the fonts' widths their ink; a gap is a space only from 0.8 em
  on such lines, a tight heading's spaces sorted before the next letter, the gutter found by every
  bin a gap covers or a strip few lines cross; `tests/glyphwise.py`). Resident 442,088. A release
  that fixes is X.Y.Z (the hub's "0.4.1"; the workflow takes any VERSION its tag matches).
  0.5 ("start 0.5", 1 Nov) under way. 0.4.2 went out the same day (v0.4.2 on 5dc88c76, cleared on
  447635df): common's inflate fix (0.4 and 0.4.1 could read a damaged stream wrongly), the ST's next
  page, a page while reading and Select all within their times (TOS 1.04: 195, 110, 45), five-place
  fractions read right, AES-256 opened in the reader's turns. Resident 449,236. Over on TOS 1.04: a
  line 80 (TOS's own drawing, the hub thinks), View > The page's lines 305, About 230 (200 with the
  hub's e9f041d). 0.5 went out on 2 Nov (v0.5 on d29ef93b, cleared on 6408af58): the reading
  stretches (EDOS_01.PDF held an ST 17 s at a time; now about 0.2 s: font heads, the chunks'
  merge sort, inflate's turns, two file windows; MEASURE's gap_ms, tools/gapat.py), the sliders
  set only when they move a pixel, a line in two turns and what a scroll owes drawn in bands of
  about 60 ms (textwin.c's owe_r0/owe_r1; tests/hatari/hold.py checks a held key against a whole
  redraw), bg_tick_at with the event's mouse, the hub's About steps (the frame, shadow, text and
  bold by the CPU into its buffer). TOS 1.04: ST:mono all within for the first time (About 140,
  a page 55, Select all 45); ST:med only About over (185: the limit is with Erez). Resident
  458,944. The heap takes 64 KB steps with no document (heap_grain, set in epdf.c and doc.c: 0.5's first
  candidate left the Desk menu no room to drop under NVDI with the four accessories; now 149 KB
  more free than 0.4.2) and, at doc_open, one block of a quarter of what is free, then the
  libc's doubling; known: ST-1's big.py has memory refused, and recovered, in manual-compendium
  and scan-stinternals (none lost), to be looked at again with the hub's task #98 (the family's
  first heap block). Still to look at: ST:med's page third band (120 ms on TOS 1.04). Then
  Erez's photos of 0.5 (2 Nov): EDOS_01.PDF's first page lost its first word into the next line
  (a drop cap, its baseline a line down: taken into the line its text starts) and read its two
  columns across each other (the right one 3 points higher, beside a picture: the gutter found as a
  strip of the body's lines without ink, `clear_strip`, kept off tables, contents pages and
  listings), text set up the page's edge is a line of its own after the page's text, a piece
  printed twice at its place comes once, `columns()` no longer runs its output into its right
  column's lines (pages with lines cut at a gutter lost lines and doubled others); a picture placed
  upside down or mirrored is shown as its page shows it ("v"/"h" after the size in its line,
  PIMG.flip, PICE.flip); the arrow set at the start (the desktop's busy bee stayed). Its first
  candidate, 8f626a3d6 (+2,228 bytes), left 0 KB under NVDI with the four accessories on Falcon-4
  (the hub's acc_all: no menu bar at all; 0.5 had passed with 2 KB): the Adobe Glyph List's 4,200
  names front-coded in blocks of 16 (pdftab.h's agl_names/agl_blk/agl_uc, tools/gen-tables.py;
  78 KB of pointers and strings to 44 KB), resident 435,864 (23 KB under 0.5); "menu-bar-failed"
  (with the KB free) and "quit" logged for the rigs. The guidelines name no reserve yet (the hub's
  #132: ClaudeST failed at about 54 KB).
  Hub's rules from the measurement report: keep `-msoft-float` on every `-m68030` build (`make test`
  checks: `tools/nofpu.sh`); decode streams with `inflate_open`/`inflate_read` so a page needn't fit
  in memory; reuse `common/atari/dspjpeg.c` with `PIC_LIBJPEG` for JPEG on the Falcon; a progressive
  JPEG on a 68000 shows its 1/8 preview with a note, never 24 s without a turn; the Liberation (OFL
  1.1) licence is cleared before a release carries it; the first GEM release gets the speed rig and
  `acc_all` with EPDF in front.
- **The road to 1.0, and EConvert (Erez, 31 Oct 2026):**
  - **1.0 is the text view, complete:** 0.2 speed on an 8 MHz ST (the guidelines' times), 0.3 big
    documents on ST-1 (streamed content, pictures' memory), 0.4 pictures (DSP JPEG on the Falcon, the
    1/8 preview on a 68000), 0.5 the rest (GDOS font on hardware, AES-256 on a 68000), then 1.0.
    ("Reports" stood here too: this session's shorthand for its own 0.5 line of 31 Oct, "anything
    0.1-0.4 turn up from you", Erez's findings on the releases before it (his EDOS_01.PDF: 0.4.1 and
    0.5); he was sent the text, 2 Nov. The GDOS font: ST users run NVDI (Erez, 2 Nov), covered by the
    speed rig under NVDI with a GDOS font on ST:mono, ST:med and Falcon-4, AERA-Atari c32220c. The
    About box on an 8 MHz ST in low or medium within 200 ms, 150 elsewhere: Erez's (a), AERA-Atari
    69da4d6, speed.py's Rig.about. Maps on an ST: "work on speed first" (Erez, 2 Nov): the
    interpreter's cost on a 68000 before the question is put again.)
    Full pages drawn by the Falcon itself (stage 2) are not part of it.
  - **EConvert** (`ECONVERT.PRG`), the AERA PDF converter, after 1.0, for the Falcon and TT (68030;
    an ST is told so). It writes a **converted PDF**: still a valid PDF, its metadata saying it was
    prepared for the Atari by EConvert (Producer and an AERA entry naming what is prepared), holding
    each page's text already reflowed in the Atari's character set, the pictures at the size an
    Atari shows them (JPX and JBIG2 too), the drawings (vector graphics) as pictures with their labels
    out of the text, and, as a setting per batch (none / mono / mono and colour; mono on), each page
    whole as a 640-pixel picture. Measured (31 Oct): a typical page of text and pictures is a few
    hundred KB as it is, 17-33 KB converted, plus 8-17 KB for the mono page, 11-34 KB in 16 colours.
  - **Two modes:** direct (the conversion on the Falcon/TT, the DSP for JPEG) and gateway (the PDF
    sent to a converter service on the Pi, AERA-Gateway, the same C code built for Linux; the
    converted file comes back). Input: many files or a whole folder, then the folder to write to. A
    progress window with two bars, the current file and the batch, each with its percentage and the
    time left, and Stop.
  - **EPDF reads both:** a standard PDF as now; a converted one from its prepared parts, with a green
    rounded badge **"Optimised"** (British spelling) at the right end of each page's heading line
    (mono: black). A standard PDF shows no badge.
  - Order: EPDF 1.0; the format and EPDF reading it (1.1); EConvert direct; then gateway mode.
- **Decisions in the code:** a program only, so `EPDF.INF` is beside it (6.1), not in the boot root.
  A picture made a row at a time is drawn from the top down as its rows come (pics_rows: one plane
  or 16 bits straight into the window's bitmap, else a 16-row band the main stack turns with
  vr_trnfm; nothing on the decoder's stack calls the VDI). The loop asks for one click (5.8);
  press_double/press_track (common's click.c) time the double click and tell a drag.
  One window: ^U quits, ^W does nothing (3.2). Find next is ^G and Find previous Shift+^G (the Mac's
  keys, 3.2). The extractor's text is UTF-8; the view's is the Atari character set with the soft
  break as CR (`cs_to_atari` keeps only CR, LF, Tab). The page's text is read a page at a time on its
  own stack; the reader's hook (`PDOC.stop`, every 8 tokens and 32 glyphs) hands the machine back when
  the slice is up; nothing on that stack may call the AES. A picture is decoded on a second stack
  (`pic_poll` hands back the same way); it may use the document's objects only before its first
  hand-back. A page that is read ends its slice, and its rows are drawn in the next turn. A picture
  over 60 % of a page that has text is left out (background or scan behind a text layer).
  The extractor marks a picture as a line "\x7F<object> <w>x<h>" in the page's text.
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
  `build/`); `make test` runs the host tests (layout, store, view, INF, search, CCITT, the object table, a page set a glyph at a time: `tests/glyphwise.py`) and checks the
  programs (`tools/nofpu.sh`, `common/tools/oddaccess.py`). Before a push that touches the GEM side:
  `make gui-test` (`tests/hatari/gui.py`, ST:mono; also Falcon:16). A change to drawing, a loop over
  the data or the start: `make speed-test` (`tests/hatari/speed.py`, build/aprof: EPDF as released
  with `gem.c` `-DAES_PROFILE=20`), its stretches on ST and Falcon-4 in the commit; `build/times/EPDF.PRG`
  adds `textwin.c`'s `-DEPDF_TIMES` marks to break a stretch down (they cost a page 25 ms on an ST).
  A change to the store, the reader's file access or the INF: `make files-test`
  (`tests/hatari/files.py`, ST-1: each file's opens from the GEMDOS trace against the budget, 6.5).
  `make shots` takes docs/screenshot.png and docs/help/WINDOW.IMG again. `make help HCP=...`, `make
  dist`. The stage 0 tools: `make measure-atari`, `make measure`, `tests/hatari/measure.py`.
  `build/pdftext FILE PAGE [marks]` shows a page's reflowed text on the host. Memory (0.3):
  `build/pdfmem FILE [FIRST [COUNT]]` gives each page's peak in the reader; `PDF_MEMCAP=KB` caps it
  on the host and names each refused allocation's callers; `tests/hatari/big.py WORK [NAME...]`
  opens the corpus on ST-1 and reports pages and pictures that failed. Stretches while reading (0.5):
  measure.py's page lines carry `gap_ms`, the longest stretch between the reader's stop hook's calls,
  and `tools/gapat.py RUN.jsonl build/m68000/measure.elf` says where it began and ended (a build
  with `XOPT="-O2 -g"` gives the lines).
