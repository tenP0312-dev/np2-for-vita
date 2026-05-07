#include	"compiler.h"
#include	"fontmng.h"
#include	"memory.h"
#include	"font.h"


#if defined(SIZE_QVGA) || defined(PSP)
#include "ank10.res"
#else
#include "ank12.res"
#endif

typedef struct {
	int		fontsize;
	UINT	fonttype;
} _FNTMNG, *FNTMNG;

static BOOL issjislead(UINT8 c);
typedef struct {
    int start;
    int end;
} VITA_KNJ10_BID;

typedef struct {
    unsigned short bmp[10];
} VITA_KNJ10;

extern VITA_KNJ10_BID knj10_bid[];
extern VITA_KNJ10 knj10[];

BOOL fontmng_init(void) {
	return(SUCCESS);
}

void fontmng_setdeffontname(const char *name) {

	(void)name;
}

void *fontmng_create(int size, UINT type, const char *fontface) {

	int		fontalign;
	int		allocsize;
	FNTMNG	ret;

	if (size < ANKFONTSIZE) {
		goto fmc_err1;
	}
	fontalign = sizeof(_FNTDAT) + (max(size, 32) * max(size, 32));
	fontalign = (fontalign + 3) & (~3);
	allocsize = sizeof(_FNTMNG) + fontalign;
	ret = (FNTMNG)_MALLOC(allocsize, "font mng");
	if (ret == NULL) {
		goto fmc_err1;
	}

	ZeroMemory(ret, allocsize);

	ret->fontsize = size;
	ret->fonttype = type;
	return(ret);

fmc_err1:
	(void)fontface;
	return(NULL);
}

void fontmng_destroy(void *hdl) {

	if (hdl) {
		_MFREE(hdl);
	}
}

static BOOL issjislead(UINT8 c)
{
    return(((((c ^ 0x20) - 0xa1) & 0xff) < 0x3c) ? TRUE : FALSE);
}

static UINT sjis_to_jis(UINT8 sjis1, UINT8 sjis2)
{
    UINT8 adjust;
    UINT8 row;
    UINT8 cell;

    adjust = (sjis1 < 0xa0) ? 0x81 : 0xc1;
    if (sjis2 >= 0x9f) {
        row = (UINT8)(((sjis1 - adjust) << 1) + 0x22);
        cell = (UINT8)(sjis2 - 0x7e);
    }
    else {
        row = (UINT8)(((sjis1 - adjust) << 1) + 0x21);
        if (sjis2 > 0x7e) {
            sjis2--;
        }
        cell = (UINT8)(sjis2 - 0x1f);
    }
    return((row << 8) | cell);
}

static void setkanjihead(FNTMNG fhdl, FNTDAT fdat)
{
    fdat->width = fhdl->fontsize;
    fdat->pitch = fhdl->fontsize + 1;
    fdat->height = fhdl->fontsize;
}


static UINT jis_to_naga10_index(UINT jis)
{
    UINT idx;
    UINT blank;
    int i;

    if (jis < 0x2121) {
        return 0;
    }
    idx = jis - 0x2121;
    idx = (idx % 256) + ((idx / 256) * 0x5e);
    blank = 0;
    for (i = 0; knj10_bid[i].start != -1; i++) {
        if ((UINT)knj10_bid[i].end > idx) {
            break;
        }
        blank += (UINT)(knj10_bid[i].end - knj10_bid[i].start - 1);
    }
    return(idx - blank);
}

static BOOL getnaga10font(FNTMNG fhdl, FNTDAT fdat, UINT jis)
{
    UINT idx;
    const VITA_KNJ10 *glyph;
    UINT8 *dst;
    int y;
    int x;

    if (fhdl->fontsize != 10) {
        return FAILURE;
    }
    setkanjihead(fhdl, fdat);
    dst = (UINT8 *)(fdat + 1);
    ZeroMemory(dst, fdat->width * fdat->height);
    idx = jis_to_naga10_index(jis);
    glyph = knj10 + idx;
    for (y = 0; y < 10; y++) {
        unsigned short bits;

        bits = glyph->bmp[y];
        for (x = 0; x < 10; x++) {
            dst[x] = (bits & (0x8000 >> x)) ? 0xff : 0x00;
        }
        dst += fdat->width;
    }
    return SUCCESS;
}
static void getkanjifont(FNTMNG fhdl, FNTDAT fdat, const char *string)
{
    UINT jis;
    UINT offset;
    const UINT8 *src;
    UINT8 *dst;
    int y;
    int x;
    int top;

    jis = sjis_to_jis((UINT8)string[0], (UINT8)string[1]);
    if (getnaga10font(fhdl, fdat, jis) == SUCCESS) {
        return;
    }
    setkanjihead(fhdl, fdat);
    dst = (UINT8 *)(fdat + 1);
    ZeroMemory(dst, fdat->width * fdat->height);
    offset = ((jis & 0x00ff) << 12) + ((jis & 0xff00) >> 4);
    if (offset >= 0x80000) {
        return;
    }
    src = fontrom + offset;
    top = 0;
    dst += top * fdat->width;
    for (y = 0; y < fdat->height; y++) {
        UINT16 bits;
        int sy;

        sy = (y * 16) / fdat->height;
        bits = (UINT16)((src[sy] << 8) | src[sy + 0x800]);
        for (x = 0; x < fdat->width; x++) {
            int sx;

            sx = (x * 16) / fdat->width;
            dst[x] = (bits & (0x8000 >> sx)) ? 0xff : 0x00;
        }
        dst += fdat->width;
    }
}
static void setfdathead(FNTMNG fhdl, FNTDAT fdat, int width) {

	if (fhdl->fonttype & FDAT_PROPORTIONAL) {
		fdat->width = width;
		fdat->pitch = width + 1;
		fdat->height = fhdl->fontsize;
	}
	else {
		fdat->width = max(width, fhdl->fontsize >> 1);
		fdat->pitch = (fhdl->fontsize >> 1) + 1;
		fdat->height = fhdl->fontsize;
	}
}

