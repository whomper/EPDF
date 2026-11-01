/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdf.h - the PDF reader: file access, cross-reference table and
 * streams, objects, filters, encryption, the page tree and text
 * extraction.
 *
 * Built to run on a 68000 ST in a few hundred KB: a file is read through
 * one window (never whole), the cross-reference table is 8 bytes an
 * object, objects are parsed into an arena that is dropped between pages,
 * and the content interpreter keeps its numbers in 16.16 fixed point.
 * The same sources build for the host (tests, the measurement run).
 */
#ifndef PDF_H
#define PDF_H

#include <stdint.h>
#include <stddef.h>
#include "util.h"

typedef int32_t fx;			/* 16.16 */
#define FX_ONE 65536
#define FX_INT(i) ((fx)(i) * FX_ONE)

/* ------------------------------------------------------------------
 * counters: what a page costs. The driver takes the difference of two
 * snapshots (PSTAT is plain longs)
 * ------------------------------------------------------------------ */
typedef struct {
	long reads, bytes_read;		/* calls to the OS and bytes they gave */
	long objs;			/* objects parsed (from the file or an object stream) */
	long objstm_loaded, objstm_bytes;	/* object streams decoded, their size unpacked */
	long streams, raw_in, out;	/* streams decoded, bytes in (as stored), bytes out */
	long crypt_bytes;		/* bytes decrypted */
	long ops_text, ops_path, ops_paint, ops_other;
	long glyphs;			/* characters shown */
	long fonts;			/* fonts loaded */
	long images, image_px, image_raw;	/* pictures drawn, their pixels and stored bytes */
	long img_dct, img_jpx, img_ccitt, img_jbig2, img_flate, img_other;
	long forms;			/* form XObjects run */
	long chunks;			/* pieces of text */
	long mem, mem_peak;		/* bytes malloc'd now, the most */
	long oom;			/* allocations refused (out of memory) */
	long tounicode_ent;		/* ToUnicode entries read */
	long font_prog, font_prog_bytes;	/* embedded font programs met (not loaded) */
	long t_stream, t_font, t_asm;	/* clock ticks in pdf_stream, in font_load (its streams apart), in assemble */
} PSTAT;

/* ------------------------------------------------------------------ objects */
enum { PO_NULL, PO_BOOL, PO_INT, PO_REAL, PO_STR, PO_NAME, PO_ARR, PO_DICT, PO_REF };

typedef struct pobj POBJ;
typedef struct pent PENT;
struct pobj {
	uint8_t t;
	uint8_t flag;			/* dict: 1 = has stream data */
	uint16_t gen;			/* ref */
	int32_t a;			/* bool/int value, real as fx, ref number, string/name length, array/dict count */
	union {
		const char *p;		/* string/name: NUL-terminated */
		POBJ *v;		/* array */
		PENT *e;		/* dict */
	} u;
};
struct pent {
	const char *key;
	POBJ v;
};

typedef struct ach {
	struct ach *next;
	long size, used;
} ACH;
typedef struct {
	ACH *head;
	long total;
} PARENA;

/* the cross-reference table: 4 bytes an object (a 34000-object manual's
   was 265 KB at 8, 0.3). The top two bits the kind (0 not set, 1 in use,
   2 compressed, 3 free); in use, the file offset (generation 0);
   compressed, the object stream's number (18 bits) and the index in it
   (12, a hint). What doesn't fit (a generation, a bigger number) is all
   ones below the kind, and the entry is in xx (xe_get, pdfobj.c) */
typedef uint32_t PXE;
typedef struct {
	int32_t num;
	uint32_t a;			/* in use: file offset; compressed: the object stream's number */
	uint16_t b;			/* in use: generation; compressed: index in the stream */
	uint8_t t;
	uint8_t pad;
} PXX;

#define PCACHE 256
typedef struct { int32_t num; POBJ *o; } PCENT;

typedef struct pcrypt PCRYPT;

#define PWIN 16384
#define POWIN 2048			/* the objects' window: a few objects near each other */

