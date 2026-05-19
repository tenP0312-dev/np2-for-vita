/*
 * Copyright (c) 2002-2003 NONAKA Kimihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <compiler.h>
#include <dosio.h>
#include "cpu.h"
#include "ia32.mcr"

#include "inst_table.h"
#include "instructions/string_inst.h"

#if defined(ENABLE_TRAP)
#include "trap/steptrap.h"
#endif

#if defined(SUPPORT_ASYNC_CPU)
#include <timing.h>
#include <nevent.h>
#include <pccore.h>
#include	<io/iocore.h>
#include	<sound/sound.h>
#include	<sound/beep.h>
#include	<sound/fmboard.h>
#include	<sound/soundrom.h>
#include	<cbus/mpu98ii.h>
#if defined(SUPPORT_SMPU98)
#include	<cbus/smpu98.h>
#endif
#endif

sigjmp_buf exec_1step_jmpbuf;

#if defined(IA32_INSTRUCTION_TRACE)
typedef struct {
	CPU_REGS		regs;
	disasm_context_t	disasm;

	BYTE			op[MAX_PREFIX + 2];
	int			opbytes;
} ia32_context_t;

#define	NCTX	1024

ia32_context_t ctx[NCTX];
int ctx_index = 0;

int cpu_inst_trace = 0;
#endif

#if defined(DEBUG)
int cpu_debug_rep_cont = 0;
CPU_REGS cpu_debug_rep_regs;
#endif

#define VITA_NATIVE_INLINE_LOOP		1
#define VITA_NATIVE_INLINE_STOS		1
#define VITA_NATIVE_INLINE_SCAS_LOOPE   1
#define VITA_NATIVE_INLINE_IOWAIT       0
#define VITA_NATIVE_INLINE_SELF_LOOP    1
#define VITA_NATIVE_INLINE_A13B72       0
#define VITA_NATIVE_INLINE_LODS         1
/* Single-byte 0xaf is SCASW/SCASD. 0f af is IMUL and is handled elsewhere. */
#define VITA_NATIVE_INLINE_IMUL		0

