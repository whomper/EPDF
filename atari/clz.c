/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* __clzsi2 for the 68000: libgcc's is 68020 code (bfffo) and faults on an ST. FreeType's __builtin_clz calls it. */
int __clzsi2(unsigned long x)
{
	int n = 0;

	if (!x)
		return 32;
	if (!(x & 0xFFFF0000UL)) { n += 16; x <<= 16; }
	if (!(x & 0xFF000000UL)) { n += 8; x <<= 8; }
	if (!(x & 0xF0000000UL)) { n += 4; x <<= 4; }
	if (!(x & 0xC0000000UL)) { n += 2; x <<= 2; }
	if (!(x & 0x80000000UL)) n += 1;
	return n;
}
