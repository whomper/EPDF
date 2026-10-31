# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
#
#   make            binary/EPDF.PRG (one program for every ST, TT and Falcon); host tools in build/
#   make test       host unit tests, and no FPU instruction in the Atari programs
#   make help       binary/EPDF.HYP and .REF from docs/GUIDE.md (HCP=path to hcp:
#                   common/tools/build-hcp.sh DIR builds it)
#   make dist       a release's files in dist/: each file, and EPDF-x.y.zip
#   make icons      icons/ and src/gem/icon*.h from common/icons/EPDF.txt, EPDFDOC.txt
#   make gui-test   EPDF.PRG driven in Hatari (tests/hatari/gui.py)
#   make measure-atari  the stage 0 measuring programs (MEASURE.PRG and the others)
#   make measure    the measurement run on this computer (corpus: tools/fetch-corpus.py)
#
# binary/ is what make builds, never committed: releases carry the program.
# The version is read from VERSION and never written by the build.

VERSION_STR := $(shell cat VERSION)
BSSL   := common/third_party/bearssl
CC     ?= cc
CFLAGS ?= -O2 -g
HCFLAGS = $(CFLAGS) -fwrapv -std=gnu99 -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -Wno-unused-function \
          -Isrc -Icommon/src -I$(BSSL)/inc -I$(BSSL)/src

PDF_SRC  = src/pdfobj.c src/pdfxref.c src/pdffilt.c src/pdfcrypt.c src/pdfdoc.c src/pdftext.c src/pdfmeas.c
COMMON_SRC = common/src/inflate.c common/src/util.c common/src/futil.c
BSSL_SRC = $(addprefix $(BSSL)/src/, hash/md5.c hash/sha2small.c hash/sha2big.c \
           symcipher/aes_big_cbcdec.c symcipher/aes_big_cbcenc.c symcipher/aes_big_dec.c symcipher/aes_big_enc.c symcipher/aes_common.c \
           codec/dec32be.c codec/enc32be.c codec/dec32le.c codec/enc32le.c codec/dec64be.c codec/enc64be.c)
