# My-Super-Marine-Clone

A C++ remake of **Super Marine**, the top-down Flash shooter published by MaxGames. You land on an alien planet as a lone marine, defend your base and its command centre, and fight north through waves of aliens to destroy their hive. Money from kills and from each wave buys armour, weapons and turrets for the base.

The goal is to stay close to the original: the art, sounds and animation timelines come from the original SWF, and the gameplay code follows the game's own ActionScript. It runs the game logic at the original 24 frames per second but draws smooth in-between frames at your monitor's refresh rate.

<!-- Add a screenshot here, e.g. ![Gameplay](docs/screenshot.png) -->

## Features

- Easy, Normal and Hard difficulty, using the original's difficulty factors
- The intro fly-in, the incoming transmission and the dropship landing (all skippable)
- Rifle upgrades plus grenade launcher, cannon and rocket launcher
- An armoury shop with masks, power cells, shields, armour, shoulders and generators; equipment can be sold back from the status panel
- Buildable Spitfire and Terminator turrets, with paid repairs
- Friendly marines from the barracks, three alien types, nests and timed waves
- Radar, status panel, field hints, a quality menu and the six-page help screen
- The original victory and defeat screens; scores submitted under your name go to a local Hall of Fame

## Controls

| Input | Action |
| --- | --- |
| WASD / arrow keys | Move |
| Mouse | Aim |
| Hold left click | Fire |
| 1 / 2 / 3 / 4 | Rifle / grenade / cannon / rocket (buy the special weapons first) |
| Space | Open the shop on the armoury pad or a turret pad; skip cinematics |
| E | On a turret pad: toggle repairs (costs credits) |
| P | Pause / resume; closes the shop |
| H | Help |
| F | Status panel (click equipment icons to sell them) |
| R | Radar |
| I | Field hints |
| Q | Quality menu |
| M | Sound on / off |
| Enter | Start from the title; pick Normal; skip cinematics |
| Esc | Go back |

Stand on the repair pad to restore health and energy.

## Building

The project is a Visual Studio 2022 solution (toolset v143, C++20) for Windows.

### Dependencies

The engine (`MyEngine`) and the SWF player (`SwfClip`) need these libraries:

- [GLFW](https://www.glfw.org/) and [GLEW](https://glew.sourceforge.net/)
- [GLM](https://github.com/g-truc/glm)
- [FreeType](https://freetype.org/) and [msdfgen](https://github.com/Chlumsky/msdfgen)
- [OpenAL](https://www.openal.org/) (OpenAL Soft) and [libsndfile](https://libsndfile.github.io/libsndfile/) (with MP3 support)
- [cereal](https://uscilab.github.io/cereal/)
- [zstd](https://github.com/facebook/zstd)
- [nanosvg](https://github.com/memononen/nanosvg), [stb](https://github.com/nothings/stb) and [nlohmann/json](https://github.com/nlohmann/json)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib)
- OpenCL headers (`CL/opencl.hpp`)

The easiest way to install them is [vcpkg](https://vcpkg.io/) with Visual Studio integration:

```bash
vcpkg install glfw3 glew glm freetype msdfgen openal-soft libsndfile cereal zstd nanosvg stb nlohmann-json cpp-httplib opencl --triplet x64-windows
```

```bash
vcpkg integrate install
```

### Build and run

1. Open `My Super Marine Clone (Sleepy Biden Engine v0.1).sln` in Visual Studio.
2. Pick a configuration: `Debug`, `Release`, or `Release - No Console` (no console window), on `x64`.
3. Build and run. The game loads files from `resources/` by relative path, so run it with the project folder as the working directory, or copy `resources/` next to the built `.exe`.

The HUD uses the Windows fonts Segoe UI and Arial.

## Project layout

| Path | Contents |
| --- | --- |
| `main.cpp` | Window setup, the main loop and the command-line test modes |
| `Game.cpp`, `Game.h` | Gameplay (ported from the original ActionScript), menus, HUD, shop and drawing |
| `SwfClip.cpp`, `SwfClip.h` | A small Flash timeline player that plays the original clips, buttons and filters |
| `MyEngine.cpp`, `MyEngine.h` | The engine: rendering, text, audio and input |
| `resources/game/` | Everything the game loads: SWF timeline data, shapes, sprites, frames, the HUD bar and sounds |
| `resources/other/` | The rest of the SWF export, including the decompiled ActionScript, kept for reference |
| `resources/SMNTgw.swf` | The original game, used by the tools and for comparison |
| `tools/` | Export scripts (`export_timelines.py`, `export_hud.py`, `index-impact-frames.py`, `patch_edit_flags.py`) and comparison scripts |
| `runtime-comparison/` | Side-by-side captures and measurements against Flash Player |
| `PORTING.md` | Detailed notes on how the port matches the original |
| `super-marine-scores.txt` | The local Hall of Fame, created after your first submitted score |

## Comparing with the original

The executable has three test modes:

```powershell
& ".\x64\Release\My Super Marine Clone (Sleepy Biden Engine v0.1).exe" --self-test
```

`--self-test` runs the built-in gameplay, presentation and input checks and exits with code 2 if any fail. `--self-test-wide` runs the same checks in a 1280×800 window. `--capture-screens <folder>` saves twelve frames each of the main screens (instructions, difficulty, scores, title, help, gameplay, victory and defeat) to an existing folder, then quits.

The PowerShell scripts in `tools/` (for example `capture-extended.ps1`, `capture-landing.ps1` and `capture-shots.ps1`) drive Flash Player 32 and the clone through the same inputs and save screenshots of both. The Python scripts measure the differences:

```bash
python tools/analyze-extended.py --directory runtime-comparison/interp-2026-10-26
```

## Credits

- **Original game:** Super Marine, published by MaxGames (maxgames.com).
- **Engine:** MyEngine by FervidFiend.

This is an unofficial fan project for learning purposes and is not affiliated with MaxGames. All the original art, sound and code belong to their owners.
