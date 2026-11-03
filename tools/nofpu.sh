#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# No FPU instruction in a program that may run on a Falcon without a 68882:
# nofpu.sh ELF... (an ELF that isn't built yet is skipped)
OBJDUMP=${OBJDUMP:-m68k-linux-gnu-objdump}
bad=0
for f in "$@"; do
	[ -f "$f" ] || continue
	# (decoded as a 68030's: with ColdFire's mnemonics a switch's jump
	# table read as "ff1")
	n=$($OBJDUMP -m m68k:68030 -d "$f" | awk -F'\t' 'NF >= 3 { split($3, w, " "); if (w[1] ~ /^f[a-z]/) print }' | wc -l)
	if [ "$n" -ne 0 ]; then
		echo "nofpu: $f has $n FPU instructions"
		bad=1
	else
		echo "nofpu: $f ok"
	fi
done
exit $bad