#if VITA_NP2_DIAG
UINT32 vita_cpu_diag_stos_hits = 0;
UINT32 vita_cpu_diag_stos_iters = 0;
UINT32 vita_cpu_diag_scas_hits = 0;
UINT32 vita_cpu_diag_scas_iters = 0;
UINT32 vita_cpu_diag_self_hits = 0;
UINT32 vita_cpu_diag_self_iters = 0;
static UINT32 vita_cpu_diag_op[256];
static UINT32 vita_cpu_diag_0f[256];
static UINT32 vita_cpu_diag_pair[256][256];
static UINT8 vita_cpu_diag_last_op;
static int vita_cpu_diag_has_last_op;
typedef struct {
	UINT16 cs;
	UINT32 eip;
	UINT8 op;
	UINT32 count;
	UINT8 bytes[16];
	UINT8 bytelen;
} VITA_CPU_DIAG_PC;
typedef struct {
	UINT16 cs;
	UINT32 eip;
	UINT32 addr;
	UINT32 branch;
	UINT8 bytes[12];
	UINT8 modrm;
	UINT8 len;
	UINT32 count;
} VITA_CPU_DIAG_A13B72;
static VITA_CPU_DIAG_PC vita_cpu_diag_pc[16];
static VITA_CPU_DIAG_A13B72 vita_cpu_diag_a13b72[8];
#define VITA_CPU_DIAG_INC(x) (x++)
static void vita_cpu_diag_record_a13b72(void);
static void vita_cpu_diag_record(UINT8 op)
{
	int i;
	int repl;

	if (op == 0xa1) {
		vita_cpu_diag_record_a13b72();
	}

	vita_cpu_diag_op[op]++;
	if (vita_cpu_diag_has_last_op) {
		vita_cpu_diag_pair[vita_cpu_diag_last_op][op]++;
	}
	vita_cpu_diag_last_op = op;
	vita_cpu_diag_has_last_op = 1;

	if (op == 0x0f) {
		vita_cpu_diag_0f[(UINT8)cpu_codefetch(CPU_EIP)]++;
	}

	repl = 0;
	for (i = 0; i < (int)NELEMENTS(vita_cpu_diag_pc); i++) {
		if (vita_cpu_diag_pc[i].count &&
		    vita_cpu_diag_pc[i].cs == CPU_CS &&
		    vita_cpu_diag_pc[i].eip == CPU_PREV_EIP) {
			vita_cpu_diag_pc[i].count++;
			vita_cpu_diag_pc[i].op = op;
			return;
		}
		if (vita_cpu_diag_pc[i].count < vita_cpu_diag_pc[repl].count) {
			repl = i;
		}
	}
	vita_cpu_diag_pc[repl].cs = CPU_CS;
	vita_cpu_diag_pc[repl].eip = CPU_PREV_EIP;
	vita_cpu_diag_pc[repl].op = op;
	vita_cpu_diag_pc[repl].bytelen = (UINT8)NELEMENTS(vita_cpu_diag_pc[repl].bytes);
	for (i = 0; i < (int)NELEMENTS(vita_cpu_diag_pc[repl].bytes); i++) {
		vita_cpu_diag_pc[repl].bytes[i] = (UINT8)cpu_codefetch(CPU_PREV_EIP + i);
	}
	vita_cpu_diag_pc[repl].count = 1;
}
static UINT vita_cpu_diag_modrm_len(UINT8 modrm, UINT32 eip_after_modrm, int as32)
{
	UINT mod = (modrm >> 6) & 3;
	UINT rm = modrm & 7;
	UINT len = 1;

	if (mod == 3) {
		return len;
	}
	if (!as32) {
		if (mod == 0 && rm == 6) {
			len += 2;
		} else if (mod == 1) {
			len += 1;
		} else if (mod == 2) {
			len += 2;
		}
		return len;
	}
	if (rm == 4) {
		UINT8 sib = (UINT8)cpu_codefetch(eip_after_modrm);
		UINT base = sib & 7;
		len += 1;
		if (mod == 0 && base == 5) {
			len += 4;
		}
	}
	if (mod == 0 && rm == 5) {
		len += 4;
	} else if (mod == 1) {
		len += 1;
	} else if (mod == 2) {
		len += 4;
	}
	return len;
}
static void vita_cpu_diag_record_a13b72(void)
{
	UINT32 eip;
	UINT32 addr;
	UINT32 after_a1;
	UINT32 after_3b;
	UINT32 branch;
	UINT8 modrm;
	UINT8 jop;
	UINT8 disp;
	UINT len;
	int i;
	int repl;

	eip = CPU_PREV_EIP;
	addr = CPU_INST_AS32 ? cpu_codefetch_d(CPU_EIP) : cpu_codefetch_w(CPU_EIP);
	after_a1 = CPU_EIP + (CPU_INST_AS32 ? 4 : 2);
	if ((UINT8)cpu_codefetch(after_a1) != 0x3b) {
		return;
	}
	modrm = (UINT8)cpu_codefetch(after_a1 + 1);
	len = 1 + vita_cpu_diag_modrm_len(modrm, after_a1 + 2, CPU_INST_AS32);
	after_3b = after_a1 + len;
	jop = (UINT8)cpu_codefetch(after_3b);
	if (jop != 0x72) {
		return;
	}
	disp = (UINT8)cpu_codefetch(after_3b + 1);
	branch = after_3b + 2 + (SINT8)disp;
	if (!CPU_INST_OP32) {
		branch &= 0xffff;
	}

	repl = 0;
	for (i = 0; i < (int)NELEMENTS(vita_cpu_diag_a13b72); i++) {
		if (vita_cpu_diag_a13b72[i].count &&
		    vita_cpu_diag_a13b72[i].cs == CPU_CS &&
		    vita_cpu_diag_a13b72[i].eip == eip &&
		    vita_cpu_diag_a13b72[i].addr == addr &&
		    vita_cpu_diag_a13b72[i].branch == branch) {
			vita_cpu_diag_a13b72[i].count++;
			return;
		}
		if (vita_cpu_diag_a13b72[i].count < vita_cpu_diag_a13b72[repl].count) {
			repl = i;
		}
	}
	vita_cpu_diag_a13b72[repl].cs = CPU_CS;
	vita_cpu_diag_a13b72[repl].eip = eip;
	vita_cpu_diag_a13b72[repl].addr = addr;
	vita_cpu_diag_a13b72[repl].branch = branch;
	vita_cpu_diag_a13b72[repl].modrm = modrm;
	vita_cpu_diag_a13b72[repl].len = (UINT8)(after_3b + 2 - eip);
	for (i = 0; i < (int)NELEMENTS(vita_cpu_diag_a13b72[repl].bytes); i++) {
		vita_cpu_diag_a13b72[repl].bytes[i] = (UINT8)cpu_codefetch(eip + i);
	}
	vita_cpu_diag_a13b72[repl].count = 1;
}
static UINT vita_cpu_diag_pick_top_op(const UINT32 counts[256], const UINT8 used[], UINT used_count)
{
	UINT best = 0;
	UINT i, u;
	for (i = 0; i < 256; i++) {
		int skip = 0;
		for (u = 0; u < used_count; u++) {
			if (used[u] == i) {
				skip = 1;
				break;
			}
		}
		if (!skip && counts[i] > counts[best]) {
			best = i;
		}
	}
	return best;
}
static UINT32 vita_cpu_diag_pair_count(UINT pair)
{
	return vita_cpu_diag_pair[(pair >> 8) & 0xff][pair & 0xff];
}
static UINT vita_cpu_diag_pick_top_pair(const UINT16 used[], UINT used_count)
{
	UINT best = 0;
	UINT i, u;
	for (i = 0; i < 65536; i++) {
		int skip = 0;
		for (u = 0; u < used_count; u++) {
			if (used[u] == i) {
				skip = 1;
				break;
			}
		}
		if (!skip && vita_cpu_diag_pair_count(i) > vita_cpu_diag_pair_count(best)) {
			best = i;
		}
	}
	return best;
}
void vita_cpu_diag_log_all(void)
{
	char buf[256];
	UINT8 used_op[12];
	UINT8 used_0f[8];
	UINT16 used_pair[8];
	UINT i;
	UINT pcbyte_logs = 0;

	snprintf(buf, sizeof(buf),
	         "cpuopt: stos=%lu/%lu scas=%lu/%lu self=%lu/%lu cs=%04x eip=%08x rem=%ld",
	         (unsigned long)vita_cpu_diag_stos_hits,
	         (unsigned long)vita_cpu_diag_stos_iters,
	         (unsigned long)vita_cpu_diag_scas_hits,
	         (unsigned long)vita_cpu_diag_scas_iters,
	         (unsigned long)vita_cpu_diag_self_hits,
	         (unsigned long)vita_cpu_diag_self_iters,
	         CPU_CS,
	         CPU_EIP,
	         (long)CPU_REMCLOCK);
	vita_np2_log(buf);

	buf[0] = '\0';
	milstr_ncat(buf, "oprank:", sizeof(buf));
	for (i = 0; i < NELEMENTS(used_op); i++) {
		char tmp[24];
		UINT op = vita_cpu_diag_pick_top_op(vita_cpu_diag_op, used_op, i);
		used_op[i] = (UINT8)op;
		if (vita_cpu_diag_op[op] == 0) {
			break;
		}
		snprintf(tmp, sizeof(tmp), " %02x=%lu", op, (unsigned long)vita_cpu_diag_op[op]);
		milstr_ncat(buf, tmp, sizeof(buf));
	}
	vita_np2_log(buf);

	buf[0] = '\0';
	milstr_ncat(buf, "op0frank:", sizeof(buf));
	for (i = 0; i < NELEMENTS(used_0f); i++) {
		char tmp[24];
		UINT op = vita_cpu_diag_pick_top_op(vita_cpu_diag_0f, used_0f, i);
		used_0f[i] = (UINT8)op;
		if (vita_cpu_diag_0f[op] == 0) {
			break;
		}
		snprintf(tmp, sizeof(tmp), " %02x=%lu", op, (unsigned long)vita_cpu_diag_0f[op]);
		milstr_ncat(buf, tmp, sizeof(buf));
	}
	vita_np2_log(buf);

	buf[0] = '\0';
	milstr_ncat(buf, "pairrank:", sizeof(buf));
	for (i = 0; i < NELEMENTS(used_pair); i++) {
		char tmp[28];
		UINT pair = vita_cpu_diag_pick_top_pair(used_pair, i);
		used_pair[i] = (UINT16)pair;
		if (vita_cpu_diag_pair_count(pair) == 0) {
			break;
		}
		snprintf(tmp, sizeof(tmp), " %02x%02x=%lu", (pair >> 8) & 0xff, pair & 0xff, (unsigned long)vita_cpu_diag_pair_count(pair));
		milstr_ncat(buf, tmp, sizeof(buf));
	}
	vita_np2_log(buf);

	buf[0] = '\0';
	milstr_ncat(buf, "pcrank:", sizeof(buf));
	for (i = 0; i < NELEMENTS(vita_cpu_diag_pc); i++) {
		UINT best = 0;
		UINT j;
		char tmp[36];
		for (j = 0; j < NELEMENTS(vita_cpu_diag_pc); j++) {
			if (vita_cpu_diag_pc[j].count > vita_cpu_diag_pc[best].count) {
				best = j;
			}
		}
		if (vita_cpu_diag_pc[best].count == 0) {
			break;
		}
		snprintf(tmp, sizeof(tmp), " %04x:%08lx/%02x=%lu",
		         vita_cpu_diag_pc[best].cs,
		         (unsigned long)vita_cpu_diag_pc[best].eip,
		         vita_cpu_diag_pc[best].op,
		         (unsigned long)vita_cpu_diag_pc[best].count);
		milstr_ncat(buf, tmp, sizeof(buf));
		if (pcbyte_logs < 6) {
			char bbuf[192];
			char hex[4];
			UINT k;
			snprintf(bbuf, sizeof(bbuf), "pcbytes: %04x:%08lx/%02x n=%lu bytes=",
			         vita_cpu_diag_pc[best].cs,
			         (unsigned long)vita_cpu_diag_pc[best].eip,
			         vita_cpu_diag_pc[best].op,
			         (unsigned long)vita_cpu_diag_pc[best].count);
			for (k = 0; k < vita_cpu_diag_pc[best].bytelen; k++) {
				snprintf(hex, sizeof(hex), "%02x", vita_cpu_diag_pc[best].bytes[k]);
				milstr_ncat(bbuf, hex, sizeof(bbuf));
			}
			vita_np2_log(bbuf);
			pcbyte_logs++;
		}
		vita_cpu_diag_pc[best].count = 0;
	}
	vita_np2_log(buf);

	buf[0] = '\0';
	milstr_ncat(buf, "a13b72:", sizeof(buf));
	for (i = 0; i < NELEMENTS(vita_cpu_diag_a13b72); i++) {
		UINT best = 0;
		UINT j;
		char tmp[96];
		for (j = 0; j < NELEMENTS(vita_cpu_diag_a13b72); j++) {
			if (vita_cpu_diag_a13b72[j].count > vita_cpu_diag_a13b72[best].count) {
				best = j;
			}
		}
		if (vita_cpu_diag_a13b72[best].count == 0) {
			break;
		}
		snprintf(tmp, sizeof(tmp),
		         " %04x:%08lx addr=%08lx br=%08lx m=%02x len=%u n=%lu bytes=%02x%02x%02x%02x%02x%02x%02x%02x",
		         vita_cpu_diag_a13b72[best].cs,
		         (unsigned long)vita_cpu_diag_a13b72[best].eip,
		         (unsigned long)vita_cpu_diag_a13b72[best].addr,
		         (unsigned long)vita_cpu_diag_a13b72[best].branch,
		         vita_cpu_diag_a13b72[best].modrm,
		         vita_cpu_diag_a13b72[best].len,
		         (unsigned long)vita_cpu_diag_a13b72[best].count,
		         vita_cpu_diag_a13b72[best].bytes[0],
		         vita_cpu_diag_a13b72[best].bytes[1],
		         vita_cpu_diag_a13b72[best].bytes[2],
		         vita_cpu_diag_a13b72[best].bytes[3],
		         vita_cpu_diag_a13b72[best].bytes[4],
		         vita_cpu_diag_a13b72[best].bytes[5],
		         vita_cpu_diag_a13b72[best].bytes[6],
		         vita_cpu_diag_a13b72[best].bytes[7]);
		milstr_ncat(buf, tmp, sizeof(buf));
		vita_cpu_diag_a13b72[best].count = 0;
	}
	vita_np2_log(buf);

	vita_cpu_diag_stos_hits = 0;
	vita_cpu_diag_stos_iters = 0;
	vita_cpu_diag_scas_hits = 0;
	vita_cpu_diag_scas_iters = 0;
	vita_cpu_diag_self_hits = 0;
	vita_cpu_diag_self_iters = 0;
	memset(vita_cpu_diag_op, 0, sizeof(vita_cpu_diag_op));
	memset(vita_cpu_diag_0f, 0, sizeof(vita_cpu_diag_0f));
	memset(vita_cpu_diag_pair, 0, sizeof(vita_cpu_diag_pair));
	memset(vita_cpu_diag_pc, 0, sizeof(vita_cpu_diag_pc));
	memset(vita_cpu_diag_a13b72, 0, sizeof(vita_cpu_diag_a13b72));
	vita_cpu_diag_has_last_op = 0;
}
#else
#define VITA_CPU_DIAG_INC(x)
#define vita_cpu_diag_record(op)
#endif

