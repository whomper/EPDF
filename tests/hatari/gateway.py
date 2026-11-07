#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EConvert's Gateway mode on an emulated Atari against a real pdfgate on
this computer (Erez, 7 Nov 2026):

    PDFGATE=.../aera-gateway/gateway gateway.py WORK [MACHINE...]   (ST:mono)

The Atari's network is the family's FAKESTNG and its serial bridge
(machines.py network()); the Gateway is AERA-Gateway's pdfgate.py with this
build's converter (build/econvert) as aerapdf, at 127.0.0.1. EConvert
starts with CONVERT=gateway and two PDFs; Return converts. Checked:
- both come back, and each is byte for byte what build/econvert writes on
  its own with the sizes EConvert sent ("gateway-sizes" in its log);
- an ST shows This Atari disabled (shot), and converts nothing itself;
- the Gateway's log names no file;
- a Gateway that isn't there: the alert, and the files stay in the list.
Then EPDF with Open on the Gateway ticked (GATEWAY=1): a book opened is
shown, converted by the Gateway meanwhile into C:\\atari\\BOOK.PDF (byte
for byte the direct conversion with EPDF's sizes) and opened in its place
(the Optimised badge: a shot); opened again, the copy at once, nothing
sent; no Gateway: the alert, the book stays shown.
"""
import os
import shutil
import subprocess
import sys
import time
import importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from pdfgen import make_pdf, book  # noqa: E402
import showcase  # noqa: E402
_spec = importlib.util.spec_from_file_location("aera_speed", os.path.join(TOP, "common", "tests", "speed", "speed.py"))
speed = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(speed)

FONTS = os.path.join(TOP, "third_party", "liberation", "fonts")


def free_port():
    import socket
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def gateway(work):
    """pdfgate on 127.0.0.1 with build/econvert: (process, port, log path)"""
    gate = os.environ.get("PDFGATE")
    if not gate:
        sys.exit("gateway.py: PDFGATE names AERA-Gateway's gateway/ folder")
    port = free_port()
    with open(os.path.join(work, "SOURCE"), "w") as f:
        f.write("rig\n")
    log = os.path.join(work, "pdfgate.log")
    p = subprocess.Popen([sys.executable, os.path.join(gate, "pdfgate.py"), "--bind", "127.0.0.1",
                          "--port", str(port), "--aerapdf", os.path.join(TOP, "build", "econvert"),
                          "--fonts", FONTS, "--source", os.path.join(work, "SOURCE"), "-v"],
                         stderr=open(log, "w"))
    time.sleep(1)
    return p, port, log


def run(work, machine, port, there=True):
    r = speed.Rig(os.path.join(work, machine.replace(":", "_") + ("" if there else "-none")), machine,
                  display=os.environ.get("XDISPLAY", ":91"))
    bad = []
    names = ("ONE.PDF", "TWO.PDF")
    try:
        r.extra += r.m.network()
        os.makedirs(os.path.join(r.hd, "ECONV"))
        os.makedirs(os.path.join(r.hd, "OUT"))
        shutil.copy(os.environ.get("ECONVERT_PRG") or os.path.join(TOP, "binary", "ECONVERT.PRG"),
                    os.path.join(r.hd, "ECONV", "ECONVERT.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        showcase.cut(os.path.join(r.hd, names[0]))     # (pictures, drawings, fonts: the hub's cut)
        make_pdf(os.path.join(r.hd, names[1]), book(5))
        r.write("ECONV/ECONVERT.INF", "WRITETO=C:\\OUT\nCONVERT=gateway\nGATE=127.0.0.1\nGATEPORT=%d\n" %
                (port if there else 1))
        r.write("LAUNCH.TXT", "C:\\ECONV\\ECONVERT.PRG\nC:\\%s C:\\%s\n" % names)
        r.start("C:\\LAUNCH.PRG")
        end = time.time() + 120
        while time.time() < end and "ECONVERT started" not in r.console():
            time.sleep(1)
        time.sleep(4)
        r.still()
        r.shot("before")
        r.key("Return")
        end = time.time() + 600
        while time.time() < end and "ECONVERT batch-done" not in r.console():
            time.sleep(2)
        time.sleep(3)
        r.still()
        r.shot("done")
        con = r.console()
        if not there:
            if "ECONVERT gateway-off" not in con:
                bad.append("no Gateway: not said so")
            if "ECONVERT files 2" not in con.split("ECONVERT batch-done")[-1] and \
                    "ECONVERT files" in con.split("ECONVERT batch-done")[-1]:
                bad.append("no Gateway: the files left the list")
            r.key("Return")
            time.sleep(2)
        else:
            sizes = [l.split()[2:4] for l in con.splitlines() if l.startswith("ECONVERT gateway-sizes")]
            for n in names:
                if "ECONVERT converted %s 0" % n not in con:
                    bad.append("%s not converted" % n)
                    continue
                if not sizes:
                    bad.append("no sizes logged")
                    break
                got = os.path.join(r.hd, "OUT", n)
                want = os.path.join(r.work, "direct-" + n)
                subprocess.run([os.path.join(TOP, "build", "econvert"), "--fonts", FONTS, "--pages", "mono",
                                "--photos", "indexed", "--pictures", "screen", "--page-width", sizes[0][0],
                                "--picture-width", sizes[0][1], os.path.join(r.hd, n), want],
                               check=True, stdout=subprocess.DEVNULL)
                if not os.path.exists(got) or open(got, "rb").read() != open(want, "rb").read():
                    bad.append("%s differs from the direct conversion" % n)
        print("%s%s: %s  (%s)" % (r.describe(), "" if there else ", no Gateway", "ok" if not bad else "; ".join(bad),
                                  r.shots))
    finally:
        r.stop()
        r.m.stop()
    return bad


def direct_same(src, sizes, got, work):
    """got is byte for byte build/econvert's own conversion of src with sizes"""
    want = os.path.join(work, "direct-" + os.path.basename(src))
    subprocess.run([os.path.join(TOP, "build", "econvert"), "--fonts", FONTS, "--pages", "mono",
                    "--photos", "indexed", "--pictures", "screen", "--page-width", sizes[0],
                    "--picture-width", sizes[1], src, want], check=True, stdout=subprocess.DEVNULL)
    return os.path.exists(got) and open(got, "rb").read() == open(want, "rb").read()


RATE = int(os.environ.get("GW_RATE", "0")) or None    # bytes a second on the serial line: the phases' shots


def run_epdf(work, machine, port, there=True):
    r = speed.Rig(os.path.join(work, "epdf-" + machine.replace(":", "_") + ("" if there else "-none")), machine,
                  display=os.environ.get("XDISPLAY", ":91"))
    bad = []
    try:
        r.extra += r.m.network(RATE)
        os.makedirs(os.path.join(r.hd, "EPDF"))
        os.makedirs(os.path.join(r.hd, "DOCS"))
        shutil.copy(os.path.join(TOP, "binary", "EPDF.PRG"), os.path.join(r.hd, "EPDF", "EPDF.PRG"))
        r.put(os.path.join(TOP, "binary", "LAUNCH.PRG"))
        src = os.path.join(r.hd, "DOCS", "BOOK.PDF")
        make_pdf(src, book(4))
        r.write("EPDF/epdf.inf", "GATEWAY=1\nGATE=127.0.0.1\nGATEPORT=%d\n" % (port if there else 1))
        r.write("LAUNCH.TXT", "C:\\EPDF\\EPDF.PRG\nC:\\DOCS\\BOOK.PDF\n")
        r.start("C:\\LAUNCH.PRG")
        end = time.time() + 900
        k = 0
        while time.time() < end and "EPDF gateway-done" not in r.console():
            time.sleep(1)
            if RATE and "EPDF gateway started" in r.console() and k < 40:
                k += 1
                if k % 4 == 0:
                    r.shot("phase%02d" % k)     # (GW_RATE: the three phases as they go)
        time.sleep(6)
        r.still()
        r.shot("after")
        con = r.console()
        if not there:
            if "EPDF gateway-done -3" not in con:
                bad.append("no Gateway: not said so")
        else:
            sizes = [l.split()[2:4] for l in con.splitlines() if l.startswith("EPDF gateway-sizes")]
            if "EPDF gateway-done 0" not in con:
                bad.append("not converted")
            elif not sizes:
                bad.append("no sizes logged")
            elif not direct_same(src, sizes[0], os.path.join(r.hd, "DOCS", "atari", "BOOK.PDF"), r.work):
                bad.append("the copy in atari isn't the direct conversion")
            if con.count("EPDF opening") < 2:
                bad.append("the copy wasn't opened after")
        print("EPDF %s%s: %s  (%s)" % (r.describe(), "" if there else ", no Gateway", "ok" if not bad else "; ".join(bad),
                                       r.shots))
    finally:
        r.stop()
        r.m.stop()
    return bad


def main(argv):
    if not argv:
        sys.exit(__doc__)
    work = os.path.abspath(argv[0])
    os.makedirs(work, exist_ok=True)
    subprocess.run(["make", "-s", "-C", TOP, "build/econvert"], check=True)
    gate, port, log = gateway(work)
    bad = 0
    try:
        for m in argv[1:] or ["ST:mono"]:
            bad += bool(run(work, m, port))
        bad += bool(run(work, (argv[1:] or ["ST:mono"])[0], port, there=False))
        for m in argv[1:] or ["ST:mono"]:
            bad += bool(run_epdf(work, m, port))
        bad += bool(run_epdf(work, (argv[1:] or ["ST:mono"])[0], port, there=False))
    finally:
        gate.kill()
        gate.wait()
    text = open(log).read()
    if any(n in text for n in ("ONE.PDF", "TWO.PDF", "ONE", "TWO")):
        print("FAIL the Gateway's log names a file")
        bad += 1
    return bad


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
