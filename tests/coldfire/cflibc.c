/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * cflibc.c - the C library EPDF's host tools need (tools/textgold.c,
 * tools/econvert.c), over Linux's calls, so that the reader and the
 * converter built for the ColdFire (-mcpu=5475, the FireBee's MCF5474) and
 * for the 68030 run under qemu-m68k as Linux programs: the m68k C library
 * on the build machine is 68020 code. The same file for both builds, so
 * what differs between their outputs is the compiled code alone
 * (tests/coldfire/run.py).
 */
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ Linux */
static long sc(long n, long a, long b, long c)
{
	register long d0 __asm__("d0") = n;
	register long d1 __asm__("d1") = a;
	register long d2 __asm__("d2") = b;
	register long d3 __asm__("d3") = c;
	__asm__ volatile("trap #0" : "+r"(d0) : "r"(d1), "r"(d2), "r"(d3) : "memory", "cc");
	return d0;
}

enum { SYS_EXIT = 1, SYS_READ = 3, SYS_WRITE = 4, SYS_OPEN = 5, SYS_CLOSE = 6, SYS_UNLINK = 10,
       SYS_LSEEK = 19, SYS_RENAME = 38, SYS_BRK = 45, SYS_GETTIMEOFDAY = 78 };

extern char **cf_environ;

ssize_t read(int fd, void *buf, size_t n) { return sc(SYS_READ, fd, (long)buf, (long)n); }
ssize_t write(int fd, const void *buf, size_t n) { return sc(SYS_WRITE, fd, (long)buf, (long)n); }
off_t lseek(int fd, off_t off, int whence) { return sc(SYS_LSEEK, fd, off, whence); }
int close(int fd) { return (int)sc(SYS_CLOSE, fd, 0, 0); }
int unlink(const char *path) { return (int)sc(SYS_UNLINK, (long)path, 0, 0); }
int rmdir(const char *path) { return (int)sc(40, (long)path, 0, 0); }
int remove(const char *path) { return unlink(path); }
int rename(const char *a, const char *b) { return (int)sc(SYS_RENAME, (long)a, (long)b, 0); }

int open(const char *path, int flags, ...)
{
	va_list ap;
	int mode = 0644;

	va_start(ap, flags);
	if (flags & O_CREAT)
		mode = va_arg(ap, int);
	va_end(ap);
	return (int)sc(SYS_OPEN, (long)path, flags, mode);
}

/* a file's size and kind: opened and its end found (a folder opens too,
   and lseek gives it no size) */
int stat(const char *path, struct stat *st)
{
	int fd = open(path, O_RDONLY);
	long n;

	if (fd < 0)
		return -1;
	n = lseek(fd, 0, SEEK_END);
	close(fd);
	st->st_size = n < 0 ? 0 : n;
	st->st_mode = n < 0 ? 0040000 : 0100000;
	st->st_mtime = 0;
	return 0;
}

int fstat(int fd, struct stat *st)
{
	long at = lseek(fd, 0, SEEK_CUR), n = lseek(fd, 0, SEEK_END);

	lseek(fd, at, SEEK_SET);
	st->st_size = n;
	st->st_mode = 0100000;
	st->st_mtime = 0;
	return 0;
}

struct cf_timeval { long sec, usec; };

int clock_gettime(int clk, struct timespec *ts)
{
	struct cf_timeval tv;

	(void)clk;
	sc(SYS_GETTIMEOFDAY, (long)&tv, 0, 0);
	ts->tv_sec = tv.sec;
	ts->tv_nsec = tv.usec * 1000;
	return 0;
}

time_t time(time_t *t)
{
	struct timespec ts;

	clock_gettime(0, &ts);
	if (t)
		*t = ts.tv_sec;
	return ts.tv_sec;
}

clock_t clock(void)
{
	struct timespec ts;

	clock_gettime(1, &ts);
	return ts.tv_sec * 1000000L + ts.tv_nsec / 1000;
}

static struct tm the_tm;
struct tm *gmtime(const time_t *t)
{
	long s = *t, d = s / 86400, y = 1970;
	static const int mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	int m = 0;

	s %= 86400;
	the_tm.tm_hour = (int)(s / 3600);
	the_tm.tm_min = (int)(s / 60 % 60);
	the_tm.tm_sec = (int)(s % 60);
	the_tm.tm_wday = (int)((d + 4) % 7);
	for (;;) {
		int yl = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
		if (d < yl)
			break;
		d -= yl;
		y++;
	}
	the_tm.tm_yday = (int)d;
	for (; m < 12; m++) {
		int ml = mdays[m] + (m == 1 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
		if (d < ml)
			break;
		d -= ml;
	}
	the_tm.tm_year = (int)(y - 1900);
	the_tm.tm_mon = m;
	the_tm.tm_mday = (int)d + 1;
	the_tm.tm_isdst = 0;
	return &the_tm;
}

struct tm *localtime(const time_t *t) { return gmtime(t); }

char *getenv(const char *name)
{
	size_t n = strlen(name);
	char **e;

	for (e = cf_environ; e && *e; e++)
		if (!strncmp(*e, name, n) && (*e)[n] == '=')
			return *e + n + 1;
	return NULL;
}

/* ------------------------------------------------------------------ the heap */
/* blocks from brk in power-of-two bins (16 bytes to 2 GB), each with its bin
   in the long before it; a freed block goes back on its bin's list */
static char *brk_end;
static void *bins[32];

static void *core(size_t n)
{
	char *p;

	if (!brk_end)
		brk_end = (char *)sc(SYS_BRK, 0, 0, 0);
	p = brk_end;
	if ((char *)sc(SYS_BRK, (long)(p + n), 0, 0) != p + n)
		return NULL;
	brk_end = p + n;
	return p;
}

void *malloc(size_t n)
{
	unsigned b = 4;
	long *p;

	while (((size_t)1 << b) < n + 8)
		b++;
	if (b >= 31)
		return NULL;
	if (bins[b]) {
		p = bins[b];
		bins[b] = *(void **)p;
	} else if (!(p = core((size_t)1 << b)))
		return NULL;
	p[0] = b;
	return p + 2;
}

void free(void *ptr)
{
	long *p;

	if (!ptr)
		return;
	p = (long *)ptr - 2;
	*(void **)p = bins[p[0]];
	bins[p[0]] = p;
}

void *calloc(size_t n, size_t m)
{
	void *p = malloc(n * m);

	if (p)
		memset(p, 0, n * m);
	return p;
}

void *realloc(void *ptr, size_t n)
{
	void *q;
	size_t have;

	if (!ptr)
		return malloc(n);
	have = ((size_t)1 << ((long *)ptr)[-2]) - 8;
	if (n <= have)
		return ptr;
	q = malloc(n);
	if (q) {
		memcpy(q, ptr, have);
		free(ptr);
	}
	return q;
}

/* ------------------------------------------------------------------ strings */
void *memcpy(void *d, const void *s, size_t n)
{
	unsigned char *a = d;
	const unsigned char *b = s;

	while (n--)
		*a++ = *b++;
	return d;
}

void *memmove(void *d, const void *s, size_t n)
{
	unsigned char *a = d;
	const unsigned char *b = s;

	if (a < b)
		while (n--)
			*a++ = *b++;
	else
		while (n--)
			a[n] = b[n];
	return d;
}

void *memset(void *d, int c, size_t n)
{
	unsigned char *a = d;

	while (n--)
		*a++ = (unsigned char)c;
	return d;
}

int memcmp(const void *x, const void *y, size_t n)
{
	const unsigned char *a = x, *b = y;

	for (; n--; a++, b++)
		if (*a != *b)
			return *a - *b;
	return 0;
}

void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *a = s;

	for (; n--; a++)
		if (*a == (unsigned char)c)
			return (void *)a;
	return NULL;
}

size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
size_t strnlen(const char *s, size_t m) { size_t n = 0; while (n < m && s[n]) n++; return n; }
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) ; return r; }

