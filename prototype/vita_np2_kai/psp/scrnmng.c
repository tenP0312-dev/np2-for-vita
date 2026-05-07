#include        "compiler.h"
// #include     <sys/time.h>
// #include     <signal.h>
// #include     <unistd.h>
#include        "scrnmng.h"
#include        "scrndraw.h"
#include        "vramhdl.h"
#include        "menubase.h"
#include        "softkbd.h"
#include "np2.h"
#include "pg.h"
#include "taskmng.h"
#if defined(VITA_NP2_PORT)
#include "psp_compat.h"
#include "memory.h"
#include "vram.h"
#include "nevent.h"
#include "sound.h"
#include "iocore.h"
#include "fmboard.h"
#include "pcm86io.h"
#endif
#include <pspge.h>

typedef struct {
    BOOL enable;
    int width;
    int height;
    int bpp;
    VRAMHDL vram;
} SCRNMNG;

typedef struct {
    int width;
    int height;
} SCRNSTAT;

static const char app_name[] = "Neko Project II";

static SCRNMNG scrnmng;
static SCRNSTAT scrnstat;
static SCRNSURF scrnsurf;

static CMNVRAM kbdvram;

typedef struct {
    int xalign;
    int yalign;
    int width;
    int height;
    int srcpos;
    int dstpos;
} DRAWRECT;

typedef struct {
    short tsx;      // texture buffer�J�n���W: (tsx, tsy)
    short tsy;
    short isx;      // image buffer�J�n���W: (isx, isy)
    short isy;
    short unknown1;
    short tex;      // texture buffer�I�����W: (tex, tey)
    short tey;
    short iex;      // image buffer�I�����W: (iex, iey)
    short iey;
    short unknown2;
} scr_vtx_t;

/*
  640*400��texture�ɕ`�悷�邪�Atexture buffer��1024*512�ɂ����
  ���܂������Ȃ��̂�512*512��2���g��)

                           <--------- 512dot---------->
      vramtop   0x04000000 +--------------------------+
                           |                          |
                           |         �����           |
                           |512*272*2*3               |
                           | 16bps*3��� 24pbs*2���? |
      texture1  0x040cc000 +--------------------------+ A
                           |                          | |
                           |     512*400              |512dot
                           |- - - - - - - - - - - - - | |
                           |                          | V
      texture2  0x0414c000 +--------------------------+ A
                           |       :  308+4*90+4  :   | |
                           |128*400:- - - - - - - -   |512dot
                           | - - - -                  | |
                           |                          | V
                0x041cc000 +--------------------------+
*/

static UINT8 full_screenl_buf[512 * 512 * 2] __attribute__((aligned(64)));
static UINT8 full_screenr_buf[512 * 512 * 2] __attribute__((aligned(64)));
#if defined(VITA_NP2_PORT)
static UINT8 vita_present_l_buf[512 * 400 * 2] __attribute__((aligned(64)));
static UINT8 vita_present_r_buf[512 * 400 * 2] __attribute__((aligned(64)));
static int vita_present_valid = 0;
#endif
static unsigned int GEcmd_buf[0x1000 / sizeof(unsigned int)] __attribute__((aligned(64)));
static short ScreenVertex_buf[0x100 / sizeof(short)] __attribute__((aligned(64)));
static short ScreenVertex2_buf[0x100 / sizeof(short)] __attribute__((aligned(64)));
static short SoftkbdVertex_buf[0x100 / sizeof(short)] __attribute__((aligned(64)));
UINT8 *full_screenl = full_screenl_buf; // ������
UINT8 *full_screenr = full_screenr_buf; // �E����
static unsigned int *GEcmd = GEcmd_buf; // 1024*512*2����
static short *ScreenVertex = ScreenVertex_buf;
static short *ScreenVertex2 = ScreenVertex2_buf;
static short *SoftkbdVertex = SoftkbdVertex_buf;

extern void Gu_Finish_Callback(int id, void *arg);
int gecbid = -1;
static unsigned int *last_gc;
short skbdx = 512, skbdy = 0; //������Ԃ͉�ʊO�ɒu���Ă���
static int psp_scrn_mode = 0;
static short tx = 0, ty = 0;
#if defined(VITA_NP2_PORT)
static int vita_scrn_dirty = 0;
#endif

// ������
scr_vtx_t scr_vtx[] = {
    {0, 0, 24, 1, 0, 512, 400, 370, 271, 0}, // scrn0 432x270
    {0, 0, 0, 0, 0, 512, 400, 384, 272, 0}, // scrn1 480x272
    {0, 0, 0, 0, 0, 512, 362, 384, 272, 0}, // scrn2 480x300
    {0, 0, 0, 0, 0, 480, 272, 480, 272, 0}  // scrn3 640x400
};
// �E����
scr_vtx_t scr_vtx2[] = {
    {0, 0, 370, 1, 0, 128, 400, 456, 271, 0}, // scrn0
    {0, 0, 384, 0, 0, 128, 400, 480, 272, 0}, // scrn1
    {0, 0, 384, 0, 0, 128, 362, 480, 272, 0}, // scrn2
    {0, 0, 480, 0, 0, 128, 272, 608, 272, 0}  // scrn3
};