#if VITA_NATIVE_INLINE_LODS
static void
cpu_inline_lods_ac(void)
{
	CPU_WORKCLOCK(5);
	CPU_INST_SEGREG_INDEX = DS_FIX;
	if (!CPU_INST_AS32) {
		CPU_AL = cpu_vmemoryread(CPU_INST_SEGREG_INDEX, CPU_SI);
		CPU_SI += STRING_DIR;
	} else {
		CPU_AL = cpu_vmemoryread(CPU_INST_SEGREG_INDEX, CPU_ESI);
		CPU_ESI += STRING_DIR;
	}
}

static void
cpu_inline_lods_ad(void)
{
	CPU_WORKCLOCK(5);
	CPU_INST_SEGREG_INDEX = DS_FIX;
	if (!CPU_INST_OP32) {
		if (!CPU_INST_AS32) {
			CPU_AX = cpu_vmemoryread_w(CPU_INST_SEGREG_INDEX, CPU_SI);
			CPU_SI += STRING_DIRx2;
		} else {
			CPU_AX = cpu_vmemoryread_w(CPU_INST_SEGREG_INDEX, CPU_ESI);
			CPU_ESI += STRING_DIRx2;
		}
	} else {
		if (!CPU_INST_AS32) {
			CPU_EAX = cpu_vmemoryread_d(CPU_INST_SEGREG_INDEX, CPU_SI);
			CPU_SI += STRING_DIRx4;
		} else {
			CPU_EAX = cpu_vmemoryread_d(CPU_INST_SEGREG_INDEX, CPU_ESI);
			CPU_ESI += STRING_DIRx4;
		}
	}
}
#endif
#if VITA_NATIVE_INLINE_STOS
static void
cpu_inline_stos_ab(void)
{
	VITA_CPU_DIAG_INC(vita_cpu_diag_stos_iters);
	CPU_WORKCLOCK(3);
	if (!CPU_INST_OP32) {
		if (!CPU_INST_AS32) {
			cpu_vmemorywrite_w(CPU_ES_INDEX, CPU_DI, CPU_AX);
			CPU_DI += STRING_DIRx2;
		} else {
			cpu_vmemorywrite_w(CPU_ES_INDEX, CPU_EDI, CPU_AX);
			CPU_EDI += STRING_DIRx2;
		}
	} else {
		if (!CPU_INST_AS32) {
			cpu_vmemorywrite_d(CPU_ES_INDEX, CPU_DI, CPU_EAX);
			CPU_DI += STRING_DIRx4;
		} else {
			cpu_vmemorywrite_d(CPU_ES_INDEX, CPU_EDI, CPU_EAX);
			CPU_EDI += STRING_DIRx4;
		}
	}
}