char *strncpy(char *d, const char *s, size_t n)
{
	size_t i = 0;

	for (; i < n && s[i]; i++)
		d[i] = s[i];
	for (; i < n; i++)
		d[i] = 0;
	return d;
}

char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }

char *strncat(char *d, const char *s, size_t n)
{
	char *e = d + strlen(d);

	while (n-- && *s)
		*e++ = *s++;
	*e = 0;
	return d;
}

int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
	for (; n; n--, a++, b++)
		if (*a != *b || !*a)
			return (unsigned char)*a - (unsigned char)*b;
	return 0;
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

int strcasecmp(const char *a, const char *b)
{
	while (*a && lower((unsigned char)*a) == lower((unsigned char)*b))
		a++, b++;
	return lower((unsigned char)*a) - lower((unsigned char)*b);
}

int strncasecmp(const char *a, const char *b, size_t n)
{
	for (; n; n--, a++, b++)
		if (lower((unsigned char)*a) != lower((unsigned char)*b) || !*a)
			return lower((unsigned char)*a) - lower((unsigned char)*b);
	return 0;
}

char *strchr(const char *s, int c)
{
	for (;; s++) {
		if (*s == (char)c)
			return (char *)s;
		if (!*s)
			return NULL;
	}
}

char *strrchr(const char *s, int c)
{
	const char *r = NULL;

	for (;; s++) {
		if (*s == (char)c)
			r = s;
		if (!*s)
			return (char *)r;
	}
}

