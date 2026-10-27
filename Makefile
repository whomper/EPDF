# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
#
#   make            build/ (host tools) and binary/ (Atari programs)
#   make test       host unit tests
#   make measure    the measurement run on this computer (corpus in build/pdfs)
#   make dist       not yet: EPDF has no program to release
#
# The version is read from VERSION and never written by the build.

VERSION_STR := $(shell cat VERSION)

.PHONY: all test measure dist help
all:
	@echo "EPDF $(VERSION_STR): nothing to build yet"
test:
	@echo "EPDF $(VERSION_STR): no tests yet"
measure:
	@echo "no measurement tool yet"
dist help:
	@echo "EPDF has no program to release yet"; exit 1
