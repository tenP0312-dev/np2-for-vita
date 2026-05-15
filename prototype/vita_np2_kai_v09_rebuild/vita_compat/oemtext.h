#ifndef VITA_OEMTEXT_H
#define VITA_OEMTEXT_H

#include "codecnv.h"

/* SJIS -> UCS-2 converter used before UTF-8 output. */
extern UINT codecnv_sjistoucs2(UINT16 *dst, UINT dcnt, const char *src, UINT scnt);

/* oemtext_sjistooem: SJIS -> UTF-8 */
static inline void oemtext_sjistooem(char *dst, int dstlen,
                                      const char *src, int srclen) {
    UINT16 ucs2[512];
    UINT n;
    if (!dst || dstlen <= 0) return;
    n = codecnv_sjistoucs2(ucs2, 511, src, (UINT)srclen);
    if (n < 512) ucs2[n] = 0;
    else ucs2[511] = 0;
    codecnv_ucs2toutf8(dst, (UINT)dstlen, ucs2, (UINT)-1);
}

/* oemtext_oemtosjis: UTF-8 -> SJIS (ASCII passthrough, others -> '?') */
static inline void oemtext_oemtosjis(char *dst, int dstlen,
                                      const char *src, int srclen) {
    int i = 0, o = 0;
    int limit = (srclen < 0) ? 0x7fffffff : srclen;
    if (!dst || dstlen <= 0) return;
    while (o < dstlen - 1 && i < limit && src[i]) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {
            dst[o++] = src[i++];
        } else if ((c & 0xE0) == 0xC0) {
            dst[o++] = '?'; i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            dst[o++] = '?'; i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            dst[o++] = '?'; i += 4;
        } else {
            dst[o++] = '?'; i++;
        }
    }
    dst[o] = '\0';
}

#endif /* VITA_OEMTEXT_H */
