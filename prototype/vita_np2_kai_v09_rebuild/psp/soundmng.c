#include	"compiler.h"
#include	"parts.h"
#include	"soundmng.h"
#include	"sound.h"
#include "pg.h"
#if defined(VITA_NP2_PORT)
#include "psp_compat.h"
#endif

#if !defined(DISABLE_SOUND)

#define	NSNDBUF 32

typedef struct {
    BOOL opened;
    int nsndbuf;
    int samples;
    int out_samples;
    int rate;
    SINT16 *buf[NSNDBUF];
} SOUNDMNG;

static SOUNDMNG soundmng;
static SINT16 vita_clip_s16(SINT32 data)
{
    if (data > 32767) {
        return 32767;
    }
    if (data < -32768) {
        return -32768;
    }
    return (SINT16)data;
}

static SINT32 vita_lerp_s32(SINT32 a, SINT32 b, UINT frac)
{
    long long aa = (long long)a * (long long)(0x10000 - frac);
    long long bb = (long long)b * (long long)frac;
    return (SINT32)((aa + bb) >> 16);
}

static void vita_resample_s16_linear(SINT16 *dst, const SINT32 *src, UINT out_frames, UINT in_frames)
{
    UINT i;

    if (dst == NULL || src == NULL || out_frames == 0 || in_frames == 0) {
        return;
    }
    if (in_frames == 1 || out_frames == 1) {
        for (i = 0; i < out_frames; i++) {
            *dst++ = vita_clip_s16(src[0]);
            *dst++ = vita_clip_s16(src[1]);
        }
        return;
    }

    for (i = 0; i < out_frames; i++) {
        unsigned long long pos = ((unsigned long long)i * (unsigned long long)(in_frames - 1) * 0x10000ull) /
                                 (unsigned long long)(out_frames - 1);
        UINT base = (UINT)(pos >> 16);
        UINT frac = (UINT)(pos & 0xffff);
        const SINT32 *sp0;
        const SINT32 *sp1;

        if (base >= (in_frames - 1)) {
            base = in_frames - 1;
            frac = 0;
        }
        sp0 = src + base * 2;
        sp1 = src + ((base + 1 < in_frames) ? (base + 1) : base) * 2;
        *dst++ = vita_clip_s16(vita_lerp_s32(sp0[0], sp1[0], frac));
        *dst++ = vita_clip_s16(vita_lerp_s32(sp0[1], sp1[1], frac));
    }
}

static void satuation_s16_22k(SINT16 *dst, const SINT32 *src, UINT size)
{
    UINT frames = size / (2 * sizeof(SINT16));
    vita_resample_s16_linear(dst, src, frames, (UINT)soundmng.samples);
}

static void satuation_s16_11k(SINT16 *dst, const SINT32 *src, UINT size)
{
    UINT frames = size / (2 * sizeof(SINT16));
    vita_resample_s16_linear(dst, src, frames, (UINT)soundmng.samples);
}

static void *sound_play_cb(void)
{
    int length;
    SINT16 *dst;
    const SINT32 *src;

    length = (int)(soundmng.out_samples * 2 * sizeof(SINT16));
    dst = soundmng.buf[soundmng.nsndbuf];
    src = sound_pcmlock();
    if (src) {
        if (soundmng.rate == 44100) {
            satuation_s16(dst, src, length);
        } else if (soundmng.rate == 22050) {
            satuation_s16_22k(dst, src,length);
        } else if (soundmng.rate == 11025) {
            satuation_s16_11k(dst, src,length);
        }
        sound_pcmunlock(src);
    }
    else {
#if defined(VITA_NP2_PORT)
        static int lockmiss_logs = 0;
        if (lockmiss_logs < 16) {
            vita_np2_log("audio: pcm lock miss");
            lockmiss_logs++;
        }
#endif
        ZeroMemory(dst, length);
    }
    soundmng.nsndbuf = (soundmng.nsndbuf + 1) % NSNDBUF;

    return dst;
}

UINT soundmng_create(UINT rate, UINT ms)
{
    UINT s;
    UINT samples, out_samples;
    SINT16 *tmp;

    if (soundmng.opened) {
        goto smcre_err1;
    }

    // ms = 500?
    s = rate * ms / (NSNDBUF * 1000);
    samples = 1;
    while(s > samples) {
        samples <<= 1;
    }
    // o—Í‘¤‚Í44100ŒÅ’è
    s = 44100 * ms / (NSNDBUF * 1000);
    out_samples = 1;
    while(s > out_samples) {
        out_samples <<= 1;
    }

    soundmng.nsndbuf = 0;
    soundmng.samples = samples;
    soundmng.out_samples = out_samples;
#if defined(VITA_NP2_PORT)
    {
        char logbuf[128];
        snprintf(logbuf, sizeof(logbuf), "audio: create rate=%u ms=%u samples=%u out=%u",
                 rate, ms, samples, out_samples);
        vita_np2_log(logbuf);
    }
#endif
    for (s = 0; s < NSNDBUF; s++) {
        tmp = (SINT16 *)_MALLOC(out_samples * 2 * sizeof(SINT16), "buf");
        if (tmp == NULL) {
            goto smcre_err2;
        }
        soundmng.buf[s] = tmp;
        ZeroMemory(tmp, out_samples * 2 * sizeof(SINT16));
    }

    pgaSetChannelCallback(sound_play_cb);
    pgaInit(out_samples);

    soundmng.rate = rate;
    soundmng.opened = TRUE;

    return(samples);

 smcre_err2:
    for (s = 0; s < NSNDBUF; s++) {
        tmp = soundmng.buf[s];
        soundmng.buf[s] = NULL;
        if (tmp) {
            _MFREE(tmp);
        }
    }

 smcre_err1:
    return(0);
}

void soundmng_destroy(void) {

#if 0
	int		i;
	SINT16	*tmp;

	if (soundmng.opened) {
		soundmng.opened = FALSE;
		SDL_PauseAudio(1);
		SDL_CloseAudio();
		for (i=0; i<NSNDBUF; i++) {
			tmp = soundmng.buf[i];
			soundmng.buf[i] = NULL;
			_MFREE(tmp);
		}
	}
#endif
}

void soundmng_play(void) {

    pgaPause(0);
#if 0
	if (soundmng.opened) {
		SDL_PauseAudio(0);
	}
#endif
}

void soundmng_stop(void) {

    pgaPause(1);
#if 0
	if (soundmng.opened) {
		SDL_PauseAudio(1);
	}
#endif
}

#endif