static unsigned short mouse_cursor[] = {
    0xffff, 0xffff, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0xffff, 0xffff, 0xffff, 0x0001, 0x0001, 0x0001, 0x0000, 0x0000,
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0001, 0x0001, 0x0000,
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0001, 0x0000,
    0xffff, 0xffff, 0xffff, 0xffff, 0x0001, 0x0001, 0x0001, 0x0000,
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0001, 0x0000, 0x0000,
    0xffff, 0x0001, 0xffff, 0xffff, 0xffff, 0x0001, 0x0000, 0x0000,
    0x0001, 0x0001, 0xffff, 0xffff, 0xffff, 0x0001, 0x0000, 0x0000,
    0x0000, 0x0001, 0xffff, 0xffff, 0xffff, 0x0001, 0x0001, 0x0000,
    0x0000, 0x0001, 0x0001, 0xffff, 0xffff, 0xffff, 0x0001, 0x0000,
    0x0000, 0x0000, 0x0001, 0xffff, 0xffff, 0xffff, 0x0001, 0x0000,
    0x0000, 0x0000, 0x0001, 0xffff, 0xffff, 0xffff, 0x0001, 0x0001,
    0x0000, 0x0000, 0x0001, 0x0001, 0xffff, 0xffff, 0xffff, 0x0001,
    0x0000, 0x0000, 0x0000, 0x0001, 0xffff, 0xffff, 0xffff, 0x0001,
    0x0000, 0x0000, 0x0000, 0x0001, 0xffff, 0xffff, 0xffff, 0x0001,
    0x0000, 0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001
};

short pspmx = 0, pspmy = 0;
static int scr0o_f = 0;
#if defined(VITA_NP2_PORT)
static void vita_diag_compose(void);
#endif

// �J�[�\���`��
static void draw_cursor(short x, short y)
{
    int i, j, idx;
    unsigned short *vp;

    vp = (unsigned short *)(pgGetVramAddr(x, y));

    for (i = 0; i < 16; i++) {
        for (j = 0; j < 8; j++) {
            idx = i * 8 + j;
            /* 0�Ȃ�`���Ȃ�(���߂�����) */
            if (mouse_cursor[idx] != 0x0) {
                *vp = mouse_cursor[idx];
            }
            vp++;
        }
        vp += 512; //���̍s
        vp -= 8; // �J�[�\�������߂�
    }
}
void scrnmng_draw_cursor()
{
    draw_cursor(pspmx, pspmy);
}

// 432x270�̉�ʃ��[�h�ŁA�O�g�̍��̈���N���A����
static void fill_scrn0out()
{
    unsigned short *vp;
    int i, j;

    if (psp_scrn_mode != 0) {
        return;
    }

    vp = (unsigned short *)pgGetVramAddr(0, 0);
    for (i = 0; i < 480; i++) {
        *vp++ = 0;
    }
    vp = (unsigned short *)pgGetVramAddr(0, 271);
    for (i = 0; i < 480; i++) {
        *vp++ = 0;
    }
    vp = (unsigned short *)pgGetVramAddr(0, 0);
    for (i = 0; i < 272; i++) {
        for (j = 0; j < 24; j++) {
            *vp++ = 0;
        }
        vp += 512;
        vp -= 24;
    }
    vp = (unsigned short *)pgGetVramAddr(480 - 24, 0);
    for (i = 0; i < 272; i++) {
        for (j = 0; j < 24; j++) {
            *vp++ = 0;
        }
        vp += 512;
        vp -= 24;
    }
}

void scrnmng_change_scrn(UINT8 scrn_mode)
{
    psp_scrn_mode = (int)scrn_mode;

    if (psp_scrn_mode == 0) {
        // �O�g��\�E���ʂƂ��ɃN���A����
        scr0o_f = 2;
    }

    if (psp_scrn_mode == 2) {
        // (400 - 2y) * 0.75 = 272 �� y = 18
        tx = 0, ty = 18; // �㉺��18�h�b�g���͂ݏo��悤�ɂ���
    } else {
        tx = 0, ty = 0;
    }
}

BOOL scrnmng_set_scrn_pos(short ax, short ay)
{
    short tx0, ty0, tmpx;

    tx0 = tx, ty0 = ty;

    // ��ʂ���͂ݏo�Ȃ���ʃ��[�h�Ȃ�I��
    if (psp_scrn_mode == 0 || psp_scrn_mode == 1) {
        return FALSE;
    }

    if (psp_scrn_mode == 2) {
        // (400 - y) * 0.75 = 272 �� y=38
        (void)taskmng_mouse_anapad(&tmpx, &ty, ax, ay, 0, 38);
    } else if (psp_scrn_mode == 3) {
        (void)taskmng_mouse_anapad(&tx, &ty, ax, ay, 160, 128);
    }

    return (tx != tx0 || ty != ty0);
}

