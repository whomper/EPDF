#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# No FPU instruction in a program that may run on a Falcon without a 68882:
# nofpu.sh ELF... (an ELF that isn't built yet is skipped)
OBJDUMP=${OBJDUMP:-m68k-linux-gnu-objdump}
bad=0
# (a hex number in any awk: mawk has no strtonum)
HEX='function hex(s,  i, v) { v = 0; s = tolower(s); for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789abcdef", substr(s, i, 1)) - 1; return v }
'
for f in "$@"; do
	[ -f "$f" ] || continue
	# (decoded as a 68030's: with ColdFire's mnemonics a switch's jump
	# table read as "ff1")
	# (frestore and fsave, the FPU's state for a task switch, are never
	# compiled from C, hard float or soft: a switch's jump table of
	# offsets 0xF3xx read so, EPDF's main 5 Nov 2026)
	# (and an absolute address's low word read as one: past a jump table
	# the disassembly can start inside "tst.l 0x000df212", ECONVERT 17 Nov
	# 2026. A hit whose word before it, with it, is an address within the
	# program is such an operand, not an instruction)
	top=$($OBJDUMP -h "$f" | awk "$HEX"'NF >= 7 && $1 ~ /^[0-9]+$/ { e = hex($4) + hex($3); if (e > m) m = e } END { printf "%d", m }')
	n=$($OBJDUMP -m m68k:68030 -d "$f" | awk -F'\t' -v top="$top" "$HEX"'
		NF >= 3 {
			split($2, h, " "); split($3, w, " ")
			if (w[1] ~ /^f[a-z]/ && w[1] !~ /^f(restore|save)/ && !(prev != "" && hex(prev h[1]) < top))
				print
			prev = h[length(h)]
		}' | wc -l)
	if [ "$n" -ne 0 ]; then
		echo "nofpu: $f has $n FPU instructions"
		bad=1
	else
		echo "nofpu: $f ok"
	fi
done
exit $bad
