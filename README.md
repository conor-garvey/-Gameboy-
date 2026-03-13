# 3C10

`Gameboy` is a Raspberry Pi Pico W arcade project written in C with the Pico SDK. It drives an ILI9341 SPI display, reads an LSM6DS3 IMU over I2C, and uses eight GPIO buttons for input.

The project currently includes:

- `PLUMBER`: a side-scrolling platformer
- `DROP`: a falling game that uses IMU tilt, with button fallback if the IMU is unavailable

## Requirements

To build from a fresh clone you need:

- Raspberry Pi Pico SDK toolchain or equivalent ARM embedded GCC setup
- `cmake`
- `ninja`
- `git`

If `PICO_SDK_PATH` is not already set, CMake is configured to fetch `pico-sdk` version `2.2.0` automatically on first configure. That makes a clean clone easier to build, but it does require internet access during the first configure step.

## Build

### Option 1: Standard CMake build

From the repository root:

```powershell
cmake -S . -B build -G Ninja -DPICO_BOARD=pico_w
cmake --build build
```

The final firmware file will be:

```text
build/3C10.uf2
```

### Option 2: Use an existing local Pico SDK

If you already have the Pico SDK installed, set `PICO_SDK_PATH` before configuring.

Windows PowerShell:

```powershell
$env:PICO_SDK_PATH="C:\path\to\pico-sdk"
cmake -S . -B build -G Ninja -DPICO_BOARD=pico_w
cmake --build build
```

macOS/Linux:

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build -G Ninja -DPICO_BOARD=pico_w
cmake --build build
```

## Flashing

1. Hold `BOOTSEL` on the Pico W and plug it into USB.
2. It should mount as a USB storage device.
3. Copy `build/3C10.uf2` onto the device.
4. The board will reboot into the program.

If you use the Raspberry Pi Pico VS Code extension, the included `.vscode/extensions.json` recommends the relevant extensions, but the project does not depend on VS Code-specific settings to build.

## Controls

### Menu

- `UP` / `DOWN`: choose game
- `A`: start selected game
- `Y`: recalibrate IMU when available

### PLUMBER

- `LEFT` / `RIGHT`: move
- `A`: jump
- `X`: run faster
- `B`: return to menu

### DROP

- Tilt the device left/right to move
- `LEFT` / `RIGHT`: fallback controls if the IMU is unavailable
- `A`: restart after game over
- `B`: return to menu

## Wiring

### ILI9341 display

- `SCK`: GPIO `18`
- `MOSI`: GPIO `19`
- `CS`: GPIO `17`
- `DC`: GPIO `20`
- `RST`: GPIO `21`
- `BL`: GPIO `22`

### LSM6DS3 IMU

- `SDA`: GPIO `27`
- `SCL`: GPIO `26`
- I2C address: `0x6A`

### Buttons

- `UP`: GPIO `2`
- `DOWN`: GPIO `3`
- `LEFT`: GPIO `4`
- `RIGHT`: GPIO `5`
- `B`: GPIO `6`
- `A`: GPIO `7`
- `X`: GPIO `8`
- `Y`: GPIO `9`

All buttons are configured as active-low inputs with internal pull-ups enabled.