static void vita_software_compose(void)
{
    UINT16 *dst = (UINT16 *)pgGetVramAddr(0, 0);
    const UINT16 *left = (const UINT16 *)full_screenl;
    const UINT16 *right = (const UINT16 *)full_screenr;

#if defined(VITA_NP2_PORT)
    {
        static int split_log = 0;
        if (!split_log) {
            vita_np2_log("compose: vita aspect fit 640x400 -> 870x544");
            split_log = 1;
        }
    }
#endif

#if defined(VITA_NP2_PORT)
    /* Keep the PC-98 640x400 image proportional on Vita: 435x272 -> 870x544. */
    const int out_x = 22;
    const int out_y = 0;
    const int out_w = 435;
    const int out_h = 272;

    for (int y = 0; y < 272; y++) {
        UINT16 *row = dst + y * 512;
        for (int x = 0; x < 480; x++) {
            row[x] = 0;
        }
    }

    for (int y = 0; y < out_h; y++) {
        int sy = (y * 400) / out_h;
        if (sy < 0) sy = 0;
        if (sy > 399) sy = 399;
        for (int x = 0; x < out_w; x++) {
            int sx = (x * 640) / out_w;
            if (sx < 0) sx = 0;
            if (sx > 639) sx = 639;
            dst[(out_y + y) * 512 + out_x + x] =
                (sx < 512) ? left[sy * 512 + sx] : right[sy * 512 + (sx - 512)];
        }
    }
#else
    for (int y = 0; y < 272; y++) {
        int sy = (psp_scrn_mode == 3) ? y : (y * 400) / 272;
        if (sy < 0) sy = 0;
        if (sy > 399) sy = 399;
        for (int x = 0; x < 480; x++) {
            int sx = (psp_scrn_mode == 3) ? x + 80 : (x * 640) / 480;
            if (sx < 0) sx = 0;
            if (sx > 639) sx = 639;
            dst[y * 512 + x] = (sx < 512) ? left[sy * 512 + sx] : right[sy * 512 + (sx - 512)];
        }
    }
#endif

    if (menuvram != NULL && menuvram->ptr != NULL && menuvram->alpha != NULL) {
        const UINT16 *menu = (const UINT16 *)menuvram->ptr;
        const UINT8 *alpha = menuvram->alpha;
        int mw = menuvram->width;
        int mh = menuvram->height;
        if (mw > 480) mw = 480;
        if (mh > 272) mh = 272;
        for (int y = 0; y < mh; y++) {
            int mbase = y * menuvram->width;
            int dbase = y * 512;
            for (int x = 0; x < mw; x++) {
                if (alpha[mbase + x]) {
                    dst[dbase + x] = menu[mbase + x];
                }
            }
        }
    }
}
static unsigned int vita_count_nonzero16(const UINT16 *p, int pixels, int stride, int width, int height)
{
    unsigned int n = 0;
    if (p == NULL) {
        return 0;
    }
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (p[y * stride + x] != 0) {
                n++;
            }
        }
    }
    return n;
}

static void __attribute__((unused)) vita_diag_log_sources(void)
{
    static int count = 0;
    char buf[256];
    unsigned int left = vita_count_nonzero16((const UINT16 *)full_screenl, 512 * 400, 512, 512, 400);
    unsigned int right = vita_count_nonzero16((const UINT16 *)full_screenr, 512 * 400, 512, 512, 400);
    unsigned int out = vita_count_nonzero16((const UINT16 *)pgGetVramAddr(0, 0), 512 * 272, 512, 480, 272);
    unsigned int menu = 0;
    unsigned int menu_alpha = 0;
    unsigned int backing = 0;

    if (menuvram != NULL) {
        menu = vita_count_nonzero16((const UINT16 *)menuvram->ptr, menuvram->width * menuvram->height,
                                    menuvram->width, menuvram->width, menuvram->height);
        if (menuvram->alpha != NULL) {
            for (int i = 0; i < menuvram->width * menuvram->height; i++) {
                if (menuvram->alpha[i]) {
                    menu_alpha++;
                }
            }
        }
    }
    if (scrnmng.vram != NULL) {
        backing = vita_count_nonzero16((const UINT16 *)scrnmng.vram->ptr, scrnmng.vram->width * scrnmng.vram->height,
                                       scrnmng.vram->width, scrnmng.vram->width, scrnmng.vram->height);
    }

    if ((count++ % 30) == 0) {
        snprintf(buf, sizeof(buf), "diag: left=%u right=%u out=%u menuvram=%p menu=%u alpha=%u backing=%u skbdx=%d mode=%d",
                 left, right, out, menuvram, menu, menu_alpha, backing, skbdx, psp_scrn_mode);
        vita_np2_log(buf);
    }
}

static void vita_diag_fill_rect(UINT16 *dst, int left, int top, int width, int height, UINT16 color)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            dst[(top + y) * 512 + left + x] = color;
        }
    }
}

static void vita_diag_draw_source(UINT16 *dst, int dx, int dy, int dw, int dh,
                                  const UINT16 *src, int sw, int sh, int stride, UINT16 border)
{
    int nonzero = 0;
    for (int y = 0; y < dh; y++) {
        int sy = (y * sh) / dh;
        for (int x = 0; x < dw; x++) {
            int sx = (x * sw) / dw;
            UINT16 c = src ? src[sy * stride + sx] : 0;
            if (c) {
                nonzero = 1;
            }
            dst[(dy + y) * 512 + dx + x] = c ? c : (UINT16)((((x / 8) ^ (y / 8)) & 1) ? 0x0841 : 0x0000);
        }
    }
    if (!nonzero) {
        vita_diag_fill_rect(dst, dx + 4, dy + 4, dw - 8, dh - 8, 0x1082);
    }
    for (int x = 0; x < dw; x++) {
        dst[dy * 512 + dx + x] = border;
        dst[(dy + dh - 1) * 512 + dx + x] = border;
    }
    for (int y = 0; y < dh; y++) {
        dst[(dy + y) * 512 + dx] = border;
        dst[(dy + y) * 512 + dx + dw - 1] = border;
    }
}

