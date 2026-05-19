#include "vita_profiler.h"

#if defined(VITA_NP2_PORT)

#include <stdio.h>
#include <psp2/kernel/threadmgr.h>
#include "pccore.h"
#include "vita_compat/psp_compat.h"

#define VITA_PROF_OPCODE_BUCKETS 512
#define VITA_PROF_OPCODE_TOPN 8

typedef struct {
    uint64_t total_us;
    uint64_t max_us;
    uint64_t start_us;
    unsigned int count;
} VITA_PROF_BUCKET;

static VITA_PROF_BUCKET vita_prof_bucket[VITA_PROF_MAX];
static VITA_PROF_BUCKET vita_prof_opcode_bucket[VITA_PROF_OPCODE_BUCKETS];
static uint64_t vita_prof_last_report_us;
static unsigned int vita_prof_last_drawcount;
static unsigned int vita_prof_exec_count;
static unsigned int vita_prof_exec_draw_count;
static unsigned int vita_prof_update_count;
static unsigned int vita_prof_present_count;
static unsigned int vita_prof_wait_count;
static unsigned int vita_prof_audio_cb_count;
static unsigned int vita_prof_audio_out_count;
static unsigned int vita_prof_io_read_count;
static unsigned int vita_prof_io_write_count;
static UINT vita_prof_io_read_bytes;
static UINT vita_prof_io_write_bytes;

uint64_t vita_prof_now_us(void)
{
    return (uint64_t)sceKernelGetSystemTimeWide();
}

void vita_prof_reset(void)
{
    unsigned int i;
    for (i = 0; i < VITA_PROF_MAX; i++) {
        vita_prof_bucket[i].total_us = 0;
        vita_prof_bucket[i].max_us = 0;
        vita_prof_bucket[i].start_us = 0;
        vita_prof_bucket[i].count = 0;
    }
    for (i = 0; i < VITA_PROF_OPCODE_BUCKETS; i++) {
        vita_prof_opcode_bucket[i].total_us = 0;
        vita_prof_opcode_bucket[i].max_us = 0;
        vita_prof_opcode_bucket[i].start_us = 0;
        vita_prof_opcode_bucket[i].count = 0;
    }
    vita_prof_last_report_us = vita_prof_now_us();
    vita_prof_last_drawcount = 0;
    vita_prof_exec_count = 0;
    vita_prof_exec_draw_count = 0;
    vita_prof_update_count = 0;
    vita_prof_present_count = 0;
    vita_prof_wait_count = 0;
    vita_prof_audio_cb_count = 0;
    vita_prof_audio_out_count = 0;
    vita_prof_io_read_count = 0;
    vita_prof_io_write_count = 0;
    vita_prof_io_read_bytes = 0;
    vita_prof_io_write_bytes = 0;
}

void vita_prof_begin(unsigned int id)
{
    if (id < VITA_PROF_MAX) {
        vita_prof_bucket[id].start_us = vita_prof_now_us();
    }
}

void vita_prof_end(unsigned int id)
{
    uint64_t now;
    uint64_t dt;
    if (id >= VITA_PROF_MAX || vita_prof_bucket[id].start_us == 0) {
        return;
    }
    now = vita_prof_now_us();
    dt = now - vita_prof_bucket[id].start_us;
    vita_prof_bucket[id].total_us += dt;
    if (dt > vita_prof_bucket[id].max_us) {
        vita_prof_bucket[id].max_us = dt;
    }
    vita_prof_bucket[id].count++;
    vita_prof_bucket[id].start_us = 0;
}

void vita_prof_opcode_begin(unsigned int key)
{
    if (key < VITA_PROF_OPCODE_BUCKETS) {
        vita_prof_opcode_bucket[key].start_us = vita_prof_now_us();
    }
}

void vita_prof_opcode_end(unsigned int key)
{
    uint64_t now;
    uint64_t dt;

    if (key >= VITA_PROF_OPCODE_BUCKETS || vita_prof_opcode_bucket[key].start_us == 0) {
        return;
    }
    now = vita_prof_now_us();
    dt = now - vita_prof_opcode_bucket[key].start_us;
    vita_prof_opcode_bucket[key].total_us += dt;
    if (dt > vita_prof_opcode_bucket[key].max_us) {
        vita_prof_opcode_bucket[key].max_us = dt;
    }
    vita_prof_opcode_bucket[key].count++;
    vita_prof_opcode_bucket[key].start_us = 0;
}