char *strstr(const char *h, const char *n)
{
	size_t k = strlen(n);

	for (; *h; h++)
		if (!strncmp(h, n, k))
			return (char *)h;
	return k ? NULL : (char *)h;
}

size_t strspn(const char *s, const char *a) { size_t n = 0; while (s[n] && strchr(a, s[n])) n++; return n; }
size_t strcspn(const char *s, const char *r) { size_t n = 0; while (s[n] && !strchr(r, s[n])) n++; return n; }
char *strpbrk(const char *s, const char *a) { s += strcspn(s, a); return *s ? (char *)s : NULL; }

char *strdup(const char *s)
{
	char *d = malloc(strlen(s) + 1);

	return d ? strcpy(d, s) : NULL;
}

char *strerror(int e) { (void)e; return "error"; }

/* ------------------------------------------------------------------ numbers */
static int digit(int c, int base)
{
	int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'z' ? c - 'a' + 10 : c >= 'A' && c <= 'Z' ? c - 'A' + 10 : 99;

	return v < base ? v : -1;
}

unsigned long long cf_strtoull(const char *s, char **end, int base, int *neg)
{
	unsigned long long v = 0;
	int d;

	while (*s == ' ' || (*s >= 9 && *s <= 13))
		s++;
	*neg = 0;
	if (*s == '-' || *s == '+')
		*neg = *s++ == '-';
	if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		s += 2, base = 16;
	else if (base == 0)
		base = s[0] == '0' ? 8 : 10;
	while ((d = digit((unsigned char)*s, base)) >= 0)
		v = v * (unsigned)base + (unsigned)d, s++;
	if (end)
		*end = (char *)s;
	return v;
}

long strtol(const char *s, char **end, int base) { int n; unsigned long v = (unsigned long)cf_strtoull(s, end, base, &n); return n ? -(long)v : (long)v; }
unsigned long strtoul(const char *s, char **end, int base) { int n; unsigned long v = (unsigned long)cf_strtoull(s, end, base, &n); return n ? -v : v; }
long long strtoll(const char *s, char **end, int base) { int n; unsigned long long v = cf_strtoull(s, end, base, &n); return n ? -(long long)v : (long long)v; }
int atoi(const char *s) { return (int)strtol(s, NULL, 10); }
long atol(const char *s) { return strtol(s, NULL, 10); }
int abs(int v) { return v < 0 ? -v : v; }
long labs(long v) { return v < 0 ? -v : v; }

