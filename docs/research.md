# Research Notes

## Sources checked

- VitaSDK official getting started: https://vitasdk.org/
- VitaSDK GitHub org: https://github.com/vitasdk
- VitaSDK autobuilds: https://github.com/vitasdk/autobuilds/releases
- NP2kai upstream: https://github.com/AZO234/NP2kai

## Environment findings

- Official VitaSDK setup recommends WSL2 on Windows, but current autobuilds also publish a Windows toolchain archive.
- Latest Windows autobuild found during setup: `master-win-v2.539`, published 2026-04-25.
- The archive SHA256 matched GitHub API metadata:
  `f5630207a8ae7201bc2cdbb900b0847646178327f4c700b35815904118350ec5`.
- Local WSL command exists, but no Linux distribution is installed.
- Local Windows environment had Git and CMake, but no VitaSDK, no make, and no Ninja before setup.
- `pkg-config` was installed with winget package `bloodrock.pkg-config-lite` because NP2kai's CMake requires `find_package(PkgConfig)`.

## NP2kai findings

- Upstream version checked out: README says `Neko Project II 0.86 kai`, dated 2026-01-18.
- CMake already has an SDL port path:
  - `BUILD_SDL`
  - `USE_SDL=2` / SDL2
  - `USE_SDL_TTF`
  - `BUILD_I286`
- VitaSDK packages include SDL2, SDL2_mixer, SDL2_ttf, freetype, zlib, libpng, and libvita2d.
- There is no Vita target in upstream NP2kai yet, so we need a new Vita cross-build path.
- A direct Vita CMake probe now configures only after setting `NP2KAI_HASH` as an environment variable, but generates no build targets. The reason is structural: upstream only creates cross-build targets for Emscripten and OpenDingux under `if(CMAKE_CROSSCOMPILING)`.

## Recommended porting path

Start from NP2kai SDL2 rather than PSP NP2. It already isolates desktop-specific frontend code better, and VitaSDK has SDL2 packages.

First milestone should be a native Vita VPK that shows a PC-98 framebuffer and accepts Vita controls. Keep features off initially:

- no network
- no libusb
- no libcdio/CD-ROM
- no VST
- no HAXM
- no wx/X frontend

Use `BUILD_I286=ON` first for lower CPU pressure, then evaluate whether Variable Geo requires NP21/IA-32 behavior.
