/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"

#define MAGIC_DskT	MAGIC4('D','s','k','T')
#define MAGIC_DskS	MAGIC4('D','s','k','S')


static int dtt_test(HIO_HANDLE *, char *, const int);
static int dtt_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader dtt_loader = {
	"Desktop Tracker (DTT)",
	dtt_test,
	dtt_load
};

static int dtt_test(HIO_HANDLE *f, char *t, const int start)
{
	if (hio_read32b(f) != MAGIC_DskT)
		return -1;

	read_title(f, t, 64);

	return 0;
}

static int dtt_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j, k;
	int n;
	uint8 buf[100];
	uint32 flags;
	uint32 pofs[256];
	uint8 plen[256];
	int sdata[64];

	LOAD_INIT();

	hio_read32b(f);

	set_type(m, "Desktop Tracker");

	hio_read(buf, 1, 64, f);
	strncpy(mod->name, (char *)buf, XMP_NAME_SIZE);
	hio_read(buf, 1, 64, f);
	/* strncpy(m->author, (char *)buf, XMP_NAME_SIZE); */
	
	flags = hio_read32l(f);
	mod->chn = hio_read32l(f);
	mod->len = hio_read32l(f);
	hio_read(buf, 1, 8, f);
	mod->spd = hio_read32l(f);
	mod->rst = hio_read32l(f);
	mod->pat = hio_read32l(f);
	mod->ins = mod->smp = hio_read32l(f);
	mod->trk = mod->pat * mod->chn;
	
	hio_read(mod->xxo, 1, (mod->len + 3) & ~3L, f);

	MODULE_INFO();

	for (i = 0; i < mod->pat; i++) {
		int x = hio_read32l(f);
		if (i < 256)
			pofs[i] = x;
	}

	n = (mod->pat + 3) & ~3L;
	for (i = 0; i < n; i++) {
		int x = hio_read8(f);
		if (i < 256)
			plen[i] = x;
	}

	INSTRUMENT_INIT();

	/* Read instrument names */

	for (i = 0; i < mod->ins; i++) {
		int c2spd, looplen;

		mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
		hio_read8(f);			/* note */
		mod->xxi[i].sub[0].vol = hio_read8(f) >> 1;
		mod->xxi[i].sub[0].pan = 0x80;
		hio_read16l(f);			/* not used */
		c2spd = hio_read32l(f);		/* period? */
		hio_read32l(f);			/* sustain start */
		hio_read32l(f);			/* sustain length */
		mod->xxs[i].lps = hio_read32l(f);
		looplen = hio_read32l(f);
		mod->xxs[i].flg = looplen > 0 ? XMP_SAMPLE_LOOP : 0;
		mod->xxs[i].lpe = mod->xxs[i].lps + looplen;
		mod->xxs[i].len = hio_read32l(f);
		hio_read(buf, 1, 32, f);
		instrument_name(mod, i, (uint8 *)buf, 32);
		sdata[i] = hio_read32l(f);

		mod->xxi[i].nsm = !!(mod->xxs[i].len);
		mod->xxi[i].sub[0].sid = i;

		D_(D_INFO "[%2X] %-32.32s  %04x %04x %04x %c V%02x %d\n",
				i, mod->xxi[i].name, mod->xxs[i].len,
				mod->xxs[i].lps, mod->xxs[i].lpe,
				mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[0].vol, c2spd);
	}

	PATTERN_INIT();

	/* Read and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = plen[i];
		TRACK_ALLOC(i);

		hio_seek(f, start + pofs[i], SEEK_SET);

		for (j = 0; j < mod->xxp[i]->rows; j++) {
			for (k = 0; k < mod->chn; k++) {
				uint32 x;

				event = &EVENT (i, k, j);
				x = hio_read32l(f);

				event->ins  = (x & 0x0000003f);
				event->note = (x & 0x00000fc0) >> 6;
				event->fxt  = (x & 0x0001f000) >> 12;

				if (event->note)
					event->note += 48;

				/* sorry, we only have room for two effects */
				if (x & (0x1f << 17)) {
					event->f2p = (x & 0x003e0000) >> 17;
					x = hio_read32l(f);
					event->fxp = (x & 0x000000ff);
					event->f2p = (x & 0x0000ff00) >> 8;
				} else {
					event->fxp = (x & 0xfc000000) >> 18;
				}
			}
		}
	}

	/* Read samples */
	D_(D_INFO "Stored samples: %d", mod->smp);
	for (i = 0; i < mod->ins; i++) {
		hio_seek(f, start + sdata[i], SEEK_SET);
		load_sample(m, f, SAMPLE_FLAG_VIDC, &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
	}

	return 0;
}
