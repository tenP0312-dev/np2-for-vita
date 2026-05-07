# Known Issues

## Audio Quality

Sampled voices and ADPCM-like playback can sound rough. `VITA_AUDIO_LIGHT=true` usually makes this worse, so the default is `false`.

## Speed Tuning

`VITA_SPEED_LIMIT=0` can let the emulator run faster than real time in light scenes. This is useful for heavy games but can make MIDI/BGM feel fast in some cases.

Use `VITA_SPEED_LIMIT=1` for stricter pacing, or `2` for the adaptive beta mode.

## Smart Core Drawskip

`VITA_CORE_DRAWSKIP=true` skips only large dirty updates. It should preserve small UI updates such as cursor movement and command selection, but the dirty-line threshold is still experimental.

If any cursor, selector, or small animation stops updating:

```ini
VITA_CORE_DRAWSKIP=false
```

## Touhou PC-98

Touhou titles can boot with high skip values, but practical playability is still poor. Treat them as out of scope for the current release candidate.

## Storage

If VPK install or file copy fails with errors such as `0x8001000c`, first check whether the SD card or Vita mount is visible and writable on the PC.
