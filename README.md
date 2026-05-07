# np2_for_vita

Goal: port Neko Project II kai to PS Vita as a native homebrew app for PC-98 games.

Current release candidate:

- Project: `prototype/vita_np2_kai_fullcore`
- App name: `NP2 Kai RC 009`
- Title ID: `NP2R00009`
- Build marker: `vita-np2-kai-fullcore-rc-v009`
- Recommended config: see `docs/vita_config.md`
- Known issues: see `docs/known_issues.md`
- Release notes: see `docs/release_notes_v009.md`

Build the current Vita VPK:

```powershell
$env:VITASDK=(Resolve-Path tools\vitasdk\vitasdk).Path
$env:PATH="$env:VITASDK\bin;$(Resolve-Path tools\ninja);$env:PATH"
cmake --build build\vita_np2_kai_fullcore -j 4
```

Output:

```text
build\vita_np2_kai_fullcore\vita_np2_kai_fullcore.vpk
```

Current baseline:

- VitaSDK Windows autobuild: `master-win-v2.539` from 2026-04-25
- Compiler: `arm-vita-eabi-gcc 15.2.0`
- Build driver: Ninja `1.13.2`
- Host pkg-config: `Pkg Config Lite 0.28-1` via winget package `bloodrock.pkg-config-lite`
- Installed VitaSDK packages: `zlib`, `libpng`, `freetype`, `libvita2d`, `sdl2`, `sdl2_mixer`, `sdl2_ttf`
- Source snapshots under `upstream/`: `NP2kai`, `vitasdk-samples`, `vitasdk-packages`

Verified:

```powershell
scripts\env.cmd
cmake -S upstream\vitasdk-samples\hello_world -B build\hello_world -G Ninja
cmake --build build\hello_world -j 4
```

This produces `build\hello_world\hello_world.vpk`.

SDL2 smoke test:

```powershell
$env:VITASDK=(Resolve-Path tools\vitasdk\vitasdk).Path
$env:PATH="$env:VITASDK\bin;$(Resolve-Path tools\ninja);$env:PATH"
cmake -S prototype\vita_sdl_smoketest -B build\vita_sdl_smoketest -G Ninja
cmake --build build\vita_sdl_smoketest -j 4
```

This produces `build\vita_sdl_smoketest\vita_sdl_smoketest.vpk`.

PSP UI probe:

```powershell
$env:VITASDK=(Resolve-Path tools\vitasdk\vitasdk).Path
$env:PATH="$env:VITASDK\bin;$(Resolve-Path tools\ninja);$env:PATH"
cmake -S prototype\vita_psp_ui_probe -B build\vita_psp_ui_probe -G Ninja
cmake --build build\vita_psp_ui_probe -j 4
```

This produces `build\vita_psp_ui_probe\vita_psp_ui_probe.vpk`.

The VPK includes `psp_key.txt` at the package root and the probe loads it at startup. `SELECT` cycles the parsed profiles.
If `ux0:data/np2vita/psp_key.txt` exists, that external file is preferred over the packaged default.
The probe also creates `ux0:data/np2vita` and scans it for disk images when using FDD/HDD Open.

Controls in the probe:

- `L`: toggle PSP-style menu
- `R`: toggle soft keyboard mock
- `START`: switch config-key / PC98 mouse mode
- `SELECT`: cycle `psp_key.txt`-style key profiles
- D-pad: navigate menu
- `Circle`: activate highlighted menu item
- `Cross`: close menu
- `Triangle`: quit

Disk browser:

- Open `FDD1 > Open...`, `FDD2 > Open...`, or `HDD > SASI1 / Open...`
- The browser lists disk images in `ux0:data/np2vita`
- Supported probe extensions: `.d88`, `.fdi`, `.xdf`, `.hdm`, `.hdi`, `.thd`, `.nfd`, `.fdd`
- `Circle`: select highlighted file
- `Cross`: close browser

If you prefer PowerShell env loading, run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
. .\scripts\env.ps1
```

This changes the policy only for the current PowerShell process.

Next porting target:

1. Try the PSP UI probe on real hardware and adjust menu scale/input feel.
2. Replace probe mock drawing with the actual PSP embedded menu renderer where practical.
3. Add a Vita CMake path for NP2kai's SDL2 port.
4. Disable desktop-only dependencies first: network/libusb/libcdio/VST/HAXM/wx/X.
5. Get `BUILD_I286=ON` running before IA-32/NP21, because the first target is PC-9801 era games.
6. After boot/display/input/audio works, profile Variable Geo and tune CPU/audio timing.
