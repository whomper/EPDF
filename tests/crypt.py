#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""AES-256 (revision 6) with qpdf as the encrypter: a book made by
pdfgen.py, encrypted with an owner password only, must read as the plain
one does; with a user password too, it must be refused (the file key from
the empty password is checked by /Perms, ISO 32000-2's algorithm 13).
Needs build/pdftext (make test); without qpdf on the machine it says so
and passes."""
import os
import shutil
import subprocess
import sys
import tempfile

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(TOP, "tests", "hatari"))
from pdfgen import make_pdf, book  # noqa: E402


def text(exe, path):
    return subprocess.run([exe, path, "1"], capture_output=True, text=True)


def main():
    if not shutil.which("qpdf"):
        # (guideline 1.2: a step not run is said so, never a pass)
        print("crypt: SKIPPED: no qpdf on this machine, the encrypted PDFs can't be made")
        print("crypt: passed, 1 skipped")
        return 0
    exe = os.path.join(TOP, "build", "pdftext")
    bad = 0
    with tempfile.TemporaryDirectory() as d:
        plain, opened, locked = (os.path.join(d, n) for n in ("plain.pdf", "open.pdf", "locked.pdf"))
        make_pdf(plain, book(2))
        subprocess.run(["qpdf", "--encrypt", "", "owner", "256", "--", plain, opened], check=True)
        subprocess.run(["qpdf", "--encrypt", "user", "owner", "256", "--", plain, locked], check=True)
        want = text(exe, plain)
        got = text(exe, opened)
        if got.returncode or got.stdout != want.stdout or not want.stdout.strip():
            print("crypt: FAIL the owner-only AES-256 file doesn't read as the plain one")
            bad += 1
        got = text(exe, locked)
        if got.returncode == 0:
            print("crypt: FAIL a file with a user password was read")
            bad += 1
    print("crypt: FAILED" if bad else "crypt: ok")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
