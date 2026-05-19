#ifndef VITA_PROFILER_H
#define VITA_PROFILER_H

#if defined(VITA_NP2_PORT)

#include <stdint.h>
#include "compiler.h"

enum {
    VITA_PROF_TASK = 0,
    VITA_PROF_EXEC,
    VITA_PROF_UPDATE,
    VITA_PROF_PRESENT,
    VITA_PROF_WAIT,
    VITA_PROF_AUDIO_CB,
    VITA_PROF_AUDIO_OUT,
    VITA_PROF_IO,
    VITA_PROF_CORE_PRE,
    VITA_PROF_CORE_CPU,
    VITA_PROF_CORE_EVENT,
    VITA_PROF_CORE_POST,
    VITA_PROF_CPU_ALLSTEP,
    VITA_PROF_CPU_STEP,
    VITA_PROF_CPU_FETCH,
    VITA_PROF_CPU_EXEC,
    VITA_PROF_CPU_REP,
    VITA_PROF_NEVT_STEP,
    VITA_PROF_NEVT_CB,
    VITA_PROF_MAX
};

uint64_t vita_prof_now_us(void);
void vita_prof_reset(void);
void vita_prof_begin(unsigned int id);
void vita_prof_end(unsigned int id);
void vita_prof_opcode_begin(unsigned int key);
void vita_prof_opcode_end(unsigned int key);
void vita_prof_add_io(uint64_t usec, UINT bytes, int is_write);
void vita_prof_count_exec(int screen_update);
void vita_prof_count_update(int presented);
void vita_prof_count_wait(void);
void vita_prof_count_audio_cb(void);
void vita_prof_count_audio_out(void);
void vita_prof_report(unsigned int loop_counter,
                      unsigned int drawcount,
                      unsigned int draw_skip,
                      int menuvram_active,
                      int screen_update,
                      int sound_renewal,
                      unsigned int cpu_clock,
                      unsigned int cpu_baseclock,
                      unsigned int cpu_remclock);

#else

#define vita_prof_reset()
#define vita_prof_begin(id)
#define vita_prof_end(id)
#define vita_prof_opcode_begin(key)
#define vita_prof_opcode_end(key)
#define vita_prof_add_io(usec, bytes, is_write)
#define vita_prof_count_exec(screen_update)
#define vita_prof_count_update(presented)
#define vita_prof_count_wait()
#define vita_prof_count_audio_cb()
#define vita_prof_count_audio_out()

#endif

#endif
