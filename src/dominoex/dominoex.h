/*
 *    dominoex.h  --  DominoEX modem
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

#ifndef _DOMINOEX_H
#define _DOMINOEX_H

#include "cmplx.h"
#include "trx.h"
#include "fftfilt.h"
#include "delay.h"
#include "varicode.h"


struct rxpipe {
	double mod[32];	/* numtones <= 18 */
	int symbol;
};

struct dominoex {
	/*
	 * Common stuff
	 */
	double phaseacc;

	int symlen;
	int numtones;
	int basetone;
	int doublespaced;
	double tonespacing;

	int counter;

	/*
	 * RX related stuff
	 */
	int rxstate;

	struct filter *hilbert;
	struct sfft *sfft;

	struct filter *filt;

	struct rxpipe *pipe;
	unsigned int pipeptr;

	unsigned int datashreg;

	complex currvector;
	complex prev1vector;
	complex prev2vector;

	int currsymbol;
	int prev1symbol;
	int prev2symbol;

	float met1;
	float met2;

	int synccounter;

	unsigned char symbolbuf[MAX_VARICODE_LEN];
	int symcounter;

	int symbolbit;

	/*
	 * TX related stuff
	 */
	int txstate;
	int txprevtone;

	struct fft *fft;

	unsigned int bitshreg;

};

enum {
	TX_STATE_PREAMBLE,
	TX_STATE_START,
	TX_STATE_DATA,
	TX_STATE_END,
	TX_STATE_FLUSH,
	TX_STATE_FINISH,
	TX_STATE_TUNE,
};

enum {
	RX_STATE_DATA,
};

/* in dominoex.c */
extern void dominoex_init(struct trx *trx);

/* in dominoexrx.c */
extern int dominoex_rxprocess(struct trx *trx, float *buf, int len);

/* in dominoextx.c */
extern int dominoex_txprocess(struct trx *trx);

#endif
