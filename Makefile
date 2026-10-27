# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
#
#   make            build/ (host tools)
#   make test       host unit tests
#   make measure    the measurement run on this computer (corpus: tools/fetch-corpus.py)
#   make dist       not yet: EPDF has no program to release
#
# The version is read from VERSION and never written by the build.

VERSION_STR := $(shell cat VERSION)
BSSL   := common/third_party/bearssl
CC     ?= cc
CFLAGS ?= -O2 -g
HCFLAGS = $(CFLAGS) -std=gnu99 -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -Wno-unused-function \
          -Isrc -Icommon/src -I$(BSSL)/inc -I$(BSSL)/src

PDF_SRC  = src/pdfobj.c src/pdfxref.c src/pdffilt.c src/pdfcrypt.c src/pdfdoc.c src/pdftext.c src/pdfmeas.c
COMMON_SRC = common/src/inflate.c common/src/util.c
BSSL_SRC = $(addprefix $(BSSL)/src/, hash/md5.c hash/sha2small.c hash/sha2big.c \
           symcipher/aes_big_cbcdec.c symcipher/aes_big_cbcenc.c symcipher/aes_big_dec.c symcipher/aes_big_enc.c symcipher/aes_common.c \
           codec/dec32be.c codec/enc32be.c codec/dec32le.c codec/enc32le.c codec/dec64be.c codec/enc64be.c)
