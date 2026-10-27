#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Put the symbols of a linked ELF into a TOS program (DRI format, GST long
names), so that Hatari's debugger and profiler name the functions.

    prgsyms.py PROGRAM.ELF IN.PRG OUT.PRG

The symbol table goes between the data and the relocation table; the header
says how long it is. For profiling only: releases carry no symbols.
"""
import struct
import subprocess
import sys


def main():
    elf, src, dst = sys.argv[1:4]
    prg = open(src, "rb").read()
    magic, tsize, dsize, bsize, ssize, res, flags, absf = struct.unpack(">HIIIIIIH", prg[:28])
    if magic != 0x601A or ssize:
        sys.exit("prgsyms: not a program without symbols")
    nm = subprocess.run(["m68k-linux-gnu-nm", "-n", "--defined-only", elf], capture_output=True, text=True, check=True).stdout
    syms = b""
    for ln in nm.splitlines():
        v, t, name = ln.split(None, 2)
        if t.lower() not in "tdbr" or name.startswith("."):
            continue
        val = int(v, 16)
        sect = 0x0200 if t.lower() in "tr" else 0x0100 if t.lower() == "d" else 0x0400
        n = name.encode()[:22]
        if len(n) > 8:
            syms += struct.pack(">8sHI", n[:8], sect | 0x0048, val) + n[8:22].ljust(14, b"\0")
        else:
            syms += struct.pack(">8sHI", n.ljust(8, b"\0"), sect, val)
    body = 28 + tsize + dsize
    out = struct.pack(">HIIIIIIH", magic, tsize, dsize, bsize, len(syms), res, flags, absf) + prg[28:body] + syms + prg[body:]
    open(dst, "wb").write(out)
    print("%d symbols" % (len(syms) // 14))


if __name__ == "__main__":
    main()
