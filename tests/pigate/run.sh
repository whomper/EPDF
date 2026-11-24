#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# pigate.c against the Gateway's protocol on this computer: the stand-in
# tests/pigate/fakegate.py with this build's converter (build/econvert); no
# other AERA program (guidelines 1.2, decided 8 Nov 2026). A PDF converted
# through it is the same, byte for byte, as build/econvert writes on its own;
# a stop half-way leaves no file; a refusal comes back in its words.
#
# By default against pdfgate.py itself, from EPDF's read-only gateway/ copy
# (tools/sync-gateway.sh; PDFGATE=FOLDER for another): EPDF tests every
# change to pdfgate (guidelines 1.2); PDFGATE=none, or no copy: the stand-in.
#
#   [PDFGATE=FOLDER|none] tests/pigate/run.sh [IN.pdf]   (a book of 6 pages, a photo on its second)
set -e
cd "$(dirname "$0")/../.."
fonts=${FONTS:-third_party/liberation/fonts}
w=$(mktemp -d)
trap 'kill $pid 2>/dev/null; rm -rf "$w"' EXIT
in=$1
if [ -z "$in" ]; then
	in=$w/BOOK.PDF
	python3 -c 'import sys; sys.path.insert(0, "tests/hatari"); from pdfgen import make_pdf, book, jpeg; make_pdf(sys.argv[1], book(6), pictures={1: (jpeg(200, 150), 200, 150, 300, 225)})' "$in"
fi
make -s build/econvert
cc -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -fsanitize=address,undefined \
   -Isrc -Icommon/src -o "$w/test_pigate" tests/pigate/test_pigate.c src/pigate.c \
   common/src/conn.c common/src/tls_none.c common/src/util.c src/pdfio_host.c src/conv/cvio_host.c
port=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')
PDFGATE=${PDFGATE:-gateway}
[ "$PDFGATE" != none ] && [ -f "$PDFGATE/pdfgate.py" ] || PDFGATE=
if [ -n "$PDFGATE" ]; then
	echo "aerapdf test" > "$w/SOURCE"
	echo "the Gateway: pdfgate.py from $PDFGATE"
	python3 "$PDFGATE/pdfgate.py" --bind 127.0.0.1 --port "$port" --aerapdf build/econvert --fonts "$fonts" \
		--source "$w/SOURCE" --tmp "$w" 2> "$w/gate.log" &
else
	echo "the Gateway: EPDF's stand-in, tests/pigate/fakegate.py"
	python3 tests/pigate/fakegate.py --port "$port" --aerapdf build/econvert --fonts "$fonts" --log "$w/gate.log" &
fi
pid=$!
sleep 1
"$w/test_pigate" "$port" "$in" "$w/pi.pdf"
build/econvert --fonts "$fonts" --pages mono --pictures screen --page-width 640 --picture-width 640 \
	"$in" "$w/direct.pdf" > /dev/null
cmp "$w/pi.pdf" "$w/direct.pdf" && echo "ok   the Pi's PDF is the direct one, byte for byte"
# Faster display: 16 pens, the column 600 (test_pigate.c's PENS); a Gateway
# that has pens= converts with them, as build/econvert does; one without
# refuses them and is asked again without (the PDF as before)
PENS=1 "$w/test_pigate" "$port" "$in" "$w/pens.pdf"
if { [ -z "$PDFGATE" ] || grep -q '"pens"' "$PDFGATE/pdfgate.py"; }; then
	hex=$(python3 -c 'print("".join("%02x%02x%02x" % (i * 17, 255 if i & 1 else 0, 255 - i * 16) for i in range(16)))')
	build/econvert --fonts "$fonts" --pages mono --pictures screen --page-width 640 --picture-width 640 \
		--pens-hex "$hex" --column 600 --aspect 16 "$in" "$w/pdirect.pdf" > /dev/null
	grep -q /AERAShown "$w/pdirect.pdf" && cmp "$w/pens.pdf" "$w/pdirect.pdf" && echo "ok   pens=: the Pi's PDF is the direct one with --pens-hex"
else
	cmp "$w/pens.pdf" "$w/direct.pdf" && echo "ok   pens= refused by this pdfgate: asked again without, the PDF as before"
fi
# the original's path (EConvert's Gateway mode): the Pi's PDF as it came,
# then an update whose catalog's /Source has /Path, read so by EPDF's reader
# and by qpdf
SOURCE_PATH='C:\DOCS\MY (1).PDF' "$w/test_pigate" "$port" "$in" "$w/path.pdf"
make -s build/pdftext
if python3 -c 'import sys; a = open(sys.argv[1], "rb").read(); b = open(sys.argv[2], "rb").read(); sys.exit(not (len(b) > len(a) and b.startswith(a)))' "$w/pi.pdf" "$w/path.pdf" &&
   PDFTEXT_SOURCE=1 build/pdftext "$w/path.pdf" 1 2>&1 >/dev/null | grep -qF 'path "C:\DOCS\MY (1).PDF" bytes '"$(stat -c %s "$in")" &&
   { ! command -v qpdf > /dev/null || qpdf --check "$w/path.pdf" > /dev/null; }; then
	echo "ok   the original's path: added at the copy's end, read back"
else
	echo "FAIL the original's path"; PDFTEXT_SOURCE=1 build/pdftext "$w/path.pdf" 1 2>&1 >/dev/null | head -3
fi
"$w/test_pigate" "$port" "$in" "$w/stopped.pdf" 30 && echo "ok   a stop at 30%: nothing left"
printf 'GIF89a not a pdf' > "$w/x.pdf"
"$w/test_pigate" "$port" "$w/x.pdf" "$w/y.pdf" | grep -q 'err "pdfgate: not a PDF"' && \
	echo "ok   a refusal in pdfgate's words"
! grep -q "$(basename "$in")" "$w/gate.log" && echo "ok   the Pi's log names no file"
# translate=CODE (docs/PI-GATEWAY.md): pdfgate.py, translateclient.py and
# translategate.py's stand-in engine from gateway/, with this build's aerapdf
if [ -n "$PDFGATE" ] && [ -f "$PDFGATE/translateclient.py" ]; then
	python3 tests/pigate/translate_gate.py "$PDFGATE" "$in"
else
	echo "SKIPPED: translate=CODE (no pdfgate.py with translateclient.py to run)"
fi
