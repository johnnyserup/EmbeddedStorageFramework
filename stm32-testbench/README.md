# STM32 Testbench

This directory is the CMake project for hardware-in-the-loop integration tests
that run on a real STM32 target with an attached MB85RS64TPN FRAM device.

## Planned content

- `src/main.cpp`        — FreeRTOS or bare-metal test runner
- `src/Stm32SpiHal.hpp` — `ISpiHal` implementation wrapping `HAL_SPI_*`
- `CMakeLists.txt`      — Cross-compilation target (arm-none-eabi)
- `stm32.ld`            — Linker script

## Prerequisites

- arm-none-eabi-g++ toolchain
- STM32CubeMX-generated HAL or equivalent
- OpenOCD or ST-Link for flashing

This project is intentionally kept separate from the host-build tree so that
the core framework can be developed and tested without any cross-compilation
toolchain installed.
