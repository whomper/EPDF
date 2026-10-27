# EPDF measurement run (stage 0), 27 October 2026

What a PDF costs on the family's machines, measured before any application is written. Everything
here is reproducible from this repository (the commands are at the end). **All Atari numbers are
Hatari's emulated clock on EmuTOS 1.4** (the session had no TOS 1.04 / 4.04 images); parsing,
inflate and FreeType are CPU-bound and TOS-independent, drawing and blitting through the AES/VDI
are not measured at all.

## Method

- **The reader** (`src/`, portable C, 68000-safe, 16.16 fixed point, no floating point): cross-reference
  tables, xref streams, hybrids and a rebuild by scanning; object streams; Flate (common's inflate),
  LZW, ASCII85, ASCIIHex, RunLength, PNG/TIFF predictors; RC4 40/128, AES-128, AES-256 (R6) with the
  empty user password (BearSSL's AES/SHA/MD5); the page tree; fonts (base encodings, Adobe Glyph List,
  Differences, ToUnicode, Type 1 and CFF built-in encodings, widths); a content interpreter that gives
  each page's text in reading order and counts what drawing would take.
- **The text is checked against pdftotext** (poppler, reference only): F1 of the words per document
  0.82–1.0 (table at the end); Hebrew comes out in logical order.
- **Machines** (`common/tools/machines.py`): `Falcon-4` (stock Falcon, 68030 at 16 MHz, 4 MB ST RAM, no
  FPU), `ST` (68000 at 8 MHz, 4 MB), `ST-1` (1 MB), and `Falcon` (32 MHz, TT RAM; the nearest to Erez's
  DFB1 that Hatari does). Times are the 200 Hz clock with MFP timer C's counter: 26 µs steps.
- **Corpus**: 25 PDFs (`tools/corpus.json`, fetched by `tools/fetch-corpus.py` into `build/pdfs`, sha256
  pinned, never committed): two novels, three manuals, two RFCs, two datasheets, a NASA magazine and two
  photo documents, three maps, a scanned patent, three scans (JPEG/CCITT), two real Hebrew texts, a
  generated Hebrew text and generated charts, and four encrypted copies (RC4-40, RC4-128, AES-128,
  AES-256). Licence of each is in the manifest; the Atari/Motorola/Yamaha items are community scans on
  archive.org, read here only.
- **Pages**: 8 spread over each document (`sample:8`, page 1 always included). 163 pages: 163 read on
  every machine (but see ST-1 below).

## Results: the reader, per document

Times for the 68000 build (the one every ST runs). "Page 1" includes loading its fonts; "other
pages" is the median (maximum) of the other sampled pages. "Heap" is common's `heap_peak`.

### Falcon-4

| document | kind | pages | open | page 1 | other pages: median (max) | heap KB | KB read a page |
|---|---|---|---|---|---|---|---|
| novel-twocities | plain text (novel) | 330 | 185 ms | 943 ms | 908 ms (1.0 s) | 120 | 64 |
| novel-boymechanic | plain text with drawings (book) | 596 | 424 ms | 314 ms | 2.3 s (3.8 s) | 407 | 200 |
| manual-compendium | manual (Atari Compendium) | 842 | 450 ms | 625 ms | 620 ms (837 ms) | 186 | 144 |
| manual-stinternals-text | scanned book with a text layer | 517 | 2.8 s | 2.0 s | 3.5 s (6.8 s) | 921 | 64 |
| scan-stinternals | scanned pages (JPEG) | 517 | 446 ms | 52 ms | 46 ms (47 ms) | 105 | 64 |
| scan-gfabasic | scanned manual (big file) | 226 | 147 ms | 37 ms | 420 ms (895 ms) | 125 | 96 |
| tech-rfc9110 | technical text (RFC) | 194 | 981 ms | 1.1 s | 1.1 s (1.3 s) | 266 | 64 |
| tech-rfc2616 | technical text (older RFC) | 114 | 41 ms | 689 ms | 931 ms (1.1 s) | 92 | 53 |
| datasheet-mc6850 | datasheet (scan) | 12 | 21 ms | 32 ms | 32 ms (32 ms) | 47 | 64 |
| datasheet-ym2151 | datasheet | 10 | 26 ms | 75 ms | 59 ms (59 ms) | 50 | 49 |
| magazine-nasa-ask | magazine with photos | 56 | 64 ms | 354 ms | 562 ms (992 ms) | 121 | 65 |
| photo-nasa-sunda | photos and captions | 1 | 312 ms | 5.4 s | - (-) | 1805 | 251 |
| photo-shuttle | pamphlet with photos | 8 | 26 ms | 87 ms | 564 ms (1.1 s) | 110 | 54 |
| map-europe | map (vectors) | 1 | 25 ms | 25 s | - (-) | 1913 | 849 |
| map-physical-world | map (vectors, big) | 1 | 29 ms | 40 s | - (-) | 1727 | 1349 |
| map-timezones | map (vectors) | 1 | 25 ms | 30 s | - (-) | 1406 | 927 |
| hebrew-idf-guide | Hebrew text | 3 | 21 ms | 2.0 s | 969 ms (1.3 s) | 401 | 41 |
| hebrew-hava | Hebrew text | 16 | 19 ms | 258 ms | 31 ms (513 ms) | 74 | 64 |
| scan-patent-ccitt | scanned patent (CCITT) | 4 | 26 ms | 260 ms | 2.9 s (4.1 s) | 259 | 158 |
| gen-hebrew | Hebrew text, embedded TrueType (generated) | 8 | 15 ms | 686 ms | 439 ms (440 ms) | 66 | 25 |
| gen-charts | charts (vectors, generated) | 8 | 14 ms | 794 ms | 119 ms (779 ms) | 98 | 23 |
| enc-rc4-40 | encrypted (RC4 40-bit, empty user password) | 330 | 178 ms | 1.1 s | 933 ms (1.1 s) | 118 | 48 |
| enc-rc4-128 | encrypted (RC4 128-bit) | 330 | 340 ms | 1.1 s | 933 ms (1.1 s) | 118 | 48 |
| enc-aes-128 | encrypted (AES 128-bit) | 330 | 344 ms | 1.5 s | 1.1 s (1.5 s) | 118 | 48 |
| enc-aes-256 | encrypted (AES 256-bit) | 330 | 39 s | 1.7 s | 1.1 s (1.6 s) | 118 | 48 |


### ST (68000 at 8 MHz)

| document | kind | pages | open | page 1 | other pages: median (max) | heap KB | KB read a page |
|---|---|---|---|---|---|---|---|
| novel-twocities | plain text (novel) | 330 | 718 ms | 3.3 s | 2.5 s (3.3 s) | 120 | 64 |
| novel-boymechanic | plain text with drawings (book) | 596 | 1.8 s | 1.1 s | 6.8 s (11 s) | 407 | 200 |
| manual-compendium | manual (Atari Compendium) | 842 | 2.1 s | 2.0 s | 2.1 s (2.6 s) | 186 | 144 |
| manual-stinternals-text | scanned book with a text layer | 517 | 14 s | 6.2 s | 11 s (21 s) | 921 | 64 |
| scan-stinternals | scanned pages (JPEG) | 517 | 1.8 s | 233 ms | 177 ms (181 ms) | 105 | 64 |
| scan-gfabasic | scanned manual (big file) | 226 | 686 ms | 139 ms | 1.3 s (2.7 s) | 125 | 96 |
| tech-rfc9110 | technical text (RFC) | 194 | 4.9 s | 3.5 s | 3.0 s (4.0 s) | 266 | 64 |
| tech-rfc2616 | technical text (older RFC) | 114 | 168 ms | 2.1 s | 2.6 s (3.1 s) | 92 | 53 |
| datasheet-mc6850 | datasheet (scan) | 12 | 73 ms | 94 ms | 93 ms (93 ms) | 47 | 64 |
| datasheet-ym2151 | datasheet | 10 | 87 ms | 258 ms | 163 ms (163 ms) | 50 | 49 |
| magazine-nasa-ask | magazine with photos | 56 | 241 ms | 1.0 s | 1.5 s (2.6 s) | 121 | 65 |
| photo-nasa-sunda | photos and captions | 1 | 1.5 s | 21 s | - (-) | 1805 | 251 |
| photo-shuttle | pamphlet with photos | 8 | 84 ms | 281 ms | 1.6 s (3.2 s) | 110 | 54 |
| map-europe | map (vectors) | 1 | 86 ms | 85 s | - (-) | 1913 | 849 |
| map-physical-world | map (vectors, big) | 1 | 100 ms | 151 s | - (-) | 1727 | 1349 |
| map-timezones | map (vectors) | 1 | 84 ms | 112 s | - (-) | 1406 | 927 |
| hebrew-idf-guide | Hebrew text | 3 | 74 ms | 6.7 s | 2.7 s (3.8 s) | 401 | 41 |
| hebrew-hava | Hebrew text | 16 | 67 ms | 858 ms | 88 ms (1.5 s) | 74 | 64 |
| scan-patent-ccitt | scanned patent (CCITT) | 4 | 88 ms | 772 ms | 8.6 s (12 s) | 259 | 158 |
| gen-hebrew | Hebrew text, embedded TrueType (generated) | 8 | 50 ms | 2.1 s | 1.3 s (1.3 s) | 66 | 25 |
| gen-charts | charts (vectors, generated) | 8 | 46 ms | 2.7 s | 394 ms (2.6 s) | 98 | 23 |
| enc-rc4-40 | encrypted (RC4 40-bit, empty user password) | 330 | 675 ms | 4.0 s | 2.7 s (3.7 s) | 118 | 48 |
| enc-rc4-128 | encrypted (RC4 128-bit) | 330 | 1.4 s | 4.0 s | 2.7 s (3.7 s) | 118 | 48 |
| enc-aes-128 | encrypted (AES 128-bit) | 330 | 1.4 s | 5.5 s | 3.1 s (4.9 s) | 118 | 48 |
| enc-aes-256 | encrypted (AES 256-bit) | 330 | 165 s | 6.2 s | 3.3 s (5.4 s) | 118 | 48 |


### 68030 build, 32 MHz Falcon, ST-1

- **A 68030 build of the text reader gains 4–9 %** on Falcon-4 (up to 17 % on CCITT pages): the work is
  byte loops in ST RAM, not multiplies. One 68000 build serves every machine for the text view.
- **Falcon at 32 MHz with TT RAM** (the DFB1's neighbour): 2.1–2.6 x faster than Falcon-4 on text
  pages (novel 0.41 s, RFC 0.48 s, manual 0.30 s). Pages with megabytes of Flate (maps, the photo
  page) were *slower* (0.4–0.7 x) in the TT RAM run; not explained (Hatari's TT RAM timing?), to be
  checked on Erez's machine.
- **ST is 2.7–3.9 x slower than Falcon-4** on every page kind.
- **ST-1 (1 MB)** reads every page, but the pages whose decoded content is bigger than the memory left
  are *cut short without an error* (the three maps and the photo page; heap peak 0.4 MB where the 4 MB
  ST used 1.4–1.9 MB). Stage 1 needs streaming content (common's `inflate_open`/`inflate_read`).

## Results: pictures (scanned pages)

Picture decode to the size shown, Falcon-4 unless named; `tools/imgdump` writes the pictures, `atari/imgbench.c`
times them (`tests/hatari/imgbench.py`).

| picture | Falcon-4 (68000 code) | Falcon-4 (68030 code) | ST |
|---|---|---|---|
| CCITT G4, 5.5 Mpx, text scan (10–13 KB) | decode 2.6 s, + shrink to grey 5.7 s | 2.4 s / 5.3 s | 8.9 s / 26.7 s |
| CCITT G4, 8.3 Mpx datasheet (22 KB) | 5.1 s / 9.6 s | 4.7 s / 9.0 s | 17.9 s / 44 s |
| CCITT G4, 7.9 Mpx patent (100 KB) | 19.9 s / 24.2 s | 18.2 s / 22.4 s | 70.9 s / 95.9 s |
| CCITT G4, 8.4 Mpx halftone page (200 KB) | 31.6 s / 36.1 s | 29.4 s / 33.8 s | 115.7 s / 142 s |
| CCITT G4, 33.6 Mpx datasheet (80 KB) | 19.4 s / 27.6 s | 17.7 s / 25.0 s | 68 s / 118 s |
| JPEG baseline, 3.2 Mpx, to 340 px wide (TJpgDec, 1/2 scale) | 63–70 s (+ 2.7 s to dither to black and white) | – | 222–246 s |
| the same JPEG, libjpeg-turbo, 68030 | 28–29 s | | |
| the same JPEG, **libjpeg-turbo + the DSP** | **3.9–4.6 s** | | |
| JPEG progressive 1.4 Mpx (TJpgDec: block averages, 1/8 size) | 24 s | | 77 s |
| JPEG progressive, libjpeg-turbo (+DSP: not for progressive) | 18–21 s | | |

At 640 px wide the 3.2 Mpx JPEG does not fit TJpgDec in 4 MB (out of memory). JPEG 2000 (15 pictures in
two documents) and JBIG2 (none in the corpus) are not implemented. Table build for CCITT: 13 ms.

## Results: FreeType, a page's text drawn with its own fonts

FreeType 2.13.3 (`third_party/freetype`, GPLv2 option), Falcon-4, glyph cache kept between the pages of a
document, 72 dpi, 8-bit grey, `tests/hatari/fttest.py`. A glyph is made on first use (load, render, keep),
drawn from the cache afterwards with a plain C blit.

| document (fonts) | page 1 cold | later pages | warm (all glyphs cached) | made glyph |
|---|---|---|---|---|
| novel (CFF) | 4.6 s (65 glyphs, 58 ms each) | 0.9–2.4 s | 0.8–1.0 s | 58 ms (30 ms, 68030 build) |
| RFC 9110 (TrueType, no hinting) | 3.2 s | 1.7–1.9 s | 1.2–1.4 s | 12 ms |
| RFC 9110 (TrueType, bytecode hinting) | 5.5 s | 2.9–4.0 s | 1.3 s | 28 ms (hinting = 2.3 x) |
| RFC 2616 / Compendium (Type 1) | 3.0–3.2 s | 1.2–2.6 s | 0.6–2.0 s | 11–13 ms |
| NASA magazine (Type 1/TT) | 3.6 s | 2.1 s | 1.7–2.1 s | 10 ms |
| 68030 build, novel | 2.7 s | 0.5–1.4 s | 0.4–0.5 s | |
| Falcon 32 MHz + TT RAM, novel | 1.2 s | 0.3–0.7 s | 0.2–0.3 s | |
| ST, TrueType hinted, 1 bit | 17.4 s | 8.5–12 s | 3.2–3.5 s | 107 ms |
| ST, CFF hinted, 1 bit | 14.2 s | 3.2 s | 2.3–2.7 s | 184 ms |

- The *warm* column is the blit of 1500–4000 glyphs: **0.5 ms a glyph** (a byte loop with clipping).
  A tuned blit should be 3–5 x quicker; it is the largest line after the first page.
- Light auto-hinting and no hinting cost the same for CFF/Type 1 (their own hinter runs either way).
- 1–2 % of the glyphs of the RFC's TrueType fonts have no character in their cmap (24–54 of 2654).
- Memory: 0.4–0.7 MB for faces and cache (128 KB of glyphs); page buffer 213 KB (1 byte a pixel) to
  27 KB (1 bit). FreeType is 340 KB of code on the 68000.

## What failed, and why

1. **Not implemented**: JPEG 2000 (JPX), JBIG2, Type 3 glyph procedures, CID-keyed CFF glyph mapping (code = glyph
   assumed), rotated text and columns (the magazine's words F1 is 0.82), non-embedded symbolic fonts.
2. **libgcc is 68020 code.** `__clzsi2` (FreeType) faults on a 68000 with an illegal instruction; the 68030
   builds saved FPU registers and died with a line-F on a Falcon-4 (no FPU). Fixed here (`atari/clz.c`,
   `-msoft-float`); worth telling the hub, since every app's 68030 code can meet them.
3. **AES-256 (revision 6) opens in 40 s on Falcon-4, 165 s on an ST**: its password check is 64+ rounds of
   AES and SHA-256/384/512 (64-bit arithmetic) twice; the key alone would take half. RC4 and AES-128 files
   open in 0.2–0.6 s, and cost 5–15 % more per page.
4. **Progressive JPEG** comes out at 1/8 size from TJpgDec's builds (block averages) and takes 24 s.
5. **Bugs the run found in the reader** (fixed, kept in the commit history): the cross-reference table
   grew an entry at a time (10 times slower opens), a 64-operand limit cut long `TJ` arrays (RFC pages
   at F1 0.68), the page tree's `/Kids` was read again for every page (100 ms a page on a 500-page
   file), strings in operands moved when a buffer grew, a missing-operand crash and an
   alignment/overflow class found by AddressSanitizer and a 250-round mutation fuzz (clean).

## Where the time goes (Hatari's cycle profile, `tests/hatari/profile.py`)

- **Flate**: common's inflate is ~145 cycles an output byte, **~110 KB/s of output on Falcon-4**, ~55 KB/s
  on an ST. A map page of 2.4 MB of content: 22 s of its 30 s. Normal text pages: 10–16 KB, 0.1–0.15 s.
- Object parsing (about 1500 cycles per object, 3–4 µs a byte on a 68000 at 16 MHz): the xref table of a
  330-page novel is 0.19 s; a flat 517-page tree 0.45 s to open.
- The content interpreter: ~1 ms for 4 glyphs; fonts: first use of a CFF font 110 ms; ToUnicode CMaps
  are cheap.

## Estimates

**Stage 1, the text view (every machine).** Falcon-4: first text on screen 1–3 s after the file is
chosen, 0.6–1.1 s a page after that (as measured, untuned); ST 2–3.4 s a page. Known cheap wins (a
streaming tuned inflate, lexer shortcuts for `/Kids` and `/Widths`, skipping path operands) should
bring it to about **0.4 s (Falcon-4) and 1.2 s (ST)**. Reflow, search and copy need the whole text:
extract in `bgwork` slices into a cache file beside the document (330 pages: 2–5 min on a Falcon-4,
8–15 min on an ST; the reader sees pages as they arrive). Memory: typical 90–420 KB heap, 180 KB of code
with the crypto; fits an ST-1 if streams are decoded in pieces. Bidi and the Hebrew keyboard exist in common.
Scans without a text layer have no text: they show as pictures (below). Size: three to four weeks of
sessions, including the family's dialogs, windows, help and rigs.

**Stage 2, page rendering (Falcon, TT).** A page of text with its own fonts: parse 1 s + glyph making
(first page 1–4.6 s, later pages few) + blit 0.5–1 s: **2–6 s a page on Falcon-4**, ~1–2 s on a 32 MHz
Falcon; on an ST 8–17 s. Scanned pages: CCITT text scans 6 s, dense ones 20–36 s; JPEG scans 4 s with
the DSP (libjpeg-turbo, as EWeb), 60 s without. Vector pages (maps, charts) cost 25–40 s *to read*,
before anything is drawn. So rendering on the stock Falcon is workable for text pages, only with the
page shown as it is made (text first, pictures after, Esc to stop) for everything else; the 50 MHz
DFB1 brings the text pages near one second. Needs: a path/clip filler with the VDI, a blit tuned to
~0.15 ms a glyph, the cache of drawn pages, CCITT (done, tested) and JPX (not). Six to eight weeks of sessions.

## Recommendation

1. **Build stage 1 now**, on every machine, as the one 68000 PRG.
2. **Stage 2 only for Falcon/TT**, with the progress-and-stop behaviour above; **Erez to decide** whether
   a stock Falcon's 2–6 s a text page and 20–60 s scans/maps are acceptable, or whether full pages are for
   accelerated machines only (the text view stays for the rest).
3. **Ask the hub** about: a faster inflate in `common/` (with a 68000 assembler core), SHA-512 on the 68000
   for the AES-256 password check (or the DSP), a `__clzsi2` and `-msoft-float` note in the guidelines, and
   the DSP JPEG path in `common/` for a PRG that has no libjpeg-turbo yet.

## Commands

```sh
make                                     # host tools, the Atari programs
python3 tools/fetch-corpus.py            # the corpus into build/pdfs
python3 tools/measure.py host sample:8   # this computer
python3 tools/measure.py atari Falcon-4:16 68000 sample:8 -j 3    # also ST:mono, ST-1:mono, "Falcon:16 68030f"
python3 tools/measure.py text sample:8   # the words against pdftotext's
python3 tools/ftmatrix.py 2              # FreeType runs
python3 tests/hatari/imgbench.py Falcon-4 68030dsp 340            # pictures (needs tools/imgdump output in build/img)
python3 tests/hatari/profile.py Falcon-4 68000 novel-twocities=8  # cycle profile
python3 tools/report.py                  # these tables
```
The hub can rerun on Erez's TOS with the same commands (the images in `~/.aera/tos` are picked up by
`machines.py`); for drawing and blitting a VDI rig is still to be written.

## Text against pdftotext

| document | pages compared | words (ours / pdftotext) | F1 of the words |
|---|---|---|---|
| novel-twocities | 8 | 3040 / 2969 | 0.976 |
| novel-boymechanic | 8 | 3433 / 3433 | 1.000 |
| manual-compendium | 8 | 1441 / 1448 | 0.978 |
| manual-stinternals-text | 7 | 1331 / 1342 | 0.988 |
| scan-stinternals | 0 | 0 / 0 | (no text) |
| scan-gfabasic | 7 | 1156 / 1162 | 0.990 |
| tech-rfc9110 | 8 | 2796 / 2798 | 0.991 |
| tech-rfc2616 | 8 | 3994 / 3990 | 0.998 |
| datasheet-mc6850 | 0 | 0 / 0 | (no text) |
| datasheet-ym2151 | 8 | 56 / 56 | 1.000 |
| magazine-nasa-ask | 8 | 1560 / 1944 | 0.817 |
| photo-nasa-sunda | 1 | 224 / 238 | 0.844 |
| photo-shuttle | 8 | 1597 / 1596 | 0.998 |
| map-europe | 1 | 427 / 422 | 0.909 |
| map-physical-world | 1 | 2256 / 1976 | 0.869 |
| map-timezones | 1 | 1176 / 1033 | 0.874 |
| hebrew-idf-guide | 3 | 1070 / 1075 | 0.987 |
| hebrew-hava | 2 | 183 / 183 | 1.000 |
| scan-patent-ccitt | 4 | 3191 / 3190 | 1.000 |
| gen-hebrew | 8 | 2781 / 2781 | 1.000 |
| gen-charts | 8 | 228 / 228 | 1.000 |
| enc-rc4-40 | 8 | 3040 / 2969 | 0.976 |
| enc-rc4-128 | 8 | 3040 / 2969 | 0.976 |
| enc-aes-128 | 8 | 3040 / 2969 | 0.976 |
| enc-aes-256 | 8 | 3040 / 2969 | 0.976 |
