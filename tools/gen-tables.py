#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Write src/pdftab.h: the tables the PDF reader needs, from Python's codecs,
fontTools' Adobe Glyph List and the standard-14 fonts' widths (as
reportlab carries them). Needs fontTools and reportlab; run it only to
change the tables:  python3 tools/gen-tables.py > src/pdftab.h

  enc_std/enc_win/enc_mac   code -> Unicode (0: none) for the three base encodings
  agl_names/agl_blk/agl_uc   glyph name -> Unicode, sorted by name: the names
                            front-coded in blocks of AGL_BLK (each a byte, the
                            characters it shares with the name before, then
                            the rest; a block's first shares none), the
                            blocks' offsets, the characters (0.5.1: 78 KB of
                            pointers and strings to 47 KB, the room NVDI's
                            menus need under four accessories)
  cff_std                   CFF's 391 standard strings (SID -> name)
  stdw_*                    advance widths (1/1000 em) of codes 32..255 in WinAnsi
"""
import sys
from fontTools import agl
from reportlab.pdfbase import _fontdata as fd

STD_HI = {  # Adobe StandardEncoding above ASCII (and its two quotes below)
    0x27: 0x2019, 0x60: 0x2018, 0xA1: 0xA1, 0xA2: 0xA2, 0xA3: 0xA3, 0xA4: 0x2044, 0xA5: 0xA5, 0xA6: 0x192,
    0xA7: 0xA7, 0xA8: 0xA4, 0xA9: 0x27, 0xAA: 0x201C, 0xAB: 0xAB, 0xAC: 0x2039, 0xAD: 0x203A, 0xAE: 0xFB01,
    0xAF: 0xFB02, 0xB1: 0x2013, 0xB2: 0x2020, 0xB3: 0x2021, 0xB4: 0xB7, 0xB6: 0xB6, 0xB7: 0x2022,
    0xB8: 0x201A, 0xB9: 0x201E, 0xBA: 0x201D, 0xBB: 0xBB, 0xBC: 0x2026, 0xBD: 0x2030, 0xBF: 0xBF,
    0xC1: 0x60, 0xC2: 0xB4, 0xC3: 0x2C6, 0xC4: 0x2DC, 0xC5: 0xAF, 0xC6: 0x2D8, 0xC7: 0x2D9, 0xC8: 0xA8,
    0xCA: 0x2DA, 0xCB: 0xB8, 0xCD: 0x2DD, 0xCE: 0x2DB, 0xCF: 0x2C7, 0xD0: 0x2014, 0xE1: 0xC6, 0xE3: 0xAA,
    0xE8: 0x141, 0xE9: 0xD8, 0xEA: 0x152, 0xEB: 0xBA, 0xF1: 0xE6, 0xF5: 0x131, 0xF8: 0x142, 0xF9: 0xF8,
    0xFA: 0x153, 0xFB: 0xDF,
}


def table(fn):
    t = []
    for c in range(256):
        try:
            ch = fn(c)
            t.append(ord(ch) if len(ch) == 1 and ord(ch) > 0x1F and not 0x7F <= ord(ch) <= 0x9F else 0)
        except Exception:                                    # noqa: BLE001
            t.append(0)
    return t


def win(c):
    if c == 0x7F or c < 32:
        return "\0"
    if c in (0x81, 0x8D, 0x8F, 0x90, 0x9D):
        return "\0"
    return bytes([c]).decode("cp1252")


def mac(c):
    return bytes([c]).decode("mac_roman") if c >= 32 and c != 0x7F else "\0"


def std(c):
    if c in STD_HI:
        return chr(STD_HI[c])
    return chr(c) if 32 <= c < 127 else "\0"


def arr(name, vals, per=12, fmt="0x%04X"):
    out = ["static const unsigned short %s[%d] = {" % (name, len(vals))]
    for i in range(0, len(vals), per):
        out.append("\t" + ", ".join(fmt % v for v in vals[i:i + per]) + ",")
    out.append("};")
    return "\n".join(out)


def main():
    print("/* SPDX-License-Identifier: GPL-2.0-or-later\n * Copyright (C) 2026 AERA | Erez Yaary */")
    print("/* Written by tools/gen-tables.py: do not edit. */\n#ifndef PDFTAB_H\n#define PDFTAB_H\n")
    print(arr("enc_std", table(std)))
    print(arr("enc_win", table(win)))
    print(arr("enc_mac", table(mac)))
    names = []
    for ln in agl._aglText.splitlines():      # the whole Adobe Glyph List, the names of one character
        if ln.startswith("#") or ";" not in ln:
            continue
        n, v = ln.split(";")
        if " " not in v.strip() and int(v, 16) < 0x10000:
            names.append((n, int(v, 16)))
    names.sort()
    blk = 16
    assert max(len(n) for n, _ in names) < 64
    offs, parts, at, prev = [], [], 0, ""
    for i, (n, u) in enumerate(names):
        p = 0
        if i % blk:
            while p < len(n) and p < len(prev) and n[p] == prev[p]:
                p += 1
        else:
            offs.append(at)
        assert p < 0x20 and all(ord(c) > 0x20 for c in n)
        parts.append("\\%03o%s" % (p, n[p:]))
        at += 1 + len(n) - p
        prev = n
    assert at < 0x10000
    print("\n#define AGL_N %d\n#define AGL_BLK %d\n#define AGL_NBLK %d" % (len(names), blk, len(offs)))
    print("static const char agl_names[%d] =" % (at + 1))
    for i in range(0, len(parts), blk):
        print('\t"%s"' % "".join(parts[i:i + blk]))
    print("\t;")
    print(arr("agl_blk", offs, 12, "%d"))
    print(arr("agl_uc", [u for _, u in names], 12, "0x%04X"))
    for font, tag in (("Helvetica", "helv"), ("Helvetica-Bold", "helvb"), ("Times-Roman", "times"),
                      ("Times-Bold", "timesb"), ("Times-Italic", "timesi"), ("Times-BoldItalic", "timesbi")):
        w = []
        for c in range(32, 256):
            u = table(win)[c]
            g = agl.UV2AGL.get(u) if u else None
            w.append(fd.widthsByFontGlyph[font].get(g, 0) if g else 0)
        print(arr("stdw_" + tag, w, 16, "%d"))
    from fontTools.cffLib import cffStandardStrings
    print("\nstatic const char *const cff_std[391] = {")
    for i in range(0, 391, 6):
        print("\t" + ", ".join('"%s"' % s for s in cffStandardStrings[i:i + 6]) + ",")
    print("};")
    print("\n#endif")


if __name__ == "__main__":
    main()