void vita_prof_add_io(uint64_t usec, UINT bytes, int is_write)
{
    vita_prof_bucket[VITA_PROF_IO].total_us += usec;
    if (usec > vita_prof_bucket[VITA_PROF_IO].max_us) {
        vita_prof_bucket[VITA_PROF_IO].max_us = usec;
    }
    vita_prof_bucket[VITA_PROF_IO].count++;
    if (is_write) {
        vita_prof_io_write_count++;
        vita_prof_io_write_bytes += bytes;
    }
    else {
        vita_prof_io_read_count++;
        vita_prof_io_read_bytes += bytes;
    }
}

void vita_prof_count_exec(int screen_update)
{
    vita_prof_exec_count++;
    if (screen_update) {
        vita_prof_exec_draw_count++;
    }
}

void vita_prof_count_update(int presented)
{
    if (presented) {
        vita_prof_present_count++;
    }
    else {
        vita_prof_update_count++;
    }
}

void vita_prof_count_wait(void)
{
    vita_prof_wait_count++;
}

void vita_prof_count_audio_cb(void)
{
    vita_prof_audio_cb_count++;
}

void vita_prof_count_audio_out(void)
{
    vita_prof_audio_out_count++;
}

static void vita_prof_opcode_label(unsigned int key, char *buf, size_t size)
{
    if (key < 0x100) {
        snprintf(buf, size, "%02X", key);
    }
    else {
        snprintf(buf, size, "0F %02X", key & 0xff);
    }
}

static unsigned long vita_prof_ms(unsigned int id)
{
    return (unsigned long)(vita_prof_bucket[id].total_us / 1000);
}

static unsigned long vita_prof_max_ms(unsigned int id)
{
    return (unsigned long)(vita_prof_bucket[id].max_us / 1000);
}