HDRS     = $(wildcard src/*.h common/src/*.h)

.PHONY: all test measure dist help clean atari fttest imgbench
all: build/pdfmeasure build/glyphjob build/imgdump atari
build:
	mkdir -p build
build/pdfmeasure: tools/pdfmeasure.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/pdfmeasure.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/glyphjob: tools/glyphjob.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/glyphjob.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/imgdump: tools/imgdump.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/imgdump.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/test_ccitt: tests/test_ccitt.c src/ccitt.c src/ccitt.h src/ccitt_tab.h | build
	$(CC) $(HCFLAGS) -fsanitize=address,undefined -o $@ tests/test_ccitt.c src/ccitt.c

test: build/test_ccitt
	python3 tests/ccitt.py
measure: build/pdfmeasure
	@echo "see tools/measure.py"
dist help:
	@echo "EPDF has no program to release yet"; exit 1
clean:
	rm -rf build/pdfmeasure

# ---------------------------------------------------------------- the Atari programs
# Built like the family's other programs: a stock m68k ELF compiler, the shared runtime in
# common/atari, and common/tools/elf2tos.py for the TOS header and fixups.
XCC      = m68k-linux-gnu-gcc
GCCINC  := $(shell $(XCC) -print-file-name=include 2>/dev/null)
XOPT    ?= -O2
XFLAGS   = $(XCPU) $(XOPT) -fomit-frame-pointer -ffreestanding -fno-builtin -fno-pic -fno-pie \
           -fno-tree-loop-distribute-patterns -fno-store-merging -fno-asynchronous-unwind-tables \
           -ffunction-sections -fdata-sections -Wall -Wextra -Werror=implicit-function-declaration \
           -Wno-sign-compare -Wno-unused-parameter -Wno-unused-function -std=gnu99 -DPDF_TOS \
           -nostdinc -isystem $(GCCINC) -Iatari/include -Icommon/atari/include -Icommon/atari -Isrc -Icommon/src \
           -I$(BSSL)/inc -I$(BSSL)/src
XLDFLAGS = $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld \
           -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments
RT_C     = common/atari/libc.c common/atari/tos.c
STACK_SIZE ?= 49152
ATARI_SRC = $(PDF_SRC) src/pdfio_tos.c atari/measure.c $(COMMON_SRC) $(BSSL_SRC) $(RT_C)

# the 68000 build runs on every ST, TT and Falcon; the 68030 one on the TT and the Falcon
define ATARI_BUILD
build/$(1)/%.o: %.c $(HDRS) | build/$(1)
	@mkdir -p $$(dir $$@)
	@echo CC $$<
	@$$(XCC) $$(XFLAGS) -c -o $$@ $$<
build/$(1)/crt0.o: common/atari/crt0.S | build/$(1)
	$$(XCC) $$(XCPU) -DSTACK_SIZE=$$(STACK_SIZE) -c -o $$@ $$<
build/$(1)/nf.o: common/atari/nf.S | build/$(1)
	$$(XCC) $$(XCPU) -c -o $$@ $$<
build/$(1):
	mkdir -p $$@
endef
$(eval $(call ATARI_BUILD,m68000))
$(eval $(call ATARI_BUILD,m68030))
$(eval $(call ATARI_BUILD,m68030lj))

OBJS = $(patsubst %.c,%.o,$(ATARI_SRC))
binary:
	mkdir -p binary
binary/MEASURE.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
binary/MEASURE.PRG: $(addprefix build/m68000/,$(OBJS)) build/m68000/crt0.o build/m68000/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68000/measure.elf \
	  build/m68000/crt0.o build/m68000/nf.o $(addprefix build/m68000/,$(OBJS)) -lgcc
	python3 common/tools/elf2tos.py build/m68000/measure.elf $@
binary/MEAS30.PRG: XCPU = -m68030
binary/MEAS30.PRG: $(addprefix build/m68030/,$(OBJS)) build/m68030/crt0.o build/m68030/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68030/measure.elf \
	  build/m68030/crt0.o build/m68030/nf.o $(addprefix build/m68030/,$(OBJS)) -lgcc
	python3 common/tools/elf2tos.py build/m68030/measure.elf $@

# ---------------------------------------------------------------- FreeType (third_party/freetype, 2.13.3)
FT      := third_party/freetype
FT_SRC  := $(addprefix $(FT)/src/, base/ftbase.c base/ftinit.c base/ftdebug.c base/ftbbox.c base/ftglyph.c base/ftbitmap.c \
           base/ftsynth.c sfnt/sfnt.c truetype/truetype.c cff/cff.c type1/type1.c psaux/psaux.c pshinter/pshinter.c \
           psnames/psmodule.c raster/raster.c smooth/smooth.c autofit/autofit.c) $(FT)/epdf/ftsystem.c
FT_DEFS  = -include $(FT)/epdf/ftprep.h -DFT2_BUILD_LIBRARY '-DFT_CONFIG_OPTIONS_H="epdf/ftoption.h"' '-DFT_CONFIG_MODULES_H="epdf/ftmodule.h"' \
           $(FT_EXTRA)
FT_INC   = -I$(FT)/include -I$(FT) -I$(FT)/epdf/include
define FT_BUILD
build/$(1)/ft/%.o: $(FT)/src/%.c | build/$(1)
	@mkdir -p $$(dir $$@)
	@echo CC $$<
	@$$(XCC) $$(XFLAGS) $$(FT_DEFS) $$(FT_INC) -Wno-unused-variable -Wno-implicit-fallthrough -Wno-cast-function-type -Wno-missing-field-initializers -Wno-dangling-pointer -I$$(dir $$<) -c -o $$@ $$<
build/$(1)/ft/epdf/%.o: $(FT)/epdf/%.c | build/$(1)
	@mkdir -p $$(dir $$@)
	@echo CC $$<
	@$$(XCC) $$(XFLAGS) $$(FT_DEFS) $$(FT_INC) -c -o $$@ $$<
build/$(1)/ft/setjmp.o: $(FT)/epdf/setjmp.S | build/$(1)
	@mkdir -p $$(dir $$@)
	$$(XCC) $$(XCPU) -c -o $$@ $$<
endef
$(eval $(call FT_BUILD,m68000))
$(eval $(call FT_BUILD,m68030))
FT_OBJS = $(patsubst $(FT)/src/%.c,ft/%.o,$(filter $(FT)/src/%,$(FT_SRC))) ft/epdf/ftsystem.o ft/setjmp.o
FTTEST_SRC = atari/fttest.c src/pdfio_tos.c $(COMMON_SRC) $(RT_C)
build/m68000/atari/fttest.o build/m68030/atari/fttest.o: XFLAGS += $(FT_DEFS) $(FT_INC)

binary/FTTEST.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
binary/FTTEST.PRG: $(addprefix build/m68000/,$(FT_OBJS)) $(addprefix build/m68000/,$(patsubst %.c,%.o,$(FTTEST_SRC))) build/m68000/crt0.o build/m68000/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68000/fttest.elf \
	  build/m68000/crt0.o build/m68000/nf.o $(addprefix build/m68000/,$(patsubst %.c,%.o,$(FTTEST_SRC))) \
	  $(addprefix build/m68000/,$(FT_OBJS)) -lgcc
	python3 common/tools/elf2tos.py build/m68000/fttest.elf $@
binary/FTTEST30.PRG: XCPU = -m68030
binary/FTTEST30.PRG: $(addprefix build/m68030/,$(FT_OBJS)) $(addprefix build/m68030/,$(patsubst %.c,%.o,$(FTTEST_SRC))) build/m68030/crt0.o build/m68030/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68030/fttest.elf \
	  build/m68030/crt0.o build/m68030/nf.o $(addprefix build/m68030/,$(patsubst %.c,%.o,$(FTTEST_SRC))) \
	  $(addprefix build/m68030/,$(FT_OBJS)) -lgcc
	python3 common/tools/elf2tos.py build/m68030/fttest.elf $@
# the picture test: CCITT, the shrinker and common's pic.c (TJpgDec, PNG, GIF)
T         := common/third_party
PIC_SRC    = common/src/pic.c common/src/png.c common/src/jpegdc.c $(T)/tjpgd/tjpgd.c $(T)/nsgif/src/gif.c $(T)/nsgif/src/lzw.c
IMG_SRC    = atari/imgbench.c src/ccitt.c src/rscale.c src/pdfio_tos.c $(PIC_SRC) $(COMMON_SRC) $(RT_C)
IMG_OBJ    = $(patsubst %.c,%.o,$(IMG_SRC))
build/m68000/atari/imgbench.o build/m68030/atari/imgbench.o build/m68000/common/src/pic.o build/m68030/common/src/pic.o \
build/m68000/common/src/png.o build/m68030/common/src/png.o build/m68000/common/src/jpegdc.o build/m68030/common/src/jpegdc.o \
build/m68000/common/third_party/tjpgd/tjpgd.o build/m68030/common/third_party/tjpgd/tjpgd.o \
build/m68000/common/third_party/nsgif/src/gif.o build/m68030/common/third_party/nsgif/src/gif.o \
build/m68000/common/third_party/nsgif/src/lzw.o build/m68030/common/third_party/nsgif/src/lzw.o: \
	XFLAGS += -fwrapv -I$(T)/tjpgd -I$(T)/nsgif/include -I$(T)/nsgif/src -Wno-error
binary/IMGBENCH.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
binary/IMGBENCH.PRG: $(addprefix build/m68000/,$(IMG_OBJ)) build/m68000/crt0.o build/m68000/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68000/imgbench.elf \
	  build/m68000/crt0.o build/m68000/nf.o $(addprefix build/m68000/,$(IMG_OBJ)) -lgcc
	python3 common/tools/elf2tos.py build/m68000/imgbench.elf $@
binary/IMGB30.PRG: XCPU = -m68030
binary/IMGB30.PRG: $(addprefix build/m68030/,$(IMG_OBJ)) build/m68030/crt0.o build/m68030/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68030/imgbench.elf \
	  build/m68030/crt0.o build/m68030/nf.o $(addprefix build/m68030/,$(IMG_OBJ)) -lgcc
	python3 common/tools/elf2tos.py build/m68030/imgbench.elf $@
# the same with libjpeg-turbo and the DSP (EWeb's JPEG path), 68030 only
LJ        := $(T)/libjpeg-turbo
LJ_C      := jaricom jcomapi jdapimin jdapistd jdarith jdcoefct jdcolor jddctmgr jdhuff jdinput jdmainct jdmarker \
             jdmaster jdmerge jdphuff jdpostct jdsample jdtrans jidctfst jidctint jidctred jmemmgr jmemnobs \
             jquant1 jquant2 jutils
LJ_SRC    := $(addprefix $(LJ)/src/,$(addsuffix .c,$(LJ_C)))
IMGLJ_SRC  = atari/imgbench.c src/ccitt.c src/rscale.c src/pdfio_tos.c common/src/pic.c common/src/png.c common/src/pic_lj8.c \
             $(T)/nsgif/src/gif.c $(T)/nsgif/src/lzw.c common/atari/dspjpeg.c $(LJ_SRC) $(COMMON_SRC) $(RT_C)
IMGLJ_OBJ  = $(patsubst %.c,%.o,$(IMGLJ_SRC))
build/m68030lj/%.o: XFLAGS += -DPIC_LIBJPEG -DNO_GETENV -DNO_PUTENV -DAERA_NO_FLOAT -fwrapv -I$(LJ)/src -I$(LJ)/config \
                              -I$(T)/nsgif/include -I$(T)/nsgif/src -Wno-error -w
binary/IMGDSP.PRG: XCPU = -m68030
binary/IMGDSP.PRG: $(addprefix build/m68030lj/,$(IMGLJ_OBJ)) build/m68030lj/crt0.o build/m68030lj/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68030lj/imgdsp.elf \
	  build/m68030lj/crt0.o build/m68030lj/nf.o $(addprefix build/m68030lj/,$(IMGLJ_OBJ)) -lgcc
	python3 common/tools/elf2tos.py build/m68030lj/imgdsp.elf $@
imgbench: binary/IMGBENCH.PRG binary/IMGB30.PRG binary/IMGDSP.PRG
atari: binary/MEASURE.PRG binary/MEAS30.PRG
fttest: binary/FTTEST.PRG binary/FTTEST30.PRG
