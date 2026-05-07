#include <compiler.h>
#include <ia32/cpu.h>

#if defined(VITA_NP2_PORT)
void iocore_vita_diag_log_recent(void)
{
}

void SSE2_PSxxWimm(void)
{
    EXCEPTION(UD_EXCEPTION, 0);
}

void SSE2_PSxxDimm(void)
{
    EXCEPTION(UD_EXCEPTION, 0);
}

void SSE2_PSxxQimm(void)
{
    EXCEPTION(UD_EXCEPTION, 0);
}
#endif
