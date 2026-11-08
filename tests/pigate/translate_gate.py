#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""pdfgate's translate=CODE (docs/PI-GATEWAY.md, "translate"), EPDF's
protocol tested by EPDF (guidelines 1.2): pdfgate.py and translateclient.py
from the read-only gateway/ copy, translategate.py's stand-in engine on a
local socket of this test's, and this build's aerapdf (build/econvert).

    translate_gate.py GATEWAY_FOLDER IN.pdf

- translate=de: 200, X-Translate-From: en, each page's /Translation; the
  PDF byte for byte what build/econvert --translate de writes with the
  same translateclient on its own;
- translate=en: X-Translate-Same: 1, every page /From /Same;
- translate=fr: 422 "pdfgate: no translation from en into fr";
- translate=EN and translate=e1: 400 "pdfgate: translate=...";
- through tests/pigate/faketranslate.py as the translator: yy (busy) 503,
  xx (too long) 504, zz (no pair) 422;
- no translate: no X-Translate header, the PDF as without translation;
- no word of the text in either gateway's log.
"""
import os
import re
import socket
import subprocess
import sys
import tempfile
import time
import zlib

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ECONV = os.path.join(TOP, "build", "econvert")
FONTS = os.path.join(TOP, "third_party", "liberation", "fonts")
FAKE = os.path.join(TOP, "tests", "pigate", "faketranslate.py")


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def wait_port(port, end=15):
    t = time.time() + end
    while time.time() < t:
        try:
            socket.create_connection(("127.0.0.1", port), 0.5).close()
            return
        except OSError:
            time.sleep(0.2)
    raise SystemExit("FAIL: nothing listening on %d" % port)


def post(port, query, data):
    """the final answer: (status, headers, body); 102s skipped"""
    s = socket.create_connection(("127.0.0.1", port), 120)
    s.sendall(("POST /convert%s HTTP/1.1\r\nHost: pi\r\nContent-Length: %d\r\n\r\n" %
               (query, len(data))).encode() + data)
    got = b""
    while True:
        b = s.recv(65536)
        if not b:
            break
        got += b
    s.close()
    while True:
        top, _, rest = got.partition(b"\r\n\r\n")
        status = int(top.split(b" ", 2)[1])
        if status != 102:
            head = top.decode("latin-1")
            return status, head, rest
        got = rest


def streams(data):
    out = {}
    for m in re.finditer(rb"(\d+) 0 obj\s*<<((?:(?!endobj).)*?)>>\s*stream\r?\n", data, re.S):
        dic = m.group(2)
        length = int(re.search(rb"/Length (\d+)", dic).group(1))
        raw = data[m.end():m.end() + length]
        out[int(m.group(1))] = (dic, zlib.decompress(raw) if b"FlateDecode" in dic else raw)
    return out


def main(argv):
    gw, src = argv
    bad = []
    pdf = open(src, "rb").read()
    w = tempfile.mkdtemp(prefix="trgate-")
    os.makedirs(os.path.join(w, "in"))
    src = os.path.join(w, "in", "in.pdf")      # (pdfgate's name for it without name=)
    open(src, "wb").write(pdf)
    sock = os.path.join(w, "local.sock")
    env = dict(os.environ, TRANSLATECLIENT_SOCKET=sock, PYTHONDONTWRITEBYTECODE="1")
    open(os.path.join(w, "SOURCE"), "w").write("aerapdf test\n")
    procs, logs, text_words = [], [], set()

    def start(cmd, name):
        log = open(os.path.join(w, name), "w")
        logs.append(os.path.join(w, name))
        procs.append(subprocess.Popen(cmd, env=env, stdout=log, stderr=log))

    tg_port, port, fport = free_port(), free_port(), free_port()
    os.makedirs(os.path.join(w, "tgstate"))
    start([sys.executable, os.path.join(gw, "translategate.py"), "--bind", "127.0.0.1", "--port", str(tg_port),
           "--engine", "stand-in", "--state", os.path.join(w, "tgstate"), "--models", w, "--local", sock], "tg.log")
    gate = [sys.executable, os.path.join(gw, "pdfgate.py"), "--bind", "127.0.0.1", "--aerapdf", ECONV,
            "--fonts", FONTS, "--source", os.path.join(w, "SOURCE")]
    os.makedirs(os.path.join(w, "t1"))
    os.makedirs(os.path.join(w, "t2"))
    start(gate + ["--port", str(port), "--tmp", os.path.join(w, "t1")], "gate.log")
    start(gate + ["--port", str(fport), "--tmp", os.path.join(w, "t2"), "--translator", FAKE], "fake.log")
    try:
        for p in (tg_port, port, fport):
            wait_port(p)
        t = time.time() + 10
        while not os.path.exists(sock) and time.time() < t:
            time.sleep(0.2)

        def check(what, ok, more=""):
            print("%s %s%s" % ("ok  " if ok else "FAIL", what, (": " + more) if not ok and more else ""))
            if not ok:
                bad.append(what)

        # translate=de: the stand-in "translates" en into de
        st, head, body = post(port, "?translate=de", pdf)
        check("translate=de: 200", st == 200, "%d %r" % (st, body[:80]))
        check("translate=de: X-Translate-From: en", "\r\nX-Translate-From: en" in head, head)
        check("translate=de: no X-Translate-Same", "X-Translate-Same" not in head)
        ss = streams(body) if st == 200 else {}
        for d, x in ss.values():
            if b"/Subtype" not in d:       # (the text streams, not pictures)
                text_words.update(re.findall(rb"[A-Za-z]{7,}", x))
        n_pages = len(re.findall(rb"/Type\s*/Page\b", body))
        check("translate=de: a /Translation for each page", body.count(b"/From /en") == n_pages and n_pages > 0,
              "%d pages, %d /From /en" % (n_pages, body.count(b"/From /en")))
        check("translate=de: the catalog's /Translate << /To /de >>",
              re.search(rb"/Translate\s*<<\s*/To\s*/de", body) is not None)
        check("translate=de: the text translated (the stand-in's de: and capitals)",
              any(b"de:" in x or b"DE:" in x for _, x in ss.values()))
        direct = os.path.join(w, "direct.pdf")
        r = subprocess.run([ECONV, "--fonts", FONTS, "--translate", "de", "--translator",
                            os.path.join(gw, "translateclient.py"), src, direct],
                           env=env, capture_output=True, text=True)
        same = r.returncode == 0 and open(direct, "rb").read() == body
        check("translate=de: the Pi's PDF is econvert --translate's, byte for byte", same,
              "econvert %d %s" % (r.returncode, r.stderr.strip()[-120:]))

        st, head, body = post(port, "?translate=en", pdf)
        check("translate=en: 200, X-Translate-Same: 1",
              st == 200 and "\r\nX-Translate-Same: 1" in head and "\r\nX-Translate-From: en" in head, head)
        check("translate=en: every page /From /Same",
              st == 200 and body.count(b"/From /Same") == len(re.findall(rb"/Type\s*/Page\b", body)) > 0)

        st, head, body = post(port, "?translate=fr", pdf)
        check("translate=fr: 422 no translation from en into fr",
              (st, body) == (422, b"pdfgate: no translation from en into fr\n"), "%d %r" % (st, body))
        for q in ("EN", "e1", "deu"):
            st, head, body = post(port, "?translate=" + q, pdf)
            check("translate=%s: 400" % q, (st, body) == (400, ("pdfgate: translate=%s\n" % q).encode()),
                  "%d %r" % (st, body))

        for code, want in (("yy", (503, b"pdfgate: the translation service is busy\n")),
                           ("xx", (504, b"pdfgate: the translation took too long\n")),
                           ("zz", 422)):
            st, head, body = post(fport, "?translate=" + code, pdf)
            okk = st == want if isinstance(want, int) else (st, body) == want
            check("the translator's exit for %s: %s" % (code, want if isinstance(want, int) else want[0]), okk,
                  "%d %r" % (st, body))

        st, head, body = post(port, "", pdf)
        plain = os.path.join(w, "plain.pdf")
        subprocess.run([ECONV, "--fonts", FONTS, src, plain], capture_output=True)
        check("no translate: no X-Translate, byte for byte as without",
              st == 200 and "X-Translate" not in head and body == open(plain, "rb").read())
    finally:
        for p in procs:
            p.terminate()
        for p in procs:
            p.wait()
    words = text_words
    if not words:
        print("FAIL no words of the page text to look for in the logs")
        bad.append("words")
    for log in logs:
        text = open(log, "rb").read()
        leaked = [x for x in words if x.lower() in text.lower()]
        if leaked:
            print("FAIL %s names the text: %s" % (os.path.basename(log), leaked[:3]))
            bad.append(log)
    if not [l for l in logs if l in bad]:
        print("ok   no word of the text in the logs")
    import shutil
    shutil.rmtree(w, ignore_errors=True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
