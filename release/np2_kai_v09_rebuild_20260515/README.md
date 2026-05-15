# vita_np2_kai_v09_rebuild_nofmgen

Purpose:
- RC009 stable line comparison build.
- Started from prototype/vita_np2_kai_fullcore and removed the v010 FMGEN/C++ build path as the first regression suspect.
- This is intended to check whether the newer heavy behavior comes from the FMGEN/C++ path or from other core/body changes.

Build marker expected in bootlog:
- main: build vita-np2-kai-fullcore-rc009-rebuild-nofmgen 20260515

Install target:
- App title: NP2 Kai RC009 Rebuild
- Title ID: NP2R009RB

Recommended first-test config:
VITA_PRESENT_SKIP=3
VITA_CORE_DRAWSKIP=false
VITA_AUDIO_LIGHT=false
VITA_SPEED_LIMIT=0

Notes:
- Extra newer cfg keys are ignored by this rebuild if that parser does not know them.
- Local VPK was built successfully.
- Copy to F: failed at build time because Windows did not have an F: drive mounted.
