/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * IMGBENCH.PRG - what a scanned page costs: a CCITT group 4 picture decoded (src/ccitt.c) and
 * shrunk to grey (src/rscale.c), or a JPEG decoded straight to the size it is shown at (common's
 * pic.c, TJpgDec) and dithered to black and white.
 *
 * Reads C:\IMGJOB.CFG, a line for each picture:  FOLDER\TAG WIDTH
 * (tools/imgdump wrote FOLDER\TAG.INF and TAG.G4 or TAG.JPG; WIDTH is the width to shrink to),
 * writes one JSON line each to C:\IMGBENCH.OUT and NatFeats.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tos.h"
#include "pdf.h"
#include "ccitt.h"
#include "rscale.h"
#include "pic.h"
#ifdef PIC_LIBJPEG
#include "dspjpeg.h"
#endif

long nf_find(const char *name);
long nf_call2(long id, long arg);
extern long heap_use, heap_peak;

static long out_h = -1;

static void emit(const char *line)
{
	long n = (long)strlen(line);

	nf_debug(line);
	if (out_h >= 0) {
		Fwrite((short)out_h, n, (void *)line);
		Fwrite((short)out_h, 2L, (void *)"\r\n");
	}
}

static long load(const char *path, unsigned char **buf)
{
	long h = Fopen(path, 0), n;

	if (h < 0)
		return -1;
	n = Fseek(0L, (short)h, 2);
	Fseek(0L, (short)h, 0);
	*buf = malloc((size_t)n + 16);
	if (!*buf || Fread((short)h, n, *buf) != n)
		n = -1;
	Fclose((short)h);
	return n;
}

/* the INF line: eight numbers and the filter's name */
static int parse_inf(const char *s, long *v, char *filt)
{
	int i;
	char *e;

	for (i = 0; i < 8; i++) {
		v[i] = strtol(s, &e, 10);
		if (e == s)
			return -1;
		s = e;
	}
	while (*s == ' ')
		s++;
	for (i = 0; *s && *s != '\r' && *s != '\n' && i < 19; )
		filt[i++] = *s++;
	filt[i] = 0;
	return 0;
}

static void g4(const char *base, const char *inf, long outw_want)
{
	long w, h, k, cols, rows, b1, dw, dh;
	(void)0;
	char path[80], line[600], filt[20];
	unsigned char *data, *row, *out;
	long n, r, rows_ok = 0, t0, t_dec, t_scale, t_init;
	int f, outw, outh;
	CCITT c;
	RSCALE rs;

	{
		long v[8];

		if (parse_inf(inf, v, filt) != 0)
			return;
		w = v[0]; h = v[1]; k = v[2]; cols = v[3]; rows = v[4]; b1 = v[5]; dw = v[6]; dh = v[7];
	}
	snprintf(path, sizeof path, "%s.G4", base);
	n = load(path, &data);
	if (n < 0) {
		snprintf(line, sizeof line, "{\"pic\":\"%s\",\"error\":\"no data\"}", base);
		emit(line);
		return;
	}
	for (f = 2; f < 8 && (cols + f - 1) / f > outw_want; f *= 2)
		;
	outw = (int)((cols + f - 1) / f);
	outh = (int)((h + f - 1) / f);
	row = malloc((size_t)(cols + 7) / 8 + 8);
	out = malloc((size_t)outw * (size_t)outh + 8);
	heap_peak = heap_use;
	/* pass 1: decoding alone */
	t0 = pdf_us();
	if (ccitt_open(&c, data, n, (int)cols, (int)k, 0) != 0) {
		emit("{\"error\":\"ccitt_open\"}");
		return;
	}
	t_init = pdf_us() - t0;
	t0 = pdf_us();
	for (r = 0; r < h && ccitt_row(&c, row); r++)
		rows_ok++;
	t_dec = pdf_us() - t0;
	ccitt_close(&c);
	/* pass 2: decoding and shrinking */
	ccitt_open(&c, data, n, (int)cols, (int)k, 0);
	rs_init(&rs, (int)cols, f);
	t0 = pdf_us();
	{
		unsigned char *o = out;

		for (r = 0; r < h && ccitt_row(&c, row); r++)
			if (rs_row(&rs, row, o))
				o += outw;
	}
	t_scale = pdf_us() - t0;
	snprintf(line, sizeof line,
		 "{\"pic\":\"%s\",\"kind\":\"g4\",\"w\":%ld,\"h\":%ld,\"bytes\":%ld,\"mpx\":%ld,\"shrink\":%d,\"out\":\"%dx%d\",\"rows_ok\":%ld,"
		 "\"tables_us\":%ld,\"decode_us\":%ld,\"decode_shrink_us\":%ld,\"heap_kb\":%ld}",
		 base, w, h, n, w * h / 1000000, f, outw, outh, rows_ok, t_init, t_dec, t_scale, heap_peak / 1024);
	emit(line);
	rs_close(&rs);
	ccitt_close(&c);
	free(data);
	free(row);
	free(out);
}

