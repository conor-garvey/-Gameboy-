# PicoBoy

PicoBoy is a Raspberry Pi Pico W handheld project built around an `ILI9341` display and an `LSM6DSL` IMU.

Right now the project includes:

- a custom `Display` class for the ILI9341
- strip-buffer rendering to reduce flicker without a full framebuffer
- menu and profile selection flow
- `Plumber Man`, a simple platformer
- `Sky Dodger`, a motion-control based game 

## Hardware

### Display Wiring

- `LCD CS` -> `GP17`
- `LCD CLK` -> `GP18`
- `LCD SDI` -> `GP19`
- `LCD RS/DC` -> `GP20`
- `LCD RST` -> `GP21`
- `LCD LED` -> `GP22`

### Buttons

- `UP` -> `GP2`
- `DOWN` -> `GP3`
- `LEFT` -> `GP4`
- `RIGHT` -> `GP5`
- `BUTTON A` -> `GP6`
- `BUTTON B` -> `GP7`
- `SELECT` -> `GP8`
- `START` -> `GP9`

### IMU

Current code configuration in [src/main.cpp](./src/main.cpp):

- `I2C` -> `i2c0`
- `SDA` -> `GP0`
- `SCL` -> `GP1`
- bus speed -> `400000`
- address -> auto-detects `0x6A` or `0x6B`

## Project Layout

- [include/picoboy/display.hpp](./include/picoboy/display.hpp) - display, rendering, text, frame timing
- [include/picoboy/buttons.hpp](./include/picoboy/buttons.hpp) - button input handling
- [include/picoboy/lsm6dsl.hpp](./include/picoboy/lsm6dsl.hpp) - IMU driver
- [include/picoboy/menu_app.hpp](./include/picoboy/menu_app.hpp) - boot flow and menus
- [include/picoboy/plumber_man_game.hpp](./include/picoboy/plumber_man_game.hpp) - platformer game
- [include/picoboy/sky_dodger_game.hpp](./include/picoboy/sky_dodger_game.hpp) - IMU-based dodging game
- [src/main.cpp](./src/main.cpp) - top-level setup and main loop

## Requirements

You need:

- Raspberry Pi Pico SDK
- CMake
- Ninja or another CMake generator
- ARM GCC toolchain for the Pico

The easiest setup on Windows is usually the Raspberry Pi Pico VS Code extension, because it installs and manages the Pico SDK and toolchain for you.

## Build Setup

### PowerShell

From the project root:

```powershell
cmake -S . -B build -G Ninja -DPICO_BOARD=pico_w
cmake --build build
```

If your Pico SDK is not already set up by the VS Code extension, set `PICO_SDK_PATH` first:

```powershell
$env:PICO_SDK_PATH="C:\path\to\pico-sdk"
cmake -S . -B build -G Ninja -DPICO_BOARD=pico_w
cmake --build build
```

### Clean Reconfigure

If you want to wipe the existing build folder and configure again:

```powershell
Remove-Item -Recurse -Force build
cmake -S . -B build -G Ninja -DPICO_BOARD=pico_w
cmake --build build
```

## Build Outputs

After a successful build, the important files are usually:

- `build/PicoBoy.uf2`
- `build/PicoBoy.elf`
- `build/PicoBoy.bin`

To flash the board:

1. Hold the Pico W `BOOTSEL` button while plugging it in.
2. Copy `build/PicoBoy.uf2` to the Pico mass-storage device.

## Current Runtime Configuration

The main app setup is in [src/main.cpp](./src/main.cpp).

Current defaults:

- rotation: `Landscape90`
- viewport: `260 x 218`
- target FPS: `60`

That section currently looks like this:

```cpp
display.setRotation(picoboy::Display::Rotation::Landscape90);
display.setViewport(260, 218);
display.init();
display.setTargetFps(60);
```

To use the full landscape screen again:

```cpp
display.setViewport(320, 240);
```

## Controls

### Menu

- `UP` / `DOWN` move selection
- `A` or `START` confirm
- `B` or `SELECT` go back

### Plumber Man

- `LEFT` / `RIGHT` move
- `A` or `UP` jump
- `B` run
- `START` restart
- `SELECT` exit

### Sky Dodger

- `A` or `START` steps through calibration
- tilt the device to steer
- `START` restart after crashing
- `SELECT` or `B` exit

## Notes

- The display renderer uses strip buffers instead of a full framebuffer to keep RAM usage reasonable on the Pico W.
- `Sky Dodger` calibrates at startup and auto-detects the steering axis.
- The codebase is still growing, so the API and game structure may continue to change as PicoBoy develops.