typedef struct pdoc {
	/* file */
	void *h;
	long size;
	unsigned char *win;		/* a stream's bytes read in turn (pdf_rd_seq) */
	long wbase, wlen, wcap;
	unsigned char *owin;		/* objects, and the rest (pdf_rd) */
	long obase, olen;
	/* cross-reference table and trailer */
	PXE *xe;
	long nxe;
	PXX *xx;			/* the entries that don't fit in 4 bytes */
	long nxx, capxx;
	int xx_sorted;
	POBJ *trailer;
	POBJ *root;
	int repaired;			/* the table was rebuilt by scanning the file */
	PCRYPT *crypt;
	int encrypted;			/* 0 no, 1 yes and opened, 2 needs a password / unsupported */
	/* objects */
	PARENA doc_a;			/* what lives as long as the document (trailer, root) */
	PARENA a;			/* what lives for a page */
	PARENA *cur;			/* where parsing allocates */
	PCENT cache[PCACHE];
	/* big objects (a page tree's shared resources, a dictionary of every
	   page's pictures) parsed once into doc_a and kept, not parsed again
	   for every page */
	PCENT kept[8];
	int nkept;
	long kept_bytes;
	/* the last object stream, decoded */
	long os_num;
	unsigned char *os_buf;
	long os_len, os_n, os_first;
	long *os_off;			/* os_n numbers, os_n offsets (pairs) */
	PSTAT st;
	int ver;			/* %PDF-1.x */
	long npages;
	long (*clock)(void);		/* a clock for the t_ counters, or NULL */
	int32_t *pageidx;		/* object number of each page, built once */
	void *fc;			/* the fonts kept from page to page (pdftext.c) */
	int skip_kids;			/* while a page's inherited attributes are read: leave /Kids arrays unparsed */
	/* asked every 8 tokens and 32 glyphs while a page's text is read: it may
	   take its time (hand the machine back); 1 stops the page (the page then
	   has stopped set; read it again later) */
	int (*stop)(void *ctx);
	void *stop_ctx;
	int stopping;
	unsigned pause_tick;
	char err[96];
} PDOC;

/* ------------------------------------------------------------------ files */
int pdf_open(PDOC *d, const char *path);	/* 0 ok; sets err */
/* the same with the stop hook set from the start: an open that takes long
   (an AES-256 file's key) calls it between pieces, and fails when it says
   stop */
int pdf_open_hook(PDOC *d, const char *path, int (*stop)(void *ctx), void *ctx);
void pdf_close(PDOC *d);
void pdf_page_reset(PDOC *d);			/* drop the page arena and cache */

/* ------------------------------------------------------------------ objects */
POBJ *pdf_get(PDOC *d, long num);		/* the object, NULL if missing; the dict for a stream */
POBJ *pdf_res(PDOC *d, POBJ *o);		/* follow references */
POBJ *pd_get(PDOC *d, POBJ *dict, const char *key);	/* resolved; NULL if absent or null */
POBJ *pd_getraw(POBJ *dict, const char *key);	/* as written (maybe a PO_REF) */
long pd_int(PDOC *d, POBJ *dict, const char *key, long def);
const char *pd_name(PDOC *d, POBJ *dict, const char *key);	/* NULL */
POBJ *pa_at(POBJ *arr, long i);			/* element i, NULL out of range */
int pdf_is_name(const POBJ *o, const char *name);

/* ------------------------------------------------------------------ streams */
/* The decoded bytes of the stream of object o (a dict with flag 1) into out.
   Image filters (DCT, JPX, CCITT, JBIG2) stop the chain: *terminal says
   which (a static name) and out holds what is left. max: bytes at most.
   Returns 0 ok, -1 damaged or missing. */
int pdf_stream(PDOC *d, POBJ *o, long onum, SBUF *out, const char **terminal, long max);
/* the stream's stored length, without reading it */
long pdf_stream_len(PDOC *d, POBJ *o);
/* a stream's bytes a piece at a time (stored, or Flate with no predictor;
   decrypted as they are read): NULL where it can't be, and pdf_stream
   decodes it whole.
   pdf_stream_read gives up to n bytes, 0 at the end, -1 damaged */
typedef struct PCSRC PCSRC;
PCSRC *pdf_stream_open(PDOC *d, POBJ *o, long onum);
long pdf_stream_read(PCSRC *s, unsigned char *out, long n);
void pdf_stream_close(PCSRC *s);
/* the same for a scan or a JPEG (CCITTFaxDecode, DCTDecode; alone or
   after Flate): the coded bytes, before the picture's own filter */
PCSRC *pdf_stream_open_coded(PDOC *d, POBJ *o, long onum);
/* the first want bytes of a stream (all of it if shorter), a piece at a
   time between the reader's stop hook; *whole: 1 if that is all of it */
int pdf_stream_head(PDOC *d, POBJ *o, long onum, long want, SBUF *out, int *whole);

/* ------------------------------------------------------------------ pages */
long pdf_pages(PDOC *d);			/* page count; -1 if the tree is broken */
/* page n (0-based): its dict with Resources, MediaBox, Rotate inherited
   into *res, *box (4 numbers as an array) and *rot; NULL if none */
