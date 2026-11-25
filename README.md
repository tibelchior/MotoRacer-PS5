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

## Quick start

You need three things:

1. A PS5 with homebrew enabled, a way to start titles from `/data/homebrew` (for example
   ShadowMountPlus) and FTP access.
2. A computer with **Docker** running (macOS or Linux; on Windows use WSL2, untested). About 16 GB of
   memory and 60 GB of free disk space.
3. Your own copy of *Moto Racer* from GOG: the offline installer `setup_moto_racer_2.1.0.11.exe`.

Then run one command:

```bash
./make.sh /path/to/setup_moto_racer_2.1.0.11.exe
```

It unpacks the installer, translates your `game.EXE`, builds everything and copies the right game files.
The first run takes 1 to 2 hours (the PS5 toolchain); later runs take minutes. When it says **Ready**,
copy `dist/PPSA99785` to your console. You never pick game files by hand.

There is no ready-made download of the title: the program file contains your game translated to
native code, so it has to be built from your own copy.

Optional: put your own `art/icon.png` and `art/wallpaper.png` in place before running, for the home-screen art.

## Building step by step

`make.sh` runs these; use them when working on the port. `GAME_DIR` is the folder with `game.EXE`.

```bash
./build.sh toolchain                      # once: PS5 toolchain into a Docker volume
GAME_DIR=/path/to/game ./build.sh gen     # translate game.EXE to C (repo/gen-ps5)
./build.sh title                          # cross-compile, link and sign
GAME_DIR=/path/to/game ./build.sh package # copy the game files into dist/PPSA99785/MotoRacer
```

Run `package` again after every `title` (the title step wipes the packaged game folder).
Options: `BUILD_VOLUME=<name>` (default `motoracerps5-build-data`), `BUILD_IMAGE=<name>`.
The link step shows an expected stock-exe link error that the script ignores.

**Game files**, if you copy them yourself: everything in the folder with `game.EXE` (including `game.EXE`
and the music `.wav` files), except other `.exe` files, `goggame*`, `__support/`,
`GameuxInstallHelper.dll` and the readme/FAQ text files. They go in `PPSA99785/MotoRacer/`.

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
