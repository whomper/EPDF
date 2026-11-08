#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A stand-in for the Gateway's translateclient, for EPDF's own tests of
aerapdf's --translate (docs/PI-GATEWAY.md, "aerapdf and the translator"):

    faketranslate.py --to CODE  < paragraphs  > paragraphs

Each paragraph (UTF-8, an empty line between) comes back as "[CODE] " and
the paragraph; "from he" when the text has Hebrew letters, else "from en",
first on the error output; text already in CODE: "same CODE" and nothing
out. CODE zz: exit 2 (no such pair), yy: 3 (busy), xx: 4 (too long).
"""
import re
import sys

to = sys.argv[sys.argv.index("--to") + 1]
codes = {"zz": 2, "yy": 3, "xx": 4}
if to in codes:
    sys.exit(codes[to])
text = sys.stdin.buffer.read().decode("utf-8")
src = "he" if re.search("[א-ת]", text) else "en"
if src == to:
    sys.stderr.write("same %s\n" % to)
    sys.exit(0)
paras = [p for p in re.split(r"\n\s*\n", text) if p.strip()]
sys.stdout.write("\n\n".join("[%s] %s" % (to, p.strip()) for p in paras) + "\n")
sys.stderr.write("from %s\n" % src)
