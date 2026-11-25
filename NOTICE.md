# Notice

This repository is a **PS5 port kit** for [MotoRacerRecomp](https://github.com/segovia-no/MotoRacerRecomp)
(segovia-no, MIT licence), a static recompilation of *Moto Racer* (Delphine Software, 1997).

- It contains **no game files and no translated game code**. The recompiler turns *your own*
  `game.EXE` (from the GOG release) into C on your own computer (`./build.sh gen`); that output
  (`repo/gen-ps5/`) is ignored by git and must not be shared or committed.
- `repo/` is a copy of MotoRacerRecomp with PS5 changes (the licence in `repo/LICENSE` is kept):
  `runtime/src/platform/ps5.cpp`, `runtime/src/ps5/`, the `PS5` branch in `CMakeLists.txt`,
  PS5-only hooks in `host/host_gpu.cpp`, `host/host_video.cpp`, `host/host_window.cpp`,
  `core/dispatch.cpp`, `dx/ddraw.cpp`, `game/game_hooks.cpp` and `game/settings.*`
  (search for `MOTORACER_PS5`).
- `ps5/` (launch glue, scePad and audio, VK_KHR_display surface, the SDL Vulkan patch) and
  `tools/` (build scripts) are this port's own files.
- **PS5 toolchain, payload SDK and Vulkan driver**: mihawk-99/PS5_Vulkan, PS5_PayloadSDK and PS5_Mesa,
  fetched at the pinned commits in `tools/pins.env` (GPL-3.0 and Mesa's licences). A finished
  title links their code, so share built titles only together with the source obligations
  those licences require.
- **SDL3** (zlib licence): the patched source comes from the Motorhead PS5 port
  (`../Motorhead-PS5`), which builds on S95Sedan's reconstruction (see its `NOTICE.md`).
- **Moto Racer** is the property of its rights holders. This project is not affiliated with or
  endorsed by Delphine Software, Infogrames, GOG or Sony. You need your own legal copy of the game.
