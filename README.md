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

## Fresh Windows Setup

If your laptop is starting from nothing, this is the easiest path:

1. Install [Visual Studio Code](https://code.visualstudio.com/).
2. Open VS Code and install the `Raspberry Pi Pico` extension published by `Raspberry Pi`.
3. Get this project onto your laptop:
   - install Git and clone it, or
   - download the repository ZIP from GitHub and extract it
4. In VS Code, open the `PicoBoy` project folder itself.
5. Let the Pico extension install the Pico SDK, toolchain, CMake, and Ninja if it prompts you.
6. Restart VS Code once that setup finishes.

After that, open a PowerShell terminal in the project folder and use the build commands below.

## Build Setup

### PowerShell

From the project root:

```powershell
cmake -S . -B build -G Ninja -DPICO_BOARD=pico_w
cmake --build build
```

If you cloned or downloaded the repo onto a machine that does not already have the Pico SDK configured by the VS Code extension, set `PICO_SDK_PATH` first:

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

### Git Clone Example

If you are using Git on a new machine:

```powershell
git clone https://github.com/YOUR-USERNAME/PicoBoy.git
cd PicoBoy
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
- target FPS: `30`

That section currently looks like this:

```cpp
display.setRotation(picoboy::Display::Rotation::Landscape90);
display.setViewport(260, 218);
display.init();
display.setTargetFps(30);
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

### Settings

- open from the `SETTINGS` item in the game-select screen
- switch `DISPLAY MODE` between `QUIET` and `ORIGINAL`
- run `NOISE DIAGNOSTIC` from the settings screen

### Plumber Man

- `LEFT` / `RIGHT` move
- `A` or `UP` jump
- `B` run
- `START` restart
- `SELECT` pause
- pause menu: return to menu or change volume

### Sky Dodger

- `A` or `START` steps through calibration
- tilt the device to steer
- `START` restart after crashing
- `SELECT` pause
- pause menu: return to menu or change volume

## Noise Diagnostic

Open the `SETTINGS` item from the game-select screen, then choose `NOISE DIAGNOSTIC`.

Inside the diagnostic:

- `UP` / `DOWN` select an item
- `A`, `LEFT`, or `RIGHT` toggle the selected item
- `START` exits back to the normal menu

The diagnostic keeps the audio pin forced low and lets you toggle:

- `SCREEN LOAD` to turn continuous display SPI updates on or off
- `BACKLIGHT` to turn the display backlight on or off
- `IMU POLL` to turn repeated IMU reads on or off

## Notes

- The display renderer uses strip buffers instead of a full framebuffer to keep RAM usage reasonable on the Pico W.
- The default display SPI clock is reduced to `10 MHz` and the default frame rate is `30 FPS` to cut display-related audio noise.
- The settings screen lets you switch back to the original fast display mode: `20 MHz` SPI and `60 FPS`.
- `Sky Dodger` calibrates at startup and auto-detects the steering axis.
- The codebase is still growing, so the API and game structure may continue to change as PicoBoy develops.
