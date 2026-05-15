#ifndef PSP
#define	PSP
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <pspuser.h>
#include <malloc.h>

#define	BYTESEX_LITTLE
#define	OSLANG_SJIS
#define RESOURCE_US
#define	OSLINEBREAK_CRLF
#define	DISABLE_MATH_H
#define SOUND_CRITICAL
#define SUPPORT_SOUND_SB16
#define SUPPORT_PEGC
#define USE_TSC
#define MEMORY_MAXSIZE 4000
#define CPU_MULTIPLE_MAX 2048
#define SUPPORT_SOFTKBD 0
#define NP2KAI_GIT_TAG "vita-fullcore"
#define NP2KAI_GIT_HASH "local" // キーボードの画像のみ使う
#ifndef VITA_NP2_DIAG
#define VITA_NP2_DIAG 0
#endif

typedef unsigned char BYTE;
typedef	signed char SINT8;
typedef	unsigned char UINT8;
typedef	signed short SINT16;
typedef SINT16 INT16;
typedef	unsigned short UINT16;
typedef	signed int SINT32;
typedef SINT32 INT32;
typedef	unsigned int UINT32;
typedef	int BOOL;
typedef unsigned int UINT;
typedef UINT16 WORD;
typedef UINT32 DWORD;
typedef signed int SINT;
typedef int INT;
typedef signed int INTPTR;
typedef unsigned int UINTPTR;
typedef INTPTR INT_PTR;
typedef UINTPTR UINT_PTR;
typedef unsigned int size_t;
typedef signed long long FILEPOS;
typedef signed long long FILELEN;
#ifdef CPUCORE_IA32 //NP21の場合はMakefileでCPUCORE_IA32がdefineされる
typedef unsigned long long UINT64;
typedef signed long long SINT64;
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef	TRUE
#define	TRUE 1
#endif

#ifndef	FALSE
#define	FALSE 0
#endif

#ifndef	MAX_PATH
#define	MAX_PATH 256
#endif
#ifndef	NHD_MAXSIZE2
#define	NHD_MAXSIZE2 2000
#endif

#ifndef offsetof
#define offsetof(type, mem) ((size_t)((char *)&((type *)0)->mem - (char *)(type *)0))
#endif

#ifndef	max
#define	max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef	MAX
#define	MAX(a,b) max((a),(b))
#endif
#ifndef	min
#define	min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef	MIN
#define	MIN(a,b) min((a),(b))
#endif

#ifndef	ZeroMemory
#define	ZeroMemory(d,n) memset((d), 0, (n))
#endif
#ifndef	CopyMemory
#define	CopyMemory(d,s,n) memcpy((d), (s), (n))
#endif
#ifndef	FillMemory
#define	FillMemory(a, b, c) memset((a), (c), (b))
#endif

#ifndef	roundup
#define	roundup(x, y) ((((x)+((y)-1))/(y))*(y))
#endif

#define	UNUSED(v) ((void)(v))

#ifndef IOOUTCALL
#define	IOOUTCALL
#endif
#ifndef IOINPCALL
#define	IOINPCALL
#endif
#ifndef DMACCALL
#define	DMACCALL
#endif
#ifndef MEMCALL
#define	MEMCALL
#endif
#ifndef VRAMCALL
#define	VRAMCALL
#endif
#ifndef PARTSCALL
#define	PARTSCALL
#endif
#ifndef SOUNDCALL
#define	SOUNDCALL
#endif


#ifndef	NELEMENTS
#define	NELEMENTS(a) ((int)(sizeof(a) / sizeof(a[0])))
#endif

#define	BRESULT UINT8
#define	OEMCHAR char
#define	OEMTEXT(string) string
#define	OEMSPRINTF sprintf
#define	OEMSNPRINTF snprintf
#define	OEMSTRLEN strlen


#include "common.h"
#include "milstr.h"
#include "_memory.h"
#include "rect.h"
#include "lstarray.h"
#include "trace.h"


//#define	GETTICK() (sceKernelLibcClock() / 1000)
#if defined(TRACE)
#define	__ASSERT(s) assert(s)
#else
#define	__ASSERT(s)
#endif
#define	SPRINTF sprintf
#define	STRLEN strlen

#define	LABEL __declspec(naked)
#define	RELEASE(x) if (x) {(x)->Release(); (x) = NULL;}

#ifdef CPUCORE_IA32
#define sigjmp_buf              jmp_buf
#define sigsetjmp(env, mask)    setjmp(env)
#define siglongjmp(env, val)    longjmp(env, val)
#endif


#define	SUPPORT_SJIS

#define	SUPPORT_16BPP
#define SCREEN_BPP 16

#ifdef SIZE_QVGA
#undef RGB16
#define RGB16 UINT32
#endif

#define	SOUNDRESERVE 100

#ifdef CPUCORE_IA32
#define	SUPPORT_PC9821
#define	SUPPORT_CRT31KHZ
#else
#define	SUPPORT_CRT15KHZ
#endif

#define	SUPPORT_HWSEEKSND