static int
cpu_inline_try_stos_loop_ab(void)
{
	UINT32 loop_eip;
	UINT32 after_loop_eip;
	UINT32 loop_target;
	UINT8 loop_op;
	UINT8 loop_disp;

	if (CPU_INST_REPUSE) {
		return 0;
	}

	loop_eip = CPU_EIP;
	loop_op = (UINT8)cpu_codefetch(loop_eip);
	if (loop_op != 0xe2) {
		cpu_inline_stos_ab();
		return 1;
	}

	loop_disp = (UINT8)cpu_codefetch(loop_eip + 1);
	after_loop_eip = loop_eip + 2;
	loop_target = after_loop_eip + (SINT8)loop_disp;
	if (!CPU_INST_OP32) {
		loop_target &= 0xffff;
	}
	if (loop_target != CPU_PREV_EIP) {
		cpu_inline_stos_ab();
		return 1;
	}

	VITA_CPU_DIAG_INC(vita_cpu_diag_stos_hits);
	for (;;) {
		cpu_inline_stos_ab();
		if (CPU_REMCLOCK <= 0) {
			CPU_EIP = loop_eip;
			return 1;
		}

		if (!CPU_INST_AS32) {
			CPU_CX--;
			if (CPU_CX != 0) {
				CPU_WORKCLOCK(8);
				CPU_EIP = loop_target;
				if (CPU_REMCLOCK <= 0) {
					return 1;
				}
			} else {
				CPU_WORKCLOCK(4);
				CPU_EIP = after_loop_eip;
				return 1;
			}
		} else {
			CPU_ECX--;
			if (CPU_ECX != 0) {
				CPU_WORKCLOCK(8);
				CPU_EIP = loop_target;
				if (CPU_REMCLOCK <= 0) {
					return 1;
				}
			} else {
				CPU_WORKCLOCK(4);
				CPU_EIP = after_loop_eip;
				return 1;
			}
		}
	}
}
#endif

#if VITA_NATIVE_INLINE_SCAS_LOOPE
static void
cpu_inline_scas_af(void)
{
	UINT32 src, dst, res;

	VITA_CPU_DIAG_INC(vita_cpu_diag_scas_iters);
	CPU_WORKCLOCK(7);
	if (!CPU_INST_OP32) {
		dst = CPU_AX;
		if (!CPU_INST_AS32) {
			src = cpu_vmemoryread_w(CPU_ES_INDEX, CPU_DI);
			WORD_SUB(res, dst, src);
			CPU_DI += STRING_DIRx2;
		} else {
			src = cpu_vmemoryread_w(CPU_ES_INDEX, CPU_EDI);
			WORD_SUB(res, dst, src);
			CPU_EDI += STRING_DIRx2;
		}
	} else {
		dst = CPU_EAX;
		if (!CPU_INST_AS32) {
			src = cpu_vmemoryread_d(CPU_ES_INDEX, CPU_DI);
			DWORD_SUB(res, dst, src);
			CPU_DI += STRING_DIRx4;
		} else {
			src = cpu_vmemoryread_d(CPU_ES_INDEX, CPU_EDI);
			DWORD_SUB(res, dst, src);
			CPU_EDI += STRING_DIRx4;
		}
	}
}

static int
cpu_inline_try_scas_loope_af(void)
{
	UINT32 loop_eip;
	UINT32 after_loop_eip;
	UINT32 loop_target;
	UINT8 loop_op;
	UINT8 loop_disp;

	if (CPU_INST_REPUSE) {
		return 0;
	}

	loop_eip = CPU_EIP;
	loop_op = (UINT8)cpu_codefetch(loop_eip);
	if (loop_op != 0xe1) {
		return 0;
	}

	loop_disp = (UINT8)cpu_codefetch(loop_eip + 1);
	after_loop_eip = loop_eip + 2;
	loop_target = after_loop_eip + (SINT8)loop_disp;
	if (!CPU_INST_OP32) {
		loop_target &= 0xffff;
	}
	if (loop_target != CPU_PREV_EIP) {
		return 0;
	}

	VITA_CPU_DIAG_INC(vita_cpu_diag_scas_hits);
	for (;;) {
		cpu_inline_scas_af();
		if (CPU_REMCLOCK <= 0) {
			CPU_EIP = loop_eip;
			return 1;
		}

		if (!CPU_INST_AS32) {
			CPU_CX--;
			if (CPU_CX != 0 && CC_Z) {
				CPU_WORKCLOCK(8);
				CPU_EIP = loop_target;
				if (CPU_REMCLOCK <= 0) {
					return 1;
				}
			} else {
				CPU_WORKCLOCK(4);
				CPU_EIP = after_loop_eip;
				return 1;
			}
		} else {
			CPU_ECX--;
			if (CPU_ECX != 0 && CC_Z) {
				CPU_WORKCLOCK(8);
				CPU_EIP = loop_target;
				if (CPU_REMCLOCK <= 0) {
					return 1;
				}
			} else {
				CPU_WORKCLOCK(4);
				CPU_EIP = after_loop_eip;
				return 1;
			}
		}
	}
}
#endif

#if VITA_NATIVE_INLINE_IOWAIT
static int
cpu_inline_try_in_test_jz_e4(void)
{
	UINT32 loop_eip;
	UINT32 after_jz_eip;
	UINT32 loop_target;
	UINT8 port;
	UINT8 test_op;
	UINT8 mask;
	UINT8 jz_op;
	UINT8 jz_disp;

	if (CPU_INST_REPUSE) {
		return 0;
	}

	loop_eip = CPU_PREV_EIP;
	port = (UINT8)cpu_codefetch(CPU_EIP);
	test_op = (UINT8)cpu_codefetch(CPU_EIP + 1);
	mask = (UINT8)cpu_codefetch(CPU_EIP + 2);
	jz_op = (UINT8)cpu_codefetch(CPU_EIP + 3);
	jz_disp = (UINT8)cpu_codefetch(CPU_EIP + 4);
	if (test_op != 0xa8 || jz_op != 0x74) {
		return 0;
	}

	after_jz_eip = CPU_EIP + 5;
	loop_target = after_jz_eip + (SINT8)jz_disp;
	if (!CPU_INST_OP32) {
		loop_target &= 0xffff;
	}
	if (loop_target != loop_eip) {
		return 0;
	}

	for (;;) {
		UINT32 tmp;

		CPU_WORKCLOCK(12);
		CPU_AL = cpu_in(port);
		CPU_WORKCLOCK(3);
		tmp = CPU_AL;
		BYTE_AND(tmp, mask);

		if (CC_NZ) {
			CPU_WORKCLOCK(2);
			CPU_EIP = after_jz_eip;
			return 1;
		}

		CPU_WORKCLOCK(7);
		CPU_EIP = loop_target;
		if (CPU_REMCLOCK <= 0) {
			return 1;
		}
	}
}
#endif

