# PSP NP2 Reuse Notes

## Result

The PSP version is usable as a source reference.

The important archive is hissorii's `NP2 for PSP v0.39`; GameBrew's outer archive contains:

- `NP2_for_PSP_v0.39.zip`: NP2 binary package
- `NP21_for_PSP_v0.39.zip`: NP21 binary package
- `np2_081as_psp_v0.39.zip`: source package

Local copy:

- `research/psp_np2/np2psphis.7z`
- `research/psp_np2/extract/hissorii_src/np2`

## License

`psp/psp_src_readme.txt` says:

- Neko Project II source is modified BSD.
- PSP-specific source code is also modified BSD.
- It says copyright display is not required on redistribution.
- The bundled `Naga 10` font data has separate terms and must follow the original Naga 10 README.

So the code can be carried forward, but do not casually embed or redistribute the generated Naga 10 font blob without checking its terms.

## What Can Be Reused

Good reuse candidates:

- `psp/sysmenu.c`, `psp/sysmenu.res`, `psp/sysmenu.str`: PSP-specific menu entries.
- `embed/menu` and `embed/menubase`: embedded menu UI framework.
- `generic/softkbd.c` and related embedded UI pieces: soft keyboard.
- `psp/keyconf.c`, `psp/psp_key.txt`: button-to-PC98 key mapping system.
- `psp/ini.c`: PSP-specific config fields such as screen mode and mouse button swap.
- `psp/psp_readme.txt`: behavior reference for controls and menu options.

High-rewrite candidates:

- `psp/scrnmng.c`: hard-coded around PSP resolution/GU/framebuffer assumptions.
- `psp/psp/pg.c`: PSP GU/audio helper code.
- `psp/soundmng.c`: partly PSP/SDL-era audio glue; Vita should use SDL2 audio or SceAudio directly.
- `psp/np2.c`: PSP module/callback/power/control setup.
- `psp/taskmng.c`: thread, exit, and platform lifecycle code.
- `psp/dosio.c`: uses PSP I/O APIs; Vita has similar concepts but different function names and paths.

## API Surface Found

The PSP platform layer directly uses PSP SDK APIs such as:

- control: `sceCtrlPeekBufferPositive`, `sceCtrlRead`, `sceCtrlSetSamplingMode`
- display/GU: `sceGuInit`, `sceDisplaySetFrameBuf`, `sceDisplayWaitVblankStart`, `sceGeListEnQueue`
- audio: `sceAudioChReserve`, `sceAudioOutputPannedBlocking`
- power: `scePowerSetClockFrequency`, `scePowerRegisterCallback`
- filesystem: `sceIoOpen`, `sceIoRead`, `sceIoDopen`, `sceIoDread`
- threading/sync: `sceKernelCreateThread`, `sceKernelCreateSema`

Vita equivalents exist for many areas, but this is not a drop-in compile.

## Practical Recommendation

Use the PSP version as the UI/control source, not as the main emulator base.

Best path:

1. Keep NP2kai or newer NP2 core as the emulator base.
2. Import the PSP embedded menu/key config/soft keyboard behavior.
3. Reimplement the PSP platform layer for Vita:
   - screen: 960x544, display 640x400 integer/filtered modes
   - input: Vita buttons/analog/touch to PSP-style menu and PC98 mouse/key modes
   - audio: SDL2 or native SceAudio
   - files: `ux0:data/np2vita` or similar
4. Avoid bundling Naga 10 font until its license is checked; prefer user-supplied `font.bmp` first.

The PSP UI is absolutely worth reusing. The PSP low-level backend is mostly a guide for what to reimplement on Vita.

## Prototype

Created `prototype/vita_psp_ui_probe`, a first Vita SDL2 VPK that exercises the PSP UI behavior without attaching an emulator core yet.

It currently uses PSP-derived menu labels and key-profile behavior:

- menu categories from `psp/sysmenu.str`
- key profile parser and VPK-packaged `psp_key.txt` compatible with the PSP layout
- PSP control convention from `psp/psp_readme.txt`

It does not yet compile the original `menubase`/`menusys` code directly. That code still depends on the NP2 core, menu VRAM, font manager, and platform screen manager. The probe is deliberately thin so we can test Vita input/display ergonomics before dragging the full dependency graph in.

Build output:

- `build/vita_psp_ui_probe/vita_psp_ui_probe.vpk`

Current packaged file check:

- `psp_key.txt` is included in the VPK root.
- Startup status reports whether `psp_key.txt` was loaded or fallback profiles were used.

Next implemented probe behavior:

- `ux0:data/np2vita` is created on startup.
- `ux0:data/np2vita/psp_key.txt` overrides the packaged `app0:/psp_key.txt`.
- FDD/HDD `Open...` entries now enter a disk browser.
- The disk browser scans `ux0:data/np2vita` for `.d88`, `.fdi`, `.xdf`, `.hdm`, `.hdi`, `.thd`, `.nfd`, and `.fdd`.

## Sources

- GameBrew: https://www.gamebrew.org/wiki/NP2_for_PSP_by_hissorii
- GameBrew: https://www.gamebrew.org/wiki/NP2_for_PSP_by_sakahi
- GameBrew: https://www.gamebrew.org/wiki/NP2_for_3DS
- Local source license note: `research/psp_np2/extract/hissorii_src/np2/psp/psp_src_readme.txt`
