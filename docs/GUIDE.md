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
| `icons/` | the desktop icons: `EPDF.RSC` for the program, `EPDFDOC.RSC` for PDF files, `ECONVERT.RSC` for EConvert (`EPDFNV.RSC`, `EPDFDNV.RSC` and `ECONVNV.RSC` under NVDI with its own palette) |
| `econvert/` | EConvert, which prepares PDFs for EPDF on a Falcon or a TT: `ECONVERT.PRG` and the fonts it draws with (`fonts/`); see [EConvert](#econvert) |

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

EPDF has one window. Its title is the document's name.

**The status bar** at the foot of the window says which page is at the top ("Viewing page 3 / 8"),
then what EPDF is doing, if it is working on something you wait for: how far it has got and about
how long is left, "Reading page 5 of 8 · 30% · 2 min left" while it reads the document's text (a
slow page, such as a map on an ST, counts too), "Picture on page 1" while a picture is made,
"Searching page 5 of 8", "Copying page 2 of 12", "Converting on the Gateway". A thin line along its foot fills from the left as
the work goes on. What a search found, or that a copy is done, is said there too.

Every page follows the one before, under a line that names it ("Page 3"). EPDF reads the text a
page at a time while you read, starting with the page you are on; a page it hasn't read yet shows
"..." until it has. You can scroll, jump to a page or open another document at any time: reading
gives way to you at once.

