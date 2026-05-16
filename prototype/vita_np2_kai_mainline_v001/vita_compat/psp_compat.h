#ifndef VITA_NP2_PSP_COMPAT_H
#define VITA_NP2_PSP_COMPAT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/rtc.h>
#include <psp2/types.h>

#define PSP_MODULE_INFO(name, attrs, major, minor)
#define PSP_MAIN_THREAD_ATTR(attr)
#define PSP_THREAD_ATTR_USER 0
#define THREAD_ATTR_USER 0
#define PSP_CTRL_MODE_ANALOG SCE_CTRL_MODE_ANALOG

#define PSP_CTRL_SELECT SCE_CTRL_SELECT
#define PSP_CTRL_START SCE_CTRL_START
#define PSP_CTRL_UP SCE_CTRL_UP
#define PSP_CTRL_RIGHT SCE_CTRL_RIGHT
#define PSP_CTRL_DOWN SCE_CTRL_DOWN
#define PSP_CTRL_LEFT SCE_CTRL_LEFT
#define PSP_CTRL_LTRIGGER SCE_CTRL_LTRIGGER
#define PSP_CTRL_RTRIGGER SCE_CTRL_RTRIGGER
#define PSP_CTRL_TRIANGLE SCE_CTRL_TRIANGLE
#define PSP_CTRL_CIRCLE SCE_CTRL_CIRCLE
#define PSP_CTRL_CROSS SCE_CTRL_CROSS
#define PSP_CTRL_SQUARE SCE_CTRL_SQUARE

#define Buttons buttons
#define Lx lx
#define Ly ly

#define PSP_O_RDONLY SCE_O_RDONLY
#define PSP_O_WRONLY SCE_O_WRONLY
#define PSP_O_RDWR SCE_O_RDWR
#define PSP_O_CREAT SCE_O_CREAT
#define PSP_O_TRUNC SCE_O_TRUNC
#define PSP_O_APPEND SCE_O_APPEND

#define FIO_SO_IFDIR SCE_SO_IFDIR

typedef SceCtrlData SceCtrlData;
typedef SceSize SceSize;
typedef int SceUID;
typedef uint64_t u64;

u64 GETTICK(void);

