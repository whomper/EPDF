#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The FireBee editions' code against the shipped one's, under QEMU
(Erez, 21-22 Nov 2026: EPDF native on the FireBee; the hub: "qemu-m68k -cpu
cfv4e byte for byte").

    run.py [WORK] [--files PDF...] [--pages N]

The reader as EPDF.PRG has it (tools/textgold.c over src/ and common/src:
every page's text, counts, pictures and every glyph) is built twice with
tests/coldfire's C library (cflibc.c over Linux's calls, cfrt.S), the same
sources and options but the CPU: for the 68000, as EPDF.PRG, and for the
ColdFire, as the FireBee package's EPDF.PRG (-mcpu=5475, as common/tools/mkfirebee.py makes
it). Each runs under qemu-m68k (-cpu m68020 for the 68000 code, whose
libgcc is 68020 code; -cpu cfv4e for the ColdFire's), on every PDF of the
corpus (build/pdfs, tools/fetch-corpus.py) and its first N pages; their
outputs must be the same byte for byte. The host's textgold is run as
well and its differences said, not failed (a host's long has 64 bits).

The converter (tools/econvert.c: render.c, raster.c, quant.c, deflate.c,
jpegenc.c, pic.c's JPEG, FreeType) the same, for the 68030 (-m68030
-msoft-float, as ECONVERT.PRG and render.ovl) against the ColdFire
(the FireBee package's ECONVERT.PRG and render.ovl): each converted file compared byte for byte.
"""
import glob
import hashlib
import os
import shlex
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
CC = "m68k-linux-gnu-gcc"
BS = "common/third_party/bearssl/src"
READER = ["tools/textgold.c", "src/pdfio_host.c", "src/pdfobj.c", "src/pdfxref.c", "src/pdffilt.c",
          "src/pdfcrypt.c", "src/pdfdoc.c", "src/pdftext.c", "src/pdfmeas.c",
          "common/src/inflate.c", "common/src/util.c", "common/src/futil.c"] + \
         [BS + "/" + s for s in ("hash/md5.c", "hash/sha2small.c", "hash/sha2big.c", "symcipher/aes_big_cbcdec.c",
                                 "symcipher/aes_big_cbcenc.c", "symcipher/aes_big_dec.c", "symcipher/aes_big_enc.c",
                                 "symcipher/aes_common.c", "codec/dec32be.c", "codec/enc32be.c", "codec/dec32le.c",
                                 "codec/enc32le.c", "codec/dec64be.c", "codec/enc64be.c")]
COPT = ["-O2", "-fomit-frame-pointer", "-ffreestanding", "-fno-builtin", "-fno-pic", "-fno-pie",
        "-fno-tree-loop-distribute-patterns", "-fwrapv", "-std=gnu99", "-Wall", "-Wno-sign-compare",
        "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable", "-Wno-unused-but-set-variable"]
BUILDS = {  # name: CPU options, QEMU CPU
    "m68000": (["-m68000"], "m68020"),         # the reader as EPDF.PRG has it
    "m68030": (["-m68030", "-msoft-float"], "m68030"),  # the converter as ECONVERT.PRG and render.ovl
    "cf": (["-mcpu=5475"], "cfv4e"),           # the FireBee package's EPDF.PRG, ECONVERT.PRG, render.ovl
}
FT = "third_party/freetype"
FT_SRC = [FT + "/src/" + s for s in ("base/ftbase.c", "base/ftinit.c", "base/ftdebug.c", "base/ftbbox.c",
          "base/ftglyph.c", "base/ftbitmap.c", "base/ftsynth.c", "sfnt/sfnt.c", "truetype/truetype.c", "cff/cff.c",
          "type1/type1.c", "psaux/psaux.c", "pshinter/pshinter.c", "psnames/psmodule.c", "raster/raster.c",
          "smooth/smooth.c", "autofit/autofit.c")] + [FT + "/epdf/ftsystem.c"]
FT_DEFS = ["-DFT2_BUILD_LIBRARY", "-DFT_CONFIG_OPTIONS_H=\"epdf/ftoption.h\"", "-DFT_CONFIG_MODULES_H=\"epdf/ftmodule.h\""]
LJ = "common/third_party/libjpeg-turbo"
LJ_SRC = [LJ + "/src/" + s + ".c" for s in ("jaricom jcomapi jdapimin jdapistd jdarith jdcoefct jdcolor jddctmgr jdhuff "
          "jdinput jdmainct jdmarker jdmaster jdmerge jdphuff jdpostct jdsample jdtrans jidctfst jidctint jidctred "
          "jmemmgr jmemnobs jquant1 jquant2 jutils").split()]
LJ_DEFS = ["-DPIC_LIBJPEG", "-DNO_GETENV", "-DNO_PUTENV", "-DAERA_NO_FLOAT", "-I" + LJ + "/src", "-I" + LJ + "/config"]
CONVERTER = ["tools/econvert.c", "src/conv/conv.c", "src/conv/render.c", "src/conv/raster.c", "src/conv/cvjpeg.c",
             "src/conv/deflate.c", "src/conv/quant.c", "src/conv/jpegenc.c", "common/src/charset.c", "src/view/trpara.c",
             "src/conv/cvio_host.c", "src/pdfimg.c", "src/ccitt.c", "src/jbig2.c", "src/mq.c", "src/jpx.c",
             "src/rscale.c", "common/src/pic.c", "common/src/png.c", "common/src/pic_lj8.c",
             "common/third_party/nsgif/src/gif.c", "common/third_party/nsgif/src/lzw.c"] + READER[1:]


def sh(cmd, **kw):
    p = subprocess.run(cmd, cwd=TOP, capture_output=True, text=True, **kw)
    if p.returncode:
        sys.stderr.write(" ".join(shlex.quote(c) for c in cmd) + "\n" + p.stdout + p.stderr)
        sys.exit("run.py: a command failed")
    return p.stdout


def build(work, name, srcs, incs, defs=(), each=None, asm=()):
    """srcs built for build name's CPU into WORK/NAME/o; each(src) gives a
    file's own options (FreeType's and libjpeg's); asm: .S files beside"""
    cpu, _ = BUILDS[name]
    gi = sh([CC, "-print-file-name=include"]).strip()
    od = os.path.join(work, name, "o-" + os.path.basename(srcs[0])[:-2])
    os.makedirs(od, exist_ok=True)
    objs = []
    for s in srcs + ["tests/coldfire/cflibc.c"]:
        o = os.path.join(od, s.replace("/", "_")[:-2] + ".o")
        objs.append(o)
        if os.path.exists(o) and os.path.getmtime(o) > os.path.getmtime(os.path.join(TOP, s)):
            continue
        sh([CC] + cpu + COPT + list(defs) + (each(s) if each else []) + ["-nostdinc", "-isystem", gi,
           "-Itests/coldfire/inc"] + ["-I" + i for i in incs] + ["-c", "-o", o, s])
    rt = os.path.join(od, "cfrt.o")
    sh([CC] + cpu + ["-c", "-o", rt, "tests/coldfire/cfrt.S"])
    for a in asm:
        o = os.path.join(od, os.path.basename(a)[:-2] + ".o")
        sh([CC] + cpu + ["-c", "-o", o, a])
        objs.append(o)
    extra = ["-lgcc"]
    if name == "cf":                    # (the m68k libgcc is 68020 code: common's ll64.c)
        ll = os.path.join(od, "ll64.o")
        sh([CC, "-mcpu=5475", "-O2", "-ffreestanding", "-fno-builtin", "-c", "-o", ll, "common/atari/ll64.c"])
        extra = [ll]
    exe = os.path.join(work, name, os.path.basename(srcs[0])[:-2])
    sh([CC] + cpu + ["-nostdlib", "-static", "-no-pie", "-Wl,-z,noexecstack", "-o", exe, rt] + objs + extra)
    return exe


def qemu(name, exe, args, out):
    with open(out, "wb") as f:
        p = subprocess.run(["qemu-m68k", "-cpu", BUILDS[name][1], exe] + args, cwd=TOP, stdout=f,
                           stderr=subprocess.PIPE, timeout=3600)
    return p.returncode, p.stderr.decode(errors="replace")[-300:]


def digest(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()[:16]


def main(argv):
    work = os.path.abspath(argv[0] if argv and not argv[0].startswith("--") else os.path.join(TOP, "build", "cfq"))
    files = sorted(glob.glob(os.path.join(TOP, "build", "pdfs", "*.pdf")))
    pages = 6
    if "--pages" in argv:
        i = argv.index("--pages")
        pages = int(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
    if "--files" in argv:
        files = [os.path.abspath(f) for f in argv[argv.index("--files") + 1:]]
    if not files:
        sys.exit("run.py: no PDFs (tools/fetch-corpus.py fills build/pdfs)")
    incs = ["src", "common/src", "common/third_party/bearssl/inc", "common/third_party/bearssl/src"]
    exes = {n: build(work, n, READER, incs) for n in ("m68000", "cf")}
    host = os.path.join(TOP, "build", "textgold")
    bad = 0
    for f in files:
        tag = os.path.basename(f)[:-4]
        outs = {}
        for n, exe in exes.items():
            o = os.path.join(work, n, tag + ".txt")
            rc, err = qemu(n, exe, [f, "1", str(pages)], o)
            outs[n] = (o, rc, err)
        same = len({digest(o) for o, _, _ in outs.values()}) == 1 and len({rc for _, rc, _ in outs.values()}) == 1
        note = ""
        if os.path.exists(host):
            ho = os.path.join(work, tag + ".host.txt")
            with open(ho, "wb") as hf:
                subprocess.run([host, f, "1", str(pages)], stdout=hf, stderr=subprocess.DEVNULL)
            note = "" if digest(ho) == digest(outs["cf"][0]) else "  (the host's differs)"
        size = os.path.getsize(outs["cf"][0])
        print("%-28s %s  %8d bytes, rc %s%s" % (tag, "same" if same else "DIFFERENT", size,
                                                  "/".join(str(v[1]) for v in outs.values()), note), flush=True)
        if not same:
            bad += 1
            for n, (o, rc, err) in outs.items():
                if rc:
                    print("    %s: %s" % (n, err.strip().splitlines()[-1:] if err.strip() else rc))
    print("reader: %d of %d the same" % (len(files) - bad, len(files)))
    if "--no-converter" not in sys.argv:
        bad += converter(work, files, incs)
    return 1 if bad else 0


def conv_each(s):
    if s.startswith(FT + "/"):
        return FT_DEFS + ["-I" + FT + "/include", "-I" + FT, "-I" + os.path.dirname(s), "-w"]
    if s.startswith(LJ + "/"):
        return LJ_DEFS + ["-w"]
    return []


def converter(work, files, incs):
    """the converter's whole job on each document (its first pages): the
    converted file from the 68030's build and the ColdFire's, byte for byte"""
    inc = incs + ["src/conv", "src/view", FT + "/include", FT, FT + "/epdf/include",
                  "common/third_party/nsgif/include", "common/third_party/nsgif/src"]
    defs = ["-DPDF_DRAW", "-DFT_CONFIG_OPTIONS_H=\"epdf/ftoption.h\"", "-DFT_CONFIG_MODULES_H=\"epdf/ftmodule.h\""] + LJ_DEFS
    exes = {n: build(work, n, CONVERTER + FT_SRC + LJ_SRC, inc, defs, conv_each, [FT + "/epdf/setjmp.S", "src/jb2t0.S"])
            for n in ("m68030", "cf")}
    fonts = os.environ.get("ECONV_FONTS", "/usr/share/fonts/truetype/liberation")
    bad = 0
    for f in files:
        tag = os.path.basename(f)[:-4]
        jobs = [("p%d.ppm" % pg, ["--fonts", fonts, "--jpeg-pic", "--render", str(pg)]) for pg in (1, 2)]
        if os.path.getsize(f) < 1 << 20:     # (a whole conversion under QEMU: the smaller documents)
            jobs.append(("conv.pdf", ["--fonts", fonts, "--pages", "colour", "--thumbs", f]))
        res = []
        for what, args in jobs:
            outs = {}
            for n, exe in exes.items():
                o = os.path.join(work, n, tag + "." + what)
                if os.path.exists(o):
                    os.remove(o)
                cmd = ["qemu-m68k", "-cpu", BUILDS[n][1], exe] + args + ([o, f] if "--render" in args else [o])
                if "--render" not in args:
                    cmd = cmd[:-2] + [f, o]
                with open(os.path.join(work, n, tag + "." + what + ".log"), "wb") as lf:
                    p = subprocess.run(cmd, cwd=TOP, stdout=lf, stderr=subprocess.STDOUT, timeout=7200)
                outs[n] = (o, p.returncode)
            have = [os.path.exists(o) for o, _ in outs.values()]
            same = len({rc for _, rc in outs.values()}) == 1 and (
                not any(have) or (all(have) and len({digest(o) for o, _ in outs.values()}) == 1))
            res.append("%s %s%s" % (what, "same" if same else "DIFFERENT",
                                    "" if all(have) else " (none)"))
            bad += not same
        print("%-28s %s" % (tag, ", ".join(res)), flush=True)
    print("converter: %d differences" % bad)
    return bad


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