double strtod(const char *s, char **end)
{
	double v = 0, f = 1;
	int neg = 0, e = 0, en = 0;

	while (*s == ' ')
		s++;
	if (*s == '-' || *s == '+')
		neg = *s++ == '-';
	while (*s >= '0' && *s <= '9')
		v = v * 10 + (*s++ - '0');
	if (*s == '.')
		for (s++; *s >= '0' && *s <= '9'; s++)
			f /= 10, v += (*s - '0') * f;
	if (*s == 'e' || *s == 'E') {
		s++;
		if (*s == '-' || *s == '+')
			en = *s++ == '-';
		while (*s >= '0' && *s <= '9')
			e = e * 10 + (*s++ - '0');
		while (e--)
			v = en ? v / 10 : v * 10;
	}
	if (end)
		*end = (char *)s;
	return neg ? -v : v;
}

double atof(const char *s) { return strtod(s, NULL); }

static unsigned long rnd = 1;
int rand(void) { rnd = rnd * 1103515245 + 12345; return (int)((rnd >> 1) & 0x7fffffff); }
void srand(unsigned s) { rnd = s; }

/* a merge sort (the order of equal elements fixed, the same in both builds) */
static void msort(char *a, char *t, size_t n, size_t sz, int (*cmp)(const void *, const void *))
{
	size_t h = n / 2, i = 0, j = h, k = 0;

	if (n < 2)
		return;
	msort(a, t, h, sz, cmp);
	msort(a + h * sz, t, n - h, sz, cmp);
	while (i < h && j < n)
		if (cmp(a + j * sz, a + i * sz) < 0)
			memcpy(t + k++ * sz, a + j++ * sz, sz);
		else
			memcpy(t + k++ * sz, a + i++ * sz, sz);
	while (i < h)
		memcpy(t + k++ * sz, a + i++ * sz, sz);
	while (j < n)
		memcpy(t + k++ * sz, a + j++ * sz, sz);
	memcpy(a, t, n * sz);
}

void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
	char *t = malloc(n * size + 1);

	if (t) {
		msort(base, t, n, size, cmp);
		free(t);
	}
}

void *bsearch(const void *key, const void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
	size_t lo = 0, hi = n;

	while (lo < hi) {
		size_t mid = (lo + hi) / 2;
		const char *p = (const char *)base + mid * size;
		int c = cmp(key, p);

		if (c == 0)
			return (void *)p;
		if (c < 0)
			hi = mid;
		else
			lo = mid + 1;
	}
	return NULL;
}

/* ------------------------------------------------------------------ files */
struct cf_file {
	int fd, err, eof;
	int wpos;			/* bytes in the write buffer */
	unsigned char buf[4096];
};
static struct cf_file f_in = { 0 }, f_out = { 1 }, f_err = { 2 };
FILE *stdin = &f_in, *stdout = &f_out, *stderr = &f_err;

int fflush(FILE *f)
{
	if (f && f->wpos) {
		write(f->fd, f->buf, (size_t)f->wpos);
		f->wpos = 0;
	}
	return 0;
}

FILE *fopen(const char *path, const char *mode)
{
	int flags = mode[0] == 'r' ? (strchr(mode, '+') ? O_RDWR : O_RDONLY)
		  : mode[0] == 'w' ? O_WRONLY | O_CREAT | O_TRUNC : O_WRONLY | O_CREAT | O_APPEND;
	int fd = open(path, flags, 0644);
	FILE *f;

	if (fd < 0 || !(f = calloc(1, sizeof *f))) {
		if (fd >= 0)
			close(fd);
		return NULL;
	}
	f->fd = fd;
	return f;
}

