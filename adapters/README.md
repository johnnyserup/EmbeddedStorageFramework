# Adapters

This directory contains **adapter** implementations for EmbeddedStorageFramework.

An adapter bridges the framework's abstract `IStorageDriver` or `ISpiHal`
interfaces to a specific hardware or OS environment (e.g. STM32 CubeHAL,
FreeRTOS, Linux character devices).

## Guidelines

### What belongs here

- Thin wrappers that translate framework interface calls to platform-specific
  HAL or RTOS calls.
- One subdirectory per platform/target (e.g. `adapters/stm32_hal/`,
  `adapters/linux_spidev/`).
- Each subdirectory exposes exactly one CMake target named `esf::adapter_<name>`.

### What does NOT belong here

- Business logic or data models — those belong in the application firmware.
- Driver implementations — those belong in `drivers/`.
- Test-only fakes — those belong in `tests/fake/`.

### CMake target convention

```cmake
add_library(esf_adapter_<name> STATIC ...)
add_library(esf::adapter_<name> ALIAS esf_adapter_<name>)

target_link_libraries(esf_adapter_<name>
    PUBLIC esf::interfaces
)
```

### Interface compatibility

Every adapter must implement exactly the interface contract it claims to satisfy
(`IStorageDriver` or `ISpiHal`) and must not introduce additional dependencies
on framework-internal headers.

## Current adapters

_None yet — add subdirectories here as platforms are supported._