#if VITA_NATIVE_INLINE_LOOP
static void
cpu_inline_loop_e1(void)
{
	UINT32 cx;

	if (!CPU_INST_AS32) {
		cx = CPU_CX;
		if (--cx != 0 && CC_Z) {
			JMPSHORT(8);
		} else {
			JMPNOP(4, 1);
		}
		CPU_CX--;
	} else {
		cx = CPU_ECX;
		if (--cx != 0 && CC_Z) {
			JMPSHORT(8);
		} else {
			JMPNOP(4, 1);
		}
		CPU_ECX--;
	}
}

static void
cpu_inline_loop_e2(void)
{
	UINT32 cx;

	if (!CPU_INST_AS32) {
		cx = CPU_CX;
		if (--cx != 0) {
			JMPSHORT(8);
		} else {
			JMPNOP(4, 1);
		}
		CPU_CX--;
	} else {
		cx = CPU_ECX;
		if (--cx != 0) {
			JMPSHORT(8);
		} else {
			JMPNOP(4, 1);
		}
		CPU_ECX--;
	}
}

#if VITA_NATIVE_INLINE_SELF_LOOP
static int
cpu_inline_try_self_loop_e2(void)
{
	UINT32 loop_eip;
	UINT32 after_loop_eip;
	UINT32 loop_target;
	UINT8 loop_disp;

	loop_eip = CPU_PREV_EIP;
	loop_disp = (UINT8)cpu_codefetch(CPU_EIP);
	after_loop_eip = CPU_EIP + 1;
	loop_target = after_loop_eip + (SINT8)loop_disp;
	if (!CPU_INST_OP32) {
		loop_target &= 0xffff;
	}
	if (loop_target != loop_eip) {
		return 0;
	}

	VITA_CPU_DIAG_INC(vita_cpu_diag_self_hits);
	for (;;) {
		VITA_CPU_DIAG_INC(vita_cpu_diag_self_iters);
		if (!CPU_INST_AS32) {
			CPU_CX--;
			if (CPU_CX != 0) {
				CPU_WORKCLOCK(8);
				CPU_EIP = loop_target;
				if (CPU_REMCLOCK <= 0) {
					return 1;
				}
			} else {
				CPU_WORKCLOCK(4);
				CPU_EIP = after_loop_eip;
				return 1;
			}
		} else {
			CPU_ECX--;
			if (CPU_ECX != 0) {
				CPU_WORKCLOCK(8);
				CPU_EIP = loop_target;
				if (CPU_REMCLOCK <= 0) {
					return 1;
				}
			} else {
				CPU_WORKCLOCK(4);
				CPU_EIP = after_loop_eip;
				return 1;
			}
		}
	}
}
#endif
#endif

#if VITA_NATIVE_INLINE_A13B72
static int
cpu_inline_try_a13b72_loop(void)
{
	UINT32 loop_eip;
	UINT32 mov_addr;
	UINT32 cmp_eip;
	UINT32 cmp_addr;
	UINT32 jcc_eip;
	UINT32 branch;
	UINT8 modrm;
	UINT8 rel;
	UINT32 src, dst, res;

	if (CPU_INST_REPUSE || CPU_INST_OP32 || CPU_INST_AS32) {
		return 0;
	}

	loop_eip = CPU_PREV_EIP;
	mov_addr = cpu_codefetch_w(CPU_EIP);
	cmp_eip = CPU_EIP + 2;
	if ((UINT8)cpu_codefetch(cmp_eip) != 0x3b) {
		return 0;
	}

	modrm = (UINT8)cpu_codefetch(cmp_eip + 1);
	if (modrm == 0x46) {
		cmp_addr = (CPU_BP + (SINT8)cpu_codefetch(cmp_eip + 2)) & 0xffff;
		jcc_eip = cmp_eip + 3;
	} else if (modrm == 0x06) {
		cmp_addr = cpu_codefetch_w(cmp_eip + 2);
		jcc_eip = cmp_eip + 4;
	} else {
		return 0;
	}

	if ((UINT8)cpu_codefetch(jcc_eip) != 0x72) {
		return 0;
	}
	rel = (UINT8)cpu_codefetch(jcc_eip + 1);
	branch = (jcc_eip + 2 + (SINT8)rel) & 0xffff;
	if (branch != loop_eip) {
		return 0;
	}
	if (!((CPU_CS == 0x1e05 && loop_eip == 0x0034 && mov_addr == 0x1ac6 && modrm == 0x46) ||
	      (CPU_CS == 0x1b13 && loop_eip == 0x0005 && mov_addr == 0x2ab2 && modrm == 0x06))) {
		return 0;
	}

	for (;;) {
		CPU_WORKCLOCK(5);
		CPU_INST_SEGREG_INDEX = DS_FIX;
		CPU_AX = cpu_vmemoryread_w(CPU_INST_SEGREG_INDEX, mov_addr);

		CPU_WORKCLOCK(5);
		dst = CPU_AX;
		src = cpu_vmemoryread_w((modrm == 0x46) ? SS_FIX : DS_FIX, cmp_addr);
		WORD_SUB(res, dst, src);

		if (CC_C) {
			CPU_WORKCLOCK(7);
			CPU_EIP = branch;
			if (CPU_REMCLOCK <= 0) {
				return 1;
			}
		} else {
			CPU_WORKCLOCK(2);
			CPU_EIP = jcc_eip + 2;
			return 1;
		}
	}
}
#endif

#if VITA_NATIVE_INLINE_IMUL
static void
cpu_inline_imul_af(void)
{
	if (!CPU_INST_OP32) {
		UINT16 *out;
		UINT32 op2;
		SINT32 res;
		SINT16 src;
		SINT16 dst;

		PREPART_REG16_EA(op2, src, out, 21, 27);
		dst = *out;
		_WORD_IMUL(res, dst, src);
		*out = (UINT16)res;
	} else {
		UINT32 *out;
		UINT32 op2;
		SINT64 res;
		SINT32 src;
		SINT32 dst;

		PREPART_REG32_EA(op2, src, out, 21, 27);
		dst = *out;
		_DWORD_IMUL(res, dst, src);
		*out = (UINT32)res;
	}
}
#endif