static void __attribute__((unused)) vita_diag_compose(void)
{
    static int diag_logs = 0;
    UINT16 *dst = (UINT16 *)pgGetVramAddr(0, 0);
    UINT16 *saved = dst + 256 * 512;
    int saved_rows = 16;

    for (int y = 0; y < saved_rows; y++) {
        memcpy(saved + y * 512, dst + y * 512, 480 * sizeof(UINT16));
    }

    if (diag_logs < 8) {
        char buf[192];
        unsigned int left = vita_count_nonzero16((const UINT16 *)full_screenl, 512 * 400, 512, 512, 400);
        unsigned int right = vita_count_nonzero16((const UINT16 *)full_screenr, 512 * 400, 512, 512, 400);
        unsigned int backing = scrnmng.vram ? vita_count_nonzero16((const UINT16 *)scrnmng.vram->ptr,
            scrnmng.vram->width * scrnmng.vram->height, scrnmng.vram->width, scrnmng.vram->width, scrnmng.vram->height) : 0;
        unsigned int out = vita_count_nonzero16(dst, 512 * 272, 512, 480, 272);
        snprintf(buf, sizeof(buf), "diag4: left=%u right=%u backing=%u out=%u skbdx=%d", left, right, backing, out, skbdx);
        vita_np2_log(buf);
        diag_logs++;
    }

    vita_diag_draw_source(dst, 0, 0, 240, 136, (const UINT16 *)full_screenl, 512, 400, 512, 0xf800);
    vita_diag_draw_source(dst, 240, 0, 240, 136, (const UINT16 *)full_screenr, 512, 400, 512, 0x07e0);
    vita_diag_draw_source(dst, 0, 136, 240, 136,
                          scrnmng.vram ? (const UINT16 *)scrnmng.vram->ptr : NULL,
                          scrnmng.vram ? scrnmng.vram->width : 1,
                          scrnmng.vram ? scrnmng.vram->height : 1,
                          scrnmng.vram ? scrnmng.vram->width : 1,
                          0x001f);
    vita_diag_draw_source(dst, 240, 136, 240, 136, saved, 480, saved_rows, 512, 0xffe0);
}
#if defined(VITA_NP2_PORT)
static UINT32 vita_diag_sample_hash16(const UINT16 *p, int width, int height, int stride)
{
    UINT32 h = 2166136261u;
    if (p == NULL) {
        return 0;
    }
    for (int y = 0; y < height; y += 4) {
        for (int x = 0; x < width; x += 4) {
            h ^= p[y * stride + x];
            h *= 16777619u;
        }
    }
    return h;
}

