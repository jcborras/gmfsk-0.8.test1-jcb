/*
     dominoextx.c  --  DominoEX modulator
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

#include <stdlib.h>
#include <string.h>

#include "dominoex.h"
#include "trx.h"
#include "fft.h"
#include "varicode.h"
#include "misc.h"
#include "snd.h"
#include "filter.h"
#include "main.h"

static inline complex mixer(struct trx *trx, complex in)
{
	struct dominoex *m = (struct dominoex *) trx->modem;
	complex z;
	float f;

	f = trx->frequency - trx->bandwidth / 2 + trx->txoffset;

	/* Basetone is always at 1000 Hz */
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

static void sendsymbol(struct trx *trx, int sym)
{
	struct dominoex *m = (struct dominoex *) trx->modem;
	complex z;
	int i;
    int tone;

    tone = (m->txprevtone + 2 + sym) % m->numtones;
    m->txprevtone = tone;

	if (trx->reverse)
		tone = (m->numtones - 1) - tone;

	fft_clear_inbuf(m->fft);
	c_re(m->fft->in[((m->doublespaced) ? 2 : 1) * tone + m->basetone]) = 1.0;

	fft_run(m->fft);

	for (i = 0; i < m->symlen; i++) {
		z = mixer(trx, m->fft->out[i]);
		trx->outbuf[i] = c_re(z);
	}

	sound_write(trx->outbuf, m->symlen);
}

static void sendchar(struct trx *trx, unsigned char c, int secondary)
{
	unsigned char *code = dominoex_varienc(c, secondary);
    int sym;

    sendsymbol(trx, code[0]);
    /* Continuation nibbles all have the MSB set */
    for (sym = 1; sym < 3; sym++) {
        if (code[sym] & 0x8) 
            sendsymbol(trx, code[sym]);
        else
            break;
    }

	trx_put_echo_char(c);
}

static void sendidle(struct trx *trx)
{
	sendchar(trx, 0, 1);	/* <NUL> */
}

static void flushtx(struct trx *trx)
{
	int i;

	/* flush the varicode decoder at the other end */
    for (i = 0; i < 4; i++)
        sendidle(trx);

}

int dominoex_txprocess(struct trx *trx)
{
	struct dominoex *m = (struct dominoex *) trx->modem;
	int i;

	if (trx->tune) {
		m->txstate = TX_STATE_TUNE;
		trx->tune = 0;
	}

	switch (m->txstate) {
	case TX_STATE_TUNE:
		sendsymbol(trx, 0);
		m->txstate = TX_STATE_FINISH;
		break;

	case TX_STATE_PREAMBLE:
        sendidle(trx);
		m->txstate = TX_STATE_START;
		break;

	case TX_STATE_START:
		sendchar(trx, '\r', 0);
		sendchar(trx, 2, 0);		/* STX */
		sendchar(trx, '\r', 0);
		m->txstate = TX_STATE_DATA;
		break;

	case TX_STATE_DATA:
		i = trx_get_tx_char();

		if (i == -1)
			sendidle(trx);
		else
			sendchar(trx, i, 0);

		if (trx->stopflag)
			m->txstate = TX_STATE_END;

		break;

	case TX_STATE_END:
		i = trx_get_tx_char();

		if (i == -1) {
			sendchar(trx, '\r', 0);
			sendchar(trx, 4, 0);		/* EOT */
			sendchar(trx, '\r', 0);
			m->txstate = TX_STATE_FLUSH;
		} else
			sendchar(trx, i, 0);

		break;

	case TX_STATE_FLUSH:
		flushtx(trx);
		return -1;

	case TX_STATE_FINISH:
		return -1;

	}
	return 0;
}
