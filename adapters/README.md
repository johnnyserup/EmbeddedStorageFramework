# Adapters

This directory contains **adapter** implementations for EmbeddedStorageFramework.

An adapter is a thin **bridge layer** between an ESF abstract interface
(`IStorageDriver` or `ISpiHal`) and hardware- or OS-specific code.  It
translates framework calls into the API of a concrete platform SDK (e.g. STM32
CubeHAL, Linux `spidev`, FreeRTOS) without any business logic of its own.

---

## Three-layer responsibility model

```
┌─────────────────────────────────────────────────────────────────┐
│                        Core Layer  (framework)                  │
│                                                                 │
│  interfaces/   IStorageDriver, IRepository<T>, StorageError     │
│  core/         Crc32, ObjectHeader, StorageObject<T,Ver,ObjId>  │
│  repositories/ RepositoryBase (CRTP) — load / save / reset      │
│  drivers/      FramStorageDriver, RamStorageDriver              │
│                                                                 │
│  ✔ No STM32 headers   ✔ No HAL calls   ✔ Testable on host       │
└───────────────────────────────┬─────────────────────────────────┘
                                │  implements ISpiHal
                                │  or IStorageDriver
┌───────────────────────────────▼─────────────────────────────────┐
│                      Adapter Layer  (adapters/)                 │
│                                                                 │
│  One sub-directory per platform target, e.g.:                   │
│    adapters/stm32_hal/   — wraps STM32 CubeHAL SPI              │
│    adapters/linux_spidev/ — wraps Linux /dev/spidevX.Y          │
│                                                                 │
│  ✔ Allowed: platform SDK headers, HAL calls, RTOS calls         │
│  ✔ Thin wrappers only — no data models, no business logic       │
│  ✘ Not allowed: STM32/RTOS headers in core or drivers/          │
└───────────────────────────────┬─────────────────────────────────┘
                                │  calls into
┌───────────────────────────────▼─────────────────────────────────┐
│                    Hardware Layer  (platform SDK)               │
│                                                                 │
│  STM32 CubeHAL: HAL_SPI_Transmit / HAL_SPI_Receive / GPIO      │
│  Linux kernel:  ioctl(SPI_IOC_MESSAGE), open/read/write         │
│  FreeRTOS:      xSemaphoreTake / xSemaphoreGive                 │
│                                                                 │
│  Owned by the MCU vendor or OS — never modified by this project │
└─────────────────────────────────────────────────────────────────┘
```

### Layer responsibilities in brief

| Layer | Owns | Must NOT contain |
|-------|------|------------------|
| **Core** | Interfaces, persistence logic, CRC, object layout | Platform headers, HAL calls, application data models |
| **Adapter** | One concrete implementation of `ISpiHal` or `IStorageDriver` per platform | Business logic, data models, direct register access outside of the HAL |
| **Hardware** | Vendor SDK, MCU peripheral drivers | (managed by vendor — do not modify) |

---

## What belongs in `adapters/`

- Thin wrapper classes that translate ESF interface calls into platform-specific
  API calls (e.g. `HAL_SPI_TransmitReceive`, `ioctl`).
- One sub-directory per platform/target (e.g. `adapters/stm32_hal/`,
  `adapters/linux_spidev/`).
- Each sub-directory exposes exactly one CMake target named
  `esf::adapter_<name>`.
- Header files that include platform SDK headers (e.g. `stm32f4xx_hal.h`).

## What does NOT belong in `adapters/`

| Item | Where it belongs instead |
|------|--------------------------|
| Business logic, data models | Application firmware project |
| Driver implementations (`FramStorageDriver`, `RamStorageDriver`) | `drivers/` |
| Test-only fakes (`FakeStorageDriver`, `FakeSpiHal`) | `tests/fake/` |
| STM32 or OS headers inside `core/` or `drivers/` | Only in `adapters/` |
| Any dynamic memory allocation | Nowhere — framework is allocation-free |

---

## CMake target convention

Every adapter sub-directory must define a static library target and expose it
through the `esf::` namespace alias:

```cmake
# adapters/<name>/CMakeLists.txt

add_library(esf_adapter_<name> STATIC
    src/<Name>SpiHal.cpp        # or <Name>StorageDriver.cpp
)

add_library(esf::adapter_<name> ALIAS esf_adapter_<name>)

target_include_directories(esf_adapter_<name>
    PUBLIC  include
    PRIVATE ${PLATFORM_SDK_INCLUDE_DIRS}   # e.g. STM32 HAL headers
)

target_link_libraries(esf_adapter_<name>
    PUBLIC  esf::interfaces     # IStorageDriver / ISpiHal contracts
    PRIVATE ${PLATFORM_SDK_LIBRARIES}
)
```

Register the new adapter in `adapters/CMakeLists.txt`:

```cmake
add_subdirectory(<name>)
```

---

## How to create a new adapter

### Option A — Implement `ISpiHal` (for FRAM over SPI)

Use this when your hardware uses `FramStorageDriver` and you need to bridge
it to a concrete SPI peripheral.

**Interface contract** (`drivers/fram/include/esf/drivers/ISpiHal.hpp`):

```cpp
class ISpiHal {
public:
    virtual void csAssert()   = 0;   // assert chip-select (active low)
    virtual void csDeassert() = 0;   // deassert chip-select
    virtual bool transmit(const uint8_t* data, size_t length) = 0;
    virtual bool receive (uint8_t* data,       size_t length) = 0;
};
```

**Minimal STM32 CubeHAL example** — file layout:

```
adapters/stm32_hal/
├── CMakeLists.txt
├── include/
│   └── esf/adapters/Stm32SpiHal.hpp
└── src/
    └── Stm32SpiHal.cpp
```

`include/esf/adapters/Stm32SpiHal.hpp`:

```cpp
#pragma once
#include "esf/drivers/ISpiHal.hpp"
#include "stm32f4xx_hal.h"   // platform header stays in the adapter

namespace esf::adapters {

class Stm32SpiHal final : public esf::drivers::ISpiHal {
public:
    Stm32SpiHal(SPI_HandleTypeDef& hspi, GPIO_TypeDef* csPort, uint16_t csPin)
        : hspi_{hspi}, csPort_{csPort}, csPin_{csPin} {}

    void csAssert()   override { HAL_GPIO_WritePin(csPort_, csPin_, GPIO_PIN_RESET); }
    void csDeassert() override { HAL_GPIO_WritePin(csPort_, csPin_, GPIO_PIN_SET);   }

    bool transmit(const uint8_t* data, size_t length) override {
        return HAL_SPI_Transmit(&hspi_,
                                const_cast<uint8_t*>(data),
                                static_cast<uint16_t>(length),
                                HAL_MAX_DELAY) == HAL_OK;
    }

    bool receive(uint8_t* data, size_t length) override {
        return HAL_SPI_Receive(&hspi_,
                               data,
                               static_cast<uint16_t>(length),
                               HAL_MAX_DELAY) == HAL_OK;
    }

private:
    SPI_HandleTypeDef& hspi_;
    GPIO_TypeDef*      csPort_;
    uint16_t           csPin_;
};

} // namespace esf::adapters
```

Wire it together in your firmware:

```cpp
#include "esf/adapters/Stm32SpiHal.hpp"
#include "esf/drivers/FramStorageDriver.hpp"
#include "MySettingsRepository.hpp"   // your RepositoryBase derivative

esf::adapters::Stm32SpiHal    hal{hspi1, FRAM_CS_GPIO_Port, FRAM_CS_Pin};
esf::drivers::FramStorageDriver<8192> driver{hal};
MySettingsRepository           repo{driver};
```

---

### Option B — Implement `IStorageDriver` directly

Use this when you want to expose a storage medium that is not FRAM over SPI
(e.g. an external NOR flash, an EEPROM, or an OS file).

**Interface contract** (`interfaces/include/esf/IStorageDriver.hpp`):

```cpp
class IStorageDriver {
public:
    virtual bool   read    (uint32_t address, uint8_t* data, size_t length) = 0;
    virtual bool   write   (uint32_t address, const uint8_t* data, size_t length) = 0;
    virtual size_t capacity() const = 0;
};
```

**Skeleton**:

```cpp
#pragma once
#include "esf/IStorageDriver.hpp"
// include platform-specific headers here

namespace esf::adapters {

class MyFlashDriver final : public esf::IStorageDriver {
public:
    bool read(uint32_t address, uint8_t* data, size_t length) override {
        // call into platform flash API
    }
    bool write(uint32_t address, const uint8_t* data, size_t length) override {
        // call into platform flash API
    }
    size_t capacity() const override { return kCapacity; }

private:
    static constexpr size_t kCapacity = 65536u;
};

} // namespace esf::adapters
```

---

## Current adapters

_None yet — add subdirectories here as platforms are supported._

---

## Related documentation

- [`docs/architecture.md`](../docs/architecture.md) — full layered architecture,
  key types, object lifecycle, and integration guide.
- [`drivers/fram/include/esf/drivers/ISpiHal.hpp`](../drivers/fram/include/esf/drivers/ISpiHal.hpp) — ISpiHal interface.
- [`interfaces/include/esf/IStorageDriver.hpp`](../interfaces/include/esf/IStorageDriver.hpp) — IStorageDriver interface.
- [`tests/fake/`](../tests/fake/) — `FakeSpiHal` and `FakeStorageDriver` for
  host-side unit testing without hardware.
