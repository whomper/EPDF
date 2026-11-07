/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * test_pigate.c - pigate.c against a real pdfgate on this computer (the
 * Pi's own Python and its aerapdf: tests/pigate/run.sh starts one): a PDF
 * sent and its converted PDF written, the progress seen in all three
 * phases, a stop, a refusal's own words, and a Pi that isn't there.
 *
 *   test_pigate PORT IN.pdf OUT.pdf [STOP-AT-PERCENT of the result received]
 *
 * The network is the build machine's sockets, made non-blocking as
 * STinG's and MiNTnet's reads are (0: nothing yet); pf_idle counts the
 * turns conn.c gives while it waits.
 */
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "plat.h"
#include "conn.h"
#include "pigate.h"

/* (pf_open, pf_write, pf_close: src/pdfio_host.c's) */
void pf_now(PFTIME *t) { memset(t, 0, sizeof(*t)); }
unsigned long pf_ms(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (unsigned long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static long turns;
static void idle(void)
{
	struct timespec ts = { 0, 2000000 };

	turns++;
	nanosleep(&ts, NULL);
}
void (*pf_idle)(void) = idle;

int net_open(const char *host, unsigned short port, char *err, int errlen)
{
	struct addrinfo hints, *ai;
	char ps[8];
	int s;

	memset(&hints, 0, sizeof hints);
	hints.ai_socktype = SOCK_STREAM;
	snprintf(ps, sizeof ps, "%u", port);
	if (getaddrinfo(host, ps, &hints, &ai)) {
		snprintf(err, errlen, "can't find %s", host);
		return -1;
	}
	s = socket(ai->ai_family, SOCK_STREAM, 0);
	if (s < 0 || connect(s, ai->ai_addr, ai->ai_addrlen)) {
		snprintf(err, errlen, "can't reach %s port %u", host, port);
		freeaddrinfo(ai);
		if (s >= 0)
			close(s);
		return -1;
	}
	freeaddrinfo(ai);
	fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
	return s;
}

long net_write(int h, const void *buf, long n)
{
	long done = 0;

	while (done < n) {
		long k = send(h, (const char *)buf + done, n - done, MSG_NOSIGNAL);

		if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			idle();
			continue;
		}
		if (k <= 0)
			return done ? done : -1;
		done += k;
	}
	return done;
}

long net_read(int h, void *buf, long max)
{
	long k = recv(h, buf, max, 0);

	if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return 0;
	return k > 0 ? k : -1;
}

void net_close(int h) { close(h); }
void net_close_wait(int h, int seconds) { close(h); }

static long seen[3], stop_at = -1;
static long last_work = -1;
static int stepped_back;

static int progress(void *u, int phase, long done, long total)
{
	seen[phase]++;
	if (phase == PG_WORK) {
		if (done < last_work)
			stepped_back = 1;
		last_work = done;
	}
	/* a stop while the converted PDF comes (the Pi's work is too quick
	   here to be caught) */
	if (phase == PG_RECV && stop_at >= 0 && total > 0 && done * 100 / total >= stop_at)
		return 1;
	return 0;
}

int main(int argc, char **argv)
{
	PGOPT o = { "mono", NULL, "screen", 640, 640, 1 };
	char err[200], q[256];
	int rc, fails = 0;

	if (argc < 4) {
		fprintf(stderr, "usage: test_pigate PORT IN.pdf OUT.pdf [STOP-AT-PERCENT]\n");
		return 2;
	}
	if (argc > 4)
		stop_at = atol(argv[4]);
	/* the query: names as pdfgate takes them */
	pg_query(&o, "C:\\PDFS\\MY FILE+1.PDF", q, sizeof q);
	if (strcmp(q, "?pages=mono&pictures=screen&page_width=640&picture_width=640&double=1&name=MY%20FILE_1.PDF")) {
		printf("FAIL the query: %s\n", q);
		fails++;
	}
	pg_query(NULL, ".hidden", q, sizeof q);
	if (strcmp(q, "?name=_hidden")) {
		printf("FAIL a dot first: %s\n", q);
		fails++;
	}
	rc = pg_convert("127.0.0.1", (unsigned short)atoi(argv[1]), &o, argv[2], argv[3], progress, NULL,
			err, sizeof err);
	printf("rc %d err \"%s\" send %ld work %ld recv %ld turns %ld\n", rc, err, seen[0], seen[1], seen[2], turns);
	if (stepped_back) {
		printf("FAIL progress went back\n");
		fails++;
	}
	if (stop_at >= 0) {
		if (rc != -2 || access(argv[3], F_OK) == 0) {
			printf("FAIL a stop: rc %d, the file %s\n", rc, access(argv[3], F_OK) ? "gone" : "left");
			fails++;
		}
		conn_stop = 0;
	} else if (rc == 0 && (!seen[PG_SEND] || !seen[PG_RECV])) {
		printf("FAIL no progress sending or receiving\n");
		fails++;
	}
	/* a Pi that isn't there: said so, nothing written */
	rc = pg_convert("127.0.0.1", 1, &o, argv[2], "/nonexistent/x.pdf", NULL, NULL, err, sizeof err);
	if (rc != -1 || !strstr(err, "can't reach")) {
		printf("FAIL no Pi: rc %d err \"%s\"\n", rc, err);
		fails++;
	}
	return fails ? 1 : 0;
}