static UINT32 vita_diag_sample_hash8(const UINT8 *p, int bytes, int step)
{
    UINT32 h = 2166136261u;
    int i;
    if (p == NULL || bytes <= 0) {
        return 0;
    }
    if (step <= 0) {
        step = 1;
    }
    for (i = 0; i < bytes; i += step) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static UINT vita_diag_count_nonzero8(const UINT8 *p, int bytes, int step)
{
    UINT n = 0;
    int i;
    if (p == NULL || bytes <= 0) {
        return 0;
    }
    if (step <= 0) {
        step = 1;
    }
    for (i = 0; i < bytes; i += step) {
        if (p[i]) {
            n++;
        }
    }
    return n;
}

void scrnmng_vita_diag_log(UINT loop_count, UINT draw_count, UINT32 real_clock,
                           UINT8 screen_update, int sound_renewal,
                           UINT32 cpu_clock, UINT32 cpu_baseclock, SINT32 cpu_remclock,
                           UINT16 cpu_cs, UINT32 cpu_eip,
                           UINT ready_events, UINT wait_events,
                           UINT first_event, SINT32 first_clock)
{
    static UINT32 last_hash = 0;
    static UINT32 last_raw0 = 0;
    static UINT32 last_raw1 = 0;
    static UINT32 last_np2v0 = 0;
    static UINT32 last_np2v1 = 0;
    static UINT last_draw = 0;
    static UINT32 last_cpu_clock = 0;
    static UINT32 last_cpu_eip = 0;
    UINT32 left_hash = vita_diag_sample_hash16((const UINT16 *)full_screenl, 512, 400, 512);
    UINT32 right_hash = vita_diag_sample_hash16((const UINT16 *)full_screenr, 128, 400, 512);
    UINT32 hash = left_hash ^ (right_hash * 16777619u);
    UINT32 raw0 = vita_diag_sample_hash8(mem + VRAM0_B, VRAM0_E - VRAM0_B, 16);
    UINT32 raw1 = vita_diag_sample_hash8(mem + VRAM1_B, VRAM1_E - VRAM1_B, 16);
    UINT32 np2v0 = vita_diag_sample_hash8(np2_vram[0], SURFACE_SIZE, 16);
    UINT32 np2v1 = vita_diag_sample_hash8(np2_vram[1], SURFACE_SIZE, 16);
    UINT32 upd = vita_diag_sample_hash8(vramupdate, 0x8000, 4);
    UINT32 ren = vita_diag_sample_hash8(renewal_line, SURFACE_HEIGHT, 1);
    UINT upd_nz = vita_diag_count_nonzero8(vramupdate, 0x8000, 4);
    UINT ren_nz = vita_diag_count_nonzero8(renewal_line, SURFACE_HEIGHT, 1);
    char buf[768];

    snprintf(buf, sizeof(buf),
             "diagkai1: loop=%u draw=%u(+%u) cpu=%lu(+%lu) pc=%04x:%08lx%s ev=%u/%u first=%u:%ld out=%08lx%s raw=%08lx/%08lx%s np2=%08lx/%08lx%s upd=%08lx/%u ren=%08lx/%u pcm=f%02x d%02x v%ld r%ld fs%ld rq%u iq%u io=466:%lu/%02x 468:%lu/%02x w46c:%lu/%02x kai=%lu/%lu/%lu lw=%lu fm=%02x/%02x/%03x/%02x/%02x pic=%02x:%02x:%02x/%02x:%02x:%02x art=%lu/%lu/%lu/%lu/%lu/%08lx dirty=%d scr=%u snd=%d",
             loop_count,
             draw_count,
             draw_count - last_draw,
             (unsigned long)cpu_clock,
             (unsigned long)(cpu_clock - last_cpu_clock),
             cpu_cs,
             (unsigned long)cpu_eip,
             (cpu_eip == last_cpu_eip) ? " samepc" : " chgpc",
             ready_events,
             wait_events,
             first_event,
             (long)first_clock,
             (unsigned long)hash,
             (hash == last_hash) ? " same" : " chg",
             (unsigned long)raw0,
             (unsigned long)raw1,
             (raw0 == last_raw0 && raw1 == last_raw1) ? " same" : " chg",
             (unsigned long)np2v0,
             (unsigned long)np2v1,
             (np2v0 == last_np2v0 && np2v1 == last_np2v1) ? " same" : " chg",
             (unsigned long)upd,
             upd_nz,
             (unsigned long)ren,
             ren_nz,
             pcm86.fifo,
             pcm86.dactrl,
             (long)pcm86.virbuf,
             (long)pcm86.realbuf,
             (long)pcm86.fifosize,
             pcm86.reqirq,
             pcm86.irqflag,
             (unsigned long)vita_pcm86_in466_count,
             vita_pcm86_last466,
             (unsigned long)vita_pcm86_in468_count,
             vita_pcm86_last468,
             (unsigned long)vita_pcm86_out46c_count,
             vita_pcm86_last46c,
              (unsigned long)vita_pcm86_kai_intrq_count,
              (unsigned long)vita_pcm86_kai_cb_irq_count,
              (unsigned long)vita_pcm86_kai_force_irq_count,
              (unsigned long)pcm86.lastclockforwait,
              fmtimer.status,
              fmtimer.reg,
              fmtimer.timera,
              fmtimer.timerb,
              fmtimer.irq,
              pic.pi[0].imr,
             pic.pi[0].irr,
             pic.pi[0].isr,
             pic.pi[1].imr,
             pic.pi[1].irr,
             pic.pi[1].isr,
             (unsigned long)vita_artic_out5f_count,
             (unsigned long)vita_artic_in5c_count,
             (unsigned long)vita_artic_in5d_count,
             (unsigned long)vita_artic_in5f_count,
             (unsigned long)vita_artic_forceexit_count,
             (unsigned long)vita_artic_getcnt_diag(),
             vita_scrn_dirty,
             screen_update,
             sound_renewal);
    vita_np2_log(buf);

    last_hash = hash;
    last_raw0 = raw0;
    last_raw1 = raw1;
    last_np2v0 = np2v0;
    last_np2v1 = np2v1;
    last_draw = draw_count;
    last_cpu_clock = cpu_clock;
    last_cpu_eip = cpu_eip;
    (void)real_clock;
    (void)cpu_baseclock;
    (void)cpu_remclock;
}
#endif
void scrnmng_gu_update()
{
    static int qid = -1;

    // �\�t�g�L�[�{�[�h
    scr_vtx_t skbd_vtx = {128, 0, skbdx, skbdy, 0,
                          128 + 308 + 4, 0 + 90 + 4,
                          skbdx + 308 + 4, skbdy + 90 + 4, 0};

#if defined(VITA_NP2_PORT)
    /* Vita owns final presentation, so PSP on-screen text must be composed
       here instead of the PSP GE finish callback. */
    static int last_overlay_active = 0;
    int overlay_active = taskmng_onscrn_active();
    int overlay_changed = (overlay_active != last_overlay_active);

    /* Skip present when screen content hasn't changed. Force present when
       menu or overlay state changes so stale UI disappears immediately. */
    {
        static const void *last_menuvram = NULL;
        int menu_changed = (menuvram != last_menuvram);
        last_menuvram = menuvram;
        if (!vita_scrn_dirty && !menuvram && !menu_changed && !overlay_active && !overlay_changed) {
            return;
        }
    }

    const UINT16 *present_l = (const UINT16 *)full_screenl;
    const UINT16 *present_r = (const UINT16 *)full_screenr;
    UINT16 *overlay = (UINT16 *)pgGetVramAddr(0, 0);

    if (overlay_active || overlay_changed) {
        memset(overlay, 0, 512 * 272 * sizeof(UINT16));
        if (overlay_active) {
            taskmng_prt_onscrn();
        }
    }
    if (menuvram) {
        scrnmng_menudraw2(NULL);
    }
    vita_np2_present_640x400_center_rgb565(
        present_l,
        present_r,
        (menuvram && menuvram->ptr) ? (const UINT16 *)menuvram->ptr : NULL,
        (menuvram && menuvram->alpha) ? menuvram->alpha : NULL,
        menuvram ? menuvram->width : 0,
        menuvram ? menuvram->height : 0,
        pspmx,
        pspmy,
        (const UINT16 *)(full_screenr + (128 + 2) * 2),
        skbdx,
        skbdy,
        overlay_active ? overlay : NULL,
        480,
        272,
        1);
    last_overlay_active = overlay_active;
    vita_scrn_dirty = 0;
    return;
#endif

    if (qid >= 0) {
        sceGeListSync(qid, 0);
    }
    *(scr_vtx_t *)ScreenVertex = scr_vtx[psp_scrn_mode];
    ((scr_vtx_t *)ScreenVertex)->tsx = tx;
    ((scr_vtx_t *)ScreenVertex)->tsy = ty;
    ((scr_vtx_t *)ScreenVertex)->tex += tx;
    ((scr_vtx_t *)ScreenVertex)->tey += ty;
    *(scr_vtx_t *)ScreenVertex2 = scr_vtx2[psp_scrn_mode];
    ((scr_vtx_t *)ScreenVertex2)->tsy = ty;
    ((scr_vtx_t *)ScreenVertex2)->tey += ty;
    if (tx > 32) {
        ((scr_vtx_t *)ScreenVertex2)->isx -= tx - 32;
        ((scr_vtx_t *)ScreenVertex2)->iex -= tx - 32;
    }
    *(scr_vtx_t *)SoftkbdVertex = skbd_vtx;

    /* ����ʂ̕\<->���ؑցB31-24bit�͕ς��Ȃ��̂�GEcmd[1]�͐G��Ȃ� */
    GEcmd[0] = 0x9C000000UL |
        ((UINT32)(UINT8 *)pgGetVramAddr(0, 0) & 0x00FFFFFF);

    if (menuvram || skbdx < 512) {
        fill_scrn0out();
        scr0o_f = 2;
    } else if (scr0o_f) {
        fill_scrn0out();
        scr0o_f--;
    }

    sceKernelDcacheWritebackAll();
    qid = sceGeListEnQueue(&GEcmd[0], last_gc, gecbid, NULL);
    vita_software_compose();
    Gu_Finish_Callback(0, NULL);
}

static int set_gecmd(int id, unsigned int *gc)
{
    while (*gc != 0) {
        GEcmd[id++] = *gc++;
    }

    return id;
}

void scrnmng_gu_init()
{
    unsigned int log2w = 9, log2h = 9; // 512*512
    int last_id;

    // �\�t�g�L�[�{�[�h
    scr_vtx_t skbd_vtx = {128, 0, skbdx, skbdy, 0,
                          128 + 308 + 4, 0 + 90 + 4,
                          skbdx + 308 + 4, skbdy + 90 + 4, 0};

    // �\�t�g�L�[�{�[�h�̊O�g�������h�肷��΂悢���A�ʓ|�Ȃ̂őS���N���A�B
    memset(full_screenr, 0, 512 * 2 * 512);

    unsigned int gc[] = {
        // Set Draw Buffer
        0x9C000000UL | ((UINT32)(UINT8 *)pgGetVramAddr(0, 0) & 0x00FFFFFF),
        0x9D000000UL |
        (((UINT32)(UINT8 *)pgGetVramAddr(0, 0) & 0xFF000000) >> 8) | 512,
        // --- 640*400�̍��� 512*400
        // Set Tex Buffer
        0xA0000000UL | ((UINT32)(UINT8 *)full_screenl & 0x00FFFFFF),
        0xA8000000UL | (((UINT32)(UINT8 *)full_screenl & 0xFF000000) >> 8) |
        512,
        0xB8000000UL | (log2h << 8) | log2w,
        // Tex Flush
        0xCB000000UL,
        // Set Vertex
        0x12000000UL |
        (1 << 23) | (0 << 11) | (0 << 9) | (2 << 7) | (0 << 5) | (0 << 2) | 2,
        0x10000000UL | (((UINT32)(void *)ScreenVertex & 0xFF000000) >> 8),
        0x01000000UL | ((UINT32)(void *)ScreenVertex & 0x00FFFFFF),
        // Draw Vertex
        0x04000000UL | (6 << 16) | 2,
        // --- 640*400�̉E�� 128*400
        // Set Tex Buffer
        0xA0000000UL | ((UINT32)(UINT8 *)full_screenr & 0x00FFFFFF),
        0xA8000000UL | (((UINT32)(UINT8 *)full_screenr & 0xFF000000) >> 8) |
        512,
        0xB8000000UL | (log2h << 8) | log2w,
        // Tex Flush
        0xCB000000UL,
        // Set Vertex
        0x12000000UL |
        (1 << 23) | (0 << 11) | (0 << 9) | (2 << 7) | (0 << 5) | (0 << 2) | 2,
        0x10000000UL |
        (((UINT32)(void *)ScreenVertex2 & 0xFF000000) >> 8),
        0x01000000UL | ((UINT32)(void *)ScreenVertex2 & 0x00FFFFFF),
        // Draw Vertex
        0x04000000UL | (6 << 16) | 2,
        // --- �\�t�g�L�[�{�[�h�p 308*90
        // Set Vertex
        0x12000000UL |
        (1 << 23) | (0 << 11) | (0 << 9) | (2 << 7) | (0 << 5) | (0 << 2) | 2,
        0x10000000UL |
        (((UINT32)(void *)SoftkbdVertex & 0xFF000000) >> 8),
        0x01000000UL | ((UINT32)(void *)SoftkbdVertex & 0x00FFFFFF),
        // Draw Vertex
        0x04000000UL | (6 << 16) | 2,
        // List End
        0x0F000000UL,
        0x0C000000UL,
        // End flag for set_gecmd()
        0
    };

    /* �\���̂̃R�s�[ */
    *(scr_vtx_t *)ScreenVertex = scr_vtx[psp_scrn_mode];
    ((scr_vtx_t *)ScreenVertex)->tsx = tx;
    ((scr_vtx_t *)ScreenVertex)->tsy = ty;
    ((scr_vtx_t *)ScreenVertex)->tex += tx;
    ((scr_vtx_t *)ScreenVertex)->tey += ty;
    *(scr_vtx_t *)ScreenVertex2 = scr_vtx2[psp_scrn_mode];
    *(scr_vtx_t *)SoftkbdVertex = skbd_vtx;

    last_id = set_gecmd(0, gc);
    last_gc = &GEcmd[last_id];
    *last_gc = 0x00000000UL; // NOP
}

void scrnmng_set_pspmxy(short x, short y)
{
    pspmx = x, pspmy = y;
}

static BOOL calcdrawrect(DRAWRECT *dr, VRAMHDL s, const RECT_T *rt) {

	int		pos;

	dr->xalign = 2; // depth(16bpp) / 8
	dr->yalign = 512 * 2; //���̍s�܂ł̃T�C�Y�H���͎���ʁH
	dr->srcpos = 0;
	dr->dstpos = 0;
	dr->width = min(scrnmng.width, s->width);
	dr->height = min(scrnmng.height, s->height);
	if (rt) {
		pos = max(rt->left, 0);
		dr->srcpos += pos;
		dr->dstpos += pos * dr->xalign;
		dr->width = min(rt->right, dr->width) - pos;

		pos = max(rt->top, 0);
		dr->srcpos += pos * s->width;
		dr->dstpos += pos * dr->yalign;
		dr->height = min(rt->bottom, dr->height) - pos;
	}
	if ((dr->width <= 0) || (dr->height <= 0)) {
		return(FAILURE);
	}
	return(SUCCESS);
}


void scrnmng_initialize(void)
{
    scrnstat.width = 640;
    scrnstat.height = 400;
}

void scrnmng_skbd_key_reverse(int x, int y, int w, int h)
{
    short *dst;
    UINT8 *tmp;
    int i, j;

    dst = (short *)((UINT8 *)kbdvram.ptr +
                    x * kbdvram.xalign + y * kbdvram.yalign);

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            *dst = ~*dst;
            dst++;
        }

#if 0
        (UINT8 *)dst -= (w * kbdvram.xalign);
        (UINT8 *)dst += kbdvram.yalign;
#else
        tmp = (UINT8 *)dst;
        tmp -= w * kbdvram.xalign;
        tmp += kbdvram.yalign;
        dst = (short *)tmp;
#endif
    }
}

