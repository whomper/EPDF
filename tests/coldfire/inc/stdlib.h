/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef CF_STDLIB_H
#define CF_STDLIB_H
#include <stddef.h>
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
void exit(int code) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
long strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
long long strtoll(const char *s, char **end, int base);
double strtod(const char *s, char **end);
double atof(const char *s);
int atoi(const char *s);
long atol(const char *s);
int abs(int v);
long labs(long v);
char *getenv(const char *name);
char *mkdtemp(char *tmpl);
void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));
int rand(void);
void srand(unsigned s);
#define RAND_MAX 0x7fffffff
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#endif
