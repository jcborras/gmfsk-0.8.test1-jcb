/*
 *    dominoex.c  --  DominoEX modem
 *
 *    Copyright (C) 2001, 2002, 2003
 *      Tomi Manninen (oh2bns@sral.fi)
 *    Copyright (C) 2006
 *      Hamish Moffatt (hamish@debian.org)
 *
 *    This file is part of gMFSK.
 *
 *    gMFSK is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    gMFSK is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with gMFSK; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdlib.h>
#include <glib.h>

#include "dominoex.h"
#include "trx.h"
#include "fft.h"
#include "sfft.h"
#include "filter.h"

static void dominoex_txinit(struct trx *trx)
{
	struct dominoex *m = (struct dominoex *) trx->modem;

	m->txstate = TX_STATE_PREAMBLE;
	m->txprevtone = 0;
	m->counter = 0;
}

static void dominoex_rxinit(struct trx *trx)
{
	struct dominoex *m = (struct dominoex *) trx->modem;

	m->rxstate = RX_STATE_DATA;
	m->synccounter = 0;
	m->symcounter = 0;
	m->met1 = 0.0;
	m->met2 = 0.0;

	m->counter = 0;
}

static void dominoex_free(struct dominoex *s)
{
	if (s) {
		fft_free(s->fft);
		sfft_free(s->sfft);
		filter_free(s->hilbert);

		g_free(s->pipe);

		filter_free(s->filt);

		g_free(s);
	}
}

static void dominoex_destructor(struct trx *trx)
{
	struct dominoex *s = (struct dominoex *) trx->modem;

	dominoex_free(s);

	trx->modem = NULL;

	trx->txinit = NULL;
	trx->rxinit = NULL;
	trx->txprocess = NULL;
	trx->rxprocess = NULL;
	trx->destructor = NULL;
}

void dominoex_init(struct trx *trx)
{
	struct dominoex *s;
	double bw, cf, flo, fhi;

	s = g_new0(struct dominoex, 1);

	s->numtones = 18;

	switch (trx->mode) {
	/* 8kHz modes */
	case MODE_DOMINOEX4:
		s->symlen = 2048;
		s->basetone = 256;		/* 1000 Hz */
		s->doublespaced = 1;
		trx->samplerate = 8000;
		break;

	case MODE_DOMINOEX8:
		s->symlen = 1024;
		s->basetone = 128;		/* 1000 Hz */
		s->doublespaced = 1;
		trx->samplerate = 8000;
		break;

	case MODE_DOMINOEX16:
		s->symlen = 512;
		s->basetone = 64;		/* 1000 Hz */
		s->doublespaced = 0;
		trx->samplerate = 8000;
		break;

	/* 11.025kHz modes */
	case MODE_DOMINOEX5:
		s->symlen = 2048;
		s->basetone = 186;		/* 1001.3 Hz */
		s->doublespaced = 1;
		trx->samplerate = 11025;
		break;

	case MODE_DOMINOEX11:
		s->symlen = 1024;
		s->basetone = 93;		/* 1001.3 Hz */
		s->doublespaced = 0;
		trx->samplerate = 11025;
		break;

	case MODE_DOMINOEX22:
		s->symlen = 512;
		s->basetone = 46;		/* 990 Hz */
		s->doublespaced = 0;
		trx->samplerate = 11025;
		break;

	default:
		dominoex_free(s);
		return;
	}

	s->tonespacing = (double) (trx->samplerate * ((s->doublespaced) ? 2 : 1)) / s->symlen;

	if (!(s->fft = fft_init(s->symlen, FFT_FWD))) {
		g_warning("dominoex_init: init_fft failed\n");
		dominoex_free(s);
		return;
	}
	if (!(s->sfft = sfft_init(s->symlen, s->basetone, s->basetone + s->numtones * ((s->doublespaced) ? 2 : 1)))) {
		g_warning("dominoex_init: init_sfft failed\n");
		dominoex_free(s);
		return;
	}
	if (!(s->hilbert = filter_init_hilbert(37, 1))) {
		g_warning("dominoex_init: init_hilbert failed\n");
		dominoex_free(s);
		return;
	}

	s->pipe = g_new0(struct rxpipe, 2 * s->symlen);

	bw = (s->numtones - 1) * s->tonespacing;
	cf = 1000.0 + bw / 2.0;

	flo = (cf - bw) / trx->samplerate;
	fhi = (cf + bw) / trx->samplerate;

	if ((s->filt = filter_init_bandpass(127, 1, flo, fhi)) == NULL) {
		g_warning("dominoex_init: filter_init failed\n");
		dominoex_free(s);
		return;
	}

	trx->modem = s;

	trx->txinit = dominoex_txinit;
	trx->rxinit = dominoex_rxinit;

	trx->txprocess = dominoex_txprocess;
	trx->rxprocess = dominoex_rxprocess;

	trx->destructor = dominoex_destructor;

	trx->fragmentsize = s->symlen;
	trx->bandwidth = (s->numtones - 1) * s->tonespacing;
}