static void palcnv(CMNPAL *dst, const RGB32 *src, UINT pals, UINT bpp)
{
    UINT i;

    if (bpp == 16) {
        for (i = 0; i < pals; i++) {
            dst[i].pal16 = ((src[i].p.r & 0xf8) << 8) |
                ((src[i].p.g & 0xfc) << 3) | (src[i].p.b >> 3);
        }
    }
}

BOOL scrnmng_create(int width, int height)
{
    scrnmng.enable = TRUE;
    scrnmng.width = width;
    scrnmng.height = height;
    scrnmng.bpp = 16; // 16bpp for psp

    softkbd_initialize();
    kbdvram.width = 512;
    kbdvram.height = 512;
    kbdvram.xalign = 2;
    kbdvram.yalign = 512 * 2;
    //�e�N�X�`���pfull_screenr�̍�128�h�b�g�̓��C���X�N���[���p��
    //�g���Ă���̂ŁA���܂��Ă��镔�����\�t�g�L�[�{�[�h�p�Ɏg���B
    kbdvram.ptr = (UINT8 *)full_screenr + (128 + 2) * kbdvram.xalign
        + 2 * kbdvram.yalign;
    kbdvram.bpp = 16;
    softkbd_paint(&kbdvram, palcnv, TRUE);

    return(SUCCESS);

}

