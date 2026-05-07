cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK.")
  endif()
endif()

project(vita_psp_ui_probe C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "NP2 PSP UI Probe")
set(VITA_TITLEID "NP2V00002")
set(VITA_VERSION "00.01")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O2")

add_executable(${PROJECT_NAME}
  src/main.c
)

target_link_libraries(${PROJECT_NAME}
  SDL2
  SceDisplay_stub
  SceCtrl_stub
  SceAudio_stub
  SceAudioIn_stub
  SceCommonDialog_stub
  SceGxm_stub
  SceHid_stub
  SceIme_stub
  SceIofilemgr_stub
  SceMotion_stub
  SceSysmodule_stub
  SceTouch_stub
  m
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE data/psp_key.txt psp_key.txt
)