void
exec_1step(void)
{
	int prefix;
	UINT32 op;

	CPU_PREV_EIP = CPU_EIP;
	CPU_STATSAVE.cpu_inst = CPU_STATSAVE.cpu_inst_default;

#if defined(ENABLE_TRAP)
	steptrap(CPU_CS, CPU_EIP);
#endif

#if defined(IA32_INSTRUCTION_TRACE)
	ctx[ctx_index].regs = CPU_STATSAVE.cpu_regs;
	if (cpu_inst_trace) {
		disasm_context_t *d = &ctx[ctx_index].disasm;
		UINT32 eip = CPU_EIP;
		int rv;

		rv = disasm(&eip, d);
		if (rv == 0) {
			char buf[256];
			char tmp[32];
			int len = d->nopbytes > 8 ? 8 : d->nopbytes;
			int i;

			buf[0] = '\0';
			for (i = 0; i < len; i++) {
				snprintf(tmp, sizeof(tmp), "%02x ", d->opbyte[i]);
				milstr_ncat(buf, tmp, sizeof(buf));
			}
			for (; i < 8; i++) {
				milstr_ncat(buf, "   ", sizeof(buf));
			}
			VERBOSE(("%04x:%08x: %s%s", CPU_CS, CPU_EIP, buf, d->str));

			buf[0] = '\0';
			for (; i < d->nopbytes; i++) {
				snprintf(tmp, sizeof(tmp), "%02x ", d->opbyte[i]);
				milstr_ncat(buf, tmp, sizeof(buf));
				if ((i % 8) == 7) {
					VERBOSE(("             : %s", buf));
					buf[0] = '\0';
				}
			}
			if ((i % 8) != 0) {
				VERBOSE(("             : %s", buf));
			}
		}
	}
	ctx[ctx_index].opbytes = 0;
#endif

	for (prefix = 0; prefix < MAX_PREFIX; prefix++) {
		GET_PCBYTE(op);
#if defined(IA32_INSTRUCTION_TRACE)
		ctx[ctx_index].op[prefix] = op;
		ctx[ctx_index].opbytes++;
#endif

		/* prefix */
		if (insttable_info[op] & INST_PREFIX) {
			(*insttable_1byte[0][op])();
			continue;
		}
		break;
	}
	if (prefix == MAX_PREFIX) {
		EXCEPTION(UD_EXCEPTION, 0);
	}

#if defined(IA32_INSTRUCTION_TRACE)
	if (op == 0x0f) {
		BYTE op2;
		op2 = cpu_codefetch(CPU_EIP);
		ctx[ctx_index].op[prefix + 1] = op2;
		ctx[ctx_index].opbytes++;
	}
	ctx_index = (ctx_index + 1) % NELEMENTS(ctx);
#endif
	vita_cpu_diag_record((UINT8)op);
	
	/* normal / rep, but not use */
	if (!(insttable_info[op] & INST_STRING) || !CPU_INST_REPUSE) {
#if defined(DEBUG)
		cpu_debug_rep_cont = 0;
#endif
		(*insttable_1byte[CPU_INST_OP32][op])();
		return;
	}

	/* rep */
	CPU_WORKCLOCK(5);
#if defined(DEBUG)
	if (!cpu_debug_rep_cont) {
		cpu_debug_rep_cont = 1;
		cpu_debug_rep_regs = CPU_STATSAVE.cpu_regs;
	}
#endif
	if (!CPU_INST_AS32) {
		if (CPU_CX != 0) {
			if (!(insttable_info[op] & REP_CHECKZF)) {
				/* rep */
				for (;;) {
					(*insttable_1byte[CPU_INST_OP32][op])();
					if (--CPU_CX == 0) {
#if defined(DEBUG)
						cpu_debug_rep_cont = 0;
#endif
						break;
					}
					if (CPU_REMCLOCK <= 0) {
						CPU_EIP = CPU_PREV_EIP;
						break;
					}
				}
			} else if (CPU_INST_REPUSE != 0xf2) {
				/* repe */
				for (;;) {
					(*insttable_1byte[CPU_INST_OP32][op])();
					if (--CPU_CX == 0 || CC_NZ) {
#if defined(DEBUG)
						cpu_debug_rep_cont = 0;
#endif
						break;
					}
					if (CPU_REMCLOCK <= 0) {
						CPU_EIP = CPU_PREV_EIP;
						break;
					}
				}
			} else {
				/* repne */
				for (;;) {
					(*insttable_1byte[CPU_INST_OP32][op])();
					if (--CPU_CX == 0 || CC_Z) {
#if defined(DEBUG)
						cpu_debug_rep_cont = 0;
#endif
						break;
					}
					if (CPU_REMCLOCK <= 0) {
						CPU_EIP = CPU_PREV_EIP;
						break;
					}
				}
			}
		}
	} else {
		if (CPU_ECX != 0) {
			if (!(insttable_info[op] & REP_CHECKZF)) {
				/* rep */
				for (;;) {
					(*insttable_1byte[CPU_INST_OP32][op])();
					if (--CPU_ECX == 0) {
#if defined(DEBUG)
						cpu_debug_rep_cont = 0;
#endif
						break;
					}
					if (CPU_REMCLOCK <= 0) {
						CPU_EIP = CPU_PREV_EIP;
						break;
					}
				}
			} else if (CPU_INST_REPUSE != 0xf2) {
				/* repe */
				for (;;) {
					(*insttable_1byte[CPU_INST_OP32][op])();
					if (--CPU_ECX == 0 || CC_NZ) {
#if defined(DEBUG)
						cpu_debug_rep_cont = 0;
#endif
						break;
					}
					if (CPU_REMCLOCK <= 0) {
						CPU_EIP = CPU_PREV_EIP;
						break;
					}
				}
			} else {
				/* repne */
				for (;;) {
					(*insttable_1byte[CPU_INST_OP32][op])();
					if (--CPU_ECX == 0 || CC_Z) {
#if defined(DEBUG)
						cpu_debug_rep_cont = 0;
#endif
						break;
					}
					if (CPU_REMCLOCK <= 0) {
						CPU_EIP = CPU_PREV_EIP;
						break;
					}
				}
			}
		}
	}
}

