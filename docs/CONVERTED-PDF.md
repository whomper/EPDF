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
| `/Prepared` | an array of names, what every page carries: `/Text`, `/Pictures`, `/Drawings`, and `/PageMono` and `/PageColour` when the batch made them |
| `/Photos` | `/Indexed` (256 colours, the default) or `/JPEG`: how the photos are stored (EConvert's per-batch setting) |
| `/Source` | a dictionary on the original: `/Name` (its file name), `/Bytes` (its size), `/Pages` (its page count) |

The document information dictionary's `/Producer` is `(EConvert 1.1 \(AERA\): prepared for the Atari)`.

The page tree has the same pages, in the same order and at the same sizes (`/MediaBox`, `/Rotate`), as the
original.

## A page

Each page dictionary has an entry `/AERA`, a dictionary:

| Key | Value |
|---|---|
| `/Text` | a stream (FlateDecode): the page's text as EPDF stores it (below) |
| `/Mono` | when the batch made it: the page whole as a picture, 640 pixels wide, 1 bit a pixel |
| `/Colour` | when the batch made it: the page whole in 16 colours, 640 pixels wide, 4 bits a pixel |

### The text

The bytes are in the Atari character set, exactly as EPDF's text store (`epdf.txc`) keeps a page:

- the text in reading order, columns read one after the other;
- a line that goes on in the next (a paragraph's soft break) ends in CR (13), and any other line ends in LF (10);
- Tab (9), CR and LF are the only control characters, apart from a picture's line;
- a picture's line is `\x7F<object> <w>x<h>` and LF. `<object>` is the object number of an image XObject in
  this file, and `<w>` and `<h>` are its size on the page in points (whole numbers). A picture is stored as
  the page shows it (turned or mirrored already), so the line never carries `v` or `h`.

A drawing (a chart, a diagram, a map) is one picture. Its labels are in the picture, not in the text.

### Pictures

Each picture is an image XObject stored at the size EPDF shows it: a point a pixel, at most 640 pixels
wide, the height in proportion.

| The original | Stored as |
|---|---|
| a photo, or any picture in colour | `/Photos /Indexed`: `/Indexed /DeviceRGB 255` and its own palette, 8 bits a pixel, FlateDecode. `/Photos /JPEG`: a baseline JPEG (`/DCTDecode`, `/DeviceRGB`) |
| a picture in grey | `/DeviceGray`, 8 bits a pixel, FlateDecode (JPEG with `/Photos /JPEG`) |
| a scan in black and white (CCITT, JBIG2, 1 bit) | `/DeviceGray`, 1 bit a pixel, FlateDecode |
| a drawing | `/Indexed /DeviceRGB 15` (16 colours, 4 bits a pixel) or `/DeviceGray` 1 bit when it has no colour, FlateDecode |

### Whole pages

`/Mono` is `/DeviceGray`, 1 bit a pixel (0 black), dithered, FlateDecode. `/Colour` is `/Indexed /DeviceRGB 15`,
4 bits a pixel, with the page's own 16 colours, FlateDecode. Both are 640 pixels wide. Their height follows
the page's shape (an A4 page 905 rows).

EPDF shows them with View > Pages, one page after another. The menu item is greyed out for a file without
them.

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
  them, and it decodes no JPEG unless the batch chose `/Photos /JPEG`.
- It draws the badge on each page's heading line.
- With View > Pages it shows `/Colour` on a screen with 16 colours or more, else `/Mono`.

## Versions

Version 1 is this document. A later version adds keys. A reader of version 1 ignores keys it doesn't know,
and a file whose `/Version` it doesn't know is read as a standard PDF.