static void jpg(const char *base, const char *inf, long outw_want)
{
	long w, h, k, cols, rows, b1, dw, dh;
	(void)0;
	char path[80], line[600], filt[20];
	unsigned char *data, *dith;
	long n, t0, t_dec, t_dith = 0;
	int tw, th, why = 0;
	PIC *p;
	PIC_PAL pal;
	static const unsigned char bw[2][3] = { { 0, 0, 0 }, { 255, 255, 255 } };

	{
		long v[8];

		if (parse_inf(inf, v, filt) != 0)
			return;
		w = v[0]; h = v[1]; k = v[2]; cols = v[3]; rows = v[4]; b1 = v[5]; dw = v[6]; dh = v[7];
	}
	snprintf(path, sizeof path, "%s.JPG", base);
	n = load(path, &data);
	if (n < 0) {
		snprintf(line, sizeof line, "{\"pic\":\"%s\",\"error\":\"no data\"}", base);
		emit(line);
		return;
	}
	tw = (int)outw_want;
	if (tw > w)
		tw = (int)w;
	th = (int)(h * tw / w);
	heap_peak = heap_use;
	t0 = pdf_us();
	p = pic_decode(data, n, tw, th, 40000000L, &why);
	t_dec = pdf_us() - t0;
#ifdef PIC_LIBJPEG
	/* the same picture again with the DSP doing the transform and colours */
	if (p && dspjpeg_present()) {
		long t_dsp, dsp_ok;
		PIC *q;

		pic_dsp = &dspjpeg_port;
		t0 = pdf_us();
		q = pic_decode(data, n, tw, th, 40000000L, &why);
		t_dsp = pdf_us() - t0;
		dsp_ok = q != NULL;
		/* the first use checks the DSP on a known picture, so once more */
		if (q) {
			pic_free(q);
			t0 = pdf_us();
			q = pic_decode(data, n, tw, th, 40000000L, &why);
			t_dsp = pdf_us() - t0;
		}
		snprintf(line, sizeof line, "{\"pic\":\"%s\",\"kind\":\"jpeg-dsp\",\"dsp_us\":%ld,\"ok\":%ld,\"state\":\"%s\",\"scale\":%d}", base,
			 t_dsp, dsp_ok, pic_dsp_state(), q ? q->scale : -1);
		emit(line);
		if (q)
			pic_free(q);
		pic_dsp = NULL;
	}
#endif
	if (p) {
		memset(&pal, 0, sizeof pal);
		pic_pal_init(&pal, 2, bw);
		dith = malloc((size_t)p->w * (size_t)p->h);
		t0 = pdf_us();
		if (dith)
			pic_dither(p, &pal, dith);
		t_dith = pdf_us() - t0;
		free(dith);
		pic_pal_free(&pal);
	}
	snprintf(line, sizeof line,
		 "{\"pic\":\"%s\",\"kind\":\"jpeg\",\"w\":%ld,\"h\":%ld,\"bytes\":%ld,\"mpx\":%ld,\"out\":\"%dx%d\",\"scale\":%d,\"ok\":%d,\"why\":%d,"
		 "\"decode_us\":%ld,\"dither_bw_us\":%ld,\"heap_kb\":%ld}",
		 base, w, h, n, w * h / 1000000, tw, th, p ? p->scale : -1, p != NULL, why, t_dec, t_dith, heap_peak / 1024);
	emit(line);
	if (p)
		pic_free(p);
	free(data);
}

static const char *nf_name;
static long nf_found;

static void sup_find(void)
{
	nf_found = nf_find(nf_name);
}

int main(void)
{
	static char cfg[4096], line[300];
	long h, n;
	char *p;

	out_h = Fcreate("C:\\IMGBENCH.OUT", 0);
	h = Fopen("C:\\IMGJOB.CFG", 0);
	if (h < 0) {
		emit("{\"error\":\"no C:\\\\IMGJOB.CFG\"}");
	} else {
		n = Fread((short)h, (long)sizeof cfg - 1, cfg);
		Fclose((short)h);
		cfg[n < 0 ? 0 : n] = 0;
		snprintf(line, sizeof line, "{\"sys\":true,\"st_free_kb\":%ld,\"tt_free_kb\":%ld}", (long)Malloc(-1L) / 1024, (long)Mxalloc(-1L, 1) / 1024);
		emit(line);
		for (p = cfg; *p;) {
			char base[48], pth[64], inf[200];
			int k;
			long outw, ih;

			while (*p == '\r' || *p == '\n' || *p == ' ')
				p++;
			if (!*p)
				break;
			for (k = 0; *p && *p != ' ' && k < 47;)
				base[k++] = *p++;
			base[k] = 0;
			while (*p == ' ')
				p++;
			outw = atoi(p);
			while (*p && *p != '\n')
				p++;
			snprintf(pth, sizeof pth, "C:\\%s.INF", base);
			ih = Fopen(pth, 0);
			if (ih < 0)
				continue;
			k = (int)Fread((short)ih, 199L, inf);
			Fclose((short)ih);
			inf[k < 0 ? 0 : k] = 0;
			{
				char full[64];

				snprintf(full, sizeof full, "C:\\%s", base);
				if (strstr(inf, "CCITT"))
					g4(full, inf, outw ? outw : 640);
				else if (strstr(inf, "DCT"))
					jpg(full, inf, outw ? outw : 640);
			}
		}
	}
	if (out_h >= 0)
		Fclose((short)out_h);
	nf_name = "NF_EXIT";
	Supexec(sup_find);
	if (nf_found > 0)
		nf_call2(nf_found, 0);
	return 0;
}
