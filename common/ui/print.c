/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * print.c - printing through the GDOS (print.h). WDIALOG's PRN_SETTINGS are read and written only at the
 * fields WDIALOG documents: first and last page (20, 22), copies (24), the driver (32), the device (128).
 */
#include <string.h>
#include "gem.h"
#include "print.h"

#define S_FIRST 20
#define S_LAST 22
#define S_COPIES 24
#define S_DRIVER 32
#define S_DEVICE 128
#define PDLG_PRINT 0x0001
#define PDLG_COPIES 0x0010
#define PDLG_OK 2

static short sh(void *s, int o) { return *(short *)((char *)s + o); }
static void sh_set(void *s, int o, short v) { *(short *)((char *)s + o) = v; }

int print_setup(PRINTJOB *pj, short pages)
{
	memset(pj, 0, sizeof(*pj));
	pj->pages = pages > 0 ? pages : 1;
	pj->first = 1, pj->last = pj->pages, pj->copies = 1;
	if (!vq_gdos())
		return 0;
	pj->dlg = pdlg_create(1);
	if (pj->dlg)
		pj->set = pdlg_new_settings(pj->dlg);
	if (!pj->set) {
		if (pj->dlg)
			pdlg_delete(pj->dlg);
		pj->dlg = 0;
		return 2;
	}
	sh_set(pj->set, S_FIRST, 1);
	sh_set(pj->set, S_LAST, pj->pages);
	return 1;
}

int print_dialog(PRINTJOB *pj, const char *name)
{
	short f, l;
	if (!pj->set)
		return 1;
	if (pdlg_do(pj->dlg, pj->set, name, PDLG_PRINT | PDLG_COPIES) != PDLG_OK)
		return 0;
	f = sh(pj->set, S_FIRST), l = sh(pj->set, S_LAST);
	pj->first = f < 1 ? 1 : f > pj->pages ? pj->pages : f;
	pj->last = l < pj->first ? pj->first : l > pj->pages ? pj->pages : l;
	pj->copies = sh(pj->set, S_COPIES) > 0 ? sh(pj->set, S_COPIES) : 1;
	return 1;
}

void print_device(PRINTJOB *pj, short driver, const char *device)
{
	if (!pj->set)
		return;
	if (driver)
		sh_set(pj->set, S_DRIVER, driver);
	if (device) {
		strncpy((char *)pj->set + S_DEVICE, device, 127);
		((char *)pj->set)[S_DEVICE + 127] = 0;
	}
}

int print_open(PRINTJOB *pj)
{
	short wo[57], d, base, i;
	memset(wo, 0, sizeof(wo));
	if (pj->set) {
		base = graf_handle(&d, &d, &d, &d);
		pj->h = v_opnprn(base, pj->set, wo);
	} else {
		short wi[11];
		wi[0] = 21;
		for (i = 1; i < 10; i++)
			wi[i] = 1;
		wi[10] = 2;
		pj->h = v_opnwk(wi, wo);
	}
	if (pj->h <= 0) {
		pj->h = 0;
		return 0;
	}
	pj->w = (short)(wo[0] + 1), pj->ht = (short)(wo[1] + 1);
	pj->um_w = wo[3], pj->um_h = wo[4];
	for (pj->planes = 0, d = (short)(wo[13] > 0 ? wo[13] : 2); d > 1; d >>= 1)
		pj->planes++;
	return 1;
}

void print_page(PRINTJOB *pj, int last)
{
	if (!pj->h)
		return;
	v_updwk(pj->h);
	if (!last)
		v_clrwk(pj->h);
}

void print_end(PRINTJOB *pj)
{
	if (pj->h)
		v_clswk(pj->h);
	pj->h = 0;
	if (pj->set)
		pdlg_free_settings(pj->set);
	if (pj->dlg)
		pdlg_delete(pj->dlg);
	pj->set = pj->dlg = 0;
}
