/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* tests/coldfire: the C library the host tools need, over Linux's calls
   (cflibc.c), for the ColdFire and 68030 builds run under qemu-m68k */
#ifndef CF_STDIO_H
#define CF_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct cf_file FILE;
extern FILE *stdin, *stdout, *stderr;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
size_t fread(void *p, size_t s, size_t n, FILE *f);
size_t fwrite(const void *p, size_t s, size_t n, FILE *f);
int fseek(FILE *f, long off, int whence);
long ftell(FILE *f);
int fflush(FILE *f);
int fputc(int c, FILE *f);
int fputs(const char *s, FILE *f);
int putchar(int c);
int puts(const char *s);
int fgetc(FILE *f);
char *fgets(char *s, int n, FILE *f);
int printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int fprintf(FILE *f, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int vfprintf(FILE *f, const char *fmt, va_list ap);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int snprintf(char *buf, size_t n, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
int sprintf(char *buf, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int sscanf(const char *s, const char *fmt, ...);
void perror(const char *s);
int remove(const char *path);
int rename(const char *a, const char *b);
#define putc fputc
#define getc fgetc
#endif
