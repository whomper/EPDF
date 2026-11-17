#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""aprofpad.py - the profiling build at the shipped build's places.

The speed rigs time build/aprof (gem.c with -DAES_PROFILE=20), and its
code is the shipped program's only where it sits at the same places mod
256 (the 68030's cache lines, the 68000's prefetch: EPDF's pads keep
2.35's places). The profiling gem.o is larger (aes() 366 bytes, its
strings 55), and linked at the end it moved 197 KB of code, the tables
and every variable: the rigs measured a layout that didn't ship (the
hub, 17 Nov 2026, from EPlay). So it is linked in the shipped gem.o's
place with pads after it, and checked.

  aprofpad.py pads SHIP_GEM_O PROF_GEM_O STR DATA TEXT_S DATA_S
      writes TEXT_S (the code pad: the two gem.o's code sizes made equal
      mod 256, and STR bytes of strings, merged with the program's) and
      DATA_S (DATA bytes of variables, linked after nfon.o)
  aprofpad.py fit SHIP_ELF TRIAL_ELF PROF_GEM_O
      prints STR and DATA for a trial linked with both 0
  aprofpad.py check SHIP_ELF PROF_ELF PROF_GEM_O
      exit 1 if any symbol not of the profiling gem.o sits elsewhere mod 256
"""
import collections
import subprocess
import sys

NM = "m68k-linux-gnu-nm"
READELF = "m68k-linux-gnu-readelf"


def text_size(obj):
    out = subprocess.run([READELF, "-SW", obj], capture_output=True, text=True, check=True).stdout
    n = 0
    for line in out.splitlines():
        f = line.replace("[ ", "[").split()
        if len(f) > 5 and f[1].startswith(".text"):
            n += int(f[5], 16)
    return n


def own_names(obj):
    out = subprocess.run([NM, obj], capture_output=True, text=True, check=True).stdout
    return {l.split()[2] for l in out.splitlines() if len(l.split()) == 3 and l.split()[1] != "U"}


def syms(elf, skip):
    """(name, kind) -> addresses in address order; kind t (code and the
    tables, which link.ld puts in .text), d or b"""
    out = subprocess.run([NM, "-n", "-S", elf], capture_output=True, text=True, check=True).stdout
    r = collections.defaultdict(list)
    for line in out.splitlines():
        f = line.split()
        if len(f) != 4 or f[3] in skip or f[3].startswith("aprof_"):
            continue
        k = f[2].lower()
        if k in "rt":
            k = "t"
        if k in "tdb":
            r[(f[3], k)].append((int(f[0], 16), int(f[1], 16)))
    return r


def shifts(ship, prof, skip):
    """per kind: [(address in prof, shift mod 256, size)] in address order"""
    a, b = syms(ship, skip), syms(prof, skip)
    res = collections.defaultdict(list)
    for key, la in a.items():
        lb = b.get(key)
        if not lb or len(lb) != len(la):
            continue
        for (aa, sz), (ab, _) in zip(la, lb):
            res[key[1]].append((ab, (ab - aa) % 256, sz))
    for k in res:
        res[k].sort()
    return res


def first_shift(lst):
    for _, d, _ in lst:
        if d:
            return d
    return 0


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    if cmd == "pads" and len(sys.argv) == 8:
        ship, prof, s, d, ts, ds = sys.argv[2:]
        t = (text_size(ship) - text_size(prof)) % 256
        s, d = int(s), int(d)
        with open(ts, "w") as f:
            f.write("\t.section .text.aprof_pad,\"ax\"\n\t.globl aprof_pad\naprof_pad:\n")
            if t:
                f.write("\t.space %d\n" % t)
            if s:
                # one string of its own (no other ends so), merged with the program's
                f.write("\t.section .rodata.aprof_pad.str1.1,\"aMS\",@progbits,1\n\t.globl aprof_str\naprof_str:\n")
                f.write("\t.ascii \"%s\"\n\t.byte 0\n" % ("\\001" * (s - 1)))
            else:
                f.write("\t.globl aprof_str\n\t.set aprof_str, aprof_pad\n")
        with open(ds, "w") as f:
            f.write("\t.section .data.aprof_pad_d,\"aw\"\n\t.globl aprof_pad_d\naprof_pad_d:\n")
            if d:
                f.write("\t.space %d\n" % d)
        return 0
    if cmd == "fit" and len(sys.argv) == 5:
        sh = shifts(sys.argv[2], sys.argv[3], own_names(sys.argv[4]))
        s = (256 - first_shift(sh["t"])) % 256
        if s == 1:
            s = 257		# (an empty string would be merged with another's end)
        dd, db = first_shift(sh["d"]), first_shift(sh["b"])
        print(s, (dd - db) % 256)
        return 0
    if cmd == "check" and len(sys.argv) == 5:
        sh = shifts(sys.argv[2], sys.argv[3], own_names(sys.argv[4]))
        bad = 0
        for k, name in (("t", "code and tables"), ("d", "variables"), ("b", "bss")):
            off = [(a, d, n) for a, d, n in sh[k] if d]
            total = sum(n for _, _, n in sh[k])
            moved = sum(n for _, _, n in off)
            print("aprofpad: %s %d of %d bytes at the shipped places mod 256" % (name, total - moved, total))
            if off:
                bad = 1
                print("aprofpad: from 0x%x moved by %d" % (off[0][0], off[0][1]))
        if bad:
            print("aprofpad: the profiling build isn't laid out as the shipped one")
        return bad
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
