/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pgshare.c - the Gateway's progress by time (src/pgshare.c) on made-up
 * exchanges: an Atari sending at a few KB a second, a Pi converting in
 * seconds. Read every 100 ms, the share must never go back, never jump by
 * more than 10 points at once, and follow the time: within 15 points of
 * the time's own share once a twentieth of it has gone, 30 for a first
 * conversion (the result's size and the work guessed). The old fixed
 * shares (a tenth, eight tenths, a tenth) are shown beside it.
 *
 *     build/test_pgshare
 */
#include <stdio.h>
#include <stdlib.h>
#include "pgshare.h"

#define PIECE 8192

typedef struct {
	const char *name;
	long size, out;		/* bytes */
	long send_bps, recv_bps;
	long queue_ms, work_ms;	/* waiting its turn, converting */
	int says;		/* the Pi's 102s while it converts (every 0.5 s) */
} CASE;

static int fails;

/* the exchange in steps of 100 ms, as the program's turns read the bar:
   pigate's calls when a piece has gone or come or the Pi has spoken,
   pgs_now between them */
static void run(const CASE *c, int second)
{
	PGSHARE s;
	unsigned long t0 = 1000, now;
	long all, last = 0, jump = 0, off_most = 0, old_most = 0, old_last = 0, old_jump = 0;
	long send_end, work_from, work_end, recv_end, sent = 0, got = 0, said = 0;
	int ph = PG_SEND;

	send_end = c->size * 1000 / c->send_bps;
	work_from = send_end + c->queue_ms;
	work_end = work_from + c->work_ms;
	recv_end = work_end + c->out * 1000 / c->recv_bps;
	all = recv_end;
	pgs_start(&s, t0);
	for (now = t0 + 100; ; now += 100) {
		long e = (long)(now - t0), sh = -1, o, tm = e * 1000 / all;

		if (e <= send_end) {
			long n = c->send_bps * e / 1000 / PIECE * PIECE;

			if (e == send_end || n > c->size)
				n = c->size;
			if (n > sent) {
				sent = n;
				sh = pgs_share(&s, PG_SEND, sent, c->size, now);
			}
		} else if (e < work_end) {
			long w = e < work_from ? 0 : (e - work_from) * 1000 / c->work_ms;

			ph = PG_WORK;
			/* the Pi's 102s: every 5 s while it waits its turn, every 0.5 s while it works */
			if (c->says && (e < work_from ? (e - send_end) / 5000 > said / 5000 && (said = e - send_end, 1) :
					 (e - work_from) / 500 > said / 500 && (said = e - work_from, 1)) && e - send_end >= 500)
				sh = pgs_share(&s, PG_WORK, w, 1000, now);
			if (e >= work_from && said > e - work_from)
				said = 0;
		} else {
			long n = c->recv_bps * (e - work_end) / 1000 / PIECE * PIECE;

			ph = PG_RECV;
			if (e >= recv_end || n > c->out)
				n = c->out;
			if (n > got) {
				got = n;
				sh = pgs_share(&s, PG_RECV, got, c->out, now);
			}
		}
		if (sh < 0)
			sh = pgs_now(&s, now);
		o = ph == PG_SEND ? sent * 100 / c->size : ph == PG_WORK ? 100 : 900 + got * 100 / c->out;
		if (ph == PG_WORK && e >= work_end - 100)
			o = 900;
		if (sh < last) {
			printf("FAIL %s: went back %ld -> %ld\n", c->name, last, sh);
			fails++;
		}
		if (sh - last > jump)
			jump = sh - last;
		if (o - old_last > old_jump)
			old_jump = o - old_last;
		if (tm >= 50 && labs(sh - tm) > off_most)
			off_most = labs(sh - tm);
		if (tm >= 50 && labs(o - tm) > old_most)
			old_most = labs(o - tm);
		if (getenv("PGTRACE") && e % 2000 == 0)
			printf("%d %5ld s time %4ld share %4ld fixed %4ld\n", ph, e / 1000, tm, sh, o);
		last = sh;
		old_last = o;
		if (got >= c->out)
			break;
	}
	if (last != 1000) {
		printf("FAIL %s: ended at %ld, not 1000\n", c->name, last);
		fails++;
	}
	/* a first conversion guesses the result's size and the work; the
	   next one has the last one's */
	if (jump > 100 || off_most > (second ? 150 : 300)) {
		printf("FAIL %s%s: off the time by up to %ld, a jump of %ld\n", c->name, second ? " (again)" : "", off_most, jump);
		fails++;
	}
	printf("%-34s %s %3ld s: off the time by %3ld, jumps %3ld (fixed shares: %3ld, %3ld)\n", c->name,
	       second ? "again" : "first", all / 1000, off_most, jump, old_most, old_jump);
}

int main(void)
{
	static const CASE cases[] = {
		{ "a 300 KB PDF, 4 KB/s, Pi 3 s", 300000, 80000, 4000, 6000, 0, 3000, 1 },
		{ "a 1.2 MB PDF, 20 KB/s, Pi 8 s", 1200000, 400000, 20000, 25000, 0, 8000, 1 },
		{ "a 60 KB PDF, the Pi says nothing", 60000, 30000, 5000, 5000, 0, 400, 0 },
		{ "a 500 KB PDF, 10 s in the queue", 500000, 150000, 8000, 8000, 10000, 4000, 1 },
		{ "a 200 KB PDF, the result as big", 200000, 200000, 6000, 6000, 0, 2000, 1 },
	};
	unsigned i;

	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		run(&cases[i], 0);
		run(&cases[i], 1);	/* (the next conversion: the last one's work and size its first guess) */
	}
	if (fails)
		return 1;
	printf("ok: the Gateway's progress follows the time\n");
	return 0;
}
