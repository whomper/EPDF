/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * INFLBENCH.PRG - what common's inflate_piece (its hot loop atari/inflate_fast.S, INFLATE_ASM)
 * costs a PDF's streams on an Atari, the reader's way (src/pdffilt.c's pieces of up to 32 KB, no turn):
 * C:\MAP.BIN and C:\TEXT.BIN (tools/inflstreams.py: a PDF's FlateDecode streams, raw DEFLATE, each
 * "u32 raw length, u32 unpacked length, the raw bytes") unpacked whole, again and again for some
 * seconds. One JSON line each, to C:\INFLBENC.OUT and NatFeats: the passes, the milliseconds of
 * one, the bytes it unpacks and a hash of them (FNV-1a over every stream, the first pass only),
 * which two builds of inflate_fast.S must give alike. With C:\PROFILE.FLG it runs each file for
 * longer and ends on an illegal instruction (inflbench.py --profile).
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tos.h"
#include "inflate.h"

long nf_on = 1;		/* a test program: NatFeats always (common's nf.S has 0) */

static long out_h = -1;

static void emit(const char *line)
{
	nf_debug(line);
	if (out_h >= 0) {
		Fwrite((short)out_h, (long)strlen(line), (void *)line);
		Fwrite((short)out_h, 2L, (void *)"\r\n");
	}
}

typedef struct {
	const unsigned char *p;
	long left;
} SRC;

static int get(void *u, unsigned char *buf, long n)
{
	SRC *s = u;

	if (n > s->left)
		n = s->left;
	memcpy(buf, s->p, (size_t)n);
	s->p += n;
	s->left -= n;
	return (int)n;
}

static unsigned long be32(const unsigned char *p)
{
	return (unsigned long)p[0] << 24 | (unsigned long)p[1] << 16 | (unsigned long)p[2] << 8 | p[3];
}

/* every stream once: the bytes unpacked; the hash too when h isn't 0; -1 damaged or no memory */
static long pass(const unsigned char *f, long n, unsigned long *h)
{
	long at = 0, bytes = 0;

	while (at + 8 <= n) {
		long raw = (long)be32(f + at), want = (long)be32(f + at + 4), got = 0, k;
		SRC s;
		INFLATE *z;
		const unsigned char *p;

		s.p = f + at + 8;
		s.left = raw;
		z = inflate_open_piece(get, &s);
		if (!z)
			return -1;
		while ((k = inflate_piece(z, &p, 32768L, NULL)) > 0) {
			if (h) {
				long i;

				for (i = 0; i < k; i++)
					*h = (*h ^ p[i]) * 16777619UL;
			}
			got += k;
		}
		inflate_close(z);
		if (k < 0 || got != want)
			return -1;
		bytes += got;
		at += 8 + raw;
	}
	return bytes;
}

static void bench(const char *name, const char *path, long secs)
{
	long fh = Fopen(path, 0), n, bytes, passes = 0;
	unsigned char *f;
	unsigned long h = 2166136261UL;
	u32 t0, t;
	char line[200];

	if (fh < 0) {
		snprintf(line, sizeof line, "{\"file\":\"%s\",\"error\":\"no file\"}", name);
		emit(line);
		return;
	}
	n = Fseek(0L, (short)fh, 2);
	Fseek(0L, (short)fh, 0);
	f = malloc((size_t)n);
	if (!f || Fread((short)fh, n, f) != n) {
		Fclose((short)fh);
		snprintf(line, sizeof line, "{\"file\":\"%s\",\"error\":\"no memory\"}", name);
		emit(line);
		free(f);
		return;
	}
	Fclose((short)fh);
	bytes = pass(f, n, &h);	/* (the first, with the hash: not timed) */
	if (bytes < 0) {
		snprintf(line, sizeof line, "{\"file\":\"%s\",\"error\":\"damaged\"}", name);
		emit(line);
		free(f);
		return;
	}
	t0 = tos_ms();
	do {
		pass(f, n, NULL);
		passes++;
		t = tos_ms();
	} while ((long)(t - t0) < secs * 1000L);
	snprintf(line, sizeof line, "{\"file\":\"%s\",\"compressed\":%ld,\"bytes\":%ld,\"hash\":\"%08lx\",\"passes\":%ld,"
		 "\"ms\":%ld,\"ms_pass\":%ld}", name, n, bytes, h, passes, (long)(t - t0), (long)(t - t0) / passes);
	emit(line);
	free(f);
}

int main(void)
{
	long prof = Fopen("C:\\PROFILE.FLG", 0);
	long secs = 6;

	if (prof >= 0) {
		Fclose((short)prof);
		secs = 20;
	}
	out_h = Fcreate("C:\\INFLBENC.OUT", 0);
	bench("map", "C:\\MAP.BIN", secs);
	bench("text", "C:\\TEXT.BIN", secs);
	emit("{\"done\":1}");
	if (out_h >= 0)
		Fclose((short)out_h);
	if (prof >= 0)
		__asm__ volatile("illegal");
	return 0;
}
