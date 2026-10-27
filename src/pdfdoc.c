/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdfdoc.c - opening a document and the page tree.
 */
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfint.h"

static void set_err(PDOC *d, const char *s)
{
	strncpy(d->err, s, sizeof d->err - 1);
	d->err[sizeof d->err - 1] = 0;
}

static int check_root(PDOC *d)
{
	POBJ *root, *pages;

	if (!d->trailer)
		return 0;
	root = pd_get(d, d->trailer, "Root");
	if (!root || root->t != PO_DICT)
		return 0;
	pages = pd_get(d, root, "Pages");
	if (!pages || pages->t != PO_DICT)
		return 0;
	d->root = root;
	return 1;
}

static void start_crypt(PDOC *d)
{
	POBJ *enc = d->trailer ? pd_get(d, d->trailer, "Encrypt") : NULL;

	if (enc && !d->crypt) {
		POBJ *id = pd_get(d, d->trailer, "ID");

		d->encrypted = pc_open(d, enc, id) == 0 ? 1 : 2;
		/* what was read before the key was known is read again */
		memset(d->cache, 0, sizeof d->cache);
	}
}

int pdf_open(PDOC *d, const char *path)
{
	unsigned char head[1024];
	long n, i;

	memset(d, 0, sizeof *d);
	d->os_num = -1;
	d->h = pdf_os_open(path, &d->size);
	if (!d->h) {
		set_err(d, "cannot open the file");
		return -1;
	}
	d->wcap = PWIN;
	d->win = pdf_malloc(d, d->wcap);
	if (!d->win) {
		set_err(d, "no memory");
		return -1;
	}
	d->cur = &d->doc_a;
	n = pdf_rd(d, 0, head, sizeof head);
	for (i = 0; i + 8 < n; i++)
		if (memcmp(head + i, "%PDF-1.", 7) == 0) {
			d->ver = head[i + 7] - '0';
			break;
		}
	if (i + 8 >= n) {
		set_err(d, "not a PDF file");
		return -1;
	}
	if (xref_load(d) == 0) {
		start_crypt(d);
		if (d->encrypted == 2 || check_root(d))
			goto ok;
		/* the table loads but leads nowhere: rebuild */
	}
	{
		long r0 = d->st.reads;

		(void)r0;
		memset(d->cache, 0, sizeof d->cache);
		if (xref_rebuild(d) != 0) {
			set_err(d, "damaged: no catalogue found");
			return -1;
		}
		start_crypt(d);
		if (d->encrypted != 2 && !check_root(d)) {
			set_err(d, "damaged: no page tree");
			return -1;
		}
	}
ok:
	if (d->encrypted == 2) {
		set_err(d, "encrypted: needs a password or an unsupported handler");
		return -2;
	}
	d->cur = &d->a;
	return 0;
}

void pdf_close(PDOC *d)
{
	pdf_text_cleanup(d);
	pdf_free(d, d->pageidx, d->npages * 4);
	pdf_free(d, d->os_buf, d->os_len);
	pdf_free(d, d->os_off, d->os_n * 2 * (long)sizeof(long));
	pdf_free(d, d->xe, d->nxe * (long)sizeof(PXE));
	pdf_free(d, d->win, d->wcap);
	pc_close(d);
	pa_free(d, &d->a);
	pa_free(d, &d->doc_a);
	if (d->h)
		pdf_os_close(d->h);
	d->h = NULL;
}

/* ------------------------------------------------------------------ page tree */
/* The page objects, in order. A node whose /Count equals the number of its
   kids has only pages among them, so they are taken without being read:
   a flat tree of 600 pages costs one array, not 600 objects. */
static long pages_walk(PDOC *d, POBJ *node, int32_t *idx, long n, long cap, int depth)
{
	POBJ *kids, *count;
	long i, nk;

	if (depth > 24 || !node || node->t != PO_DICT)
		return n;
	kids = pd_get(d, node, "Kids");
	if (!kids || kids->t != PO_ARR) {
		return n;
	}
	nk = kids->a;
	count = pd_get(d, node, "Count");
	if (count && count->t == PO_INT && count->a == nk) {
		for (i = 0; i < nk; i++) {
			POBJ *k = &kids->u.v[i];

			if (k->t == PO_REF) {
				if (idx && n < cap)
					idx[n] = k->a;
				n++;
			}
		}
		return n;
	}
	for (i = 0; i < nk; i++) {
		POBJ *kr = &kids->u.v[i], *k;

		if (kr->t != PO_REF)
			continue;
		k = pdf_get(d, kr->a);
		if (!k || k->t != PO_DICT)
			continue;
		if (pd_getraw(k, "Kids"))
			n = pages_walk(d, k, idx, n, cap, depth + 1);
		else {
			if (idx && n < cap)
				idx[n] = kr->a;
			n++;
		}
	}
	return n;
}

long pdf_pages(PDOC *d)
{
	POBJ *pages;
	long n, cnt;

	if (d->pageidx)
		return d->npages;
	pages = pd_get(d, d->root, "Pages");
	if (!pages)
		return -1;
	n = pages_walk(d, pages, NULL, 0, 0, 0);
	if (n <= 0)
		return -1;
	cnt = n;
	d->pageidx = pdf_malloc(d, cnt * 4);
	if (!d->pageidx)
		return -1;
	d->npages = cnt;
	if (pages_walk(d, pages, d->pageidx, 0, cnt, 0) != cnt)
		return -1;
	return cnt;
}

POBJ *pdf_page(PDOC *d, long n, POBJ **res, POBJ **box, long *rot)
{
	POBJ *pg, *p;
	int depth;

	if (pdf_pages(d) < 0 || n < 0 || n >= d->npages)
		return NULL;
	d->skip_kids = 1;
	pg = pdf_get(d, d->pageidx[n]);
	if (!pg || pg->t != PO_DICT) {
		d->skip_kids = 0;
		return NULL;
	}
	*res = NULL;
	*box = NULL;
	*rot = 0;
	{
		int have_rot = 0;

		for (p = pg, depth = 0; p && depth < 32; depth++) {
			POBJ *x, *par;

			if (!*res && (x = pd_get(d, p, "Resources")) && x->t == PO_DICT)
				*res = x;
			if (!*box && (x = pd_get(d, p, "CropBox")) && x->t == PO_ARR)
				*box = x;
			if (!*box && (x = pd_get(d, p, "MediaBox")) && x->t == PO_ARR)
				*box = x;
			if (!have_rot && (x = pd_get(d, p, "Rotate"))) {
				*rot = x->a;
				have_rot = 1;
			}
			par = pd_get(d, p, "Parent");
			if (!par || par == p)
				break;
			p = par;
		}
	}
	d->skip_kids = 0;
	return pg;
}
