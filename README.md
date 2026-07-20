# EmbeddedStorageFramework

Reusable C++20 embedded persistence framework for STM32 systems with FRAM persistence, repository pattern, versioned objects, migration support and hardware-independent unit testing.

---

## Features

| Feature | Description |
|---------|-------------|
| **Hardware-independent core** | Core interfaces compile on any host — no STM32 headers required |
| **Repository pattern** | Application code uses typed domain repositories, never raw addresses |
| **Versioned objects** | Every stored slot carries a version tag and CRC for safe migration and corruption detection |
| **Dependency injection** | All hardware details are injected — drivers, SPI HAL, etc. |
| **No dynamic allocation** | Every allocation is stack or statically sized |
| **TDD-friendly** | `FakeStorageDriver` and `RamStorageDriver` make host-based tests trivial |
| **C++20** | Concepts, `[[nodiscard]]`, constexpr, `std::expected` |
| **STM32 compatible** | FramStorageDriver targets MB85RS64TPN; ISpiHal wraps CubeHAL |

---

## Directory Structure

```
EmbeddedStorageFramework/
├── interfaces/          Pure-abstract interface headers (IStorageDriver, IRepository)
├── core/                CRC-32, ObjectHeader, StorageObject<T>
├── repositories/        RepositoryBase (CRTP), SettingsRepository example
├── drivers/
│   ├── ram/             RamStorageDriver — static array, allocation-free
│   └── fram/            FramStorageDriver — MB85RS SPI FRAM skeleton + ISpiHal
├── tests/
│   ├── fake/            FakeStorageDriver with fault injection and call counters
│   └── unit/            Google Test unit tests
├── docs/                Architecture documentation
├── examples/            Usage examples
└── stm32-testbench/     STM32 hardware integration test project
```

---

## Quick Start

### Prerequisites

- CMake ≥ 3.20
- C++20 compiler (GCC 12+, Clang 14+, MSVC 19.29+)
- Internet access for the first build (Google Test is fetched via FetchContent)

### Building and running tests (host)

```bash
cmake -B build -S .
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Integrating as a Git submodule

```bash
# In your firmware project
git submodule add https://github.com/johnnyserup/EmbeddedStorageFramework.git extern/esf

# In your CMakeLists.txt
add_subdirectory(extern/esf)
target_link_libraries(my_firmware PRIVATE esf::repositories esf::driver_fram)
```

---

## Architecture Overview

See [docs/architecture.md](docs/architecture.md) for a full description.

```
┌────────────────────────────────────────┐
│           Application Layer            │
│   (uses ISettingsRepository only)      │
└──────────────────┬─────────────────────┘
                   │ IRepository<T>
┌──────────────────▼─────────────────────┐
│         RepositoryBase (CRTP)          │
│  load() → validate header+CRC → data  │
│  save() → build StorageObject → write  │
└──────────────────┬─────────────────────┘
                   │ IStorageDriver
        ┌──────────┴──────────┐
        │                     │
┌───────▼──────┐   ┌──────────▼──────────┐
│ RamStorage   │   │  FramStorageDriver   │
│ Driver<N>    │   │  (MB85RS via SPI)    │
└──────────────┘   └──────────┬──────────┘
                               │ ISpiHal
                    ┌──────────▼──────────┐
                    │  Concrete SPI HAL   │
                    │  (STM32 CubeHAL)    │
                    └─────────────────────┘
```

---

## Usage Example

```cpp
#include "esf/SettingsRepository.hpp"
#include "esf/drivers/RamStorageDriver.hpp"

// Use RamStorageDriver for host tests; swap for FramStorageDriver on target
esf::drivers::RamStorageDriver<1024> driver;
esf::SettingsRepository repo{driver};

esf::Settings s{};
if (repo.load(s) != esf::StorageError::Ok) {
    repo.reset(); // write factory defaults
}

s.volumeLevel = 80;
repo.save(s);
```

---

## Adding a New Repository

1. Define your data struct (trivially copyable, no pointers, ordered large→small to avoid padding).
2. Create `MyRepository` extending `RepositoryBase<MyRepository, MyData, Ver>`.
3. Supply `storageAddress()` and `defaultValue()` static methods.
4. Optionally override `migrate()` for version upgrades.

---

## License

[MIT](LICENSE)

