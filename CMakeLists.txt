# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Gameboy C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable (target name = Gameboy)
add_executable(Gameboy
    src/main.c
)

pico_set_program_name(Gameboy "Gameboy")
pico_set_program_version(Gameboy "0.1")

# stdio over UART/USB
pico_enable_stdio_uart(Gameboy 0)
pico_enable_stdio_usb(Gameboy 1)

# Add include directories for headers
target_include_directories(Gameboy PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Link Pico standard library
target_link_libraries(Gameboy
    pico_stdlib
)

# Create UF2/ELF/BIN etc.
pico_add_extra_outputs(Gameboy)