static void getlength1(FNTMNG fhdl, FNTDAT fdat,
											const char *string, int length) {

	int		c;

	c = string[0] - 0x20;
	if ((c < 0) || (c >= 0x60)) {
		c = 0x1f;							// ?
	}
	setfdathead(fhdl, fdat, ankfont[c * ANKFONTSIZE]);
}

static void getfont1(FNTMNG fhdl, FNTDAT fdat,
											const char *string, int length) {

	int		c;
const UINT8	*src;
	int		width;
	UINT8	*dst;
	int		x;
	int		y;

	c = string[0] - 0x20;
	if ((c < 0) || (c >= 0x60)) {
		c = 0x1f;							// ?
	}
	src = ankfont + (c * ANKFONTSIZE);
	width = *src++;
	setfdathead(fhdl, fdat, width);
	dst = (UINT8 *)(fdat + 1);
	ZeroMemory(dst, fdat->width * fdat->height);
	dst += ((fdat->height - ANKFONTSIZE) / 2) * fdat->width;
	dst += (fdat->width - width) / 2;
	for (y=0; y<(ANKFONTSIZE - 1); y++) {
		dst += fdat->width;
		for (x=0; x<width; x++) {
			dst[x] = (src[0] & (0x80 >> x))?0xff:0x00;
		}
		src++;
	}
}

BOOL fontmng_getsize(void *hdl, const char *string, POINT_T *pt) {

	FNTMNG	fhdl;
	char	buf[4];
	_FNTDAT	fdat;
	int		width;
	int		leng;

	if ((hdl == NULL) || (string == NULL)) {
		goto fmgs_exit;
	}
	fhdl = (FNTMNG)hdl;

	width = 0;
	buf[2] = '\0';
	while(1) {
		buf[0] = *string++;
		if ((((buf[0] ^ 0x20) - 0xa1) & 0xff) < 0x3c) {
			buf[1] = *string++;
			if (buf[1] == '\0') {
				break;
			}
			leng = 2;
		}
		else if (buf[0]) {
			buf[1] = '\0';
			leng = 1;
		}
		else {
			break;
		}
		if (leng == 2) {
            setkanjihead(fhdl, &fdat);
        }
        else {
            getlength1(fhdl, &fdat, buf, leng);
        }
		width += fdat.pitch;
	}
	if (pt) {
		pt->x = width;
		pt->y = fhdl->fontsize;
	}
	return(SUCCESS);

fmgs_exit:
	return(FAILURE);
}

BOOL fontmng_getdrawsize(void *hdl, const char *string, POINT_T *pt) {

	FNTMNG	fhdl;
	char	buf[4];
	_FNTDAT	fdat;
	int		width;
	int		leng;
	int		posx;

	if ((hdl == NULL) || (string == NULL)) {
		goto fmgds_exit;
	}
	fhdl = (FNTMNG)hdl;

	width = 0;
	posx = 0;
	buf[2] = '\0';
	while(1) {
		buf[0] = *string++;
		if ((((buf[0] ^ 0x20) - 0xa1) & 0xff) < 0x3c) {
			buf[1] = *string++;
			if (buf[1] == '\0') {
				break;
			}
			leng = 2;
		}
		else if (buf[0]) {
			buf[1] = '\0';
			leng = 1;
		}
		else {
			break;
		}
		if (leng == 2) {
            setkanjihead(fhdl, &fdat);
        }
        else {
            getlength1(fhdl, &fdat, buf, leng);
        }
		width = posx + max(fdat.width, fdat.pitch);
		posx += fdat.pitch;
	}
	if (pt) {
		pt->x = width;
		pt->y = fhdl->fontsize;
	}
	return(SUCCESS);

fmgds_exit:
	return(FAILURE);
}

FNTDAT fontmng_get(void *hdl, const char *string) {

	FNTMNG	fhdl;
	FNTDAT	fdat;
	int		leng;

	if ((hdl == NULL) || (string == NULL)) {
		goto fmg_err;
	}
	fhdl = (FNTMNG)hdl;
	fdat = (FNTDAT)(fhdl + 1);

	leng = 1;
	if ((issjislead((UINT8)string[0])) &&
		(string[1] != '\0')) {
		leng = 2;
	}
	if (leng == 2) {
        getkanjifont(fhdl, fdat, string);
    }
    else {
        getfont1(fhdl, fdat, string, leng);
    }
	return(fdat);

fmg_err:
	return(NULL);
}

