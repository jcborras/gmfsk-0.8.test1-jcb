/*
 *    dominoexrx.c  --  DominoEX demodulator
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
                                                                                
/* AIX requires this to be the first thing in the file.  */
#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "dominoex.h"
#include "filter.h"
#include "sfft.h"
#include "varicode.h"
#include "misc.h"

static void recvchar(struct dominoex *m, int c)
{
	if (c == -1)
		return;
	if (c & 0x100) /* Discard the secondary channel */
		return;

	trx_put_rx_char(c);
}

static void decodesymbol(struct trx *trx, unsigned char curtone, unsigned char prevtone)
{
	struct dominoex *m = (struct dominoex *) trx->modem;
	int c, i, sym, ch;

	/* Decode the IFK+ sequence, which results in a single nibble */
	if (trx->reverse) {
		curtone = m->numtones - 1 - curtone;
		prevtone = m->numtones - 1 - prevtone;
	}

	c = (curtone - prevtone - 2 + m->numtones) % m->numtones;

	/* If the new symbol is the start of a new character (MSB is low), complete the previous character */
	if (!(c & 0x8)) {
		if (m->symcounter <= MAX_VARICODE_LEN) {
			sym = 0;
			for (i = 0; i < m->symcounter; i++)
				sym |= m->symbolbuf[i] << (4 * i);
			ch = dominoex_varidec(sym);
			recvchar(m, ch);
		}

		m->symcounter = 0;
	}

	/* Add to the symbol buffer. Position 0 is the newest symbol. */
	for (i = MAX_VARICODE_LEN-1; i >= 0; i--)
		m->symbolbuf[i] = m->symbolbuf[i-1];
	m->symbolbuf[0] = c;

	/* Increment the counter, but clamp at max+1 to avoid overflow */
	m->symcounter++;
	if (m->symcounter > MAX_VARICODE_LEN + 1)
		m->symcounter = MAX_VARICODE_LEN + 1;

}

static complex mixer(struct trx *trx, complex in)
{
	struct dominoex *m = (struct dominoex *) trx->modem;
	complex z;
	float f;

	f = trx->frequency - trx->bandwidth / 2;

	/* Basetone is always 1000 Hz */
	f -= 1000.0;

	c_re(z) = cos(m->phaseacc);
	c_im(z) = sin(m->phaseacc);

	z = cmul(z, in);

	m->phaseacc -= 2.0 * M_PI * f / trx->samplerate;

	if (m->phaseacc > M_PI)
		m->phaseacc -= 2.0 * M_PI;
	else if (m->phaseacc < M_PI)
		m->phaseacc += 2.0 * M_PI;

	return z;
}

static int harddecode(struct trx *trx, complex *in)
{
	struct dominoex *m = (struct dominoex *) trx->modem;
	double x, max = 0.0;
	int i, symbol = 0;

	in += m->basetone;

	for (i = 0; i < m->numtones; i++) {
		if (m->doublespaced)
			x = cmod(in[i*2]) + 0.5 * cmod(in[i*2-1]) + 0.5 * cmod(in[i*2+1]);
		else
			x = cmod(in[i]);

		if (x > max) {
			max = x;
			symbol = i;
		}
	}

	return symbol;
}

static void update_syncscope(struct dominoex *m)
{
	float *data;
	int i, j;

	data = alloca(2 * m->symlen * sizeof(float));

	for (i = 0; i < 2 * m->symlen; i++) {
		j = (i + m->pipeptr) % (2 * m->symlen);
		data[i] = m->pipe[j].mod[m->prev1symbol];
	}

	trx_set_scope(data, 2 * m->symlen, TRUE);
}

static void synchronize(struct dominoex *m)
{
	int i, j, syn = -1;
	float val, max = 0.0;

	if (m->currsymbol == m->prev1symbol)
		return;
	if (m->prev1symbol == m->prev2symbol)
		return;

	j = m->pipeptr;

	for (i = 0; i < 2 * m->symlen; i++) {
		val = m->pipe[j].mod[m->prev1symbol];

		if (val > max) {
			max = val;
			syn = i;
		}

		j = (j + 1) % (2 * m->symlen);
	}

	m->synccounter += (int) floor((syn - m->symlen) / 16.0 + 0.5);
}

static void afc(struct trx *trx)
{
	struct dominoex *m = (struct dominoex *) trx->modem;
	complex z;
	float x;

	if (trx->afcon == FALSE || trx->metric < trx->mfsk_squelch)
		return;

	if (m->currsymbol != m->prev1symbol)
		return;

	z = ccor(m->prev1vector, m->currvector);
	x = carg(z) / m->symlen / (2.0 * M_PI / trx->samplerate);

	if (x > -m->tonespacing / 2.0 &&  x < m->tonespacing / 2.0)
		trx_set_freq(trx->frequency + (x / 8.0));
}

int dominoex_rxprocess(struct trx *trx, float *buf, int len)
{
	struct dominoex *m = (struct dominoex *) trx->modem;
	complex z, *bins;
	int i;

	while (len-- > 0) {
		/* create analytic signal... */
		c_re(z) = c_im(z) = *buf++;

		filter_run(m->hilbert, z, &z);

		/* ...so it can be shifted in frequency */
		z = mixer(trx, z);

		filter_run(m->filt, z, &z);

		/* feed it to the sliding FFT */
		bins = sfft_run(m->sfft, z);

		/* copy current vector to the pipe */
		if (m->doublespaced) {
			for (i = 0; i < m->numtones; i++)
				m->pipe[m->pipeptr].mod[i] = cmod(bins[i * 2 + m->basetone])
					/*+ 0.5 * cmod(bins[i * 2 - 1 + m->basetone])
					+ 0.5 * cmod(bins[i * 2 + 1 + m->basetone])*/;
		} else {
			for (i = 0; i < m->numtones; i++)
				m->pipe[m->pipeptr].mod[i] = cmod(bins[i + m->basetone]);
		}

		if (--m->synccounter <= 0) {
			m->synccounter = m->symlen;

			m->currsymbol = harddecode(trx, bins);
			m->currvector = bins[m->currsymbol * ((m->doublespaced) ? 2 : 1) + m->basetone];

			/* decode symbol */
            decodesymbol(trx, m->currsymbol, m->prev1symbol);

			/* update the scope */
			update_syncscope(m);

			/* symbol sync */
			synchronize(m);

			/* frequency tracking */
			afc(trx);

			m->prev2symbol = m->prev1symbol;
			m->prev2vector = m->prev1vector;
			m->prev1symbol = m->currsymbol;
			m->prev1vector = m->currvector;
		}

		m->pipeptr = (m->pipeptr + 1) % (2 * m->symlen);
	}

	return 0;
}