HDRS     = $(wildcard src/*.h src/gem/*.h src/view/*.h common/src/*.h common/ui/*.h common/atari/*.h common/atari/include/*.h)

.PHONY: all test measure dist help clean atari measure-atari fttest imgbench gui-test
all: build/pdfmeasure build/pdftext build/glyphjob build/imgdump binary/EPDF.PRG
build:
	mkdir -p build
build/pdfmeasure: tools/pdfmeasure.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/pdfmeasure.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/pdftext: tools/pdftext.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/pdftext.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/glyphjob: tools/glyphjob.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/glyphjob.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/imgdump: tools/imgdump.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/imgdump.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/test_ccitt: tests/test_ccitt.c src/ccitt.c src/ccitt.h src/ccitt_tab.h | build
	$(CC) $(HCFLAGS) -fsanitize=address,undefined -o $@ tests/test_ccitt.c src/ccitt.c

build/pdfmeasure_asan: tools/pdfmeasure.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined -o $@ tools/pdfmeasure.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/test_textview: tests/test_textview.c src/view/textview.c src/view/textview.h | build
	$(CC) -Isrc/view -Wall -Wextra -O2 -o $@ tests/test_textview.c src/view/textview.c

build/test_search: tests/test_search.c src/view/search.c src/view/search.h | build
	$(CC) -Isrc/view -Wall -Wextra -O2 -o $@ tests/test_search.c src/view/search.c
build/test_inf: tests/test_inf.c src/view/inf.c src/view/inf.h | build
	$(CC) -Isrc/view -Wall -Wextra -O2 -o $@ tests/test_inf.c src/view/inf.c
VIEW_TEST_SRC = src/view/docview.c src/view/textview.c src/view/tstore.c src/pdfio_host.c
build/test_tstore build/test_docview: build/test_%: tests/test_%.c $(VIEW_TEST_SRC) $(wildcard src/view/*.h) src/plat.h | build
	$(CC) -Isrc -Isrc/view -Icommon/src -Wall -Wextra -O2 -o $@ $< $(VIEW_TEST_SRC)

test: build/test_ccitt build/test_textview build/test_inf build/test_tstore build/test_docview build/test_search
	python3 tests/ccitt.py
	build/test_textview
	build/test_inf
	build/test_tstore build
	build/test_docview
	build/test_search
	@if [ -f build/m68000/epdf.elf ]; then python3 common/tools/oddaccess.py build/m68000/epdf.elf; fi
	tools/nofpu.sh build/m68000/epdf.elf build/m68030/measure.elf build/m68030/fttest.elf
measure: build/pdfmeasure
	@echo "see tools/measure.py"
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
           -ffunction-sections -fdata-sections -fwrapv -Wall -Wextra -Werror=implicit-function-declaration \
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
# the CPU follows the folder, whichever program asks for an object first
build/m68000/%: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
build/m68030/%: XCPU = -m68030 -msoft-float
build/m68030lj/%: XCPU = -m68030 -msoft-float
binary/LAUNCH.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
# the GEM building blocks as the family builds them, for size (at -O2 GCC 13
# once filled dd.c's char name[] with long moves at an odd stack offset:
# common c80905c copies it now; oddaccess.py in make test watches for more)
build/m68000/common/ui/%: XOPT = -Os
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
binary/MEAS30.PRG: XCPU = -m68030 -msoft-float
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
binary/FTTEST30.PRG: XCPU = -m68030 -msoft-float
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
binary/IMGB30.PRG: XCPU = -m68030 -msoft-float
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
binary/IMGDSP.PRG: XCPU = -m68030 -msoft-float
binary/IMGDSP.PRG: $(addprefix build/m68030lj/,$(IMGLJ_OBJ)) build/m68030lj/crt0.o build/m68030lj/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68030lj/imgdsp.elf \
	  build/m68030lj/crt0.o build/m68030lj/nf.o $(addprefix build/m68030lj/,$(IMGLJ_OBJ)) -lgcc
	python3 common/tools/elf2tos.py build/m68030lj/imgdsp.elf $@
imgbench: binary/IMGBENCH.PRG binary/IMGB30.PRG binary/IMGDSP.PRG
# the 68030 programs again with the TOS header flags 7: loaded and Malloc'd in TT RAM where there is some
binary/MEAS30F.PRG: build/m68030/measure.elf | binary
	PRGFLAGS=7 python3 common/tools/elf2tos.py $< $@
binary/FTTEST30F.PRG: build/m68030/fttest.elf | binary
	PRGFLAGS=7 python3 common/tools/elf2tos.py $< $@
atari: binary/EPDF.PRG binary/LAUNCH.PRG
measure-atari: binary/MEASURE.PRG binary/MEAS30.PRG binary/MEAS30F.PRG
fttest: binary/FTTEST.PRG binary/FTTEST30.PRG

# ---------------------------------------------------------------- EPDF.PRG (the GEM program)
# one build for every machine (68000); the text view of the pages
build/m68000/version.c: FORCE | build/m68000
	@printf '/* written by the Makefile: do not edit */\nconst char app_version[] = "%s";\n' "$(VERSION_STR)" > $@.new
	@if cmp -s $@.new $@; then rm $@.new; else mv $@.new $@; fi
FORCE:
.PHONY: FORCE
GEM_SRC = src/gem/epdf.c src/gem/doc.c src/gem/textwin.c src/gem/find.c src/gem/gemui.c src/gem/menu.c src/gem/about.c \
          src/view/textview.c src/view/docview.c src/view/tstore.c src/view/inf.c src/view/search.c common/ui/bgwork.c common/ui/twin.c \
          common/atari/gem.c common/atari/crash.c common/ui/mbar.c common/ui/aboutbox.c common/ui/aboutline.c \
          common/ui/alert.c common/ui/dialog.c common/ui/field.c common/ui/av.c common/ui/text.c common/ui/clip.c common/ui/iconify.c common/ui/help.c common/ui/dd.c common/ui/popup.c \
          common/src/charset.c common/src/bidi.c common/ui/accmem.c common/atari/bigbuf.c
GEM_OBJ = $(patsubst %.c,build/m68000/%.o,$(GEM_SRC)) build/m68000/version.o
$(patsubst %.c,build/m68000/%.o,$(GEM_SRC)): XFLAGS += -Isrc/gem -Isrc/view -Icommon/ui
build/m68000/version.o: build/m68000/version.c
	$(XCC) $(XFLAGS) -c -o $@ $<
build/m68000/coro.o: src/gem/coro.S | build/m68000
	$(XCC) -m68000 -c -o $@ $<
build/m68000/crash_s.o: common/atari/crash.S | build/m68000
	$(XCC) $(XCPU) -c -o $@ $<
binary/EPDF.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
EPDF_OBJS = $(GEM_OBJ) $(addprefix build/m68000/,$(patsubst %.c,%.o,$(RT_C))) $(addprefix build/m68000/,$(patsubst %.c,%.o,$(PDF_SRC) src/pdfio_tos.c $(COMMON_SRC))) $(addprefix build/m68000/,$(patsubst %.c,%.o,$(BSSL_SRC)))
binary/EPDF.PRG: $(EPDF_OBJS) build/m68000/crt0.o build/m68000/nf.o build/m68000/crash_s.o build/m68000/coro.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68000/epdf.elf \
	  build/m68000/crt0.o build/m68000/nf.o build/m68000/crash_s.o build/m68000/coro.o $(filter-out %/crt0.o %/nf.o %/crash_s.o %/coro.o %/link.ld,$(filter build/%,$^)) -lgcc
	PRGFLAGS=7 python3 common/tools/elf2tos.py build/m68000/epdf.elf $@

# starts a program with a command line, for the rigs (atari/launch.c)
binary/LAUNCH.PRG: build/m68000/atari/launch.o build/m68000/common/atari/gem.o build/m68000/common/atari/tos.o build/m68000/common/atari/libc.o build/m68000/crt0.o build/m68000/nf.o common/atari/link.ld | binary
	$(XCC) -m68000 -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68000/launch.elf \
	  build/m68000/crt0.o build/m68000/nf.o build/m68000/atari/launch.o build/m68000/common/atari/gem.o build/m68000/common/atari/tos.o build/m68000/common/atari/libc.o -lgcc
	python3 common/tools/elf2tos.py build/m68000/launch.elf $@

# ---------------------------------------------------------------- the icons (guidelines 5.5)
# drawn in AERA-Atari (common/icons/EPDF.txt, EPDFDOC.txt for the documents); made into
# icons/ (committed), src/gem/icon*.h and docs/icon*.png by common/tools/appicon.py
icons: common/icons/EPDF.txt common/icons/EPDFDOC.txt common/tools/appicon.py
	mkdir -p icons docs
	python3 common/tools/appicon.py EPDF --rsc icons/EPDF.RSC --rscnv icons/EPDFNV.RSC \
		--icn icons/EPDF.ICN --mask icons/EPDFMK.ICN --headers src/gem --docs docs
	python3 common/tools/appicon.py EPDFDOC --rsc icons/EPDFDOC.RSC --rscnv icons/EPDFDNV.RSC --label PDF
.PHONY: icons

# ---------------------------------------------------------------- the guide and the release
# EPDF.HYP from docs/GUIDE.md with the family's front page (docs/front.json,
# common/tools/helpfront.py), and EPDF.REF, its pages' names for ST-Guide's index
HCP ?= hcp
help: binary/EPDF.HYP
binary/EPDF.HYP: docs/GUIDE.md tools/mkhelp.py VERSION docs/front.json docs/help/WINDOW.IMG \
                 common/help/EPDF.IMG common/tools/helpfront.py common/tools/gemimg.py | binary
	mkdir -p build/help
	python3 tools/mkhelp.py docs/GUIDE.md VERSION build/help/EPDF.STG
	python3 common/tools/helpfront.py page docs/front.json build/help/EPDF.STG build/help/EPDF.STG
	$(HCP) -q -o $@ build/help/EPDF.STG >/dev/null
	test -s binary/EPDF.REF

# what a user needs, by name: the program, its guide, the licences and the icons
DIST  := binary/EPDF.PRG binary/EPDF.HYP binary/EPDF.REF NOTICES.TXT
ICONS := icons/EPDF.RSC icons/EPDFNV.RSC icons/EPDF.ICN icons/EPDFMK.ICN icons/EPDFDOC.RSC icons/EPDFDNV.RSC
dist: binary/EPDF.PRG help
	rm -rf dist
	mkdir -p dist/zip/icons
	cp $(DIST) $(ICONS) dist/
	cp $(DIST) dist/zip/
	cp $(ICONS) dist/zip/icons/
	cd dist/zip && zip -q -9 -r ../EPDF-$(VERSION_STR).zip .
	rm -rf dist/zip

gui-test: binary/EPDF.PRG binary/LAUNCH.PRG
	python3 tests/hatari/gui.py ST:mono build/gui

# docs/screenshot.png and docs/help/WINDOW.IMG again, from Hatari (commit them)
shots: binary/EPDF.PRG binary/LAUNCH.PRG
	python3 tests/hatari/shots.py build/shots
.PHONY: shots

# the speed rig (the guidelines' Speed): EPDF.PRG with common/atari/gem.c built
# -DAES_PROFILE=20, each stretch of 20 ms or more between event calls on the
# emulator's console (AESGAP); tests/hatari/speed.py checks them on the stock machines
build/aprof/gem.o: common/atari/gem.c $(HDRS) | build
	mkdir -p build/aprof
	$(XCC) $(XFLAGS) -m68000 -mstrict-align -fdisable-tree-bswap -DAES_PROFILE=20 -c -o $@ $<
build/aprof/textwin.o: src/gem/textwin.c $(HDRS) | build
	mkdir -p build/aprof
	$(XCC) $(XFLAGS) -m68000 -mstrict-align -fdisable-tree-bswap -Isrc/gem -Isrc/view -Icommon/ui -DEPDF_TIMES -c -o $@ $<
build/aprof/EPDF.PRG: binary/EPDF.PRG build/aprof/gem.o build/aprof/textwin.o
	$(XCC) -m68000 -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/aprof/epdf.elf \
	  build/m68000/crt0.o build/m68000/nf.o build/m68000/crash_s.o build/m68000/coro.o \
	  $(filter-out build/m68000/common/atari/gem.o build/m68000/src/gem/textwin.o,$(filter build/%.o,$(EPDF_OBJS))) \
	  build/aprof/gem.o build/aprof/textwin.o -lgcc
	PRGFLAGS=7 python3 common/tools/elf2tos.py build/aprof/epdf.elf $@
speed-test: build/aprof/EPDF.PRG binary/LAUNCH.PRG
	python3 tests/hatari/speed.py build/speed
.PHONY: speed-test