void scrnmng_destroy(void) {

	scrnmng.enable = FALSE;
}

RGB16 scrnmng_makepal16(RGB32 pal32) {
	RGB16	ret;
#if defined(PSP) && !defined(VITA_NP2_PORT)
	ret = (pal32.p.b & 0xf8) << 7;
	ret += (pal32.p.g & 0xf8) << 2;
	ret += pal32.p.r >> 3;
#else
	ret = (pal32.p.r & 0xf8) << 8;
#if defined(SIZE_QVGA)
	ret += (pal32.p.g & 0xfc) << (3 + 16);
#else
	ret += (pal32.p.g & 0xfc) << 3;
#endif
	ret += pal32.p.b >> 3;

#endif
	return(ret);
}

void scrnmng_setwidth(int posx, int width)
{
    scrnstat.width = width;
}

void scrnmng_setheight(int posy, int height)
{
    scrnstat.height = height;
}

const SCRNSURF *scrnmng_surflock(void)
{
if (scrnmng.vram == NULL) {
        scrnsurf.ptr = (UINT8 *)full_screenl;
        scrnsurf.xalign = 2; // depth(16bpp) / 8
        scrnsurf.yalign = 512 * 2;
        scrnsurf.bpp = 16; // depth(16bpp)
    } else {
        scrnsurf.ptr = scrnmng.vram->ptr;
        scrnsurf.xalign = scrnmng.vram->xalign;
        scrnsurf.yalign = scrnmng.vram->yalign;
        scrnsurf.bpp = scrnmng.vram->bpp;
    }
    scrnsurf.width = min(scrnstat.width, 640);
    scrnsurf.height = min(scrnstat.height, 400);
    scrnsurf.extend = 0;
    return(&scrnsurf);
}

