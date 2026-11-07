# The Pi converter (pdfgate)

EConvert and EPDF can have a PDF converted by the Raspberry Pi instead of
by the Atari itself: the PDF goes to pdfgate, AERA-Gateway's `pdf`
service, and the converted PDF comes back. The converted file is the same,
byte for byte, as EConvert writes on a Falcon with the same settings
(`CONVERTED-PDF.md`): the Pi runs `aerapdf`, which is EPDF's own converter
(`tools/econvert.c` and `src/conv/`) built for Linux.

The Pi's side lives in AERA-Gateway (`gateway/pdfgate.py`, `pdf/`), its
copy of the converter taken from this repository by its
`tools/sync-pdf.sh` at a named commit. The protocol is EPDF's: a change to
it is made here first, then in pdfgate.

## The exchange

HTTP/1.1 over a plain TCP connection to the Pi (port 8380 by default), one
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

`name` is what the converted PDF's AERA entry records as the original
(`/Name`): letters, digits, `_`, `.`, `-` and spaces, at most 64, not
starting with a dot. Anything else in the query is refused.

While the Pi converts, it answers with interim responses, at most two a
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
| 413    | the PDF is bigger than the Pi takes (64 MB)           |
| 415    | the body isn't a PDF                                  |
| 422    | the converter couldn't convert it (its reason)        |
| 503    | no converter on the Pi                                |
| 504    | the conversion took too long (600 s)                  |

The connection closes after the final response.

`GET /version` answers `aerapdf COMMIT`: the EPDF commit the Pi's
converter was built from.

## On the Atari

- Through common's `net_tos.c` (STinG, MiNTnet, MagiC-Net); the Pi by its
  address (`PI_IP`), not its name.
- The PDF is read from disk and the result written to disk a piece at a
  time: nothing is held whole, so an ST with 1 MB can do it.
- Esc stops it; the window is answered between pieces, within the
  family's times.
- With the Pi converter off, nothing of this runs and nothing is sent.
- The log names the Pi and the bytes, never a file's name or contents.

## On the Pi

`sudo aera-gateway install pdf --allow ATARI_IP` builds `aerapdf` (with
`cc` and `make`) and starts pdfgate. One conversion at a time, each with
600 s and 768 MB at most; the PDF and the result are on the Pi's disk
only while it runs. The log says who converted how many bytes, never the
file's name or what is in it.