- **File:** Open... (^O), Save Optimised... (a converted PDF written where you choose, as it is:
  nothing is converted again; greyed for a PDF that isn't converted), Open on the Gateway
  (ticked: see [The Gateway](#the-gateway)), Settings... (^,), Quit (^Q).
- **Edit:** Copy (^C) puts the text you selected on the clipboard (a big selection is copied a
  page at a time, with its progress in the status bar; Esc stops it); Select all (^A); Find... (^F)
  looks for a word or a phrase, whatever its case, across the lines it was set on; Find next (^G)
  and Find previous (⇧^G) look again. A search goes round the whole document, reading pages it
  hasn't read yet first, and says in the status bar where it found the text.
- **View:** Font... (⇧^F): the system font in its two sizes, or a fixed-width GDOS font, kept in
  `EPDF.INF`. **The page's lines**: ticked, each line is as it is on the page; not ticked (as EPDF
  starts), the lines of a paragraph are joined and wrapped to the window, so the text reads at any
  window width. **Pages**: each page whole, as it is printed: on a Falcon or a TT any PDF (see
  below), on an ST a converted PDF's; greyed where there are none. **Zoom in** (the keypad's +)
  and **Zoom out** (the keypad's -) show the page in view bigger: 150, 200, 300 and 400 %, as far
  as the memory holds; a click on a page goes to 200 % and a click back. On a Falcon or a TT the
  bigger page is drawn by the Atari, in colour (a converted PDF's from its original where EPDF
  knows it, the Gateway's). On an ST, it's EConvert's double page in black and white (200 %).
  A zoomed page is moved by dragging it with the mouse, or with the arrow keys; Esc gives the
  pages back.
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

**Converted PDFs** are PDFs EConvert prepared for the Atari (EConvert comes with EPDF; it runs on
a Falcon or a TT, and the files it makes read on every Atari). EPDF reads them at once, with no
fonts to read and no JPEG to decode on a 68000, and each page's line says **Optimised** at its
right. Their charts and maps are pictures, with their labels in them. When EConvert was asked
for the pages whole, **View > Pages** shows each page as a picture, in 16 colours on a screen with
them, else in black and white; choose it again for the text.

**Pages as printed** (a Falcon or a TT): View > Pages shows any PDF's pages whole, with their
fonts, pictures and drawings where the page has them, drawn by the Atari itself. A page is drawn
when it comes into view (the status bar says "Drawing page 3 / 9").
EPDF needs `RENDER.OVL` beside `EPDF.PRG` and the standard fonts in a `FONTS` folder beside it
(or EConvert's, in `ECONVERT\FONTS`), as the release's zip has them.

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
| keypad +, keypad - | zoom in, zoom out (View > Pages) |
| ^, | Settings (the Gateway) |
| Insert | the menu for the selection (as the right mouse button) |
| ^U, ^Q | quit (EPDF has one window, so ^U closes it and quits) |
| Help | this guide |

The Control keys work even when EPDF was busy as you pressed them.

## Settings

EPDF keeps its settings in `EPDF.INF` beside `EPDF.PRG`, and writes it as soon as you change one.
Each setting has a note above it in the file:

- `WINDOW=` the window's place and size, as you left it;
- `FOLDER=` the folder **File > Open...** shows first: the one you last opened a document from;
- `LINES=` 1 when **View > The page's lines** is ticked;
- `GATEWAY=` 1 when **File > Open on the Gateway** is ticked;
- `GATE=` and `GATEPORT=` the Gateway's address and port (**File > Settings...**).

A line EPDF doesn't know is kept as it is.

## The Gateway

The Gateway (AERA-Gateway, on a Raspberry Pi at home) can convert PDFs for EPDF much faster than
an Atari can, and for every Atari, an ST included. On the Raspberry Pi, `sudo aera-gateway install
pdf --allow ATARI_IP` sets it up. On the Atari you need STinG, MiNTnet or MagiC-Net.

- **File > Settings...** (^,): **Through the Gateway:** its address and port (8380).
- **File > Open on the Gateway:** ticked, a PDF you open that isn't converted yet is shown as it
  is, while the Gateway converts it; the status bar says "Converting on the Gateway" (or "Waiting
  for the Gateway", while it converts another Atari's), and when the converted PDF has come, EPDF
  shows it in its place, with View > Pages. Nothing is written beside the PDF: the converted PDF
  is kept only while it is open (beside `EPDF.PRG`); **File > Save Optimised...** keeps it where
  you choose. Esc stops it.

## EConvert

EConvert prepares PDFs for EPDF: each page's text already read, its pictures at the size EPDF
shows them, its charts and maps drawn as pictures, and, if you want them, its pages whole as
pictures (**View > Pages** in EPDF). The files it writes are still PDFs that any viewer opens, and
EPDF reads them quickly on every Atari. EConvert converts on a Falcon or a TT (a 68030); on any
Atari, an ST too, it can have the Gateway convert them instead.

- **Install:** copy the `econvert` folder (`ECONVERT.PRG` and its `fonts` folder) anywhere. It
  keeps its settings in `ECONVERT.INF` beside itself.
- **The batch:** **Add files...** (^O) adds a PDF; **Add folder...** adds every PDF in the folder
  you pick a file in; **Clear** empties the list. You can also drop PDFs on its window or on
  `ECONVERT.PRG`.
- **Write to...** chooses the folder the converted PDFs go to, with their own names: a folder of
  their own, not the PDFs' own.
- **Whole pages:** None, Mono (black and white, for every Atari), or Mono and colour (16 colours
  as well, for a Falcon or a TT; a page with little colour gets the black and white one only).
- **Photos:** 256 colours (fast on every Atari) or JPEG (smaller files, but slow to show on an
  8 MHz ST). Charts and pictures of few colours are kept exact either way.
- **Convert on:** This Atari, or the Gateway (its address and port in **File > Settings...**, ^,;
  until you set them there, EPDF's, from `EPDF.INF` in the folder above). On an ST only the Gateway. The Gateway's converted PDFs are the same as the Falcon's.
- **Convert** (Return, ^R) works through the batch. Two bars show the file and the whole batch,
  each with how far it has got and about how long is left. **Stop** (Esc) stops at once; the file
  under way is left out, the ones done stay.

A page takes some seconds on a Falcon, more with many pictures or a map. You can go on using the
Atari meanwhile: EConvert works in turns.

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