void scrnmng_surfunlock(const SCRNSURF *surf)
{
#if defined(VITA_NP2_PORT)
        /* full_screenl/r are stable after pccore_exec returns.
           vita_present_*_buf copies are unnecessary in a single-threaded loop. */
        vita_scrn_dirty = 1;
#else
        scrnmng_gu_update();
#endif
}


// ----

BOOL scrnmng_entermenu(SCRNMENU *smenu)
{
    if (smenu == NULL) {
        goto smem_err;
    }
    vram_destroy(scrnmng.vram);
    scrnmng.vram
        = vram_create(scrnmng.width, scrnmng.height, FALSE, scrnmng.bpp);
    if (scrnmng.vram == NULL) {
        goto smem_err;
    }
#if !defined(VITA_NP2_PORT)
    scrndraw_redraw();
#endif
#if 0
    smenu->width = scrnmng.width;
    smenu->height = scrnmng.height;
#else
    // ���j���[��ʂ�PSP�T�C�Y�ɂ���
    smenu->width = 480;
    smenu->height = 272;
#endif
    smenu->bpp = (scrnmng.bpp == 32)?24:scrnmng.bpp;
    return(SUCCESS);

 smem_err:
    return(FAILURE);
}

void scrnmng_leavemenu(void) {

	VRAM_RELEASE(scrnmng.vram);
}

void scrnmng_menudraw(const RECT_T *rct) {

    DRAWRECT dr;
    const UINT8 *p;
    const UINT8 *q;
//    UINT8 *r;
    UINT8 *a;
    int salign;
    int dalign;
    int x;

    if ((!scrnmng.enable) && (menuvram == NULL)) {
        return;
    }

    if (calcdrawrect(&dr, menuvram, rct) == SUCCESS) {
        p = scrnmng.vram->ptr + (dr.srcpos * 2);
        q = menuvram->ptr + (dr.srcpos * 2);
//        r = pgGetVramAddr(0, 0) + dr.dstpos;
        a = menuvram->alpha + dr.srcpos;
        salign = menuvram->width;
        dalign = dr.yalign - (dr.width * dr.xalign);
        do {
            x = 0;
            do {
                if (a[x]) {
                    if (!(a[x] & 2)) {
                        a[x] = 0;
                    }
#if 0
                    if (a[x] & 2) {
                        // �_�C�A���O�`��
//                        *(UINT16 *)r = *(UINT16 *)(q + (x * 2));
                    } else {
                        // �_�C�A���O�����E�ړ����̔w�i(PC-98���)�`��
                        a[x] = 0;
//                        *(UINT16 *)r = *(UINT16 *)(p + (x * 2));
                    }
#endif
                }
//                r += dr.xalign;
            } while(++x < dr.width);
            p += salign * 2;
            q += salign * 2;
//            r += dalign;
            a += salign;
        } while(--dr.height);
    }
}

void scrnmng_menudraw2(const RECT_T *rct) {

    DRAWRECT dr;
    const UINT8 *p;
    const UINT8 *q;
    UINT8 *r;
    UINT8 *a;
    int salign;
    int dalign;
    int x;

    if ((!scrnmng.enable) && (menuvram == NULL)) {
        return;
    }

    // �S�͈͍ĕ`��Ȃ̂ŁA��3������NULL���w��
    if (calcdrawrect(&dr, menuvram, NULL) == SUCCESS) {
        p = scrnmng.vram->ptr + (dr.srcpos * 2);
        q = menuvram->ptr + (dr.srcpos * 2);
        r = (UINT8 *)(pgGetVramAddr(0, 0) + dr.dstpos);
        a = menuvram->alpha + dr.srcpos;
        salign = menuvram->width;
        dalign = dr.yalign - (dr.width * dr.xalign);
        do {
            x = 0;
            do {
                if (a[x]) {
                    if (a[x] & 2) {
                        // �_�C�A���O�`��
                        *(UINT16 *)r = *(UINT16 *)(q + (x * 2));
                    } else {
                        // �_�C�A���O�����E�ړ����̔w�i(PC-98���)�`��
                        a[x] = 0;
                        *(UINT16 *)r = *(UINT16 *)(p + (x * 2));
                    }
                }
                r += dr.xalign;
            } while(++x < dr.width);
            p += salign * 2;
            q += salign * 2;
            r += dalign;
            a += salign;
        } while(--dr.height);
    }
}
