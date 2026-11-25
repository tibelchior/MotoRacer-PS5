# Moto Racer (1997) for PS5

*Moto Racer* running natively on a jailbroken PS5: the GOG release, statically recompiled by
[MotoRacerRecomp](https://github.com/segovia-no/MotoRacerRecomp) (segovia-no), with the PS5 platform
layer from this repository and [mihawk-99's PS5 Vulkan stack](https://github.com/mihawk-99/PS5_Vulkan).
Title ID `PPSA99785`.

**You bring your own game.** This repository contains no game files and no translated game code.
`./build.sh gen` recompiles *your* `game.EXE` on *your* computer.

## What you get

- The repo's own Direct3D renderer on the console GPU (SDL_GPU over Vulkan and RADV), at the display's
  full **4K** with 60 fps: HUD scaling, filtered textures, widescreen, internal resolution up to 7680x4320 in
  races. The game's original software renderer is the automatic fallback.
- DualSense, sound through the console's audio output, CD-audio tracks from the GOG WAV files.
- Quitting from the game menu hands the exit to the console, so the title closes cleanly.

Status: tested on a console (menus, races, sound, pad, exit). 4K held 60 fps in a race.

## What you need

- A PS5 with homebrew enabled, a way to start titles from `/data/homebrew` (for example
  ShadowMountPlus) and FTP access.
- A computer with **Docker** (macOS or Linux, 16 GB memory, about 60 GB free for the toolchain) and
  Python 3 for the host step.
- The GOG installer `setup_moto_racer_2.1.0.11.exe`, unpacked with [innoextract](https://constexpr.org/innoextract/)
  (`innoextract --gog setup_moto_racer_2.1.0.11.exe`). The folder with `game.EXE` is the game folder.

## Build

```bash
# 0. once: build the PS5 toolchain into a Docker volume (1 to 2 hours)
./build.sh toolchain

# 1. host step, once: recompile your game.EXE into C (needs repo/.venv with pefile and capstone)
python3 -m venv repo/.venv && repo/.venv/bin/pip install pefile capstone
GAME_DIR=/path/to/game/folder ./build.sh gen

# 2. cross-compile, link and sign the title (first run downloads SDL3 and builds it for the console)
./build.sh title

# 3. copy the game files next to the title
GAME_DIR=/path/to/game/folder ./build.sh package
```

The result is `dist/PPSA99785/`. Run `./build.sh package` again after every `./build.sh title`
(the title step wipes the packaged game folder). Add `art/icon.png` and `art/wallpaper.png`
(your own images) before step 2 for the home-screen art.

Options: `BUILD_VOLUME=<name>` (default `motoracerps5-build-data`), `GAME_DIR=<folder with game.EXE>`.
The step `./build.sh sdl` builds only SDL3; the link step ends with an expected stock-exe link error
that the script ignores.

## Install

Copy `dist/PPSA99785` to `/data/homebrew/PPSA99785`, rescan, start **Moto Racer**.

```
PPSA99785/
  eboot.bin, sce_sys/, sce_module/   the title
  MotoRacer/                         your game files (settings and best times are saved here)
  data/                              what the port writes
    motoracer.log                    the log (one file for the runtime, SDL and the console glue)
```

### Optional files in `data/`

| File | Effect |
|---|---|
| `output` | display height: `1080`, `1440` or `2160` (default: the largest, 4K) |
| `scale` | internal resolution multiplier, 1 to 8 (default: output height / 480, so 5 at 4K). Lower it if a race stutters |
| `renderer` | containing `software` forces the game's own software renderer |
| `gpu-trying` | written by the title at each GPU start and removed at the first presented frame. If it is still there, the last GPU start failed and the next start uses the software renderer: delete it to try the GPU again |

## Controls

D-pad / left stick: menus and steering. Cross = Enter, **Square = Space**, Circle and Options = Escape
(ignored on the main menu so it cannot quit the game), touchpad = cycle the speedometer.
In a race the game reads the pad itself.

## How it works

- `repo/` is MotoRacerRecomp with a `PS5` platform: 4 GB guest memory window from direct memory,
  the guest runs on a 512 MB host stack, POSIX files, crash report to the log.
- `ps5/glue/ps5_launch.cpp` starts the game, picks the renderer and scale, and routes exit through the system.
  `ps5_io.cpp` maps scePad to an SDL gamepad and audio to sceAudioOut, `ps5_vulkan.cpp` makes the
  VK_KHR_display surface, `ps5_present.cpp` presents software-renderer frames.
- `ps5/sdl/` patches SDL3's dummy video driver to expose Vulkan, and makes SDL_GPU accept the console's
  RADV (which reports no conformance version).
- The recompiled game code is built with `-ffp-contract=off` so x87 rounding matches the PC.

If something goes wrong, `data/motoracer.log` has the reason (`[sdl ...]`, `[ps5-vulkan]`, `[ps5-io]` lines).

See [NOTICE.md](NOTICE.md) for licences and credits.
