#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A stand-in for the Gateway's PDF service, for EPDF's own tests: EPDF's
protocol (docs/PI-GATEWAY.md) answered on this computer with this build's
converter, build/econvert. No AERA program but EPDF's own is used (Erez,
8 Nov 2026, guidelines 1.2: an app tests itself alone); AERA-Gateway's
pdfgate is tested in its own repository.

    fakegate.py --port N [--bind 127.0.0.1] --aerapdf build/econvert --fonts DIR [--log FILE]

POST /convert?OPTIONS with a PDF: interim "102 Processing" answers with
X-Progress DONE/TOTAL as the converter reports them, then 200 and the
converted PDF, or one line "pdfgate: ..." with 400, 411, 413, 415 or 422.
GET /version: "aerapdf test". The log has each exchange's status and bytes,
never a file's name.
"""
import argparse
import http.server
import os
import re
import subprocess
import sys
import tempfile
import urllib.parse

OPTS = {
    "pages": ("--pages", ("none", "mono", "colour")),
    "photos": ("--photos", ("indexed", "jpeg")),
    "pictures": ("--pictures", ("page", "screen")),
    "page_width": ("--page-width", None),
    "picture_width": ("--picture-width", None),
    "double": (None, ("0", "1")),
    "name": (None, None),
}
MOST = 64 << 20


class Gate(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass

    def note(self, status, n_in, n_out):
        with open(self.server.log, "a") as f:
            f.write("%s %d in %d out %d\n" % (self.client_address[0], status, n_in, n_out))

    def refuse(self, status, text, n_in=0):
        body = ("pdfgate: %s\n" % text).encode()
        self.send_response(status)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.note(status, n_in, 0)

    def do_GET(self):
        if self.path != "/version":
            return self.refuse(400, "no such request")
        body = b"aerapdf test\n"
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        url = urllib.parse.urlsplit(self.path)
        if url.path != "/convert":
            return self.refuse(400, "no such request")
        args, name = [], "in.pdf"
        for k, v in urllib.parse.parse_qsl(url.query, keep_blank_values=True):
            if k not in OPTS:
                return self.refuse(400, "no option %s" % k)
            flag, ok = OPTS[k]
            if k in ("page_width", "picture_width"):
                if not v.isdigit() or not (64 <= int(v) <= 4096):
                    return self.refuse(400, "%s out of range" % k)
            elif k == "name":
                if not re.fullmatch(r"[A-Za-z0-9_. -]{1,64}", v) or v.startswith("."):
                    return self.refuse(400, "a name it doesn't take")
                name = v
                continue
            elif v not in ok:
                return self.refuse(400, "%s=%s" % (k, v))
            if k == "double":
                if v == "0":
                    args.append("--normal-pages")
                continue
            args += [flag, v]
        if "Content-Length" not in self.headers:
            return self.refuse(411, "no Content-Length")
        size = int(self.headers["Content-Length"])
        if size > MOST:
            return self.refuse(413, "the PDF is too big")
        with tempfile.TemporaryDirectory() as work:
            # the PDF under its own name (the result's AERA entry names it),
            # the result in a folder of its own (docs/PI-GATEWAY.md's name)
            os.makedirs(os.path.join(work, "in"))
            os.makedirs(os.path.join(work, "out"))
            src, dst = os.path.join(work, "in", name), os.path.join(work, "out", name)
            with open(src, "wb") as f:
                left = size
                while left > 0:
                    b = self.rfile.read(min(65536, left))
                    if not b:
                        break
                    f.write(b)
                    left -= len(b)
            with open(src, "rb") as f:
                if not f.read(1024).lstrip(b"\x00\t\n\r ").startswith(b"%PDF-"):
                    return self.refuse(415, "not a PDF", size)
            p = subprocess.Popen([self.server.aerapdf, "--fonts", self.server.fonts, "--progress"] + args + [src, dst],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            for line in p.stderr:
                m = re.match(r"progress (\d+) (\d+)", line)
                if m:
                    self.wfile.write(("HTTP/1.1 102 Processing\r\nX-Progress: %s/%s\r\n\r\n" %
                                      (m.group(1), m.group(2))).encode())
                    self.wfile.flush()
            if p.wait() != 0 or not os.path.exists(dst):
                return self.refuse(422, "it couldn't be converted", size)
            n = os.path.getsize(dst)
            self.send_response(200)
            self.send_header("Content-Type", "application/pdf")
            self.send_header("Content-Length", str(n))
            self.send_header("Connection", "close")
            self.end_headers()
            with open(dst, "rb") as f:
                while True:
                    b = f.read(65536)
                    if not b:
                        break
                    self.wfile.write(b)
            self.note(200, size, n)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--bind", default="127.0.0.1")
    ap.add_argument("--port", type=int, required=True)
    ap.add_argument("--aerapdf", required=True)
    ap.add_argument("--fonts", required=True)
    ap.add_argument("--log", default=os.devnull)
    a = ap.parse_args(argv)
    srv = http.server.ThreadingHTTPServer((a.bind, a.port), Gate)
    srv.aerapdf, srv.fonts, srv.log = os.path.abspath(a.aerapdf), os.path.abspath(a.fonts), a.log
    srv.serve_forever()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
