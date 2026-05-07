# NP2 Kai RC 009 for Vita

Release candidate for the native Vita NP2 Kai port.

## Status

- Native Vita VPK build works.
- PSP-style menu, mouse cursor, virtual keyboard, and game/menu layer display work.
- Japanese file names in the disk browser work.
- STOP key recovery workaround is implemented.
- Fullcore NP2 Kai base is used for improved compatibility.
- Anime-heavy games such as Ani Mahjong can progress past the previous fade-out stall.

## Recommended Config

Stable:

```ini
VITA_PRESENT_SKIP=3
VITA_CORE_DRAWSKIP=false
VITA_AUDIO_LIGHT=false
VITA_SPEED_LIMIT=0
```

High-speed beta:

```ini
VITA_PRESENT_SKIP=3
VITA_CORE_DRAWSKIP=true
VITA_AUDIO_LIGHT=false
VITA_SPEED_LIMIT=0
```

## Build

```powershell
$env:VITASDK=(Resolve-Path tools\vitasdk\vitasdk).Path
$env:PATH="$env:VITASDK\bin;$(Resolve-Path tools\ninja);$env:PATH"
cmake --build build\vita_np2_kai_fullcore -j 4
```

Output:

```text
build\vita_np2_kai_fullcore\vita_np2_kai_fullcore.vpk
```

## Release Files

The local release folder is:

```text
release\np2_kai_vita_rc009
```

VPK files are ignored by Git and should be attached to a GitHub Release rather than committed.