static inline void vita_np2_log_reset(void) {
  sceIoMkdir("ux0:data", 0777);
  sceIoMkdir("ux0:data/np2vita", 0777);
  int fd = sceIoOpen("ux0:data/np2vita/bootlog.txt",
                     SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
  if (fd >= 0) {
    sceIoClose(fd);
  }
}
static inline void vita_np2_log(const char *msg) {
  sceIoMkdir("ux0:data/np2vita", 0777);
  int fd = sceIoOpen("ux0:data/np2vita/bootlog.txt",
                     SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
  if (fd >= 0) {
    if (msg) {
      sceIoWrite(fd, msg, strlen(msg));
    }
    sceIoWrite(fd, "\n", 1);
    sceIoClose(fd);
  }
}

static inline int vita_np2_show_test_pattern(void) {
  static uint32_t fb[960 * 544] __attribute__((aligned(256)));
  for (int y = 0; y < 544; y++) {
    for (int x = 0; x < 960; x++) {
      uint32_t c = 0xff202020u;
      if (x < 12 || x >= 948 || y < 12 || y >= 532) {
        c = 0xff20e080u;
      }
      if (x > 300 && x < 660 && y > 180 && y < 364) {
        c = 0xffc06040u;
      }
      if (((x / 32) ^ (y / 32)) & 1) {
        c ^= 0x00101010u;
      }
      fb[y * 960 + x] = c;
    }
  }

  SceDisplayFrameBuf param;
  memset(&param, 0, sizeof(param));
  param.size = sizeof(param);
  param.base = fb;
  param.pitch = 960;
  param.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
  param.width = 960;
  param.height = 544;
  int ret = sceDisplaySetFrameBuf(&param, SCE_DISPLAY_SETBUF_NEXTFRAME);
  vita_np2_log(ret == 0 ? "display: native test ok" : "display: native test failed");
  return ret;
}

#define PSP_POWER_CB_POWER_SWITCH 0x00000100
#define PSP_POWER_CB_SUSPENDING 0x00010000
#define PSP_POWER_CB_RESUME_COMPLETE 0x00400000

typedef struct PspGeCallbackData {
  void (*signal_func)(int id, void *arg);
  void *signal_arg;
  void (*finish_func)(int id, void *arg);
  void *finish_arg;
} PspGeCallbackData;

static inline int sceCtrlSetSamplingCycle(int cycle) {
  (void)cycle;
  return 0;
}

static inline int sceCtrlRead(SceCtrlData *pad, int count) {
  return sceCtrlPeekBufferPositive(0, pad, count);
}

static inline int psp_sceCtrlPeekBufferPositive(SceCtrlData *pad, int count) {
  return sceCtrlPeekBufferPositive(0, pad, count);
}

#define sceCtrlPeekBufferPositive(pad, count) psp_sceCtrlPeekBufferPositive((pad), (count))

static inline void sceKernelExitGame(void) {
  sceKernelExitThread(0);
}

static inline int psp_sceKernelCreateThread(const char *name, SceKernelThreadEntry entry,
                                            int init_priority, SceSize stack_size,
                                            SceUInt attr, void *option) {
  (void)option;
  /* PSP priorities below 64 are in kernel space on Vita; clamp to valid user range. */
  if (init_priority < 64)  init_priority = 64;
  if (init_priority > 191) init_priority = 191;
  return sceKernelCreateThread(name, entry, init_priority, stack_size, attr, 0, NULL);
}

#define sceKernelCreateThread(name, entry, init_priority, stack_size, attr, option) \
  psp_sceKernelCreateThread((name), (entry), (init_priority), (stack_size), (attr), (option))

static inline int psp_sceKernelWaitThreadEnd(SceUID thid, int *stat) {
  return sceKernelWaitThreadEnd(thid, stat, NULL);
}

#define sceKernelWaitThreadEnd(thid, stat) psp_sceKernelWaitThreadEnd((thid), (stat))

static inline int psp_sceKernelCreateCallback(const char *name,
                                              SceKernelCallbackFunction func,
                                              void *user_data) {
  return sceKernelCreateCallback(name, 0, func, user_data);
}

#define sceKernelCreateCallback(name, func, user_data) \
  psp_sceKernelCreateCallback((name), (SceKernelCallbackFunction)(func), (user_data))

static inline int sceKernelRegisterExitCallback(int cbid) {
  (void)cbid;
  return 0;
}

static inline int sceKernelSleepThreadCB(void) {
  sceKernelDelayThreadCB(1000 * 1000);
  return 0;
}

static inline int scePowerSetClockFrequency(int cpu, int bus, int gpu) {
  (void)cpu;
  (void)bus;
  (void)gpu;
  return 0;
}

#define scePowerRegisterCallback(slot, cbid) ((void)(slot), scePowerRegisterCallback(cbid))

static inline time_t sceKernelLibcTime(time_t *t) {
  time_t now = 0;
  if (t) {
    *t = now;
  }
  return now;
}

static inline int psp_sceRtcGetCurrentTick(u64 *tick) {
  if (tick) {
    *tick = (u64)((clock() * 1000000ULL) / CLOCKS_PER_SEC);
  }
  return 0;
}

#define sceRtcGetCurrentTick(tick) psp_sceRtcGetCurrentTick((tick))

static inline int sceAudioChReserve(int channel, int sample_count, int format) {
  (void)channel;
  (void)format;
  /* Map PSP sceAudioChReserve to Vita BGM port, stereo S16, 44100 Hz */
  return sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, sample_count, 44100,
                             SCE_AUDIO_OUT_MODE_STEREO);
}

static inline int sceAudioOutputPannedBlocking(int channel, int leftvol, int rightvol, const void *buf) {
  if (channel < 0) return -1;
  int vols[2] = {leftvol, rightvol};
  sceAudioOutSetVolume(channel, (SceAudioOutChannelFlag)(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), vols);
  return sceAudioOutOutput(channel, buf);
}

static inline int sceAudioChRelease(int channel) {
  if (channel < 0) return 0;
  return sceAudioOutReleasePort(channel);
}

static inline void sceKernelDcacheWritebackAll(void) {
}

static inline uint32_t vita_np2_rgb565_to_a8b8g8r8(uint16_t p) {
  uint32_t r = ((p >> 11) & 0x1f) << 3;
  uint32_t g = ((p >> 5) & 0x3f) << 2;
  uint32_t b = (p & 0x1f) << 3;
  return 0xff000000u | (b << 16) | (g << 8) | r;
}

static inline void vita_np2_blit2x_rgb565(uint32_t *fb, int dx, int dy, const uint16_t *src,
                                          const uint8_t *alpha, int sw, int sh, int stride,
                                          int transparent_zero) {
  if (!fb || !src) {
    return;
  }
  for (int y = 0; y < sh; y++) {
    int py = dy + y * 2;
    if (py < 0 || py + 1 >= 544) {
      continue;
    }
    for (int x = 0; x < sw; x++) {
      int px = dx + x * 2;
      if (px < 0 || px + 1 >= 960) {
        continue;
      }
      if (alpha && !alpha[y * stride + x]) {
        continue;
      }
      uint16_t p = src[y * stride + x];
      if (transparent_zero && !p) {
        continue;
      }
      uint32_t c = vita_np2_rgb565_to_a8b8g8r8(p);
      fb[py * 960 + px] = c;
      fb[py * 960 + px + 1] = c;
      fb[(py + 1) * 960 + px] = c;
      fb[(py + 1) * 960 + px + 1] = c;
    }
  }
}

static inline int vita_np2_present_640x400_center_rgb565(const uint16_t *full640,
                                                         const uint16_t *left, const uint16_t *right,
                                                         const uint16_t *menu, const uint8_t *menu_alpha,
                                                         int menu_w, int menu_h,
                                                         int mouse_x, int mouse_y,
                                                         const uint16_t *skbd, int skbdx, int skbdy,
                                                         const uint16_t *overlay, int overlay_w, int overlay_h,
                                                         int sync) {
  static uint32_t fallback_fb[2][960 * 544] __attribute__((aligned(256)));
  static uint32_t *vita_fb[2] = { NULL, NULL };
  static SceUID vita_fb_uid[2] = { -1, -1 };
  static int vita_fb_idx = 0;
  static int present_count = 0;
  static int last_menu = -1;
  int cur_menu = (menu && menu_alpha && menu_w > 0 && menu_h > 0) ? 1 : 0;
  uint32_t *vita_fb_cur;

  if (!vita_fb[0]) {
    void *base0 = NULL;
    void *base1 = NULL;
    const SceSize fb_size = 960 * 544 * sizeof(uint32_t);
    const SceSize block_size = (fb_size + 0x3ffff) & ~0x3ffff;
    vita_fb_uid[0] = sceKernelAllocMemBlock("np2vita_fb0",
                                            SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                            block_size, NULL);
    vita_fb_uid[1] = sceKernelAllocMemBlock("np2vita_fb1",
                                            SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                            block_size, NULL);
    if (vita_fb_uid[0] >= 0 && vita_fb_uid[1] >= 0 &&
        sceKernelGetMemBlockBase(vita_fb_uid[0], &base0) >= 0 && base0 &&
        sceKernelGetMemBlockBase(vita_fb_uid[1], &base1) >= 0 && base1) {
      vita_fb[0] = (uint32_t *)base0;
      vita_fb[1] = (uint32_t *)base1;
      vita_np2_log("present: fb cdram double ok");
    } else if (vita_fb_uid[0] >= 0 && sceKernelGetMemBlockBase(vita_fb_uid[0], &base0) >= 0 && base0) {
      vita_fb[0] = (uint32_t *)base0;
      vita_fb[1] = (uint32_t *)base0;
      vita_np2_log("present: fb cdram single ok");
    } else {
      char buf[128];
      snprintf(buf, sizeof(buf), "present: fb fallback size=%u uid0=0x%08x uid1=0x%08x", (unsigned int)block_size, (unsigned int)vita_fb_uid[0], (unsigned int)vita_fb_uid[1]);
      vita_np2_log(buf);
      vita_fb[0] = fallback_fb[0];
      vita_fb[1] = fallback_fb[1];
    }
  }

  vita_fb_idx ^= 1;
  vita_fb_cur = vita_fb[vita_fb_idx];
#if VITA_NP2_DIAG
  if (cur_menu != last_menu) {
    char buf[96];
    snprintf(buf, sizeof(buf), "present: mode %d->%d", last_menu, cur_menu);
    vita_np2_log(buf);
    last_menu = cur_menu;
  }
#else
  last_menu = cur_menu;
#endif
  present_count++;

  /* Clear framebuffer each frame. Use memset(0) — alpha is ignored by Vita
     display output, so 0x00000000 and 0xff000000 both appear black.
     memset uses NEON SIMD and is much faster than the previous scalar loop. */
  memset(vita_fb_cur, 0, 960 * 544 * sizeof(uint32_t));

  if (menu && menu_alpha && menu_w > 0 && menu_h > 0) {
    if (menu_w > 480) menu_w = 480;
    if (menu_h > 272) menu_h = 272;
    vita_np2_blit2x_rgb565(vita_fb_cur, 0, 0, menu, menu_alpha, menu_w, menu_h, menu_w, 0);
  } else if (full640) {
    const int ox = (960 - 640) / 2;
    const int oy = (544 - 400) / 2;
    for (int y = 0; y < 400; y++) {
      uint32_t *dst = vita_fb_cur + (oy + y) * 960 + ox;
      const uint16_t *src = full640 + y * 640;
      for (int x = 0; x < 640; x++) {
        dst[x] = vita_np2_rgb565_to_a8b8g8r8(src[x]);
      }
    }
  } else if (left && right) {
    const int ox = (960 - 640) / 2;
    const int oy = (544 - 400) / 2;
    for (int y = 0; y < 400; y++) {
      uint32_t *dst = vita_fb_cur + (oy + y) * 960 + ox;
      for (int x = 0; x < 512; x++) {
        dst[x] = vita_np2_rgb565_to_a8b8g8r8(left[y * 512 + x]);
      }
      for (int x = 0; x < 128; x++) {
        dst[512 + x] = vita_np2_rgb565_to_a8b8g8r8(right[y * 512 + x]);
      }
    }
  }

  if (overlay && overlay_w > 0 && overlay_h > 0) {
    if (overlay_w > 480) overlay_w = 480;
    if (overlay_h > 272) overlay_h = 272;
    vita_np2_blit2x_rgb565(vita_fb_cur, 0, 0, overlay, NULL, overlay_w, overlay_h, 512, 1);
  }
  if (menu && menu_alpha && mouse_x >= 0 && mouse_y >= 0) {
    for (int y = 0; y < 16; y++) {
      for (int x = 0; x <= y / 2 && x < 8; x++) {
        int px = mouse_x * 2 + x * 2;
        int py = mouse_y * 2 + y * 2;
        if (px >= 0 && px + 1 < 960 && py >= 0 && py + 1 < 544) {
          uint32_t c = (x == 0 || x == y / 2) ? 0xff000000u : 0xffffffffu;
          vita_fb_cur[py * 960 + px] = c;
          vita_fb_cur[py * 960 + px + 1] = c;
          vita_fb_cur[(py + 1) * 960 + px] = c;
          vita_fb_cur[(py + 1) * 960 + px + 1] = c;
        }
      }
    }
  }

  if (skbd && skbdx < 512) {
    vita_np2_blit2x_rgb565(vita_fb_cur, skbdx * 2, skbdy * 2, skbd, NULL, 312, 94, 512, 1);
  }

  SceDisplayFrameBuf fb;
  memset(&fb, 0, sizeof(fb));
  fb.size = sizeof(fb);
  fb.base = vita_fb_cur;
  fb.pitch = 960;
  fb.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
  fb.width = 960;
  fb.height = 544;
  int ret = sceDisplaySetFrameBuf(&fb, sync ? SCE_DISPLAY_SETBUF_NEXTFRAME : SCE_DISPLAY_SETBUF_IMMEDIATE);
  if (ret < 0) {
    char buf[96];
    snprintf(buf, sizeof(buf), "present: setfb error=0x%08x base=%p", (unsigned int)ret, fb.base);
    vita_np2_log(buf);
  }
  return ret;
}

static inline int psp_sceDisplaySetFrameBuf(void *base, int line_size, int pixel_size, int sync) {
#if defined(VITA_NP2_PORT)
  (void)base;
  (void)line_size;
  (void)pixel_size;
  (void)sync;
  return 0;
#else
  static uint32_t vita_fb[960 * 544] __attribute__((aligned(256)));
  const uint16_t *src = (const uint16_t *)base;
  int nonzero = 0;

  if (src) {
    for (int y = 0; y < 272; y++) {
      for (int x = 0; x < 480; x++) {
        uint16_t p = src[y * line_size + x];
        nonzero |= p;
        uint32_t r = ((p >> 11) & 0x1f) << 3;
        uint32_t g = ((p >> 5) & 0x3f) << 2;
        uint32_t b = (p & 0x1f) << 3;
        uint32_t c = 0xff000000u | (b << 16) | (g << 8) | r;
        int dx = x * 2;
        int dy = y * 2;
        vita_fb[dy * 960 + dx] = c;
        vita_fb[dy * 960 + dx + 1] = c;
        vita_fb[(dy + 1) * 960 + dx] = c;
        vita_fb[(dy + 1) * 960 + dx + 1] = c;
      }
    }
  }

  if (!src || !nonzero) {
    for (int y = 0; y < 544; y++) {
      for (int x = 0; x < 960; x++) {
        uint32_t c = 0xff080818u;
        if (x < 12 || x >= 948 || y < 12 || y >= 532) {
          c = 0xffe040c0u;
        }
        if (((x / 32) ^ (y / 32)) & 1) {
          c ^= 0x00101010u;
        }
        vita_fb[y * 960 + x] = c;
      }
    }
  }

  SceDisplayFrameBuf fb;
  memset(&fb, 0, sizeof(fb));
  fb.size = sizeof(fb);
  fb.base = vita_fb_cur;
  fb.pitch = 960;
  fb.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
  fb.width = 960;
  fb.height = 544;
  int ret = sceDisplaySetFrameBuf(&fb, sync ? SCE_DISPLAY_SETBUF_NEXTFRAME : SCE_DISPLAY_SETBUF_IMMEDIATE);
  static int logged = 0;
  if (!logged) {
    vita_np2_log(ret == 0 ? "display: psp frame ok" : "display: psp frame failed");
    logged = 1;
  }
  return ret;
#endif
}

#define sceDisplaySetFrameBuf(base, line_size, pixel_size, sync) \
  psp_sceDisplaySetFrameBuf((base), (line_size), (pixel_size), (sync))

static inline int sceDisplaySetMode(int mode, int width, int height) {
  (void)mode;
  (void)width;
  (void)height;
  return 0;
}

#define sceDisplayWaitVblank() sceDisplayWaitVblankStart()

static inline void sceGuInit(void) {
}

static inline int sceGeListEnQueue(const void *list, const void *stall, int cbid, const void *arg) {
  (void)list;
  (void)stall;
  (void)cbid;
  (void)arg;
  return 0;
}

static inline int sceGeListSync(int qid, int sync_type) {
  (void)qid;
  (void)sync_type;
  return 0;
}

static inline int sceGeSetCallback(const void *cb) {
  (void)cb;
  return 0;
}

#endif
