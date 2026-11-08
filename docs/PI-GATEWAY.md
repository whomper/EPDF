# Gateway mode: PDFs converted by the Gateway (pdfgate)

EConvert and EPDF can have a PDF converted by the Gateway (AERA-Gateway
on a Raspberry Pi) instead of by the Atari itself: the PDF goes to pdfgate, AERA-Gateway's `pdf`
service, and the converted PDF comes back. The converted file is the same,
byte for byte, as EConvert writes on a Falcon with the same settings
(`CONVERTED-PDF.md`): the Gateway runs `aerapdf`, which is EPDF's own converter
(`tools/econvert.c` and `src/conv/`) built for Linux. One difference: a
Falcon whose memory can't hold a page's double size (1280 pixels wide)
whole leaves that page's double picture out (a stock 4 MB one does); the
Gateway always has the room for it.

The Gateway's side lives in AERA-Gateway (`gateway/pdfgate.py`, `pdf/`), its
copy of the converter taken from this repository by its
`tools/sync-pdf.sh` at a named commit. The protocol is EPDF's: a change to
it is made here first, then in pdfgate. Everything the user sees says
"Gateway" (guideline 7.3, Erez 7 Nov); "Raspberry Pi" only for the
hardware and its install steps.

## The exchange

HTTP/1.1 over a plain TCP connection to the Gateway (port 8380 by default), one
PDF per connection. EConvert and EPDF send:

    POST /convert?pages=mono&photos=indexed&pictures=screen&page_width=640&picture_width=640&double=1&name=REPORT.PDF HTTP/1.1
    Host: PI_IP
    Content-Length: N

and then the PDF's N bytes, read from disk and sent in pieces. Every
option may be left out (the converter's own default then); they are
EConvert's settings:

| Option          | Values                      | econvert's flag      |
|-----------------|-----------------------------|----------------------|
| `pages`         | `none`, `mono`, `colour`    | `--pages`            |
| `photos`        | `indexed`, `jpeg`           | `--photos`           |
| `pictures`      | `page`, `screen`            | `--pictures`         |
| `page_width`    | 160 to 4096                 | `--page-width`       |
| `picture_width` | 64 to 4096                  | `--picture-width`    |
| `double`        | `1`, `0`                    | `--normal-pages` (0) |
| `name`          | the PDF's own name          | (kept in the result) |
| `translate`     | a language, ISO 639-1 (`en`, `he`, ...) | `--translate` (below) |

`translate` (Erez, 8 Nov 2026; EPDF with View > Translate ticked): the converted PDF also carries each
page's text translated into that language (CONVERTED-PDF.md, `/Translation`), translated on the
Gateway's own machine by its translation service, so the text crosses the network once. Without
`translate` the exchange is byte for byte as before. The 102 answers cover converting and translating
(the Atari says "The Gateway is converting and translating..."). The final 200 also carries
`X-Translate-From: CODE` (the language most of the pages were in) and, when every page was in that
language already, `X-Translate-Same: 1`. The translation service's refusals come back as pdfgate's:
422 `pdfgate: no translation from LANG into LANG` (a pair not installed: "Settings > Languages" on the
Atari), 503 `pdfgate: the translation service is busy`, 504 as before.

`name` is what the converted PDF's AERA entry records as the original
(`/Name`): letters, digits, `_`, `.`, `-` and spaces, at most 64, not
starting with a dot. Anything else in the query is refused.

While the Gateway converts, it answers with interim responses, at most two a
second:

    HTTP/1.1 102 Processing
    X-Progress: 3125/8000

(`DONE/TOTAL`, thousandths of the file's pages, as `CVOPT.progress` gives
them), and then one final response:

- `200`, `Content-Type: application/pdf`, `Content-Length`: the converted
  PDF, written to disk in pieces as it comes;
- or an error, `Content-Type: text/plain`, one line starting `pdfgate: `,
  which the Atari shows in its alert as it is:

| Status | When                                                  |
|--------|-------------------------------------------------------|
| 400    | an option or value it doesn't take                    |
| 403    | the Atari isn't on the home network, or not `--allow`ed |
| 411    | no `Content-Length`                                   |
| 413    | the PDF is bigger than the Gateway takes (64 MB)           |
| 415    | the body isn't a PDF                                  |
| 422    | the converter couldn't convert it (its reason)        |
| 503    | no converter on the Gateway                               |
| 504    | the conversion took too long (600 s)                  |

The connection closes after the final response.

`GET /version` answers `aerapdf COMMIT`: the EPDF commit the Gateway's
converter was built from.

### aerapdf and the translator (who does what)

aerapdf is EPDF's converter (`tools/econvert.c`): EPDF builds and tests it. The translator is the
Gateway's (AERA-Gateway's session builds it): a program `translateclient` on the Gateway that sends
text to translategate on the same machine. pdfgate runs

    aerapdf ... --translate CODE --translator PATH IN.pdf OUT.pdf

and aerapdf, for each page, after its text is made:

- runs `PATH --to CODE` with the page's paragraphs on its standard input: UTF-8, paragraphs separated
  by an empty line, a paragraph's soft breaks as spaces, the lines CONVERTED-PDF.md leaves out not sent
  (a page with nothing to send is not run);
- reads the translation from its standard output, the same paragraphs in the same order, separated
  by an empty line, and on its standard error first line `from CODE` (or `same CODE`);
- exit status 0 done; 2 the pair isn't installed, 3 busy, 4 too long: aerapdf stops and pdfgate
  answers 422, 503 or 504; any other: 422.

`--progress` goes on counting the pages' thousandths, translating included. The translator splits the
text into pieces of 2 KB for translategate, keeps nothing and logs no word of it (TRANSLATION-SERVICE.md).
A stand-in translator in EPDF's tests (`tests/pigate/`) lets aerapdf's side be tested without an engine.

## On the Atari

- Through common's `net_tos.c` (STinG, MiNTnet, MagiC-Net); the Gateway by its
  address (`PI_IP`), not its name.
- The PDF is read from disk and the result written to disk a piece at a
  time: nothing is held whole, so an ST with 1 MB can do it.
- Esc stops it; the window is answered between pieces, within the
  family's times.
- With Gateway mode off, nothing of this runs and nothing is sent.
- The log names the Gateway and the bytes, never a file's name or contents.

## On the Gateway (the Raspberry Pi)

`sudo aera-gateway install pdf --allow ATARI_IP` builds `aerapdf` (with
`cc` and `make`) and starts pdfgate. One conversion at a time, each with
600 s and 768 MB at most; the PDF and the result are on the Gateway's disk
only while it runs. The log says who converted how many bytes, never the
file's name or what is in it.
