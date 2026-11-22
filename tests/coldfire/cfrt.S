/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
| cfrt.S - a Linux program's start for tests/coldfire (EPDF's): argc and argv
| from the stack the kernel gives, main(), exit(). Instructions a 68030 and a
| ColdFire both have, so one file serves both builds under qemu-m68k.
	.text
	.globl	_start
_start:
	move.l	(%sp),%d0		| argc
	lea	4(%sp),%a0		| argv
	move.l	%d0,%d1
	addq.l	#1,%d1
	lsl.l	#2,%d1
	lea	(%a0,%d1.l),%a1		| envp
	move.l	%a1,cf_environ
	move.l	%a0,-(%sp)
	move.l	%d0,-(%sp)
	jsr	main
	move.l	%d0,-(%sp)
	jsr	exit
1:	bra.s	1b
	.bss
	.globl	cf_environ
	.balign	4
cf_environ:	.space	4
