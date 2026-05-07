# Vita Configuration

`ux0:data/np2vita/np2.cfg` can contain Vita-specific tuning keys.

Recommended stable settings:

```ini
VITA_PRESENT_SKIP=3
VITA_CORE_DRAWSKIP=false
VITA_AUDIO_LIGHT=false
VITA_SPEED_LIMIT=0
```

Recommended high-speed beta settings:

```ini
VITA_PRESENT_SKIP=3
VITA_CORE_DRAWSKIP=true
VITA_AUDIO_LIGHT=false
VITA_SPEED_LIMIT=0
```

## Keys

- `VITA_PRESENT_SKIP=0-10`
  - Skips Vita display presentation.
  - `0` presents every loop.
  - `3` presents roughly once per 4 loops.
  - Old compatible name: `VITA_FRAMESKIP`.

- `VITA_CORE_DRAWSKIP=true/false`
  - Enables smart internal draw generation skipping.
  - Small dirty updates are always drawn.
  - Large dirty updates are skipped according to `VITA_PRESENT_SKIP`.
  - Old compatible name: `VITA_DRAWSKIP_CORE`.

- `VITA_SPEED_LIMIT=0/1/2`
  - `0`: fastest, does not wait on skipped presentation loops.
  - `1`: strict wait, less fast-forward risk but slower.
  - `2`: adaptive wait, waits only when the draw counter is not changing.

- `VITA_AUDIO_LIGHT=true/false`
  - Experimental audio load reduction.
  - Forces lower sampling rate and higher latency.
  - Keep `false` unless testing audio performance.

## Compatibility Notes

The old names remain supported:

```ini
VITA_FRAMESKIP=3
VITA_DRAWSKIP_CORE=false
```

For new configs, prefer:

```ini
VITA_PRESENT_SKIP=3
VITA_CORE_DRAWSKIP=false
```