POBJ *pdf_page(PDOC *d, long n, POBJ **res, POBJ **box, long *rot);

/* ------------------------------------------------------------------ text */
typedef struct {
	long obj;			/* the font program's object number */
	int kind;			/* 1 Type 1, 2 TrueType, 3 CFF / OpenType, 4 standard font (none embedded) */
	long len;			/* its stored length */
} PFPROG;

typedef struct {
	int32_t x, y, size;		/* the glyph's origin and the font's size on the page (16.16, page points) */
	uint32_t uni;			/* its character; for a Type 0 font its code (the glyph index) */
	int comp;			/* a Type 0 font */
	long prog;			/* the font program's object number; -1: a standard font; -2 (a gap): none */
	int kind;			/* the program's kind, as PFPROG */
	const char *base;		/* the font's name (static, for a standard font) */
} PGLYPH;

typedef struct {
	long obj;			/* the image XObject */
	long w, h;			/* pixels */
	fx dw, dh;			/* the size it is drawn at, page points */
	fx top;				/* its top edge on the page (page points, y up) */
	char filter[16];
} PIMG;

typedef struct {
	SBUF text;			/* the page's text, UTF-8 */
	PIMG img[64];			/* with rec set: the pictures drawn */
	int nimg;
	PGLYPH *gl;			/* with rec set: every glyph shown, to draw them (tools/glyphjob) */
	long ngl, capgl;
	int rec;
	PFPROG fp[24];			/* the font programs the page's fonts use */
	int nfp;
	long lines;
	long glyphs;
	long unmapped;			/* characters with no Unicode */
	long fonts_nomap;		/* fonts with no ToUnicode and no usable encoding */
	int failed;			/* the page could not be read */
	int stopped;			/* d->stop said stop: the text is not whole */
	int pnext;			/* the next picture to place (pdftext.c) */
	int pics;			/* set by the caller: each picture drawn (an image XObject)
					   gets a line of its own where it stands among the lines,
					   "\x7F<object> <width>x<height>" (its size in points) */
	int soft;			/* set by the caller: a line that goes on in the next ends
					   in '\v' (a paragraph's soft break), else in '\n' */
} PTEXT;
void ptext_init(PTEXT *t);
void ptext_free(PTEXT *t);
/* run page n's content and set its text in reading order */
int pdf_page_text(PDOC *d, long n, PTEXT *t);

/* ------------------------------------------------------------------ fixed point */
#if defined(__mc68000__) && !defined(__mc68020__)
/* one mulu.w: 16 x 16 -> 32 bits (GCC would call the software multiply for 32-bit operands) */
static inline uint32_t mulu16(uint32_t a, uint32_t b)
{
	__asm__("mulu.w %1,%0" : "+d"(a) : "d"(b));
	return a;
}
#else
static inline uint32_t mulu16(uint32_t a, uint32_t b)
{
	return (a & 0xFFFF) * (b & 0xFFFF);
}
#endif

static inline fx fxmul(fx a, fx b)
{
	int neg = 0;
	uint32_t ua, ub, ah, al, bh, bl, r;

	/* most matrix entries are 0 or 1.0 */
	if (a == FX_ONE)
		return b;
	if (b == FX_ONE)
		return a;
	if (!a || !b)
		return 0;
	if (a < 0) { ua = (uint32_t)-a; neg = 1; } else ua = (uint32_t)a;
	if (b < 0) { ub = (uint32_t)-b; neg ^= 1; } else ub = (uint32_t)b;
	ah = ua >> 16; al = ua & 0xFFFF; bh = ub >> 16; bl = ub & 0xFFFF;
	r = (mulu16(ah, bh) << 16) + mulu16(ah, bl) + mulu16(al, bh) + (mulu16(al, bl) >> 16);
	return neg ? -(fx)r : (fx)r;
}

/* allocation, counted */
void *pdf_malloc(PDOC *d, long n);
void *pdf_realloc(PDOC *d, void *p, long oldn, long n);
void pdf_free(PDOC *d, void *p, long n);

/* the OS file layer (pdfio_*.c) */
void *pdf_os_open(const char *path, long *size);
long pdf_os_read(void *h, long off, void *buf, long n);
void pdf_os_close(void *h);
/* the file window's size: 32 KB on a 68020 or better, 16 KB on a 68000
   (the guidelines' 6.5: big pieces, never small ones) */
long pdf_os_window(void);
/* time in milliseconds, for the drivers */
long pdf_ms(void);
long pdf_us(void);				/* the Atari's finer clock (pdfio_tos.c) */

#endif