int fclose(FILE *f)
{
	fflush(f);
	close(f->fd);
	if (f != stdin && f != stdout && f != stderr)
		free(f);
	return 0;
}

size_t fwrite(const void *p, size_t s, size_t n, FILE *f)
{
	size_t len = s * n;
	const unsigned char *b = p;

	if (len >= sizeof f->buf) {
		fflush(f);
		write(f->fd, b, len);
		return n;
	}
	if (f->wpos + len > sizeof f->buf)
		fflush(f);
	memcpy(f->buf + f->wpos, b, len);
	f->wpos += (int)len;
	if (f == stderr)
		fflush(f);
	return n;
}

size_t fread(void *p, size_t s, size_t n, FILE *f)
{
	size_t len = s * n, got = 0;

	fflush(f);
	while (got < len) {
		long r = read(f->fd, (char *)p + got, len - got);
		if (r <= 0) {
			f->eof = 1;
			break;
		}
		got += (size_t)r;
	}
	return s ? got / s : 0;
}

int fseek(FILE *f, long off, int whence) { fflush(f); f->eof = 0; return lseek(f->fd, off, whence) < 0 ? -1 : 0; }
long ftell(FILE *f) { fflush(f); return lseek(f->fd, 0, SEEK_CUR); }
int fputc(int c, FILE *f) { unsigned char b = (unsigned char)c; fwrite(&b, 1, 1, f); return c; }
int fputs(const char *s, FILE *f) { fwrite(s, 1, strlen(s), f); return 0; }
int putchar(int c) { return fputc(c, stdout); }
int puts(const char *s) { fputs(s, stdout); fputc('\n', stdout); return 0; }

int fgetc(FILE *f)
{
	unsigned char b;

	return fread(&b, 1, 1, f) == 1 ? b : EOF;
}

char *fgets(char *s, int n, FILE *f)
{
	int i = 0, c;

	while (i < n - 1 && (c = fgetc(f)) != EOF) {
		s[i++] = (char)c;
		if (c == '\n')
			break;
	}
	s[i] = 0;
	return i ? s : NULL;
}

/* ------------------------------------------------------------------ formatting */
typedef struct { char *p; size_t n, at; FILE *f; } OUT;

static void put(OUT *o, char c)
{
	if (o->f) {
		fputc(c, o->f);
		o->at++;
		return;
	}
	if (o->at + 1 < o->n)
		o->p[o->at] = c;
	o->at++;
}

static void put_num(OUT *o, unsigned long long v, int base, int up, int neg, int width, int prec, int left, int zero, int plus, int space, int alt)
{
	char tmp[72];
	int k = 0, len, pad;
	const char *dg = up ? "0123456789ABCDEF" : "0123456789abcdef";
	char sign = neg ? '-' : plus ? '+' : space ? ' ' : 0;

	if (v == 0 && prec != 0)
		tmp[k++] = '0';
	while (v) {
		tmp[k++] = dg[v % (unsigned)base];
		v /= (unsigned)base;
	}
	while (k < prec)
		tmp[k++] = '0';
	if (alt && base == 8 && (k == 0 || tmp[k - 1] != '0'))
		tmp[k++] = '0';
	len = k + (sign != 0) + (alt && base == 16 ? 2 : 0);
	pad = width > len ? width - len : 0;
	if (!left && !(zero && prec < 0))
		while (pad-- > 0)
			put(o, ' ');
	if (sign)
		put(o, sign);
	if (alt && base == 16)
		put(o, '0'), put(o, up ? 'X' : 'x');
	if (!left && zero && prec < 0)
		while (pad-- > 0)
			put(o, '0');
	while (k)
		put(o, tmp[--k]);
	if (left)
		while (pad-- > 0)
			put(o, ' ');
}