void
exec_allstep(void)
{
	int prefix;
	UINT32 op;
	void (*func)(void);
	
	do {

		CPU_PREV_EIP = CPU_EIP;
		CPU_STATSAVE.cpu_inst = CPU_STATSAVE.cpu_inst_default;

#if defined(ENABLE_TRAP)
		steptrap(CPU_CS, CPU_EIP);
#endif

#if defined(IA32_INSTRUCTION_TRACE)
		ctx[ctx_index].regs = CPU_STATSAVE.cpu_regs;
		if (cpu_inst_trace) {
			disasm_context_t *d = &ctx[ctx_index].disasm;
			UINT32 eip = CPU_EIP;
			int rv;

			rv = disasm(&eip, d);
			if (rv == 0) {
				char buf[256];
				char tmp[32];
				int len = d->nopbytes > 8 ? 8 : d->nopbytes;
				int i;

				buf[0] = '\0';
				for (i = 0; i < len; i++) {
					snprintf(tmp, sizeof(tmp), "%02x ", d->opbyte[i]);
					milstr_ncat(buf, tmp, sizeof(buf));
				}
				for (; i < 8; i++) {
					milstr_ncat(buf, "   ", sizeof(buf));
				}
				VERBOSE(("%04x:%08x: %s%s", CPU_CS, CPU_EIP, buf, d->str));

				buf[0] = '\0';
				for (; i < d->nopbytes; i++) {
					snprintf(tmp, sizeof(tmp), "%02x ", d->opbyte[i]);
					milstr_ncat(buf, tmp, sizeof(buf));
					if ((i % 8) == 7) {
						VERBOSE(("             : %s", buf));
						buf[0] = '\0';
					}
				}
				if ((i % 8) != 0) {
					VERBOSE(("             : %s", buf));
				}
			}
		}
		ctx[ctx_index].opbytes = 0;
#endif

		for (prefix = 0; prefix < MAX_PREFIX; prefix++)
		{
#if defined(USE_CPU_MODRMPREFETCH)
			if ((CPU_EIP + 1) & CPU_PAGE_MASK)
			{
				op = cpu_opcodefetch(CPU_EIP);
#if defined(USE_CPU_EIPMASK)
				CPU_EIP = (CPU_EIP + 1) & CPU_EIPMASK;
#else
				if (CPU_STATSAVE.cpu_inst_default.op_32)
				{
					CPU_EIP = (CPU_EIP + 1);
				}
				else
				{
					CPU_EIP = (CPU_EIP + 1) & 0xffff;
				}
#endif
			}
			else
#endif
			{
				GET_PCBYTE(op);
			}
#if defined(IA32_INSTRUCTION_TRACE)
			ctx[ctx_index].op[prefix] = op;
			ctx[ctx_index].opbytes++;
#endif

			/* prefix */
			if (insttable_info[op] & INST_PREFIX)
			{
#if defined(USE_CPU_INLINEINST)
				// 繧､繝ｳ繝ｩ繧､繝ｳ蜻ｽ莉､鄒､縲髢｢謨ｰ繝・・繝悶Ν繧医ｊ繧ょ他縺ｳ蜃ｺ縺励′鬮倬溘□縺後∝､壹￥鄂ｮ縺阪☆縺弱ｋ縺ｨif縺悟｢励∴縺ｦ騾・↓驕・￥縺ｪ繧九ゅ↑縺ｮ縺ｧ蜻ｼ縺ｳ蜃ｺ縺鈴ｻ蠎ｦ縺碁ｫ倥＞迚ｩ繧貞━蜈医＠縺ｦ驟咲ｽｮ
				if (!(op & 0x26)) {
					(*insttable_1byte[0][op])();
					continue;
				}
				else {
					if (op == 0x66)
					{
						CPU_INST_OP32 = !CPU_STATSAVE.cpu_inst_default.op_32;
						continue;
					}
					else if (op == 0x26)
					{
						CPU_INST_SEGUSE = 1;
						CPU_INST_SEGREG_INDEX = CPU_ES_INDEX;
						continue;
					}
					else if (op == 0x67)
					{
						CPU_INST_AS32 = !CPU_STATSAVE.cpu_inst_default.as_32;
						continue;
					}
					else if (op == 0x2E)
					{
						CPU_INST_SEGUSE = 1;
						CPU_INST_SEGREG_INDEX = CPU_CS_INDEX;
						continue;
					}
					//else if (op == 0xF3)
					//{
					//	CPU_INST_REPUSE = 0xf3;
					//	continue;
					//}
					else
					{
						(*insttable_1byte[0][op])();
						continue;
					}
				}
#else

				(*insttable_1byte[0][op])();
				continue;
#endif
			}
			break;
		}
		if (prefix == MAX_PREFIX) {
			EXCEPTION(UD_EXCEPTION, 0);
		}

#if defined(IA32_INSTRUCTION_TRACE)
		if (op == 0x0f) {
			BYTE op2;
			op2 = cpu_codefetch(CPU_EIP);
			ctx[ctx_index].op[prefix + 1] = op2;
			ctx[ctx_index].opbytes++;
		}
		ctx_index = (ctx_index + 1) % NELEMENTS(ctx);
#endif
		vita_cpu_diag_record((UINT8)op);
	
		/* normal / rep, but not use */
#if defined(USE_CPU_INLINEINST)
		if (CPU_INST_OP32)
		{
			// 繧､繝ｳ繝ｩ繧､繝ｳ蜻ｽ莉､鄒､縲髢｢謨ｰ繝・・繝悶Ν繧医ｊ繧ょ他縺ｳ蜃ｺ縺励′鬮倬溘□縺後∝､壹￥鄂ｮ縺阪☆縺弱ｋ縺ｨif縺悟｢励∴縺ｦ騾・↓驕・￥縺ｪ繧九ゅ↑縺ｮ縺ｧ蜻ｼ縺ｳ蜃ｺ縺鈴ｻ蠎ｦ縺碁ｫ倥＞迚ｩ繧貞━蜈医＠縺ｦ驟咲ｽｮ
			if (op == 0x8b)
			{
				UINT32* out;
				UINT32 op2, src;

				PREPART_REG32_EA(op2, src, out, 2, 5);
				*out = src;
				continue;
			}
			else if (op == 0x0f)
			{
				UINT8 repuse = CPU_INST_REPUSE;

				GET_MODRM_PCBYTE(op);
#ifdef USE_SSE
				if (insttable_2byte660F_32[op] && CPU_INST_OP32 == !CPU_STATSAVE.cpu_inst_default.op_32)
				{
					(*insttable_2byte660F_32[op])();
				}
				else if (insttable_2byteF20F_32[op] && repuse == 0xf2)
				{
					(*insttable_2byteF20F_32[op])();
				}
				else if (insttable_2byteF30F_32[op] && repuse == 0xf3)
				{
					(*insttable_2byteF30F_32[op])();
				}
				else
				{
					(*insttable_2byte[1][op])();
				}
#else
				(*insttable_2byte[1][op])();
#endif
				continue;
			}
			else if (op == 0x74)
			{
				if (CC_NZ)
				{
					JMPNOP(2, 1);
				}
				else
				{
					JMPSHORT(7);
				}
				continue;
			}
		}
#endif
#if VITA_NATIVE_INLINE_STOS
		if (op == 0xab) {
			if (cpu_inline_try_stos_loop_ab()) {
				continue;
			}
		} else
#endif
#if VITA_NATIVE_INLINE_LODS
		if (op == 0xac && !CPU_INST_REPUSE) {
			cpu_inline_lods_ac();
			continue;
		} else if (op == 0xad && !CPU_INST_REPUSE) {
			cpu_inline_lods_ad();
			continue;
		} else
#endif
#if VITA_NATIVE_INLINE_A13B72
		if (op == 0xa1) {
			if (cpu_inline_try_a13b72_loop()) {
				continue;
			}
		} else
#endif
#if VITA_NATIVE_INLINE_SCAS_LOOPE
		if (op == 0xaf) {
			if (cpu_inline_try_scas_loope_af()) {
				continue;
			}
		} else
#endif
#if VITA_NATIVE_INLINE_IOWAIT
		if (op == 0xe4) {
			if (cpu_inline_try_in_test_jz_e4()) {
				continue;
			}
		} else
#endif
#if VITA_NATIVE_INLINE_LOOP
		if (op == 0xe1) {
			cpu_inline_loop_e1();
			continue;
		} else if (op == 0xe2) {
#if VITA_NATIVE_INLINE_SELF_LOOP
			if (cpu_inline_try_self_loop_e2()) {
				continue;
			}
#endif
			cpu_inline_loop_e2();
			continue;
		} else
#endif
#if VITA_NATIVE_INLINE_IMUL
		if (op == 0xaf && !CPU_INST_REPUSE) {
			cpu_inline_imul_af();
			continue;
		}
#endif
		if (!(insttable_info[op] & INST_STRING) || !CPU_INST_REPUSE) {
#if defined(DEBUG)
			cpu_debug_rep_cont = 0;
#endif
			(*insttable_1byte[CPU_INST_OP32][op])();
			continue;
		}

		/* rep */
		CPU_WORKCLOCK(5);
#if defined(DEBUG)
		if (!cpu_debug_rep_cont) {
			cpu_debug_rep_cont = 1;
			cpu_debug_rep_regs = CPU_STATSAVE.cpu_regs;
		}
#endif
		func = insttable_1byte[CPU_INST_OP32][op];
		if (!CPU_INST_AS32) {
			if (CPU_CX != 0) {
				if(CPU_CX==1){
					(*func)();
					--CPU_CX;
				}else{
					if (!(insttable_info[op] & REP_CHECKZF)) {
						if(insttable_1byte_repfunc[CPU_INST_OP32][op]){
							(*insttable_1byte_repfunc[CPU_INST_OP32][op])(0);
						}else{
							/* rep */
							for (;;) {
								(*func)();
								if (--CPU_CX == 0) {
#if defined(DEBUG)
									cpu_debug_rep_cont = 0;
#endif
									break;
								}
								if (CPU_REMCLOCK <= 0) {
									CPU_EIP = CPU_PREV_EIP;
									break;
								}
							}
						}
					} else if (CPU_INST_REPUSE != 0xf2) {
						if(insttable_1byte_repfunc[CPU_INST_OP32][op]){
							(*insttable_1byte_repfunc[CPU_INST_OP32][op])(1);
						}else{
							/* repe */
							for (;;) {
								(*func)();
								if (--CPU_CX == 0 || CC_NZ) {
#if defined(DEBUG)
									cpu_debug_rep_cont = 0;
#endif
									break;
								}
								if (CPU_REMCLOCK <= 0) {
									CPU_EIP = CPU_PREV_EIP;
									break;
								}
							}
						}
					} else {
						if(insttable_1byte_repfunc[CPU_INST_OP32][op]){
							(*insttable_1byte_repfunc[CPU_INST_OP32][op])(2);
						}else{
							/* repne */
							for (;;) {
								(*func)();
								if (--CPU_CX == 0 || CC_Z) {
#if defined(DEBUG)
									cpu_debug_rep_cont = 0;
#endif
									break;
								}
								if (CPU_REMCLOCK <= 0) {
									CPU_EIP = CPU_PREV_EIP;
									break;
								}
							}
						}
					}
				}
			}
		} else {
			if (CPU_ECX != 0) {
				if(CPU_ECX==1){
					(*func)();
					--CPU_ECX;
				}else{
					if (!(insttable_info[op] & REP_CHECKZF)) {
						if(insttable_1byte_repfunc[CPU_INST_OP32][op]){
							(*insttable_1byte_repfunc[CPU_INST_OP32][op])(0);
						}else{
							/* rep */
							for (;;) {
								(*func)();
								if (--CPU_ECX == 0) {
#if defined(DEBUG)
									cpu_debug_rep_cont = 0;
#endif
									break;
								}
								if (CPU_REMCLOCK <= 0) {
									CPU_EIP = CPU_PREV_EIP;
									break;
								}
							}
						}
					} else if (CPU_INST_REPUSE != 0xf2) {
						if(insttable_1byte_repfunc[CPU_INST_OP32][op]){
							(*insttable_1byte_repfunc[CPU_INST_OP32][op])(1);
						}else{
							/* repe */
							for (;;) {
								(*func)();
								if (--CPU_ECX == 0 || CC_NZ) {
#if defined(DEBUG)
									cpu_debug_rep_cont = 0;
#endif
									break;
								}
								if (CPU_REMCLOCK <= 0) {
									CPU_EIP = CPU_PREV_EIP;
									break;
								}
							}
						}
					} else {
						if(insttable_1byte_repfunc[CPU_INST_OP32][op]){
							(*insttable_1byte_repfunc[CPU_INST_OP32][op])(2);
						}else{
							/* repne */
							for (;;) {
								(*func)();
								if (--CPU_ECX == 0 || CC_Z) {
#if defined(DEBUG)
									cpu_debug_rep_cont = 0;
#endif
									break;
								}
								if (CPU_REMCLOCK <= 0) {
									CPU_EIP = CPU_PREV_EIP;
									break;
								}
							}
						}
					}
				}
			}
		}
	} while (CPU_REMCLOCK > 0);
}


