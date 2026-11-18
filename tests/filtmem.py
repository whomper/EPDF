#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The reader's LZW and RunLength decoders when their output can't grow
(2.39: RFC 2616's LZW content written past a refused growth overwrote a
1 MB ST's heap, 2.38's crash in tech-rfc2616 on ST-1):

    filtmem.py [WORK]

A page whose content stream is LZW-packed, and one RunLength-packed, are
read by build/pdftext_oom (ASan, with a realloc that refuses every request
over REALLOC_MAX bytes) under no cap, which must give the page's text
whole, and under caps from 256 bytes to 64 KB, which must give no ASan
report. Exit 1 if any does.
"""
import os
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(HERE)
LINES = 600


def lzw(data):
    """PDF's LZW (EarlyChange 1), literal codes only, a clear code every 250 so the
    codes stay 9 bits: a valid stream, every code adding the decoder's entry"""
    codes, n = [256], 0
    for b in data:
        if n == 250:
            codes.append(256)
            n = 0
        codes.append(b)
        n += 1
    codes.append(257)
    out, acc, have = bytearray(), 0, 0
    for c in codes:
        acc = acc << 9 | c
        have += 9
        while have >= 8:
            out.append(acc >> (have - 8) & 255)
            have -= 8
    if have:
        out.append(acc << (8 - have) & 255)
    return bytes(out)


def runlength(data):
    """runs of 3 or more alike as repeats (the decoder's sb_room path), the rest literal"""
    out, i = bytearray(), 0
    while i < len(data):
        j = i
        while j < len(data) and j - i < 128 and data[j] == data[i]:
            j += 1
        if j - i >= 3:
            out += bytes([257 - (j - i), data[i]])
            i = j
            continue
        k = i
        while k < len(data) and k - i < 128 and not (k + 2 < len(data) and data[k] == data[k + 1] == data[k + 2]):
            k += 1
        k = max(k, i + 1)
        out += bytes([k - i - 1]) + data[i:k]
        i = k
    return bytes(out + b"\x80")


def make_pdf(path, filt, enc):
    body = ["BT /F1 9 Tf 40 800 Td 10 TL"]
    for i in range(LINES):
        body.append("(Line %d          of the page, aaaaaaaa bbbbbbbb words.) '" % i)
    body.append("ET")
    content = enc("\n".join(body).encode())
    objs = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 4 0 R"
        b" /Resources << /Font << /F1 5 0 R >> >> >>",
        b"<< /Length %d /Filter /%s >>\nstream\n" % (len(content), filt) + content + b"\nendstream",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    ]
    out = bytearray(b"%PDF-1.4\n")
    offs = []
    for i, o in enumerate(objs):
        offs.append(len(out))
        out += b"%d 0 obj\n" % (i + 1) + o + b"\nendobj\n"
    x = len(out)
    out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)
    for o in offs:
        out += b"%010d 00000 n \n" % o
    out += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, x)
    open(path, "wb").write(out)


def main(argv):
    work = argv[0] if argv else os.path.join(TOP, "build", "filtmem")
    os.makedirs(work, exist_ok=True)
    tool = os.path.join(TOP, "build", "pdftext_oom")
    bad = []
    caps = [256, 300, 512, 700, 1024, 1500, 2048, 3000, 4096, 6000, 8192, 12000, 16384, 24000, 32768, 48000, 65536]
    for filt, enc in ((b"LZWDecode", lzw), (b"RunLengthDecode", runlength)):
        pdf = os.path.join(work, filt.decode() + ".pdf")
        make_pdf(pdf, filt, enc)
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
        p = subprocess.run([tool, pdf, "1"], env=env, capture_output=True, text=True)
        if p.returncode or "Line 0 " not in p.stdout or "Line %d " % (LINES - 1) not in p.stdout:
            bad.append("%s with no cap: the page's text not whole (exit %d)" % (filt.decode(), p.returncode))
        refused = 0
        for cap in caps:
            p = subprocess.run([tool, pdf, "1"], env=dict(env, REALLOC_MAX=str(cap)), capture_output=True, text=True)
            # (a cap too small for the document to open fails it in order: exit 1, no ASan report)
            if "AddressSanitizer" in p.stderr or "runtime error" in p.stderr:
                bad.append("%s under %d bytes: %s" % (filt.decode(), cap,
                           [l for l in p.stderr.split("\n") if "Sanitizer" in l or "runtime error" in l][0][:120]))
            if "Line %d " % (LINES - 1) not in p.stdout:
                refused += 1
        if not refused:
            bad.append("%s: no cap refused the decoder's output (the case proves nothing)" % filt.decode())
        print("filtmem: %s, %d of %d caps refused its output" % (filt.decode(), refused, len(caps)))
    for b in bad:
        print("FAIL " + b)
    if not bad:
        print("passed")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
