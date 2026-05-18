# NP2 Kai Vita Mainline v002 Minimal

Purpose:
- Regression check after mainline v001 became heavier.
- Keep the light v09 nofmgen behavior almost unchanged.
- Do not add the v001 expanded cfg controls such as VITA_CPU_MULT, VITA_FORCE_*, VITA_SOUND_MODE, VITA_SOUND_BOARD, VITA_USE_FMGEN, or VITA_UPD72020.

Base:
- prototype/vita_np2_kai_v09_rebuild

Build marker:
- main: build vita-np2-kai-mainline-v002-minimal 20260518

Expected cfg support:
- VITA_FRAMESKIP
- VITA_PRESENT_SKIP
- VITA_DRAWSKIP_CORE
- VITA_CORE_DRAWSKIP
- VITA_AUDIO_LIGHT
- VITA_SPEED_LIMIT
- VITA_TOUHOU_PRESET

Test intent:
- If this becomes light again, v001 expanded cfg/control block caused the slowdown.
- If this is still heavy, compare against the packaged RC009 VPK or source copy fidelity.
