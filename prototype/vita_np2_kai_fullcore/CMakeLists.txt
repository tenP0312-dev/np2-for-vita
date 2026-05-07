cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK.")
  endif()
endif()

project(vita_np2_kai_fullcore C ASM)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "NP2 Kai RC 009")
set(VITA_TITLEID "NP2R00009")
set(VITA_VERSION "01.00")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O3 -fomit-frame-pointer -Wno-unused-but-set-variable -Wno-unused-variable -Wno-parentheses -Wno-missing-braces -Wno-error=implicit-function-declaration -Wno-implicit-function-declaration -Wno-error=int-conversion -Wno-int-conversion -Wno-error=incompatible-pointer-types -Wno-incompatible-pointer-types")

set(NP2_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB NP2_SOURCES
  ${NP2_ROOT}/*.c
  ${NP2_ROOT}/common/*.c
  ${NP2_ROOT}/codecnv/*.c
  ${NP2_ROOT}/diskimage/*.c
  ${NP2_ROOT}/diskimage/cd/*.c
  ${NP2_ROOT}/diskimage/fd/*.c
  ${NP2_ROOT}/i386c/*.c
  ${NP2_ROOT}/i386c/ia32/*.c
  ${NP2_ROOT}/i386c/ia32/instructions/*.c
  ${NP2_ROOT}/i386c/ia32/instructions/fpu/*.c
  ${NP2_ROOT}/i386c/ia32/instructions/mmx/*.c
  ${NP2_ROOT}/i386c/ia32/instructions/sse/*.c
  ${NP2_ROOT}/i386c/ia32/instructions/sse2/*.c
  ${NP2_ROOT}/i386c/ia32/instructions/sse3/*.c
  ${NP2_ROOT}/i386c/ia32/instructions/sse4/*.c
  ${NP2_ROOT}/i386c/ia32/instructions/sse4a/*.c
  ${NP2_ROOT}/i386c/ia32/instructions/ssse3/*.c
  ${NP2_ROOT}/mem/*.c
  ${NP2_ROOT}/io/*.c
  ${NP2_ROOT}/cbus/*.c
  ${NP2_ROOT}/bios/*.c
  ${NP2_ROOT}/sound/*.c
  ${NP2_ROOT}/sound/mame/*.c
  ${NP2_ROOT}/sound/vermouth/*.c
  ${NP2_ROOT}/sound/getsnd/*.c
  ${NP2_ROOT}/textnorm/*.c
  ${NP2_ROOT}/vram/*.c
  ${NP2_ROOT}/wab/*.c
  ${NP2_ROOT}/fdd/*.c
  ${NP2_ROOT}/lio/*.c
  ${NP2_ROOT}/font/*.c
  ${NP2_ROOT}/generic/*.c
  ${NP2_ROOT}/embed/*.c
  ${NP2_ROOT}/embed/menu/*.c
  ${NP2_ROOT}/embed/menubase/*.c
  ${NP2_ROOT}/psp/*.c
  ${NP2_ROOT}/psp/psp/*.c
)

list(REMOVE_ITEM NP2_SOURCES
  ${NP2_ROOT}/debugsub.c
  ${NP2_ROOT}/generic/dipswbmp.c
  ${NP2_ROOT}/generic/hostdrv.c
  ${NP2_ROOT}/generic/hostdrvs.c
  ${NP2_ROOT}/generic/keydisp.c
  ${NP2_ROOT}/generic/np2info.c
  ${NP2_ROOT}/generic/unasm.c
  ${NP2_ROOT}/cbus/gpibio.c
  ${NP2_ROOT}/i386c/ia32/instructions/fpu/fpemul.c
  ${NP2_ROOT}/i386c/ia32/instructions/fpu/fpemul_dosbox.c
  ${NP2_ROOT}/i386c/ia32/instructions/fpu/fpemul_dosbox2.c
  ${NP2_ROOT}/i386c/ia32/instructions/fpu/fpemul_softfloat.c
  ${NP2_ROOT}/diskimage/cd/cdd_libcdio.c
  ${NP2_ROOT}/diskimage/cd/cdd_real.c
  ${NP2_ROOT}/psp/psp/font.c
)

add_executable(${PROJECT_NAME}
  ${NP2_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${NP2_ROOT}/vita_compat
  ${NP2_ROOT}
  ${NP2_ROOT}/common
  ${NP2_ROOT}/codecnv
  ${NP2_ROOT}/diskimage
  ${NP2_ROOT}/i386c
  ${NP2_ROOT}/i386c/ia32
  ${NP2_ROOT}/i386c/ia32/instructions
  ${NP2_ROOT}/i386c/ia32/instructions/fpu
  ${NP2_ROOT}/mem
  ${NP2_ROOT}/io
  ${NP2_ROOT}/cbus
  ${NP2_ROOT}/bios
  ${NP2_ROOT}/lio
  ${NP2_ROOT}/vram
  ${NP2_ROOT}/wab
  ${NP2_ROOT}/sound
  ${NP2_ROOT}/sound/mame
  ${NP2_ROOT}/sound/vermouth
  ${NP2_ROOT}/sound/getsnd
  ${NP2_ROOT}/textnorm
  ${NP2_ROOT}/fdd
  ${NP2_ROOT}/font
  ${NP2_ROOT}/generic
  ${NP2_ROOT}/embed
  ${NP2_ROOT}/embed/menu
  ${NP2_ROOT}/embed/menubase
  ${NP2_ROOT}/psp
  ${NP2_ROOT}/psp/psp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  PSP
  VITA_NP2_PORT
  CPUCORE_IA32
  SOUND_CRITICAL
  SUPPORT_SOUND_SB16
  SUPPORT_SMPU98
  SUPPORT_KAI_IMAGES
  USE_MAME
)

target_link_libraries(${PROJECT_NAME}
  SceDisplay_stub
  SceCtrl_stub
  SceTouch_stub
  SceAudio_stub
  SceIofilemgr_stub
  SceKernelThreadMgr_stub
  SceSysmem_stub
  ScePower_stub
  SceRtc_stub
  m
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
)
