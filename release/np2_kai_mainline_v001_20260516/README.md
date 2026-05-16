# NP2 Kai Vita Mainline v001

Base:
- prototype/vita_np2_kai_v09_rebuild
- no FMGEN/C++ path; keeps the lighter v09-style build.

Build marker:
- main: build vita-np2-kai-mainline-v001 20260516

Included from the v09 line:
- Japanese filename rendering path
- STOP key recovery path
- menu/game overlay layer presentation
- centered 640x400 presentation, color fix, menu stable present path
- frame/present skip and core draw skip controls

New cfg keys accepted in this build:
- VITA_CPU_MULT
- VITA_DRAW_SKIP
- VITA_FORCE_NOWAIT
- VITA_FORCE_SKIPLINE
- VITA_FORCE_GDC
- VITA_SOUND_MODE
- VITA_SOUND_BOARD
- VITA_USE_FMGEN (accepted but ignored in nofmgen build)
- VITA_UPD72020

Suggested first test:
VITA_PRESENT_SKIP=3
VITA_CORE_DRAWSKIP=false
VITA_AUDIO_LIGHT=false
VITA_SPEED_LIMIT=0
VITA_CPU_MULT=12
VITA_FORCE_SKIPLINE=true
VITA_FORCE_NOWAIT=false
VITA_FORCE_GDC=0
VITA_SOUND_MODE=0
VITA_USE_FMGEN=255
