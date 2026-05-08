# NP2 Kai Touhou 010 for Vita

Experimental build for checking RetroArch-like NP2 Kai timing on Vita.

## Changes

- Enables `SUPPORT_FMGEN` and builds the bundled `sound/fmgen/*.cpp` sources.
- Adds `VITA_TOUHOU_PRESET`.
- Uses a separate Vita title ID: `NP2T00010`.
- Fixes the Vita PSP audio compatibility wrapper for C++ compilation.

## Touhou Preset

```ini
VITA_TOUHOU_PRESET=true
VITA_AUDIO_LIGHT=false
```

When enabled, the preset forces:

```ini
clk_base=2457600
clk_mult=8 or higher
SampleHz=44100
Latencys=0
skipline=true
skplight=255
VITA_PRESENT_SKIP=0
VITA_CORE_DRAWSKIP=false
VITA_SPEED_LIMIT=1
```

## Test Focus

- Touhou PC-98 boot and playability.
- Whether FMGEN changes music load or compatibility.
- Whether strict timing feels cleaner than the fast RC009 preset.

## Output

```text
release\np2_kai_vita_touhou_v010\vita_np2_kai_fullcore_touhou_v010.vpk
```
