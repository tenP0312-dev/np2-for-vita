#include	"compiler.h"
#include	"scrnmng.h"
#include	"scrndraw.h"
#include	"sdraw.h"
#include	"palettes.h"

#if !defined(SIZE_QVGA) || defined(SIZE_VGATEST)

#if defined(SUPPORT_8BPP)
#define	SDSYM(sym)				sdraw8##sym
#define	SDSETPIXEL(ptr, pal)	*(ptr) = (pal) + START_PAL
#include	"sdraw.mcr"
#undef	SDSYM
#undef	SDSETPIXEL
#endif

#if defined(SUPPORT_16BPP)
#define	SDSYM(sym)				sdraw16##sym
#ifdef PSP
#if defined(VITA_NP2_PORT)
extern UINT8 *full_screenl;
extern UINT8 *full_screenr;
static inline UINT8 *vita_sdraw_ptr(int x, UINT8 *ptr) {
	if ((x >= 512) && (ptr >= full_screenl) && (ptr < (full_screenl + (512 * 512 * 2)))) {
		ptr += (full_screenr - full_screenl) - 0x400;
	}
	return ptr;
}
#define	SDSETPIXEL(x, ptr, pal) \
{ \
	*(UINT16 *)vita_sdraw_ptr((x), (UINT8 *)(ptr)) = np2_pal16[(pal)]; \
}
#else
#define	SDSETPIXEL(x, ptr, pal) \
{ \
    if (x >= 512 && ((UINT32)ptr & 0x08000000) == 0) { \
        *(UINT16 *)((UINT8 *)ptr + 0x80000 - 0x400) = np2_pal16[(pal)]; \
    } else { \
        /* Ge VRAM???x?512????????MENU????VRAM??? */ \
        *(UINT16 *)(ptr) = np2_pal16[(pal)]; \
    } \
}
#endif
#else //PSP
#define	SDSETPIXEL(ptr, pal)	*(UINT16 *)(ptr) = np2_pal16[(pal)]
#endif //PSP
#define VITA_SDRAW_SPLIT_16
#include	"sdraw.mcr"
#undef VITA_SDRAW_SPLIT_16
#include	"sdrawex.mcr"
#undef	SDSYM
#undef	SDSETPIXEL
#endif

#if defined(SUPPORT_24BPP)
#define	SDSYM(sym)				sdraw24##sym
#define	SDSETPIXEL(ptr, pal)	(ptr)[RGB24_R] = np2_pal32[(pal)].p.r;	\
								(ptr)[RGB24_G] = np2_pal32[(pal)].p.g;	\
								(ptr)[RGB24_B] = np2_pal32[(pal)].p.b
#include	"sdraw.mcr"
#include	"sdrawex.mcr"
#undef	SDSYM
#undef	SDSETPIXEL
#endif

#if defined(SUPPORT_32BPP)
#define	SDSYM(sym)				sdraw32##sym
#define	SDSETPIXEL(ptr, pal)	*(UINT32 *)(ptr) = np2_pal32[(pal)].d
#include	"sdraw.mcr"
#include	"sdrawex.mcr"
#undef	SDSYM
#undef	SDSETPIXEL
#endif


// ----

static const SDRAWFN *tbl[] = {
#if defined(SUPPORT_8BPP)
			sdraw8p,
#else
			NULL,
#endif
#if defined(SUPPORT_16BPP)
			sdraw16p,
#else
			NULL,
#endif
#if defined(SUPPORT_24BPP)
			sdraw24p,
#else
			NULL,
#endif
#if defined(SUPPORT_32BPP)
			sdraw32p,
#else
			NULL,
#endif

#if defined(SUPPORT_NORMALDISP)
#if defined(SUPPORT_8BPP)
			sdraw8n,
#else
			NULL,
#endif
#if defined(SUPPORT_16BPP)
			sdraw16n,
#else
			NULL,
#endif
#if defined(SUPPORT_24BPP)
			sdraw24n,
#else
			NULL,
#endif
#if defined(SUPPORT_32BPP)
			sdraw32n,
#else
			NULL,
#endif
#endif
};

const SDRAWFN *sdraw_getproctbl(const SCRNSURF *surf) {

	int		proc;

	proc = ((surf->bpp >> 3) - 1) & 3;
#if defined(SUPPORT_NORMALDISP)
	if (surf->extend) {
		proc += 4;
	}
#endif
	return(tbl[proc]);
}


// ---- PC-9821

#if defined(SUPPORT_PC9821)

static const SDRAWFN *tblex[] = {
			NULL,
#if defined(SUPPORT_16BPP)
			sdraw16pex,
#else
			NULL,
#endif
#if defined(SUPPORT_24BPP)
			sdraw24pex,
#else
			NULL,
#endif
#if defined(SUPPORT_32BPP)
			sdraw32pex,
#else
			NULL,
#endif

#if defined(SUPPORT_NORMALDISP)
			NULL,
#if defined(SUPPORT_16BPP)
			sdraw16nex,
#else
			NULL,
#endif
#if defined(SUPPORT_24BPP)
			sdraw24nex,
#else
			NULL,
#endif
#if defined(SUPPORT_32BPP)
			sdraw32nex,
#else
			NULL,
#endif
#endif
};

const SDRAWFN *sdraw_getproctblex(const SCRNSURF *surf) {

	int		proc;

	proc = ((surf->bpp >> 3) - 1) & 3;
#if defined(SUPPORT_NORMALDISP)
	if (surf->extend) {
		proc += 4;
	}
#endif
	return(tblex[proc]);
}
#endif

#endif

