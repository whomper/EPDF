/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef CF_UNISTD_H
#define CF_UNISTD_H
#include <stddef.h>
typedef long ssize_t;
typedef long off_t;
ssize_t read(int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
off_t lseek(int fd, off_t off, int whence);
int close(int fd);
int unlink(const char *path);
int rmdir(const char *path);
int fork(void);
int execl(const char *path, const char *arg, ...);
int dup2(int a, int b);
int pipe(int fd[2]);
void _exit(int code) __attribute__((noreturn));
#endif
