# np2 kai native v036 restart 20260519

This release is a restart build based on the partial backup:
prototype/vita_np2_kai_native/backup_before_native_20260514_v037_cfg_tuning

Restored/overlaid files:
- CMakeLists.txt
- psp/np2.c
- psp/ini.c
- psp/np2.h

Confirmed source markers:
- psp/np2.c contains: main: build vita-np2-kai-native-v036-fastpack 20260514
- CMake title is NP2 Kai Native 036FAST
- Title ID is NP2N00036

Caveat:
This backup was partial, so the folder was created by copying the current vita_np2_kai_native tree and overlaying the v036-fastpack-era files above. Files not present in the backup are inherited from the current native tree.

Build note:
This v036-era CMake path includes C++/FMGEN sources. Treat this as a comparison/restart build, not as the later minimal nofmgen v09/v002 line.

VPK:
- vita_np2_kai_native_v036_restart.vpk
