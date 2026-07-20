# EmbeddedStorageFramework

Reusable C++20 embedded persistence framework for STM32 systems with FRAM persistence, repository pattern, versioned objects, migration support and hardware-independent unit testing.

---

## Features

| Feature | Description |
|---------|-------------|
| **Hardware-independent core** | Core interfaces compile on any host — no STM32 headers required |
| **Repository pattern** | Application code uses typed domain repositories, never raw addresses |
| **Versioned objects** | Every stored slot carries magic, object id, version, length and CRC metadata for safe migration and corruption detection |
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
├── core/                CRC-32, ObjectHeader, StorageObject<T,Ver,ObjectId>
├── repositories/        RepositoryBase (CRTP)
├── drivers/
│   ├── ram/             RamStorageDriver — static array, allocation-free
│   └── fram/            FramStorageDriver — MB85RS SPI FRAM skeleton + ISpiHal
├── examples/            Example data types and repositories (host builds only)
├── tests/
│   ├── fake/            FakeStorageDriver with fault injection and call counters
│   └── unit/            Google Test unit tests
├── docs/                Architecture documentation
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
# Add the framework to your firmware project
git submodule add https://github.com/johnnyserup/EmbeddedStorageFramework.git extern/esf
git submodule update --init --recursive
```

In your `CMakeLists.txt`:

```cmake
add_subdirectory(extern/esf)

# Link only the layers your firmware needs
target_link_libraries(my_firmware PRIVATE
    esf::repositories   # RepositoryBase + IRepository<T>
    esf::driver_fram    # FramStorageDriver + ISpiHal
)
```

The `examples` and `tests` subdirectories are excluded automatically when
`CMAKE_CROSSCOMPILING` is set (i.e. when building for an embedded target).

See [docs/architecture.md](docs/architecture.md) for a detailed integration guide.

---

## Architecture Overview

See [docs/architecture.md](docs/architecture.md) for a full description.

```
┌────────────────────────────────────────┐
│           Application Layer            │
│   (uses IRepository<T> interfaces)     │
└──────────────────┬─────────────────────┘
                   │ IRepository<T>
┌──────────────────▼─────────────────────┐
│         RepositoryBase (CRTP)          │
│  load() → read header → validate → data/migrate │
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

The `examples/` directory contains `ExampleSettingsRepository` which shows
how to derive a concrete repository from `RepositoryBase`.  In your own
firmware project define your application data types and repositories there,
not inside the framework.

```cpp
// ── In your firmware project ─────────────────────────────────────────────
#include "esf/RepositoryBase.hpp"
#include "esf/drivers/RamStorageDriver.hpp"

// 1. Your application data type (lives in your project, not in the framework)
struct MySettings {
    uint32_t serialNumber = 0u;
    uint8_t  channel      = 1u;
    uint8_t  _reserved[3] = {};
};

// 2. Your concrete repository
class MySettingsRepository
    : public esf::RepositoryBase<MySettingsRepository, MySettings, 1u, 0x2001u>
{
public:
    static constexpr uint32_t   kAddress = 0u;
    static constexpr uint32_t   kSlotSize =
        esf::StorageObject<MySettings, 1u, 0x2001u>::kTotalSize;
    static constexpr MySettings kDefault{};

    explicit MySettingsRepository(esf::IStorageDriver& drv) noexcept
        : RepositoryBase(drv) {}

    static constexpr uint32_t   storageAddress() noexcept { return kAddress; }
    static constexpr MySettings defaultValue()   noexcept { return kDefault; }
};

// 3. Wire up and use
esf::drivers::RamStorageDriver<1024> driver;  // swap for FramStorageDriver on target
MySettingsRepository repo{driver};

MySettings s{};
if (repo.load(s) != esf::StorageError::Ok) {
    repo.reset(); // write factory defaults
}
s.channel = 3;
repo.save(s);
```

---

## Adding a New Repository

1. Define your data struct (trivially copyable, standard layout, no pointers/references/dynamic containers, explicit reserved bytes when layout stability matters).
2. Create `MyRepository` extending
   `RepositoryBase<MyRepository, MyData, Ver, ObjectId>`.
3. Supply `storageAddress()` and `defaultValue()` static methods.
4. Optionally override `migrate(oldVersion, payloadBytes, payloadSize, out)`
   for version upgrades. The hook receives CRC-validated raw payload bytes
   after `load()` has read and validated the `ObjectHeader`.

---

## License

[MIT](LICENSE)