void vita_prof_report(unsigned int loop_counter,
                      unsigned int drawcount,
                      unsigned int draw_skip,
                      int menuvram_active,
                      int screen_update,
                      int sound_renewal,
                      unsigned int cpu_clock,
                      unsigned int cpu_baseclock,
                      unsigned int cpu_remclock)
{
    uint64_t now = vita_prof_now_us();
    uint64_t elapsed = now - vita_prof_last_report_us;
    char buf[2048];
    unsigned int draws;

    if (elapsed < 1000000ULL) {
        return;
    }

    draws = drawcount - vita_prof_last_drawcount;
    snprintf(buf, sizeof(buf),
             "profile: sec=%lu loop=%u menu=%d draw=%u skp=%u "
             "task=%lums/%u exec=%lums/%u upd=%lums/%u present=%lums/%u wait=%lums/%u "
             "audio_cb=%lums/%u audio_out=%lums/%u io=%lums/%u "
             "core_pre=%lums/%u core_cpu=%lums/%u core_evt=%lums/%u core_post=%lums/%u "
             "cpu_all=%lums/%u cpu_step=%lums/%u cpu_fetch=%lums/%u cpu_exec=%lums/%u cpu_rep=%lums/%u "
             "nevt_step=%lums/%u nevt_cb=%lums/%u rb=%u/%u wb=%u/%u "
             "max exec=%lums upd=%lums present=%lums wait=%lums audio=%lums io=%lums "
             "core_pre=%lums core_cpu=%lums core_evt=%lums core_post=%lums cpu_all=%lums cpu_step=%lums cpu_fetch=%lums cpu_exec=%lums cpu_rep=%lums nevt_step=%lums nevt_cb=%lums "
             "pc drawexec=%u scrupd=%d sndren=%d clk=%u base=%u rem=%u",
             (unsigned long)(elapsed / 1000000ULL),
             loop_counter,
             menuvram_active,
             draws,
             draw_skip,
             vita_prof_ms(VITA_PROF_TASK), vita_prof_bucket[VITA_PROF_TASK].count,
             vita_prof_ms(VITA_PROF_EXEC), vita_prof_exec_count,
             vita_prof_ms(VITA_PROF_UPDATE), vita_prof_update_count,
             vita_prof_ms(VITA_PROF_PRESENT), vita_prof_present_count,
             vita_prof_ms(VITA_PROF_WAIT), vita_prof_wait_count,
             vita_prof_ms(VITA_PROF_AUDIO_CB), vita_prof_audio_cb_count,
             vita_prof_ms(VITA_PROF_AUDIO_OUT), vita_prof_audio_out_count,
             vita_prof_ms(VITA_PROF_IO), vita_prof_bucket[VITA_PROF_IO].count,
             vita_prof_ms(VITA_PROF_CORE_PRE), vita_prof_bucket[VITA_PROF_CORE_PRE].count,
             vita_prof_ms(VITA_PROF_CORE_CPU), vita_prof_bucket[VITA_PROF_CORE_CPU].count,
             vita_prof_ms(VITA_PROF_CORE_EVENT), vita_prof_bucket[VITA_PROF_CORE_EVENT].count,
             vita_prof_ms(VITA_PROF_CORE_POST), vita_prof_bucket[VITA_PROF_CORE_POST].count,
             vita_prof_ms(VITA_PROF_CPU_ALLSTEP), vita_prof_bucket[VITA_PROF_CPU_ALLSTEP].count,
             vita_prof_ms(VITA_PROF_CPU_STEP), vita_prof_bucket[VITA_PROF_CPU_STEP].count,
             vita_prof_ms(VITA_PROF_CPU_FETCH), vita_prof_bucket[VITA_PROF_CPU_FETCH].count,
             vita_prof_ms(VITA_PROF_CPU_EXEC), vita_prof_bucket[VITA_PROF_CPU_EXEC].count,
             vita_prof_ms(VITA_PROF_CPU_REP), vita_prof_bucket[VITA_PROF_CPU_REP].count,
             vita_prof_ms(VITA_PROF_NEVT_STEP), vita_prof_bucket[VITA_PROF_NEVT_STEP].count,
             vita_prof_ms(VITA_PROF_NEVT_CB), vita_prof_bucket[VITA_PROF_NEVT_CB].count,
             vita_prof_io_read_count, vita_prof_io_read_bytes,
             vita_prof_io_write_count, vita_prof_io_write_bytes,
             vita_prof_max_ms(VITA_PROF_EXEC),
             vita_prof_max_ms(VITA_PROF_UPDATE),
             vita_prof_max_ms(VITA_PROF_PRESENT),
             vita_prof_max_ms(VITA_PROF_WAIT),
             vita_prof_max_ms(VITA_PROF_AUDIO_CB),
             vita_prof_max_ms(VITA_PROF_IO),
             vita_prof_max_ms(VITA_PROF_CORE_PRE),
             vita_prof_max_ms(VITA_PROF_CORE_CPU),
             vita_prof_max_ms(VITA_PROF_CORE_EVENT),
             vita_prof_max_ms(VITA_PROF_CORE_POST),
             vita_prof_max_ms(VITA_PROF_CPU_ALLSTEP),
             vita_prof_max_ms(VITA_PROF_CPU_STEP),
             vita_prof_max_ms(VITA_PROF_CPU_FETCH),
             vita_prof_max_ms(VITA_PROF_CPU_EXEC),
             vita_prof_max_ms(VITA_PROF_CPU_REP),
             vita_prof_max_ms(VITA_PROF_NEVT_STEP),
             vita_prof_max_ms(VITA_PROF_NEVT_CB),
             vita_prof_exec_draw_count,
             screen_update,
             sound_renewal,
             cpu_clock,
             cpu_baseclock,
             cpu_remclock);
    vita_np2_log(buf);

    {
        unsigned int used[VITA_PROF_OPCODE_BUCKETS];
        unsigned int top;
        unsigned int i;
        for (i = 0; i < VITA_PROF_OPCODE_BUCKETS; i++) {
            used[i] = 0;
        }
        for (top = 0; top < VITA_PROF_OPCODE_TOPN; top++) {
            unsigned int best = VITA_PROF_OPCODE_BUCKETS;
            uint64_t best_total = 0;
            for (i = 0; i < VITA_PROF_OPCODE_BUCKETS; i++) {
                if (used[i]) {
                    continue;
                }
                if (vita_prof_opcode_bucket[i].count == 0) {
                    continue;
                }
                if (vita_prof_opcode_bucket[i].total_us > best_total) {
                    best_total = vita_prof_opcode_bucket[i].total_us;
                    best = i;
                }
            }
            if (best >= VITA_PROF_OPCODE_BUCKETS) {
                break;
            }
            {
                char opbuf[16];
                char line[256];
                unsigned long total_ms = (unsigned long)(vita_prof_opcode_bucket[best].total_us / 1000);
                unsigned long max_ms = (unsigned long)(vita_prof_opcode_bucket[best].max_us / 1000);
                vita_prof_opcode_label(best, opbuf, sizeof(opbuf));
                snprintf(line, sizeof(line),
                         "profile-op: #%u op=%s total=%lums cnt=%u max=%lums",
                         top + 1, opbuf, total_ms, vita_prof_opcode_bucket[best].count, max_ms);
                vita_np2_log(line);
            }
            used[best] = 1;
        }
    }

    vita_prof_reset();
    vita_prof_last_drawcount = drawcount;
    vita_prof_last_report_us = now;
}

#endif