static void put_dbl(OUT *o, double v, int prec, int width, int left, int conv)
{
	char tmp[80];
	int k = 0, i, neg = v < 0;
	unsigned long ip;
	double fp, r = 0.5;

	if (prec < 0)
		prec = 6;
	if (neg)
		v = -v;
	if (conv == 'g' && prec == 0)
		prec = 1;
	for (i = 0; i < prec; i++)
		r /= 10;
	v += r;
	ip = v < 4e9 ? (unsigned long)v : 4000000000UL;	/* (the tools print no larger) */
	fp = v - (double)ip;
	if (neg)
		tmp[k++] = '-';
	{
		char d[24];
		int n = 0;

		do {
			d[n++] = (char)('0' + ip % 10);
			ip /= 10;
		} while (ip);
		while (n)
			tmp[k++] = d[--n];
	}
	if (prec > 0) {
		tmp[k++] = '.';
		for (i = 0; i < prec && k < 70; i++) {
			int dd;

			fp *= 10;
			dd = (int)fp;
			tmp[k++] = (char)('0' + dd);
			fp -= dd;
		}
		if (conv == 'g') {
			while (tmp[k - 1] == '0')
				k--;
			if (tmp[k - 1] == '.')
				k--;
		}
	}
	if (!left)
		for (i = k; i < width; i++)
			put(o, ' ');
	for (i = 0; i < k; i++)
		put(o, tmp[i]);
	if (left)
		for (i = k; i < width; i++)
			put(o, ' ');
}

static int format(OUT *o, const char *fmt, va_list ap)
{
	for (; *fmt; fmt++) {
		int left = 0, zero = 0, plus = 0, space = 0, alt = 0, width = 0, prec = -1, lng = 0;
		unsigned long long u;
		long long s;
		char c;

		if (*fmt != '%') {
			put(o, *fmt);
			continue;
		}
		for (fmt++;; fmt++) {
			if (*fmt == '-') left = 1;
			else if (*fmt == '0') zero = 1;
			else if (*fmt == '+') plus = 1;
			else if (*fmt == ' ') space = 1;
			else if (*fmt == '#') alt = 1;
			else break;
		}
		if (*fmt == '*')
			width = va_arg(ap, int), fmt++;
		else
			while (*fmt >= '0' && *fmt <= '9')
				width = width * 10 + (*fmt++ - '0');
		if (width < 0)
			left = 1, width = -width;
		if (*fmt == '.') {
			prec = 0;
			fmt++;
			if (*fmt == '*')
				prec = va_arg(ap, int), fmt++;
			else
				while (*fmt >= '0' && *fmt <= '9')
					prec = prec * 10 + (*fmt++ - '0');
		}
		for (;; fmt++) {
			if (*fmt == 'l') lng++;
			else if (*fmt == 'z' || *fmt == 't' || *fmt == 'j') lng = *fmt == 'j' ? 2 : 1;
			else if (*fmt == 'h') ;
			else break;
		}
		c = *fmt;
		switch (c) {
		case 'd': case 'i':
			s = lng >= 2 ? va_arg(ap, long long) : lng ? va_arg(ap, long) : va_arg(ap, int);
			put_num(o, s < 0 ? -(unsigned long long)s : (unsigned long long)s, 10, 0, s < 0, width, prec, left, zero, plus, space, 0);
			break;
		case 'u': case 'x': case 'X': case 'o':
			u = lng >= 2 ? va_arg(ap, unsigned long long) : lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
			put_num(o, u, c == 'u' ? 10 : c == 'o' ? 8 : 16, c == 'X', 0, width, prec, left, zero, 0, 0, alt);
			break;
		case 'p': case 'P':
			put_num(o, (unsigned long)va_arg(ap, void *), 16, c == 'P', 0, width, prec, left, zero, 0, 0, 1);
			break;
		case 'c':
			put(o, (char)va_arg(ap, int));
			break;
		case 's': {
			const char *str = va_arg(ap, const char *);
			int n, i;

			if (!str)
				str = "(null)";
			n = (int)(prec >= 0 ? strnlen(str, (size_t)prec) : strlen(str));
			if (!left)
				for (i = n; i < width; i++)
					put(o, ' ');
			for (i = 0; i < n; i++)
				put(o, str[i]);
			if (left)
				for (i = n; i < width; i++)
					put(o, ' ');
			break;
		}
		case 'f': case 'g': case 'e':
			put_dbl(o, va_arg(ap, double), prec, width, left, c);
			break;
		case '%':
			put(o, '%');
			break;
		default:
			put(o, '%');
			if (c)
				put(o, c);
			else
				fmt--;
		}
	}
	return (int)o->at;
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
	OUT o = { buf, n, 0, NULL };
	int r = format(&o, fmt, ap);

	if (n)
		buf[o.at < n ? o.at : n - 1] = 0;
	return r;
}

