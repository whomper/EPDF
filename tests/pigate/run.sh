#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# pigate.c against the Gateway's protocol on this computer: the stand-in
# tests/pigate/fakegate.py with this build's converter (build/econvert); no
# other AERA program (guidelines 1.2, Erez 8 Nov 2026). A PDF converted
# through it is the same, byte for byte, as build/econvert writes on its own;
# a stop half-way leaves no file; a refusal comes back in its words.
#
#   tests/pigate/run.sh [IN.pdf]   (a book of 6 pages)
set -e
cd "$(dirname "$0")/../.."
fonts=${FONTS:-third_party/liberation/fonts}
w=$(mktemp -d)
trap 'kill $pid 2>/dev/null; rm -rf "$w"' EXIT
in=$1
if [ -z "$in" ]; then
	in=$w/BOOK.PDF
	python3 -c 'import sys; sys.path.insert(0, "tests/hatari"); from pdfgen import make_pdf, book; make_pdf(sys.argv[1], book(6))' "$in"
fi
make -s build/econvert
cc -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -fsanitize=address,undefined \
   -Isrc -Icommon/src -o "$w/test_pigate" tests/pigate/test_pigate.c src/pigate.c \
   common/src/conn.c common/src/tls_none.c common/src/util.c src/pdfio_host.c src/conv/cvio_host.c
port=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')
python3 tests/pigate/fakegate.py --port "$port" --aerapdf build/econvert --fonts "$fonts" --log "$w/gate.log" &
pid=$!
sleep 1
"$w/test_pigate" "$port" "$in" "$w/pi.pdf"
build/econvert --fonts "$fonts" --pages mono --pictures screen --page-width 640 --picture-width 640 \
	"$in" "$w/direct.pdf" > /dev/null
cmp "$w/pi.pdf" "$w/direct.pdf" && echo "ok   the Pi's PDF is the direct one, byte for byte"
"$w/test_pigate" "$port" "$in" "$w/stopped.pdf" 30 && echo "ok   a stop at 30%: nothing left"
printf 'GIF89a not a pdf' > "$w/x.pdf"
"$w/test_pigate" "$port" "$w/x.pdf" "$w/y.pdf" | grep -q 'err "pdfgate: not a PDF"' && \
	echo "ok   a refusal in pdfgate's words"
! grep -q "$(basename "$in")" "$w/gate.log" && echo "ok   the Pi's log names no file"
