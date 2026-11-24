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
- **Speed is set by the stock machines** (decided 26 Oct 2026; the guidelines' "Speed"): an 8 MHz ST
  and a stock Falcon (`ST`, `ST-1`, `Falcon-4`), not an accelerated one. Design and code for them: draw only
  what changed, keep the 68000's slow operations out of inner loops, do work once. A change to
  drawing, a loop over the data or the start gives its `AESGAP` stretches on `ST` and `Falcon-4`,
  before and after, in its commit message. A GEM release passes the speed rig within the
  guidelines' times.
- **A new capability goes to the hub first** (decided 5 Nov 2026; guidelines 1.2): before building anything the family might share (a keyboard, a dialog, a format, a protocol, a helper, a setting), ask the hub whether `common/` has it or should get it. Never write your own copy.
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
- **Only formal releases leave the build machine.** the owner gets vX.Y releases made by the Release workflow from
  `main`, nothing else: no test builds, no CI artifacts, no builds from a branch.
  Work lands on `main`; to try a fix on real hardware, release it. If a release doesn't work, fix
  forward: a new release with the next number, before any other work; never a rollback (decided 7 Nov 2026).
- **Releases go out without asking the owner** (his decision, 23 Oct 2026; guidelines 1.2), through **the
  regression gate** (decided 17 Nov 2026; AERA-Atari a22574e, guidelines 1.2 and standards/TEST-MATRIX.md's gate table). Every code
  candidate, in this order:
  1. this repo's checks;
  2. the rigs on every row of the gate table EPDF runs on, on real TOS images and NVDI (EmuTOS is no
     evidence for a release), each row named in the report with `describe()`, a row not run said why:
     the reference configuration; Falcon-4:16; Falcon:65536; TT (ttmed, ttlow; TOS's VDI and NVDI); MiNT +
     XaAES, plain, ct60 and ext2 (`tests/hatari/mint.py`); the FireBee stand-in; ST mono, med, low and
     ST-1 (TOS's VDI and NVDI: `EPDF_NVDI=1` for gui.py); STE; the network paths (gateway.py,
     translate_view.py, `make pi-test`); one boot with NatFeats off (`tests/hatari/nfoff.py`). EPDF
     has no accessory edition (row 10);
  3. the paired speed run against the last release on the reference configuration, Falcon-4:16 and ST:mono,
     the builds at the shipped layout (tools/aprofpad.py; common's tools/profpad.py after 2.37): no row
     slower beyond a tick on any (a second and third pair for a row the first flags); fix it first, and
     only the owner accepts one;
  4. the candidate commit with the results to the hub, which runs the family check and answers go or
     the faults it found;
  5. only then the release. Give the owner the link in one line, with anything he has to do, and send the
     hub the link.

  A fault a user reports becomes a case in the rigs, in the release that fixes it. A fault found after
  a release is fixed forward by a new release that comes before any other work. Decisions that are his
  still go to him first.
- **Who decides what** (decided 31 Oct 2026; the guidelines' "Who decides what"). The owner's part, put
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
- **Testing with another AERA program uses its latest release**, what the owner runs, never a build of its
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
- **The owner is not named as the one who asked or decided** anywhere on GitHub: release notes,
  commit messages, comments, docs, code comments, this file. Write "decided 22 Nov 2026" or what the
  program does and why, never "Erez asked" or "Erez, <date>" (AERA-Atari, `standards/GITHUB-IDENTITY.md`).
  The author and copyright lines keep "AERA | Erez Yaary". Old text is cleaned when next touched.
- **Never commit:**
  - real addresses from the owner's network: use `192.168.1.x`, `PI_IP`, `ATARI_IP`
  - passwords, session keys, `.KEY` / `.INF` files from a test run
  - photos that still carry EXIF/GPS data
  - anything the owner provides (decided 24 Nov 2026; AERA-Atari standards/GITHUB-IDENTITY.md, Commits,
    2f6101e): his files (PDFs, archives, pictures, photos of the screen), logs (epdf.log), share links
    or file IDs, his messages and words, his machine's settings; on no branch, and no commit message,
    test name or note quotes them. A fault he reports becomes a case made from scratch that shows the
    same thing. His epdf.log is read on the build machine, never committed.
- **README:** keep the section order from the template (Highlights, Getting started, How it reaches the
  servers, Building, Source, Tests, Licence, family footer). Use British English.

## Talking with the owner

- Write to the owner only when he has a decision to make, with a short context and the decision: no progress
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

- **Where it stands (1 Nov 2026):** stage 1, the text view, approved (29 Oct) and under way; stage 2 (page rendering) is not started and waits for his answer. Done:
  the window with every page one after another (`src/view/docview.c`), reflow (the extractor's soft
  break, `PTEXT.soft`), reading in slices on a second stack (`src/gem/coro.S`), the text store with
  `EPDF.TXC` (`src/view/tstore.c`), Find/^G/Shift+^G, selection (XOR), Copy, Select all, the context
  menu, Hebrew (common's bidi), View > Font... (fontsel), pictures (`src/pdfimg.c`, `src/gem/pics.c`,
  decoded on a stack of their own), two columns read one after the other, `EPDF.INF` beside the
  program, the icon, iconify, Help, drops, the guide, README, NOTICES.TXT. The hub ran 7eff410 on
  real TOS (acc_all clear; gui.py's Shift+^F, then fixed in the rig and in Copy's speed), cleared
  35b5f97, and 0.1 went out (v0.1 on deb68d9, 31 Oct). 0.2, the stock machines' speed, went out
  the same day (v0.2 on bcd2adc): text, scroll and selection drawn into TOS's screen through
  common's text_plain, scr_rows and scr_xor; the Falcon within every time but Select all (55); on
  TOS 1.04 a line 80, a page 145, next page 310, Select all 85, released so by the owner's choice
  (option 3 now, then option 1). Option 1 is on main since: rows wholly selected shown to the
  text's right edge, inverted as one box (win_sel_change: only rows whose shape changed are drawn).
  0.3, big documents on ST-1 (started 31 Oct), is on main as 8db7191b: content and scans read in
  pieces, pictures made a row at a time into the screen's bitmap (common's pic_dither_row), the
  reader's memory cut (pieces joined, big objects kept, a 4-byte object table), the text store's pool
  given back and the page or picture tried again when memory is refused. On ST-1 (big.py) 22 of the
  corpus's 25 pass; the 3 fail on JPEGs too big for a 68000 (0.4). The hub cleared cc798ec3 on the owner's
  TOS (gui and acc_all; ST 1.04: a line 80, next page 320, Select all 60, About 250) and 0.3 went out
  (v0.3 on b58255c5, 31 Oct); the hub's big.py on ST-1 with TOS 1.04 was still running (a loss there:
  0.3.x). Phase report sent through the hub. 0.4, pictures, started 1 Nov, went out
  the same day (v0.4 on 2a4abf21, cleared on c96aaef7): a JPEG streamed and made a row at a time (common's pic_jpeg_rows), every picture made a
  row at a time shown from the top down as its rows come (the owner; the plan's 1/8
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
  (a PDF from Calamus SL: the fonts' widths their ink; a gap is a space only from 0.8 em
  on such lines, a tight heading's spaces sorted before the next letter, the gutter found by every
  bin a gap covers or a strip few lines cross; `tests/glyphwise.py`). Resident 442,088. A release
  that fixed was X.Y.Z (0.4.1 to 0.5.4); from 0.6 the versions are X.Y only, as every other app's
  (decided 3 Nov 2026: 0.5.5 was never released, its work went out in 0.6).
  0.5 (started 1 Nov) under way. 0.4.2 went out the same day (v0.4.2 on 5dc88c76, cleared on
  447635df): common's inflate fix (0.4 and 0.4.1 could read a damaged stream wrongly), the ST's next
  page, a page while reading and Select all within their times (TOS 1.04: 195, 110, 45), five-place
  fractions read right, AES-256 opened in the reader's turns. Resident 449,236. Over on TOS 1.04: a
  line 80 (TOS's own drawing, the hub thinks), View > The page's lines 305, About 230 (200 with the
  hub's e9f041d). 0.5 went out on 2 Nov (v0.5 on d29ef93b, cleared on 6408af58): the reading
  stretches (a Calamus SL PDF held an ST 17 s at a time; now about 0.2 s: font heads, the chunks'
  merge sort, inflate's turns, two file windows; MEASURE's gap_ms, tools/gapat.py), the sliders
  set only when they move a pixel, a line in two turns and what a scroll owes drawn in bands of
  about 60 ms (textwin.c's owe_r0/owe_r1; tests/hatari/hold.py checks a held key against a whole
  redraw), bg_tick_at with the event's mouse, the hub's About steps (the frame, shadow, text and
  bold by the CPU into its buffer). TOS 1.04: ST:mono all within for the first time (About 140,
  a page 55, Select all 45); ST:med only About over (185: the limit is with the owner). Resident
  458,944. The heap takes 64 KB steps with no document (heap_grain, set in epdf.c and doc.c: 0.5's first
  candidate left the Desk menu no room to drop under NVDI with the four accessories; now 149 KB
  more free than 0.4.2) and, at doc_open, one block of a quarter of what is free, then the
  libc's doubling; known: ST-1's big.py has memory refused, and recovered, in manual-compendium
  and scan-stinternals (none lost), to be looked at again with the hub's task #98 (the family's
  first heap block). Still to look at: ST:med's page third band (120 ms on TOS 1.04). Then
  a report on 0.5 (2 Nov): a Calamus SL page lost its first word into the next line
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
  #132: ClaudeST failed at about 54 KB). The hub cleared 5ae99e168 (acc_all under NVDI "free-kb
  18 2 1") and 0.5.1 went out on 2 Nov (v0.5.1 on 5573c536); known in its notes: ST high under NVDI
  with a 14-point GDOS font, a new page about a third of a second. The progress row (decided 2 Nov; his choices: a row shown only while EPDF works, the
  job the user waits on): textwin.c's win_progress (bar_h, whole/work), epdf.c's progress() each
  turn: Copy, a search, opening, the pages read (PDOC.page_prog within a page: its content streams'
  stored bytes read), a picture (PICE.made rows); shown after 0.5 s of a job, gone after 0.3 s of
  none, the text moved a row by one copy; drawn in turns of its own at most every 250 ms, only what
  changed, each pixel once (cells newly filled inverted, a changed letter on the filled part by
  MD_ERASE then MD_TRANS); the time left from the job's rate, from its second second. The hub's first ST runs under
  NVDI with Courier 10 Pitch 14 (TOS 1.04): ST:mono's next page and The page's lines 325 (300), two
  thirds of it NVDI's v_gtext (its gemprof); where the text goes through the VDI
  (text_plain_state() < 0) win_draw_all clears the rows' area and owes their text to the bands of
  about 60 ms (the hub's option (a)); a row drawn by the VDI ends with a fill, not with spaces.
  `-DEPDF_BAND_ALWAYS` (XOPT) bands on any font, for the rigs here, which have no NVDI.
  The row's first turn split (the text moved and its place cleared, then the row drawn whole:
  Falcon-4 under NVDI with Courier 14, Copy's turns 60 -> 40), and common's pic.c built with
  PIC_NO_GIF/PIC_NO_PNG (AERA-Atari 6588b07, this session's patch: resident 424,504, "free-kb 29"
  under NVDI with four accessories). The hub cleared ca99f40e5 and 0.5.2 went out on 2 Nov (v0.5.2
  on a002695e). Next: Select all on ST:mono at its 50, a longer document for speed.py's shot of the
  row, About against Rig.about, the maps' speed on an ST. Then the owner (2 Nov): the window has no info line
  (as EWeb's); textwin.c's status bar at the foot, always there, ch + 5 high (+ 7 in a tall font):
  a line along its top, "Viewing page 3 / 8" then the status (found on page ..., copied) and the
  job's words, and along its foot a line 2 pixels high (3) that fills; white ground (EWeb's grey
  would need the letters drawn twice: flicker), only the letters that changed and the line's new
  part drawn, each pixel once. The front page's picture taken again (shots.py's BADGES). The hub cleared 57dd90f99 (acc_all
  under NVDI "free-kb 32"; its ST:mono Desk menu check took 2,000 changed pixels, AERA-Atari 8ac3046, the
  white bar under the menu changing fewer) and 0.5.3 went out on 2 Nov (v0.5.3 on f0c05019). Resident
  421,648. Next (the hub): tests/hatari/speed.py to take common's Rig.about (ST:med's About 180 is within
  its 200), a longer document for the shot of the job in the bar, Select all on ST:mono at its 50.
  Then the family's 6.6 (decided 2 Nov; AERA-Atari 408e806, 4a4f886): every file name EPDF gives is in
  lowercase (epdf.inf, epdf.txc, epdf.hyp; common's crash.log, ehypview.prg), and so is every file a
  release and its zip carry (`make dist` names them so and fails on a capital); on MiNT's ext2
  EPDF.INF and epdf.inf are two files. Names shown to the user (the guide, README, alerts) stay in
  capitals; names from outside are used as they came. files.py fails on an own name with a capital
  and finds epdf.txc with machines.ci_path. The first release with it says once in its notes: "File
  names are now in lowercase. On an ext2 drive, delete the old folder and install this release
  fresh." The hub ran it under FreeMiNT on ext2 (common/tests/xaaes, run.py --ext2: epdf.inf written
  and read back in lowercase), cleared fc06515ce with the round (speed.py with Rig.about; the bar's
  job shot on a book of 400 pages, bar_shot, where the 30-page book was read first; Select all kept
  as it is, 40-45 of its 50: box_op, the window's rows inverted, 19 ms, is the 68000's floor) and,
  after EUpdate 1.28, 0.5.4 went out on 2 Nov (v0.5.4 on 1fec4feb). Resident 421,768. Next: the
  bar's separator 0xFA, the middle dot (0xF9 draws high in TOS 1.04's font, the hub). Then, for
  the owner's (3 Nov): a map's numbers read only when an operator wants them
  (pdftext.c's num_val: OP.s/.n kept in the window, read before a refill moves it; nums_run, numbers
  and white space in a loop of their own on register copies; run_of's 3-instruction scan to the
  window's sentinel CSENT; ccls, the characters' classes; the stop hook every 8 operators; the window
  at the parity of the bytes coming, pdf_stream_parity), and common's inflate window moved a long at
  a time (this session's patch, AERA-Atari 6edf6c1): on ST map-europe 74.7 -> 40.7 s, map-timezones
  218 -> 116, map-physical-world 418 -> 211, photo-nasa-sunda 93 -> 43, text pages about 5 %; the
  maps' longest stretch between hooks 200 -> 120 ms; the corpus's text the same (4,228 pages, an ASan
  run clean). Still on a map: inflate_fast 16 s of 41 (the hub has the hot addresses), the content
  loop 14 s. And the family's progress bar (guideline 5.9, decided 3 Nov: common's progbar, one bar
  filling left to right, blues on 256 colours, pens 1-15 on 16, black elsewhere) as the status bar's
  line: progbar_pens once after the workstation opens, progbar_fill from what was drawn to now in
  PB_FULLths (bar_fill/bar_unfill); this session's progbar fix for EmuTOS's ST high (both pens read
  white: AERA-Atari 3ed8fb6). Resident 425,608. The hub cleared b697b3314 (acc_all on ST:mono,
  Falcon-4:16 plain and NVDI, twice booted; speed.py all within, ST:mono next page 180, About 140; ST:med
  About 185 of its 200) and 0.6 went out on 3 Nov (v0.6 on cae2ef5f). Known: 0xFA reads low in TOS
  1.04's and NVDI's fonts, near a full stop (a centred one, if those fonts have it, later). Next: the
  maps' remaining cost (inflate_fast with the hub; the content loop), ST:med's page third band.
  1.0 went out on 3 Nov (v1.0 on 6033667ab; resident 427,332): the content loop's numbers on
  register copies inside BT…ET (nums_run), CC_PATH/CC_PAINT's fast path, ST:med's band; on ST
  map-europe 40.7 -> 36.0 s, map-timezones 116 -> 102, map-physical-world 210 -> 181.
  1.1 (with the converter, started 3 Nov) under way: EPDF reads a converted
  PDF (`docs/CONVERTED-PDF.md`; pdf_prepared_page, PDOC.prepared) with the Optimised badge
  (v_rfbox, pen 3, black in mono) and View > Pages (a menu toggle, the owner's choice; CMD_PAGES,
  doc_set_pages: the page's picture lines in the store); pics.c's direct pen path, raw rows and
  mono_masks (a page's picture on ST 39 -> 7.5 s). EConvert (`src/conv/`, `src/econv/`,
  ECONVERT.PRG, 68030 soft-float): the renderer (raster.c's coverage fills, render.c's FreeType
  glyphs with a cache, Liberation 2.1.5 in `third_party/liberation/`), deflate, quant (Bayer),
  jpegenc (photos 256 colours by default or JPEG, a per-batch setting: the owner), the window with two
  progbars, Help at epdf.hyp's "EConvert"; the job on its own stack, turns within 50 (9b370af3a:
  the job's turns 155 -> 40, Stop 210 -> 50 on Falcon-4). Rigs: tests/hatari/prepared.py,
  econv.py, econv_speed.py (`make econv-speed-test`), tests/convtext.py in `make test`. The owner asked
  (3 Nov) when the exact layout comes: answered 1.1's View > Pages via EConvert, and stage 2 (the
  page drawn by EPDF on a Falcon/TT, EConvert's renderer) as his decision after 1.1; his answer is
  pending. The hub cleared 78bb278d1 (acc_all, speed.py, econv.py, econv_speed.py: start 145, the
  job's turns 40, Stop 45; prepared.py; deflate.c's host warning silenced in the bump) and 1.1 went
  out on 3 Nov (v1.1 on 78bb278d1). EConvert carries EPDF's icon in 1.1 (the owner): `make icons` draws icons/ECONV* and src/econv/icon*.h from common/icons/ECONVERT.txt once
  the hub has drawn the one the owner picks (its three sketches), for 1.2. Its fonts sit in `fonts`
  beside econvert.prg. Later: JPX (OpenJPEG) and JBIG2, gateway mode with AERA-Gateway.
  Hub's rules from the measurement report: keep `-msoft-float` on every `-m68030` build (`make test`
  checks: `tools/nofpu.sh`); decode streams with `inflate_open`/`inflate_read` so a page needn't fit
  in memory; reuse `common/atari/dspjpeg.c` with `PIC_LIBJPEG` for JPEG on the Falcon; a progressive
  JPEG on a 68000 shows its 1/8 preview with a note, never 24 s without a turn; the Liberation (OFL
  1.1) licence is cleared before a release carries it; the first GEM release gets the speed rig and
  `acc_all` with EPDF in front.
- **Stage 2 (started 7 Nov 2026, Falcon/TT only; first goal a page in seconds on Falcon-4, drawn from the
  top down):** the renderer is `render.ovl` (`src/rovl/`: EConvert's renderer and its own reader, 68030,
  linked with common's `tlsovl/ovl.ld`), loaded by EPDF.PRG with common's `ovl.c` through
  `src/gem/rdraw.c` only on a 68020 or better: the ST keeps its memory. JPEGs there go through pic.c
  (the DSP; `cv_jpeg_pic`), never in EConvert or the Gateway (their files stay byte for byte the same).
  Standard fonts are read from their files as FreeType asks (render.c's RSTREAM), not held whole.
  `make rovl-test` (tests/hatari/rovl.py, ROVLTEST.PRG). View > Pages for any PDF is in (1.7). Zoom
  (decided 8 Nov 2026): View > Zoom in and out, the keypad's + and -, 150/200/300/400 % of the page's
  line, as far as memory holds (a size that failed isn't offered again for the document); on a
  Falcon/TT render.ovl draws the page at that width (PICS_PAGE + page, any document: a converted
  copy's from its original, doc_set_original, else from the copy), in colour; on an ST EConvert's
  mono double at 200 %. A zoomed page is dragged with the mouse (press_track, drawn through
  press_update_begin). render.ovl draws in smaller bands when its canvas can't be had. Rig:
  tests/hatari/zoom.py (Falcon:16 ST:mono; a stock Falcon-4 has room for 150 % at most). Gateway
  copies are EPDF's own, gate1.pdf/gate2.pdf beside EPDF.PRG, deleted when not open (decided 8 Nov
  2026, no folder atari); File > Save Optimised... writes the open converted PDF
  where the user says. Translation (EPDF 2.0, decided 8 Nov 2026; AERA-Atari standards/TRANSLATION-SERVICE.md):
  View > Translate (TRANSLATE=, TRANSLATETO=; TRPORT= for the rigs), src/gem/trview.c (direct: trpara.c's
  paragraphs to translategate through common's translate.c on a stack of its own, the page in view
  first), doc.c's trstore (memory only, over the store as read: TSTORE.under), a converted copy's
  /Translation (pdfgate's translate=CODE with Open on the Gateway: nothing asked by EPDF), Settings'
  Translate into and Languages... (gatedlg.c's gate_dialog_tr, common's langs.c), the right-click
  Translate...; Off runs nothing of it. Rig: tests/hatari/translate_view.py (gateway/'s translategate.py
  stand-in and pdfgate.py; EPDF_PRG=build/aprof/EPDF.PRG for the turns). Then speed. The family's UI (decided 9 Nov 2026; the hub's
  review): Options (Settings... ^, , Font... ⇧^F, a line, Open on the Gateway), Help (EPDF help,
  Keys...); Settings is common's settings_run() (AERA-Atari 07c5ab1; src/gem/gatedlg.c gives its pages:
  General: Translate into, a set_value button whose act opens the Gateway's languages over it,
  Languages...; Secure: the Gateway's address and port; EConvert: Secure only; 17 rows), Cancel
  puts every value back; Keys... with common's dlgbuild. Settings
  stops the page being translated first (tr_stop: one connection at a time; the hub's Address
  Error); translate_view.py's direct mode checks it (TRV_DELAY: slowgate.py). EConvert's window is
  the owner's B2 (band, list with panebar and buttons at its right, lines, pop-up buttons, two bars),
  iconifiable. The owner's answers (9 Nov 2026; AERA-Atari standards/ui/DECISIONS-2026-11-09.md and, through the
  hub: release, and EPDF aligned with EWeb): Font... in Options with no key (D6); View >
  Larger text ^+, Smaller text ^-, Actual size ^0, the keypad's + and - too (EWeb's; since 14 Nov 2026
  one item, View > Zoom In ^+, ticked while zoomed: two sizes, Normal and Zoom at 200 %, the owner; the
  window's sliders and arrows move a zoomed page; the horizontal bar only while zoomed, decided 14 Nov:
  the window made again at its place, textwin.c's remake); Help opens the
  guide's first page (D12); jobs say "45%", "12 KB of 3.4 MB", "2 min left" (D35); "Copied to the
  clipboard." / "Couldn't write the clipboard." (D50); the Find dialog on common's dlgbuild (band,
  Cancel then Find, 10 wide); every GEM object built -DDLG_CLICK (C16); EConvert keeps B2 (D34).
  tests/hatari/fontdlg.py opens Font... from the menu (gui.py types keys only). A report on 2.4 (9 Nov): a page in 16 colours
  takes a colour near a pen as that pen, its error dropped (src/rovl/pens.c snap_pen: flat fills and grey
  text were dithered into specks); a zoom stops another size's page drawing at once
  (doc_pic_stop_page_unless: 62 -> 29 s to the sharp page on Falcon:16); EConvert's window in the B2
  mock-up's colours, each the nearest of the 16 pens (pen_nearest; mono black and white). A screen of 256 colours (10 Nov):
  render.ovl's pens (snap and Floyd-Steinberg) take 2 to 256 pens and 1 to 8 planes (pens.c's tables of
  shorts, 128 KB; pack() a plane at a time past 4), so a page in 256 colours no longer goes through
  common's dither (a Calamus SL page on Falcon:256: shown 22.5 -> 8.1 s, at 150 % 48.5 -> 16.6 s). The UI's
  colours stay within pens 0-15 (pen_nearest's rule): EConvert's navy band and light-blue row in 256 colours are pens 250
  and 251 set to the mock-up's colours while it runs and given back at its end and in crash_quit
  (pens_take/pens_give; guidelines 5.2's own-theme pens, the hub 10 Nov), painted over the tree (paint256). A page in bands re-reads the page once a band (18
  bands: 4 to 15 times slower): draw it whole where memory allows.
  EConvert 2.8 (decided 10 Nov 2026): the batch's end said in the window (the batch's line: "Stopped: 1
  converted in 0:34"), tips on its buttons (common's tooltip), WM_BOTTOMED; the progress drawn flicker
  free and fast (decided 10 Nov 2026): only the letters that changed
  (span/cells, text.c's text_n: TOS's VDI is slow with a clip tight round text) and the bar's new
  part, off the screen through common's offdraw; draw() notes what a redraw drew (note_drawn); at
  the end only the rows that changed. Falcon-4 (EmuTOS): the job's turns 135 -> 85 (the first tick,
  the list's), Stop 140 -> 175 (the end drawn in the window, not an alert). Whole pages: Colour
  (CV_PAGES_COLOUR_ONLY, INF "colouronly", --pages colouronly; the owner: "What if I just want color
  pages"): a page with colour gets /Colour only, one without its /Mono; EPDF on a mono screen
  shows /Colour dithered. Through the Gateway pages=colouronly (AERA-Gateway 1.42, EPDF 2.9;
  gateway.py with GW_PAGES=colouronly).
  The page strip (decided 15 Nov 2026; EPDF 2.33): View > Page strip
  (PAGESTRIP=, off by default: the owner), a column of 112 pixels at the window's left, src/gem/strip.c (a column of cells shaped to
  move to common: common's panebar for its slider, press_track for its clicks), each cell an A4 box with
  its number: the page's /Thumb (a converted PDF's from EConvert or the Gateway, docs/CONVERTED-PDF.md),
  in View > Pages on a Falcon/TT the page drawn by render.ovl, else the page's first line; the view's top
  page framed twice, the column following it, drawn only when it changes. On an ST and a FireBee the box
  and the /Thumb only. Never in the text's stretch: the frames moved at once (one polyline each), cells
  that changed due and drawn one part a turn of the event loop (strip_turn: a cell's ground, box and
  number; then its words a line a turn, or its preview; the bar in three pieces, or where its slider
  moved, pb_moved with pb_vdi); a layout that changes nothing leaves it be. Its 6x6 font is set on the
  VDI only (told to text.c, the text view lost its CPU path after each cell: Select all on an ST 40 ->
  80 ms). With the strip off it asks no AES (strip_width before pb_width, the menu ticked only when on).
  The /Thumb only when asked (decided 15 Nov 2026: the cost accepted, "make it a setting flag, off by
  default"): EConvert's Options > Settings... > General > Page previews (PREVIEWS=1; gatedlg.c's
  gate_dialog_conv), CVOPT.thumbs, econvert's --thumbs, the Gateway's thumbs=1 (AERA-Gateway 1.48 on: an
  older pdfgate refuses it); EPDF's Open on the Gateway asks for it while the strip is shown
  (gw_set_thumbs). Rigs: tests/hatari/econv_settings.py (the tick, OK, Esc), econv.py (none by default;
  ECONV_PREVIEWS=1 every page). EConvert's /Thumb is made from the page's rows as they are drawn (tb_rows: the canvas whole, or the
  whole page's loops' bands), each row dithered as it is made: never drawn again (a 4 MB Falcon under
  NVDI lost its page 3 to memory). It costs CONVTEST about 1.9 s a page on Falcon-4:256 (the canvas read
  again from ST-RAM); CONVTEST's "thumbs" pairs it. Rig: tests/hatari/strip.py (`make strip-test`);
  speed.py's EPDF_STRIP=1.
  2.34: pages passed over aren't drawn (#96, decided 14 Nov 2026: moving away from the page read "kicks off
  various rendering activities"): a move within 400 ms of another holds the pictures till the view has
  been still 400 ms (doc.c's doc_view_moved, checked in doc_pic_start; main() as it was), a move on its
  own draws at once, and the pictures asked for on pages left and not begun are forgotten
  (pics_unwant_outside); rig tests/hatari/passing.py ("page-start" lines, "ahead" for those drawn ahead).
  EConvert's turns in the same release ("Convert: its turns" on the showcase 160 -> 50): quant's split
  a counting sort taking qsort's block (the heap as before), w_picture's loops in turns, the grey test
  as 2.33's compiled loop (inline asm: every C loop since missed the 68030's cache), convert_page left
  as it was. Under 098183d the comparison is the paired runs' medians (the hub, 16 Nov 2026).
  2.35: render.ovl's room set aside (the hub's option 1, 16 Nov 2026): on a 4 MB Falcon under NVDI
  render.ovl (643 KB, 99 % code) came after the document's heap blocks (about 577 KB each) and the
  page's bitmap, and loaded only when two blocks lay end to end (2.34 by luck: 100 bytes more in the
  program and View > Pages drew nothing; drawn.py). doc_open sets a GEMDOS block of its size aside
  before the heap grows (rdraw_reserve: a 68020+, render.ovl beside EPDF.PRG, no TT RAM, under 4 MB
  free in one piece; "ovl-reserve") and render.ovl is loaded into it (common's ovl_load_into,
  AERA-Atari a6f6a46); when the text view or a picture is refused memory and render.ovl isn't in it,
  it goes back to TOS first ("ovl-unreserve", give_back's 2) and the store's memory only if refused
  again. render.ovl built with PIC_NO_GIF (9 KB; its own pads RV_PIC_PAD, RV_GIF_PAD/_RO/_STR).
  Rigs: drawn.py with EPDF_BIN (a variant's: the luck test with a 100-byte log line), big.py --bin
  --rovl --erz (the corpus in the text view with render.ovl installed, against a release).
  #48, JPX and JBIG2 pictures (decided 16 Nov 2026, through the hub: the recommendations approved; the
  hub's conditions: both EPDF's own, not in common/ till another app asks; T.88 and T.800 only, never
  jbig2dec's source; OpenJPEG and poppler host oracles only; make fuzz; every stretch under 50 ms on
  Falcon-4; each decoder's size against render.ovl's reservation). JBIG2: src/jbig2.c + src/mq.c
  (the MQ coder, shared) + src/jb2t0.S (template 0's whole bytes, 68000 code): generic regions
  (arithmetic, MMR through ccitt.c), symbol dictionaries, text regions, refinement; a page a row at a
  time (streamable(): a generic region's three rows, a text region's placements sorted by y), whole
  only as a fallback; -Os with HOT inner loops. JPX: src/jpx.c + src/jpx.h (stdint only, rows to a
  callback, a poll, JPX_STOPPED: movable to common/src unchanged), decoded reduced (the r finest
  resolutions' code-blocks passed over, the synthesis stopped r levels early): tier 1 by neighbour
  flags (OpenJPEG's way, from T.800 D), the synthesis in place on 16-bit coefficients (FRAC 5), the
  9/7's K and 1/K folded out, products 16 x 16 (muls.w by asm on a 68000: GCC makes many a
  __mulsi3 call), its state on the heap (a picture's stack is 12 KB). pdfimg.c reduces while the
  picture stays at least as big as shown, smaller again when memory is refused. On the way: CCITT's
  BlackIs1 and /Decode ink sense, an ImageMask's /Decode, a picture's /SMask over white (the text
  view; render.c draws through it). Tests: tests/jbig2.py (synthetic streams, the corpus against
  poppler, row and whole), tests/jpx.py (OpenJPEG-made code-streams at every reduction against
  opj_decompress, the stop path under ASan), tests/onebit.py (13 one-bit and JPX cases in both views
  against poppler); make fuzz (tools/fuzz_jpx.c, tools/fuzz_jbig2.c, libFuzzer, seeds by
  tools/fuzzseeds.py). Benches: JPXBENCH/JPXB30 and JB2BENCH/JB2B30 (tests/hatari/jpxbench.py,
  jb2bench.py; --profile for Hatari's profiler; gap_ms with gap_from/gap_at, the functions a stretch
  began and ended in). Turns: JPX every stripe of a pass, 4 rows of a lifting step or the output,
  32 code-blocks of a packet header; JBIG2 every row, symbol and 16 text-region instances, a big
  symbol's bitmap cleared in pieces, the placements merge-sorted with a turn a pass: every stretch
  35 ms or under on Falcon-4 (16 Nov 2026). Cost on Falcon-4: a JPX picture 13 s (181 x 424) to 61 s
  (414 x 632 of a dense scan; the MQ coder's decisions and the lifting), a JBIG2 page 18-39 s;
  ST-1 a 163 x 237 grey JPX 14 s. render.ovl +42 KB (JPX 20 KB, JBIG2 16 KB), its reservation the
  same. A picture's work in the status bar (the hub: a JPX page is a long wait): jpx_progress (code-blocks)
  and jb2_progress (symbols and placements) into pdfimg_work_done/_of, 900 of the bar's 1000 the
  decoder's (doc_pic_progress; render.c's render_progress the same); Esc stops the picture and holds
  the rest till the view moves (doc_pic_hold, pics_held: "picture-stopped"), leaving its page stops it
  within a turn (pic_give_way); rig tests/hatari/jpxview.py (Falcon-4). Fuzzing found a text region's
  coordinates overflowing (TR_LIM, 2^24) and a bomb of page-sized symbols: a page or region past 16 MB
  a bit a pixel and the dictionaries' symbols past 4 MB together (the corpus's biggest is 699 KB) are JB2_MEMORY (JB2_MAX_BYTES,
  JB2_DICT_BYTES). Pads against 2.35: EP_PAD_J after doc.o, EP_PAD_H after pdfimg.o, EP_PAD_I after
  jpx.o (and their _RO, with EP_PAD_A_RO), PIC_PAD (CONVTEST) / EC_PIC_PAD / RV_PIC_PAD after pic.o
  (164 / 170 / 128 at 2.36), render.c's RPAD_A and RPAD_E (image_hook's soft mask, render_progress);
  after common's crash.o and progbar.o (AERA-Atari 1c01cfd: crash.c's interrupt-time faults, the
  256-colour bar without violets) EP_PAD_K / EP_PAD_L and EConvert's EC_CR_PAD / EC_PB_PAD; EPDF.PRG's
  variables (#48 moved most 44 bytes mod 256, the glyph cache 12) put back by EP_PAD_L_DATA 244:
  Falcon-4:16's speed rows summed 1,277.5 ms against 2.35's 1,257.5, with it 1,255.0 (two runs each,
  alone; with the host loaded the rig loses its keys: run nothing beside a speed or timing rig).
  2.36 went out on 17 Nov 2026 (v2.36 on d50e76186, Release c056da755): #48 with render.ovl's soft
  mask drawn in turns (page 6 of the showcase 115 -> 45 ms on Falcon-4:256 NVDI; ROVLTEST names a
  stretch's ends, rovl.py remakes stale host pages), the data pad, common 1c01cfd. The Release
  workflow installs make test's oracles (poppler-utils, libopenjp2-tools, qpdf). Known: a document is
  read more slowly while a JPX or JBIG2 picture decodes; Copy on a Falcon 5-10 ms over 2.35's (within
  50). The Pi's aerapdf decodes JPX/JBIG2 once AERA-Gateway rebuilds it from 2.36 (AERA-Gateway 1.49
  does). 2.37 (the hub's list, closing #48): pictures only ahead of the view wait while pages are
  still to be read (doc_pic_ahead_only; the one in view takes turns as before: the hub's "text first,
  pictures in view next, the rest after", over the owner's 4 Nov pictures ahead); a picture's rows hand
  back a row at a time (pics.c's sink_row: common's dither finds its nearest pens as a picture's
  colours first come, 40 ms a row of 488 on Falcon-4; the JPX decoder's emit polls every 4 rows), its
  slice ends when the step since its last turn wouldn't fit again (slice_up) and the main loop runs
  one picture slice a turn, the turn's whole (80 ms on a 68000, 40 else): manual-stinternals-text's
  JPX on Falcon-4:16 185 -> 40 ms, the run's stretches over 50 346 -> 60 (2.35 49); the log line
  "picture-what FILTER page N rR WxH in MS ms N slices, most MS" (-DPDFIMG_WHAT, EPDF's pdfimg.o only:
  render.ovl, ECONVERT.PRG and CONVTEST.PRG byte for byte 2.36's); EP_PAD_M after pics.o and the pads
  reset against 2.35. JPX's reduction by the box was already in (r2, 488 x 711, for that picture: its
  own time 40 s, 15 of them the decoder's, the rest the rows' dither). The 68030's lifting measured:
  GCC already makes muls.w #k there; a side-by-side loop was 14,992 -> 14,706 ms (-1.9 %) for +1,142
  bytes in EPDF.PRG, not taken. Copy on the reference configuration: its turn's work is the same as 2.35's
  under gemprof (6.9 ms EPDF, 3.1 ms TOS); the rig's Copy 45 35 45 45 (2.36 45 45, 2.35 35 <20),
  its turns 40 x 4 (2.36 45 45). Known: on ST-1 big.py's novel-boymechanic has pictures refused for
  memory and stretches to 2.4 s, as 2.35 (7.4 s) and 2.36. No pace (the evnt_multi timer 0) while the
  picture on the page in view is made (3140db6cd: scan-gfabasic's picture in view 50.6 -> 35.5 s on
  Falcon-4:16; on ST-1 it changes nothing, 86 pages against 83). The rigs' profiling builds
  (build/aprof EPDF.PRG and ECONVERT.PRG) laid out as the shipped ones: tools/aprofpad.py pads after
  the profiling gem.o (code, a merged string, nfon.o's variable) and fails the build if anything else
  sits elsewhere mod 256 (3261e8c64; the hub, 17 Nov 2026: linked at the end it had moved 197 KB of
  code; common's tools/profpad.py does the same for the family, to be taken after 2.37). sink_row
  ends in a jump over 222 bytes of NOPs, 256 bytes longer than 2.36's (its 22-byte growth had moved
  the rest of pics.o, 10.9 KB, off its cache places: View > The page's lines on the reference configuration
  165 against 155, now 155/165/155 against 155/165/155), EP_PAD_M 0. The regression gate (the owner,
  17 Nov 2026) ran for 2.37 first: every row (TT low and ST low refused by design, 320 wide;
  tests/hatari/mint.py and nfoff.py new, gui.py's EPDF_NVDI and quit check), pairs within a tick on
  the reference configuration, Falcon-4:16 and ST:mono (Copy 35 against 45 on both Falcons). ST-1 with
  big.py's key every 20 s reads novel-boymechanic's 83-86 pages against 2.36's 105-128: the page-6
  scan (1460x2224, one bit) is made for 11 s and refused memory, twice, in both; 6af0ea53c's rows a
  row at a time only change when its first refusal comes (51fad0b0a: 130). With no keys both read
  119, on a 4 MB ST 14 and 13: no slower reading. A picture that can't fit failing fast is with the
  hub's #98 (the family's heap).
  2.38 (the owner's Hebrew report, 17 Nov 2026: FreeMono drew TOS's Hebrew codes as other letters, its
  runs off their cells): with a GDOS font the text view's rows go through common's textcells.c
  (text_hebrew, text_cells_probe, text_cells_n: EMail's runs and cells, AERA-Atari ae79b2a), its
  Hebrew by the font's own glyphs (fontsel_hebrew: FS_HEB_UNICODE) or, where it has none, the system
  font's in its cells; the system font keeps one text_plain a row as 2.37 (font_gdos(): a6f596b65
  had sent it through the cells too, the reference configuration's page scroll 75 -> 115); the cell holds
  vqt_width('M'). Rig tests/hatari/hebfont.py (`make hebfont-test`, under NVDI: FreeMono "font",
  Courier 10 Pitch "system", the system font; the right-to-left rows end on one edge). Section 8's
  last rows: D19 (Copy, Find..., Find next/previous, Open..., Save Optimised... greyed while a job
  runs: job_running), D30 (the Gateway's port out of range refused with an alert, the cursor in
  it), D33 (EConvert's Stop on the press). common 7b8ced9 (net_tos's connect that gives up,
  textcells, dialog.c's dlg_touch_up before Settings draws, the hub's dbfc8e2). common's text_font
  does its setup every call now: font.c's font_text tells it only a change (Falcon-4:16's start
  250 -> 245, 2.37 240). The sync moved all of EPDF.PRG off 2.37's cache places: every pad
  refitted, new pads N-T and EP_PAD_A_DATA (the Makefile's note); still off, the files that
  changed inside. Then common 5f0b8e7 (the hub's 1c8f1cd, 32b3341, 90b1bbd, cc1ae54, b4b4597:
  dlg_find's own walk in a 3D AES, a press kept between a Settings page's turns, key_state_init and
  popup_m last in their files, NVDI on the TT); dialog.c's growth moved text.c's drawing, put back by
  EP_PAD_V after dialog.o. The pairs against 2.37 within a tick on the reference configuration, Falcon-4:16,
  ST:mono and ST:med. EPAD's pads are NOPs, not zeros (the same bytes' count: no code moved), so
  oddaccess.py's objdump reads doc_view_moved in step (zeros it skips four bytes at a time).
  The regression gate ran on a46bb0b17 (5b2b5b279's rows stand: only the pads' bytes differ):
  every row ok but TT low and ST low (refused, 320 wide); big.py --erz --rovl 25 of 25 clean. Three
  pairs against 2.37: within a tick but ST:mono's Select all (45/55/55 against 45/45/45) and next
  page (205 against 195 median), cleared by the hub on gemprof (the same work to 0.1 ms: Select all
  EPDF 35.7 + TOS 15.3 ms, over its 50 in 2.37 too; next page 170.0 + 36.1). 2.38 went out on
  18 Nov 2026 (v2.38 on 406f07b64, the hub's go on a46bb0b17 after its acc_all with four
  accessories under NVDI, two boots). Next (2.39): common's 796e05e/2fa3aa7 (Settings in turns),
  profpad.py, pic.c's grey nearest pens, ST-1's fail-fast with the hub's #98.
  2.39 (decided 18 Nov 2026): (1) common
  2fa3aa7 (Settings in turns), EP_PAD_V/R refitted, EConvert's new EC_DL_PAD/EC_ST_PAD (10083092b; the
  reference is a clean v2.38 build, byte for byte the release: a working tree's stale objects misled once);
  (2) profpad.py: three gaps found (a section's start, an archive's member, the merged strings), sent to
  the hub as base64 (this session may not push to AERA-Atari), in common at 02fb09d; the timed builds
  (build/aprof) are linked by common/tools/profpad.py from the shipped links' maps, tools/aprofpad.py
  gone (0c163ac8d, b4e861d78); (3) pic.c's dither: row_near/row_few (ROW_PENS) in 16 bits and a 12 KB
  table for 3-4 pens, the pens byte for byte (tests/pic, pens, test68k, aerapens old against new 1400
  requests), DITHBENCH.PRG (tests/hatari/dithbench.py) on Falcon-4 ST medium 56.1 -> 16.9 s, 16 colours
  21.6 -> 14.1, on ST 194.0 -> 57.6, 71.0 -> 51.0; in common; (4) the hub's heap zones (common 235203a:
  heap_zone, heap_zone_use, heap_zone_end, heap_room) for a picture on a 68000, by the hub's bands
  (18 Nov 2026; doc.c's pz_begin, b1d966376): heap_room() under pdfimg.c's pdf_image_rows_need (with a
  margin, fitted to big.py's peaks) refuses it at once ("picture-zone-refused"), up to twice it takes a
  zone ("picture-zone-begin", its peak logged at its end as "picture-zone"), more than that the main heap
  as in 2.38 ("picture-zone-main": zones always cut scan-gfabasic on ST-1 from 55 pages to 19);
  pdf_main_heap and bm_alloc keep what outlives a picture on the main heap. A page whose reading was
  refused memory lost a picture's Do without a word (its XObject's load refused, then cached unloadable
  for the page): pdftext.c's missed_pics gives each of the page's image XObjects not met its line after
  the page's text, pdfobj.c's pdf_retry_obj lets it load again; "missed N" on the page's log line,
  big.py's notes (with each picture's start and the zones' peaks); tests/missedpic.py in make test (2.38
  fails at 51-53 and 75-77 KB). The 3 KB: 2.39's growth against 2.38 was 7.2 KB (the dither 3.1, the
  zones 2.7, the estimate 0.5, missed_pics 0.7 and a 64-bit divide, taken out); epdf.c's Languages...
  state (4 KB of BSS) is taken while that dialog is up, so 2.39 is 2.9 KB over 2.38 in all; translate_view.py
  opens Languages... over Settings and leaves it with Esc. (6) map-europe on ST (profile.py): inflate_fast
  45 %, run_content_in 27 %; inflate_fast is EPDF's to study (the hub, 18 Nov 2026: a patch against
  common's main, in inflate_fast.S's place with the same entry, exit, window and tables; tests/inflate on
  the host and m68k, byte for byte over the corpus's streams and the fuzz seeds, coldfire's make test and
  make same, cycles per output byte on 68000 and 68030 before and after, build's tables and inflate's
  memory on a 1 MB ST). (5) and (6) move to 2.40 (the hub, 18 Nov 2026): 2.39 is the fix-forward
  of a crash in 2.38 and before, found by the #98 evidence (big.py, ST-1, TOS 1.04): pdffilt.c's f_lzw
  and f_rl wrote a code's string after an unchecked sb_room, so a 1 MB ST refused the output's growth
  and the decoder wrote over the heap (tech-rfc2616, LZW; 2.38 crashed in it and in
  manual-stinternals-text). Found by a scratch build with libc's heap_check on every allocation and an
  allocation ring (never committed). Both decoders now stop on a refusal (3dc4cc3f9); tests/filtmem.py in
  make test (build/pdftext_oom: ASan, tests/oomwrap.c's realloc refusing past REALLOC_MAX). The notes
  say: "a 1 MB ST could crash reading an LZW-compressed PDF". missed_pics only for a Do whose own
  lookup was refused (do_xobject notes it; a book's shared resources had given every image a line).
  CCITT's estimate by its decoder's cost, NEED_PIECE + 10240 + sw * 5 + w * 2 (a 6,600-wide scan
  refused where 2.38 made it; 9c662ec88). Under the estimate's own block, fits_in_pieces takes it in
  at most four blocks and gives them back, then the main heap ("picture-zone-main ... pieces":
  heap_room() is the largest block, a Flate picture's biggest is inflate's 65,797; db6266fb8). The
  evidence against 2.38 (two emulators a side on a 4-CPU host: big.py counts pages over wall time):
  ST-1 no document reads fewer pages beyond one in a run, and several read far more; the 4 MB ST is
  within a page, and MEASURE.PRG on ST times the reader the same (manual-compendium 60 pages 69.91 s
  both). Known: manual-stinternals-text on ST-1 has 3-4 pages refused memory at the reader's 510 KB
  peak (2.38 crashes in it). The gate on db6266fb8, its rows the counting touches again on 13992c199
  (a stream refused its buffer's growth counted as memory, not damage): every row ok but TT low and ST
  low (refused, 320 wide), big.py --erz --rovl 25 of 25; three pairs against 2.38 within a tick on
  the reference configuration, Falcon-4:16 and ST:mono, so the pads stay (93 % of the code off 2.38's cache
  places, no row moved). The hub's family check (ECal, EContact, EHypView, EMail check under NVDI,
  two boots) and go on 13992c199; 2.39 went out on 19 Nov 2026 (v2.39 on 8acef7368; the Release
  workflow's first attempt hung in Tools, cancelled before anything was published, the re-run went
  through). 2.40 (the hub, 19 Nov 2026, the owner's): the FireBee fault fix alone, common
  synced at exactly f553f9336 (a 68000 program's Line-A 0xa000 is the ColdFire's EMAC macw: text.c,
  offdraw.c, aboutbox.c and badge.c take the VDI's way where _CF_ is set; Hatari's FireBee has no EMAC,
  so unconfirmed on a real one). Pads refitted against a clean v2.39 build by the link maps
  (each object's start mod 256 in .text, .rodata, .data and .bss): EP_PAD_E 66 (+_DATA 254), a new EP_PAD_W
  after text.o (120, _RO 116, _DATA 254; its _ro and _data need their own -u), EC_AB_PAD 48 (+_DATA),
  EC_OD_PAD 152 (+_DATA), a new EC_TX_PAD after text.o (84, _RO 128, _DATA 254): all at 2.39's places
  but inside the changed files; render.ovl byte for byte 2.39's. The gate as 2.39's (TT low and ST
  low refused; big.py ST-1 23 of 25, the two of 2.39), three pairs within a tick, the hub's family
  check (four accessories under NVDI, two boots) and go on 76ca15681; 2.40 went out on 19 Nov 2026
  (v2.40 on a509c1fe3). Faster display (branch v240: EConvert's Optimise for, /AERAShown, pens= on
  the Gateway with a fallback, the pdfgate patch held for the AERA-Gateway session) and inflate_fast
  go to 2.41, with common 74d6095 or later (scrpens in common as ui/scrpens.c, 70aff63; past c6ef2f0
  EPDF.PRG's libc.c needs -DLIBC_ZONES: only doc.c calls the zones).
  2.41 (Faster display; the hub's go 20 Nov 2026): a picture's /AERAShown copy in the screen's pens at
  the size the text view shows it (conv.c's w_shown, docs/CONVERTED-PDF.md), copied by pics.c where its
  pens are the screen's; EConvert's Options > Settings... > General > Optimise for (Faster display the
  default, OPTIMISE=; the owner kept the page as tests/hatari/econv_settings.py shoots it, 20 Nov 2026); the
  Gateway's pens=/column=/aspect= (pigate.c; aerapdf's --pens-hex, --column, --aspect; a pdfgate without
  them refuses "no option pens" and is asked again without; AERA-Gateway holds pdfgate's side for
  2.41's sync-pdf.sh); put_pens 8 pixels at a time through two tables (showcase 15.3 -> 13.4 s on
  the reference configuration). common at exactly 72e4e66 (scrpens, LIBC_ZONES, the V4SA dragging trails).
  The memory 2.41 added is taken when used (put_pens' tables, pigate's query and line, gwopen's pens:
  6.6 KB of bss had cost ST-1's big.py two documents' pictures). Pads refitted against v2.40 by the
  link maps for every section (new: EP_PAD_X after pigate.o, EP_PAD_Y after pdftext.o for the merged
  strings, bss pads through ftpad.S's FT_PAD_BSS, EConvert's after econvert.o, libc.o, pigate.o and
  conv.o, CONVTEST's after conv.o and before inflate_fast.o, render.ovl's RV_GIF_PAD_STR 258).
  the owner accepted (20 Nov 2026) the start 330 -> 345 on his configuration and the About box 120 -> 130
  on Falcon-4:16; the About box's close 40 -> 85 under NVDI is common 55022752e's trade. The
  inflate_fast study (INFLBENCH.PRG, tools/inflstreams.py, tests/hatari/inflbench.py): 207/146 cycles a
  byte on an ST (map, text), 68/49 on Falcon-4; two-byte refill and copy gave -2 % on an ST and +18 %
  on a Falcon (the loop past the 68030's 256-byte cache): no change (the hub, 20 Nov 2026), build()
  left as it is. Not in 2.41: a 256-colour whole page and its double (a new 8-bit dither, pages about
  twice the size). Next: common's about_prepare (the hub: in the next release).
  2.42 (21 Nov 2026; v2.42 on 04473ea20, the hub's go on b8efdf1c9): the owner's 2.41 fault, "the precise
  zoomed in image is not presented": a copy EConvert made zoomed from its own stored page. The
  converter writes /Source /Path (EConvert on the Atari only: CVOPT.source_path, econvert
  --source-path; aerapdf's files unchanged); doc.c's find_original takes it, else /Name in the copy's
  folder's parent, each only when its size is /Bytes (a 68020+; "original path|parent|none"), and
  View > Pages draws such pages sharp too. zoom.py's two converted cases (the folder above, /Path)
  check the zoomed page's glyphs: 2.40's build fails them, 2.42 passes on Falcon:256 and Falcon:256 under NVDI with TT RAM (21 Nov 2026; the
  hub keeps Falcon-4:256 the floor for the pairs). Where no zoom fits (Falcon-4:256: 200 % is 2 MB
  of bitmap) the status bar says "Not enough memory to zoom" (the owner). EConvert's Translate (decided 20
  Nov 2026: a pop-up in the window, Off at the start, not kept): the Gateway's languages asked when
  it opens; This Atari through common's translate.c (tr_direct), the Gateway translate=;
  tests/hatari/econv_translate.py. EPDF's Options > Settings... > Gateway (the owner: a page of its own,
  today's defaults): Whole pages (Automatic, None, Mono, Colour, Mono and colour), Photos, Optimise
  for, Page previews (With the page strip, Always, Never); GWPAGES= GWPHOTOS= GWOPTIMISE= GWPREVIEWS=
  written once changed; tests/hatari/gwsettings.py, gateway.py's GW_EPDF (its EConvert now converts
  with OPTIMISE=size: pdfgate 1.51 takes pens=, run.sh checks those byte for byte). gateway/ synced
  to AERA-Gateway 1.51 (7d67b84). Pads refitted against a clean v2.41 build (EPDF all sections at
  2.41's places; EConvert's scrpens.o and sting_s.o off, outside loops). The pairs: two ST:mono rows
  (About 135 -> 142.5, Select all 45 -> 50 medians of six) cleared by the hub on gemprof (the same
  work to 0.1 ms). Next: 2.43, printing (branch print243: File > Print... ^P through NVDI, the owner's
  words of 21 Nov 2026), then common's about_prepare.
  2.43 (22 Nov 2026; v2.43 on cadca7570's programs, 1b51bd8b5, the hub's go): File > Print... ^P through
  NVDI or another GDOS (src/gem/prjob.c on common's print.c, 523d378), decided 21 Nov 2026: the
  two alerts (as EMail's), "Printing page 3 of 12" with the family's bar, "Printed." / "Printing
  stopped.", a picture left out for memory "Printed without some pictures: not enough memory." (the
  shorter wording, his choice). An ST prints the text view's text; a 68020+ with render.ovl draws each
  page (on under 2 MB free render.ovl's document closed after each page: the owner's), the pictures paused first (doc_pics_pause, doc_print_room). Without WDIALOG print.c
  opens device 21 itself; a paperless device (META.SYS, 0..32767) is held to A4. prjob.o and print.o
  linked last; pads refitted against v2.42 (new eppad_z, eppad_u, bss pads on b and j). Rig
  tests/hatari/print.py (the reference configuration, ST:mono, Falcon:256 14 MB; pages 2-4, Esc, no WDIALOG,
  page 3 alone on Falcon-4: its pictures left out and said so). A page with pictures takes 5-8 min on
  a 4 MB Falcon, about 2 with 14 MB; an ST's text page 35 s. Next: FireBee native (#131), about_prepare.
  2.44 (22 Nov 2026; the hub's go on 1069b2325): two packages, epdf-x.y.zip and epdf-x.y-firebee.zip
  (decided 22 Nov 2026; the guidelines' 7.4): binary/fb/ holds the ColdFire's
  EPDF.PRG, ECONVERT.PRG and render.ovl (common/tools/mkfirebee.py), each package's program names the other
  on the wrong machine (tests/hatari/fbalert.py); the release publishes only the two zips, the FireBee's
  first and the standard last (the release check reads the API's asset order). The log (6.5, D8): Options >
  Write a log, its last item, and Settings' General page (EConvert both too), LOG= off at first, through
  common's conn.c conn_log to epdf.log/epdf.old and econvert.log beside the program; log_event's lines and
  the print steps (print-open's driver and device from PRN_SETTINGS, "print-bands N copies B ink W" a
  page); tests/hatari/logfile.py (`make log-test`). Printing was reported failing where EMail prints (a printer driver Hatari can't open), so 2.44's notes
  ask for epdf.log after printing a page; the fix in the release after. Pads refitted against v2.43 (EPDF at its places but print.o, cold
  and last). The pairs: View > The page's lines on the reference configuration 155 -> 165 (medians of six),
  cleared by the hub on gemprof (the same work to 0.1 ms). Known (the hub's family check): ST-1 on TOS 1.04
  with ECal, EContact or EHypView loaded, the Desk menu doesn't open in the first round and EPDF's quit
  doesn't bring the desktop back, as in 2.43: EPDF's 1 MB limit beside the bigger accessories, for after
  the 1.0s, with the free memory from the log. Next: the print fix from the log, soft picture colours
  (#132), about_prepare.
  Next, after 2.36 (decided 17 Nov 2026): the converter prepares pictures and whole pages in the screen's own colours, so EPDF
  only unpacks and copies (the rigs' logs: on Falcon-4:256 a photo 1.2 s decoding, 6.7 s fitting to
  the screen's colours). EConvert's Settings > General "Optimise for: Smaller files / Faster display",
  Faster display the default, for the converting machine's screen; the Gateway the same from EPDF's
  Open on the Gateway, for the screen EPDF runs on; a 256-colour whole page and its double; a file so
  made opens anywhere, at today's speed where the screen differs. Measure on the showcase first; show
  the owner the Settings page as the rig shoots it.
  2.11 (decided 10 Nov 2026: both fixes, a band on the page): a converted copy's
  View > Pages on a Falcon/TT in colour is drawn sharp from its original by render.ovl over the stored
  page (doc.c's sharp_pages, pdbl.stand, doc_stand_in; the stored one decoded first, pics_next),
  where memory holds both and the canvas whole (a 4 MB Falcon keeps the stored page); "Drawing a
  sharper page... 40%" across the page's top in view and the zoomed view's (sharper_band,
  win_band_tick). The converter's 16-colour page snaps a colour near a pen (quant.c snap_of, as
  render.ovl's snap_pen): no pattern over greys. With 6 MB free at the start 24 pictures are kept
  (pics_room; past 8 only while 2 MB stay free): switching Pages and Text decodes nothing again.
- **The road to 1.0, and EConvert (decided 31 Oct 2026):**
  - **1.0 is the text view, complete:** 0.2 speed on an 8 MHz ST (the guidelines' times), 0.3 big
    documents on ST-1 (streamed content, pictures' memory), 0.4 pictures (DSP JPEG on the Falcon, the
    1/8 preview on a 68000), 0.5 the rest (GDOS font on hardware, AES-256 on a 68000), then 1.0.
    (Reports stood here too: faults found in the releases before 0.5, fixed in 0.4.1 and 0.5. The GDOS font: ST users run NVDI (decided 2 Nov), covered by the
    speed rig under NVDI with a GDOS font on ST:mono, ST:med and Falcon-4, AERA-Atari c32220c. The
    About box on an 8 MHz ST in low or medium within 200 ms, 150 elsewhere: the owner's (a), AERA-Atari
    69da4d6, speed.py's Rig.about. Maps on an ST: speed first (decided 2 Nov): the
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
  own stack; the reader's hook (`PDOC.stop`, every 8 operators or 256 numbers, and 32 glyphs) hands the machine back when
  the slice is up; nothing on that stack may call the AES. A picture is decoded on a second stack
  (`pic_poll` hands back the same way); it may use the document's objects only before its first
  hand-back. A page that is read ends its slice, and its rows are drawn in the next turn. A picture
  over 60 % of a page that has text is left out (background or scan behind a text layer).
  The extractor marks a picture as a line "\x7F<object> <w>x<h>" in the page's text.
- **Governance:** the hub owns AERA-Atari and `common/`. Never change `common/` here: pull it with
  `tools/sync-common.sh`; a needed change goes to the hub as a patch (`git format-patch`, plain text)
  with its reason and tests. A release goes out through the regression gate above (decided 17 Nov 2026:
  the hub's family check before the release again, in place of 7 Nov's "the hub's checks follow"). Gateway code (pdfgate,
  aerapdf) is EPDF's protocol and EPDF tests every change to it (decided 8 Nov 2026, guidelines 1.2):
  `make pi-test` and `tests/hatari/gateway.py` run pdfgate.py itself from `gateway/`, a read-only
  copy of AERA-Gateway's gateway branch (`tools/sync-gateway.sh`; change it in AERA-Gateway with its
  session, never here), before each release; `PDFGATE=none` runs EPDF's stand-in,
  tests/pigate/fakegate.py, instead. It reaches the Pi only through the AERA-Gateway session, and a release that needs it names the
  Gateway version and goes out after it (guideline 7.3). Ask the hub before copying anything from
  another app.
- **Licences:** everything must be GPL-2.0-or-later compatible. No MuPDF (AGPL), nothing else
  incompatible. FreeType (FTL/GPLv2) is fine and works in fixed point. Third-party code lives in
  `third_party/<name>/` at a pinned version with its README and any patches as patch files (EWeb's
  way). The standard 14 PDF fonts come from metric-compatible free fonts (Liberation, DejaVu…); check
  each licence. poppler/mupdf-tools may be installed on a build machine as a comparison oracle, but
  nothing of them is linked, copied or committed.
- **Never commit** (on top of the list above): real TOS images and NVDI (`~/.aera/tos`,
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
  Falcon-4), on real TOS 1.04 and 4.04 where present (`~/.aera/tos`), else EmuTOS. EPDF and
  EConvert log through NatFeats only when their INF has `NATFEATS=1` (decided 15 Nov 2026; a Vampire V4SA
  hung on the probe): every rig calls `tests/hatari/natfeats.py`'s `on(hd)` before it starts them,
  the test programs (`atari/*.c`) set `nf_on = 1`, and the rigs' builds (build/aprof, build/times)
  link `atari/nfon.c`. Not in Settings. A FireBee (FireTOS's `_CF_`, which also says `_CPU 60`) loads
  no render.ovl and converts only through the Gateway, as an ST (rdraw_machine, EConvert's has_030;
  rovl.py and gui.py on common's FireBee:16 stand-in). EPDF.PRG's, EConvert's and CONVTEST's pads (EP_PAD_A..D,
  EP_PAD_A_RO for the tables, EC_PAD, CT_PAD) keep their code at 2.32's cache places (tools/rpad.py,
  EPDF.PRG by nm against a 2.32 build) after the code before it grew. A report names the
  machine and the TOS (`describe()`). The cloud session that made the measurement run had no TOS
  images: its numbers are on EmuTOS 1.4 (CPU-bound work is TOS-independent; file and GEM calls are not).
- **Build and test:** see the Makefile's header. `make` builds `binary/EPDF.PRG` (host tools under
  `build/`); `make test` runs the host tests (layout, store, view, INF, search, CCITT, the object table, a page set a glyph at a time: `tests/glyphwise.py`) and checks the
  programs (`tools/nofpu.sh`, `common/tools/oddaccess.py`). Before a push that touches the GEM side:
  `make gui-test` (`tests/hatari/gui.py`, ST:mono; also the reference configuration). the reference configuration
  (the hub, 10 Nov 2026: Falcon-4:256 with TOS 4.04 and NVDI, common's machines.ERZ_MACHINE,
  `tests/hatari/erz.py`) is the Falcon of prepared.py, econv.py, econv_fsel.py, drawn.py, rovl.py and
  translate_view.py; zoom.py keeps Falcon:16 and Falcon:256. With no TOS images or NVDI here it is the
  same machine on EmuTOS: a stand-in. A change to drawing, a loop over
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