int snprintf(char *buf, size_t n, const char *fmt, ...) { va_list ap; int r; va_start(ap, fmt); r = vsnprintf(buf, n, fmt, ap); va_end(ap); return r; }
int sprintf(char *buf, const char *fmt, ...) { va_list ap; int r; va_start(ap, fmt); r = vsnprintf(buf, (size_t)1 << 30, fmt, ap); va_end(ap); return r; }
int vfprintf(FILE *f, const char *fmt, va_list ap) { OUT o = { NULL, 0, 0, f }; return format(&o, fmt, ap); }
int fprintf(FILE *f, const char *fmt, ...) { va_list ap; int r; va_start(ap, fmt); r = vfprintf(f, fmt, ap); va_end(ap); return r; }
int printf(const char *fmt, ...) { va_list ap; int r; va_start(ap, fmt); r = vfprintf(stdout, fmt, ap); va_end(ap); return r; }
void perror(const char *s) { fprintf(stderr, "%s: error\n", s); }

/* the few sscanf the tools take: %d, %ld, %x and literal text */
int sscanf(const char *s, const char *fmt, ...)
{
	va_list ap;
	int n = 0;

	va_start(ap, fmt);
	for (; *fmt; fmt++) {
		if (*fmt == '%') {
			char *e;
			int lng = 0, base = 10;

			fmt++;
			while (*fmt == 'l')
				lng = 1, fmt++;
			if (*fmt == 'x')
				base = 16;
			{
				long v = strtol(s, &e, base);
				if (e == s)
					break;
				s = e;
				if (lng)
					*va_arg(ap, long *) = v;
				else
					*va_arg(ap, int *) = (int)v;
				n++;
			}
		} else if (*fmt == ' ') {
			while (*s == ' ')
				s++;
		} else if (*s++ != *fmt)
			break;
	}
	va_end(ap);
	return n;
}

/* ------------------------------------------------------------------ the end */
void exit(int code)
{
	fflush(stdout);
	fflush(stderr);
	sc(SYS_EXIT, code, 0, 0);
	for (;;)
		;
}

void abort(void) { exit(134); }

/* (econvert's --translator runs another program: not here) */
#include <sys/wait.h>
int fork(void) { return -1; }
int execl(const char *path, const char *arg, ...) { (void)path; (void)arg; return -1; }
int dup2(int a, int b) { (void)a; return b; }
int pipe(int fd[2]) { (void)fd; return -1; }
void _exit(int code) { exit(code); }
/* (a ColdFire has no bfffo: FreeType's FT_MSB calls this, as common's tos.c gives it) */
int __clzsi2(unsigned long x)
{
	int n = 0;

	if (!x)
		return 32;
	while (!(x & 0x80000000UL))
		x <<= 1, n++;
	return n;
}
char *mkdtemp(char *tmpl) { (void)tmpl; return NULL; }
int errno;
pid_t waitpid(pid_t pid, int *status, int options) { (void)pid; (void)status; (void)options; return -1; }
