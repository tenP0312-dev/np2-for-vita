$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$vitasdk = Join-Path $root "tools\vitasdk\vitasdk"
$bash = "C:\Program Files\Git\bin\bash.exe"

if (!(Test-Path $vitasdk)) {
  throw "VitaSDK is missing. Extract the autobuild into tools\vitasdk first."
}
if (!(Test-Path $bash)) {
  throw "Git Bash is missing at $bash."
}

$env:PATH = "C:\Program Files\Git\usr\bin;C:\Program Files\Git\mingw64\bin;$env:PATH"

& $bash -lc @"
cd /c/Users/bv/Desktop/np2_for_vita
export VITASDK=/c/Users/bv/Desktop/np2_for_vita/tools/vitasdk/vitasdk
export PATH="`$VITASDK/bin:/usr/bin:/mingw64/bin:`$PATH"
vdpm zlib libpng freetype libvita2d sdl2 sdl2_mixer sdl2_ttf
"@

