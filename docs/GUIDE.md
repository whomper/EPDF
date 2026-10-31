# EPDF - user guide

EPDF shows PDF documents on the Atari ST, STE, TT and Falcon. Its text view gives you the text of
every page, one after another in one window, on any machine down to a 1 MB ST: the paragraphs
joined and wrapped to the window, or each line as it is on the page.

## Getting started

| File | What it is |
|---|---|
| `EPDF.PRG` | the program, for every ST, STE, TT and Falcon |
| `EPDF.HYP`, `EPDF.REF` | this guide, for EHypView or ST-Guide |
| `NOTICES.TXT` | the licences of the parts EPDF is made with |
| `icons/` | the desktop icons: `EPDF.RSC` for the program, `EPDFDOC.RSC` for PDF files (`EPDFNV.RSC` and `EPDFDNV.RSC` under NVDI with its own palette) |

- **Install:** copy `EPDF.PRG` into a folder of its own, such as `C:\EPDF\`. It keeps its
  settings (`EPDF.INF`), the text of the document open (`EPDF.TXC`, removed when you close it)
  and any `CRASH.LOG` beside itself.
- **This guide:** copy `EPDF.HYP` and `EPDF.REF` into `C:\GUIDES\`, or beside `EPDF.PRG`. **Help**
  in EPDF opens it in EHypView or ST-Guide.
- **Open a PDF:** **File > Open...** (^O), drop a PDF on EPDF's window or on `EPDF.PRG`, or
  double-click a PDF once EPDF is installed as the application for `PDF` files (on TOS 2, 3 and 4:
  **Options > Install application...** on the desktop).

**Needs:** any ST, STE, TT or Falcon with 1 MB or more; TOS 1.04 or newer, EmuTOS, MagiC or MiNT;
a screen 640 pixels wide or more. EPDF takes about 390 KB when it starts, and more as it reads: on a
1 MB ST, run it without accessories.

**Under TOS, put Atari's `FOLDR100.PRG` first in `AUTO`**, renamed for more room (`FOLDR400.PRG`).

## Windows and menus

EPDF has one window. Its title is the document's name; the line under it says which page is at the
top of the window and, while EPDF is still reading the document, how far it has got.

Every page follows the one before, under a line that names it ("Page 3"). EPDF reads the text a
page at a time while you read, starting with the page you are on; a page it hasn't read yet shows
"..." until it has. You can scroll, jump to a page or open another document at any time: reading
gives way to you at once.

- **File:** Open... (^O), Quit (^Q).
- **Edit:** Copy (^C) puts the text you selected on the clipboard (a big selection is copied a
  page at a time, with its progress under the title; Esc stops it); Select all (^A); Find... (^F)
  looks for a word or a phrase, whatever its case, across the lines it was set on; Find next (^G)
  and Find previous (⇧^G) look again. A search goes round the whole document, reading pages it
  hasn't read yet first, and says on the line under the title where it found the text.
- **View:** Font... (⇧^F): the system font in its two sizes, or a fixed-width GDOS font, kept in
  `EPDF.INF`. **The page's lines**: ticked, each line is as it is on the page; not ticked (as EPDF
  starts), the lines of a paragraph are joined and wrapped to the window, so the text reads at any
  window width.
- **Page:** the previous and the next page (^Left, ^Right), the first and the last (^Clr/Home,
  ⇧^Clr/Home).
- **Help:** this guide.

**Pictures** stand where they are on the page, as wide as the window lets them (a point of the
page a pixel at most), in the screen's colours. A frame stands in the picture's place until it is
decoded, which a big JPEG takes a while on an ST; the text goes on being read meanwhile. A
picture over most of a page that has text (a background, or the scan behind a text layer) is left
out: the text is what you read. Pictures EPDF can't show (JPEG 2000, JBIG2) keep their frame and
say so.

**Hebrew** shows in the Atari's own Hebrew letters, each line in the order it is read, and a line
that starts in Hebrew is set at the right. A letter the Atari's character set doesn't have shows as
"?".

**Selecting text:** drag over it with the mouse; ⇧-click takes the selection on to where you click; a
double-click takes a word; a click alone lets the selection go. The right mouse button, or Insert,
opens a menu of what you can do there.

Two columns side by side are read one after the other, the left one first.

The window can be moved, sized, sent to the back and iconified (where the AES can iconify); the
icon shows the page you are on.

## Keyboard

| Key | Does |
|---|---|
| Up, Down | a line up or down |
| ⇧Up, ⇧Down, Space, ⇧Space | a window's height up or down |
| Clr/Home, ⇧Clr/Home | the start, the end of the document |
| ^Left, ^Right | the previous, the next page |
| ^Clr/Home | the first page |
| ⇧^Clr/Home, ^End | the last page |
| ^O | open a document |
| ^C, ^A | copy the selection, select everything |
| ^F, ^G, ⇧^G | find, find next, find previous |
| ⇧^F | the font |
| Insert | the menu for the selection (as the right mouse button) |
| ^U, ^Q | quit (EPDF has one window, so ^U closes it and quits) |
| Help | this guide |

The Control keys work even when EPDF was busy as you pressed them.

## Settings

EPDF keeps its settings in `EPDF.INF` beside `EPDF.PRG`, and writes it as soon as you change one.
Each setting has a note above it in the file:

- `WINDOW=` the window's place and size, as you left it;
- `FOLDER=` the folder **File > Open...** shows first: the one you last opened a document from;
- `LINES=` 1 when **View > The page's lines** is ticked.

A line EPDF doesn't know is kept as it is.

## Troubleshooting

- **"EPDF can't read this file as a PDF."** The file isn't a PDF, or it is damaged beyond what
  EPDF can repair, or it is encrypted with a password you would have to type (EPDF opens only
  documents whose user password is empty).
- **A page shows no text, or letters that make no sense.** The page is a scanned picture with no
  text in it, or its fonts don't say which letters they draw. EPDF can't read such text.
- **Columns run into each other.** EPDF finds two columns side by side by the gap between them;
  three or more, or columns that don't line up, are read line by line across the page.
- **EPDF stopped with an alert about a crash.** What happened is in `CRASH.LOG` beside
  `EPDF.PRG`; EPDF offers to show it the next time it starts. Please send it with a bug report.
- **The machine halts with "OUT OF INTERNAL MEMORY".** Put `FOLDR400.PRG` first in `AUTO` (see
  [Getting started](#getting-started)).

## About

EPDF is made by AERA | Erez Yaary, one of the AERA programs for the Atari (ECal, EContact,
EMail, EFTP, EHypView, ClaudeST, EWeb, EUpdate), and is free software under the GNU General Public
License, version 2 or later. Its source and releases: github.com/whomper/EPDF.

The parts it is made with and their licences are listed in `NOTICES.TXT`. PDF is a trademark of
Adobe; EPDF is not made by or affiliated with Adobe.
