# The converted PDF (EConvert's files), version 1

A converted PDF is what EConvert writes: a PDF prepared for the Atari. It is still a valid PDF that any
viewer opens. It also carries each page in the form EPDF shows it, so EPDF reads it with almost no work:
no content to interpret, no fonts, and no JPEG, JPEG 2000 or JBIG2 to decode on a 68000.

EPDF reads a standard PDF as before. It reads a converted one from its prepared parts and shows the green
"Optimised" badge at the right end of each page's heading line (black in mono).

## How a converted file is recognised

The document catalog has an entry `/AERA`, a dictionary:

| Key | Value |
|---|---|
| `/Type` | `/AERAPrepared` |
| `/Version` | `1`. A reader that doesn't know the version reads the file as a standard PDF. |
| `/Creator` | a string: the program and its version, `(EConvert 1.1)` |
| `/Prepared` | an array of names, what every page carries: `/Text`, `/Pictures`, `/Drawings`, and `/PageMono` and `/PageColour` when the batch made them (colour only: `/PageColour` alone, and a page without colour still has its `/Mono`) |
| `/Photos` | `/Indexed` (256 colours, the default) or `/JPEG`: how the photos are stored (EConvert's per-batch setting) |
| `/Source` | a dictionary on the original: `/Name` (its file name), `/Bytes` (its size), `/Pages` (its page count) |
| `/Translate` | only in a file translated by the Gateway (below): a dictionary, `/To` the language the text was translated into, an ISO 639-1 name (`/en`, `/he`, ...) |

The document information dictionary's `/Producer` is `(EConvert 1.1 \(AERA\): prepared for the Atari)`. Its `/Title`, `/Author`, `/Subject`, `/Keywords`, `/Creator`, `/CreationDate` and `/ModDate` are the original's, each as it was (written as a hex string).

The page tree has the same pages, in the same order and at the same sizes (`/MediaBox`, `/Rotate`), as the
original.

## A page

Each page dictionary has an entry `/AERA`, a dictionary:

| Key | Value |
|---|---|
| `/Text` | a stream (FlateDecode): the page's text as EPDF stores it (below) |
| `/Mono` | when the batch made it (with colour only: on a page without colour): the page whole as a picture, 1 bit a pixel, as wide as EPDF's text column in a window the size of the converting machine's desktop (640 before EConvert 1.3) |
| `/Colour` | when the batch made it: the page whole in 16 colours, 4 bits a pixel, as wide as `/Mono` |
| `/Double` | where the converting machine had the memory: `/Mono` at twice its width, shown when the page is clicked in View > Pages |
| `/Translation` | in a translated file (the catalog's `/AERA /Translate`): a stream (FlateDecode), the page's text translated into `/To` (below) |
| `/From` | in a translated file: the language the page's text was in, a name as `/To`'s; `/Same` when it was in `/To` already (then the page has no `/Translation`, and `/Text` is shown) |

A page's pictures are kept at their own pixels up to the converting machine's window width (a drawing at what the page drawn gives it); their lines in `/Text` keep their size on the page, so EPDF shows them as the page does and whole when clicked.

### The text

The bytes are in the Atari character set, exactly as EPDF's text store (`epdf.txc`) keeps a page:

- the text in reading order, columns read one after the other;
- a line that goes on in the next (a paragraph's soft break) ends in CR (13), and any other line ends in LF (10);
- Tab (9), CR and LF are the only control characters, apart from a picture's line;
- a picture's line is `\x7F<object> <w>x<h>` and LF. `<object>` is the object number of an image XObject in
  this file, and `<w>` and `<h>` are its size on the page in points (whole numbers). A picture is stored as
  the page shows it (turned or mirrored already), so the line never carries `v` or `h`.

A drawing (a chart, a diagram, a map) is one picture. Its labels are in the picture, not in the text.

### The translation (Erez, 8 Nov 2026)

When the Atari asked the Gateway for a translation (PI-GATEWAY.md's `translate`), each page keeps its
`/Text` as it is and carries its translation beside it, so EPDF shows either without converting again:

- the same character set and line rules as `/Text`, in the language of `/To`;
- each paragraph of `/Text` (lines joined by soft breaks, up to a line ending in LF) is a paragraph of
  the translation, in the same order: one line ending in LF (EPDF wraps it to the window), or, for a
  paragraph whose lines were all short (a list, an address), its lines as they came back, each ending
  in LF;
- a picture's line, an empty line and a Tab-separated table line stay where they are, as they were:
  they are not sent for translation;
- Hebrew is in logical order, as in `/Text`.

EPDF shows the translation in the text view when View > Translate is ticked and the target language
in Settings is `/To`; the page's head says where it came from ("Page 3 · from Hebrew"). The page
pictures (`/Mono`, `/Colour`, `/Double`) are never translated.

### Pictures

Each picture is an image XObject stored at the size EPDF shows it: a point a pixel, at most 640 pixels
wide, the height in proportion.

| The original | Stored as |
|---|---|
| a photo, or any picture in colour | `/Photos /Indexed`: `/Indexed /DeviceRGB 255` and its own palette, 8 bits a pixel, FlateDecode. `/Photos /JPEG`: a baseline JPEG (`/DCTDecode`, `/DeviceRGB`) |
| a picture in grey | `/DeviceGray`, 8 bits a pixel, FlateDecode (JPEG with `/Photos /JPEG`) |
| a scan in black and white (CCITT, JBIG2, 1 bit) | `/DeviceGray`, 1 bit a pixel, FlateDecode |
| a drawing | `/Indexed /DeviceRGB 15` (16 colours, 4 bits a pixel) or `/DeviceGray` 1 bit when it has no colour, FlateDecode |

### Faster display (EPDF 2.40; Erez, 17 Nov 2026)

With Faster display (EConvert's Settings, the default; the Gateway when EPDF or EConvert asks it with
its screen's pens: PI-GATEWAY.md's `pens`), a picture in colour or grey also has a copy for one screen: `/AERAShown` in its dictionary refers
to an image XObject the size EPDF's text view shows the picture at on that screen (its width in points up
to EPDF's text column in a window the size of the desktop, its height by the screen's pixel shape: a
point a pixel on a square-pixel screen), dithered once in that screen's pens. The copy is
`/Indexed /DeviceRGB` with as many entries as the screen has pens, colour *i* register *i*'s (the value
the screen's bitmap holds for that pen), at 1, 2, 4 or 8 bits a pixel (2, 4, 16 or 256 pens), FlateDecode.
A picture shown at two sizes is stored twice, each with its copy. A black-and-white picture has none (EPDF
copies its bits already).

EPDF uses the copy when it is the size it shows the picture at and its colours are this screen's pens; it
copies its pixels as they are. On any other screen, or in a window narrower than the picture, it draws the
picture itself as before. Another viewer ignores the key.

### Whole pages

`/Mono` is `/DeviceGray`, 1 bit a pixel (0 black), dithered, FlateDecode. `/Colour` is `/Indexed /DeviceRGB 15`,
4 bits a pixel, with the page's own 16 colours, FlateDecode. Both are 640 pixels wide. Their height follows
the page's shape (an A4 page 905 rows).

EPDF shows them with View > Pages, one page after another. The menu item is greyed out for a file without
them.

### The page's preview (EPDF 2.33; Erez, 15 Nov 2026)

When asked (EConvert's **Options > Settings... > General > Page previews**, `PREVIEWS=1` in its INF; the
Gateway's `thumbs=1`; econvert's `--thumbs`; off by default, Erez, 15 Nov 2026: it costs a 4 MB Falcon about
1.9 s a page), each page carries the PDF's own `/Thumb` (ISO 32000 12.3.4), whatever the whole-page setting: an image
XObject 96 pixels wide, its height the page's shape (an A4 page 136 rows), `/Indexed /DeviceRGB 15` with TOS's
16 colours, 4 bits a pixel, FlateDecode, about half a kilobyte. It is the page as the converter drew it, made 96
wide (each pixel the mean of those under it). A page the converter held in bands (a machine short of memory) has
one only when a whole page is written too; a reader falls back to its own view of the page. EPDF's page strip (View > Page strip) shows it,
dithered on a mono screen. A file without them (from before 2.33, or converted with Page previews off) has
none: the strip shows a box with the page's first line.
EPDF shows any PDF's `/Thumb` so, converted or not.

### What other viewers see

The page's `/Contents` draws the prepared parts, so any viewer shows something sensible:

- with `/Colour`: that picture over the whole page; else with `/Mono`, that one;
- else: the text set in Helvetica (WinAnsiEncoding) from the top of the page, its size the largest from 10
  points down to 6 that fits the page, and each picture at its place in the text, scaled to the page's width
  when wider.

## What EPDF does with it

- It opens the file as any PDF, then finds the catalog's `/AERA` with `/Version 1`.
- For a page's text, it reads `/AERA /Text` into its store as it is, without interpreting the content.
- It draws pictures as it draws any image XObject. They are already the size it shows, so it doesn't scale
  them, and it decodes no JPEG unless the batch chose `/Photos /JPEG`. A picture's `/AERAShown` copy made
  for its screen (Faster display) it copies as it is, with no dither.
- It draws the badge on each page's heading line.
- With View > Pages it shows `/Colour` on a screen with 16 colours or more, else `/Mono`.
- With View > Translate ticked and the file's `/Translate /To` its target, it shows each page's
  `/Translation` in the text view (`/Text` where the page is `/Same` or has none).

## Versions

Version 1 is this document; `/Translate`, `/Translation` and `/From` were added to it on 8 Nov 2026
(a reader that doesn't know them shows `/Text`, as before). A later version adds keys. A reader of version 1 ignores keys it doesn't know,
and a file whose `/Version` it doesn't know is read as a standard PDF.
