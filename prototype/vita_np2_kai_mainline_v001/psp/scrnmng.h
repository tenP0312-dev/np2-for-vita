#ifndef _PSP_SCRNMNG_H_
#define _PSP_SCRNMNG_H_

enum {
	RGB24_B	= 0,
	RGB24_G	= 1,
	RGB24_R	= 2
};

typedef struct {
	UINT8	*ptr;
	int		xalign;
	int		yalign;
	int		width;
	int		height;
	UINT	bpp;
	int		extend;
} SCRNSURF;

#ifdef __cplusplus
extern "C" {
#endif

void scrnmng_gu_update();
void scrnmng_gu_init();
void scrnmng_draw_cursor();
void scrnmng_change_scrn(UINT8 scrn_mode);
BOOL scrnmng_set_scrn_pos(short ax, short ay);
void scrnmng_set_pspmxy(short x, short y);
void scrnmng_fill_scrn0out();
#if defined(VITA_NP2_PORT)
void scrnmng_vita_diag_log(UINT loop_count, UINT draw_count, UINT32 real_clock,
                           UINT8 screen_update, int sound_renewal,
                           UINT32 cpu_clock, UINT32 cpu_baseclock, SINT32 cpu_remclock,
                           UINT16 cpu_cs, UINT32 cpu_eip,
                           UINT ready_events, UINT wait_events,
                           UINT first_event, SINT32 first_clock);
#endif

void scrnmng_setwidth(int posx, int width);
#define scrnmng_setextend(e)
void scrnmng_setheight(int posy, int height);
const SCRNSURF *scrnmng_surflock(void);
void scrnmng_surfunlock(const SCRNSURF *surf);

#define	scrnmng_haveextend()	(0)
#define	scrnmng_getbpp()		(16)
#define	scrnmng_allflash()		
#define	scrnmng_palchanged()	

RGB16 scrnmng_makepal16(RGB32 pal32);

#ifdef __cplusplus
}
#endif


// ---- for SDL

void scrnmng_initialize(void);
void scrnmng_skbd_key_reverse(int x, int y, int w, int h);
BOOL scrnmng_create(int width, int height);
void scrnmng_destroy(void);


// ---- for menubase

typedef struct {
	int		width;
	int		height;
	int		bpp;
} SCRNMENU;

BOOL scrnmng_entermenu(SCRNMENU *smenu);
void scrnmng_leavemenu(void);
void scrnmng_menudraw(const RECT_T *rct);
void scrnmng_menudraw2(const RECT_T *rct);

#endif
