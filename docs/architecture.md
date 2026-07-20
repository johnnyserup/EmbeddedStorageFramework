# EmbeddedStorageFramework — Architecture

## Overview

EmbeddedStorageFramework is a layered C++20 library that provides safe, versioned
persistence for embedded firmware.  It follows the **Repository pattern** and
**Dependency Injection** throughout so that:

- The application layer never knows about physical addresses, FRAM opcodes or SPI details.
- Every layer is independently testable on a host machine without hardware.
- Concrete storage backends can be swapped or stacked without touching application code.

---

## Layers

```
┌───────────────────────────────────────────────────────┐
│                  Application Layer                    │
│       (depends on IRepository<T> interfaces only)     │
└──────────────────────────┬────────────────────────────┘
                           │
┌──────────────────────────▼────────────────────────────┐
│              Repository Layer  (repositories/)        │
│   RepositoryBase<Derived,T,Ver>                       │
│   ExampleSettingsRepository  (example in examples/)  │
│                                                       │
│   Responsibilities:                                   │
│   • Wrap T in StorageObject<T,Ver> (adds header+CRC)  │
│   • Implement load / save / reset                     │
│   • Dispatch to a version-migration handler           │
└──────────────────────────┬────────────────────────────┘
                           │  IStorageDriver
              ┌────────────┴────────────┐
              │                         │
┌─────────────▼──────────┐  ┌──────────▼─────────────┐
│   RamStorageDriver<N>  │  │  FramStorageDriver<N>  │
│   (drivers/ram/)       │  │  (drivers/fram/)       │
│                         │  │                        │
│   Static array,         │  │  MB85RS FRAM opcodes   │
│   allocation-free       │  │  (WREN / READ / WRITE) │
│   host+target           │  │  Requires ISpiHal      │
└─────────────────────────┘  └──────────┬─────────────┘
                                         │  ISpiHal
                              ┌──────────▼─────────────┐
                              │  Concrete SPI HAL       │
                              │  (e.g. STM32 CubeHAL)  │
                              └─────────────────────────┘
```

---

## Key Types

### `IStorageDriver`  _(interfaces/)_

Low-level, byte-oriented hardware abstraction.

```cpp
bool read (uint32_t address, uint8_t* data, size_t length);
bool write(uint32_t address, const uint8_t* data, size_t length);
size_t capacity() const;
```

### `IRepository<T>`  _(interfaces/)_

Domain-level, type-safe persistence.

```cpp
StorageError load (T& out);
StorageError save (const T& value);
StorageError reset(); // writes factory defaults to the slot
```

### `StorageObject<T, Ver>`  _(core/)_

The unit of persistence.  Combines an `ObjectHeader` (magic, object id,
version, size, CRC-32) with the raw bytes of `T`.  The on-storage layout is:

```
[ ObjectHeader (16 bytes) ][ T (sizeof(T) bytes) ]
```

### `ObjectHeader`  _(core/)_

```
Offset  Size  Field
 0       2    magic     (0xCAFE — marks a written slot)
 2       2    objectId  (stable logical identifier for the repository slot)
 4       2    version   (incremented when T layout changes)
 6       2    reserved  (must be zero; reserved for future flags/alignment)
 8       4    dataSize  (payload byte count at write time)
12       4    crc       (CRC-32 of raw payload bytes)
```

### `Crc32`  _(core/)_

Table-free, allocation-free CRC-32 (ISO 3309 / Ethernet / zlib polynomial).
Both a byte-buffer overload and a typed template overload are provided.

### `RepositoryBase<Derived, T, Ver, ObjectId>`  _(repositories/)_

CRTP base that implements `load` / `save` / `reset` on top of any
`IStorageDriver`.  Derived classes supply only:

- `storageAddress()` — byte offset in the driver.
- `objectId` — a stable slot identifier encoded in the header.
- `defaultValue()` — factory-default T returned when no valid slot exists.
- Optionally `migrate(oldVersion, payloadBytes, payloadSize, out)` for version
  upgrades when an older payload layout must be mapped into the current `T`.

---

## Object Lifecycle

```
save(value)
    │
    ▼
StorageObject<T,Ver>::make(value)
    • header.magic    = 0xCAFE
    • header.objectId = ObjectId
    • header.version  = Ver
    • header.reserved = 0
    • header.dataSize = sizeof(T)
    • header.crc      = Crc32::compute(value)
    │
    ▼
driver.write(address, &obj, kTotalSize)

─────────────────────────────────────

load(out)
    │
    ▼
driver.read(address, &header, sizeof(ObjectHeader))
    │
   ├─ header invalid (wrong magic / wrong objectId / reserved != 0)?
    │       → out = defaultValue(); return InvalidMagic
    │
   ├─ payload out of bounds?
   │       → out = defaultValue(); return InvalidMagic
    │
    ├─ version != Ver?
   │       → driver.read(payload)
   │       → validate CRC over raw payload bytes
   │       → derived().migrate(oldVersion, payloadBytes, payloadSize, out)
   │
   ├─ dataSize != sizeof(T)?
   │       → out = defaultValue(); return InvalidMagic
   │
   ├─ driver.read(current T payload) fails?
   │       → return DriverError
   │
   ├─ CRC mismatch?
   │       → out = defaultValue(); return CrcMismatch
   │
   └─ all ok → out = obj.data; return Ok
```

---

## Adding a Storage Backend

1. Implement `IStorageDriver`.
2. Constructor-inject it into any `RepositoryBase` derived class.
3. No other changes are required.

For SPI FRAM devices implement `ISpiHal` and pass it to `FramStorageDriver`.

---

## Version Migration

When the layout of a persisted struct changes:

1. Increment the `Ver` template argument on the repository.
2. Override `migrate(oldVersion, payloadBytes, payloadSize, out)` in the
   derived repository.
3. Decode the old payload bytes according to `oldVersion`, map old fields to
   the new struct, then write the migrated version back with `save()`.

The migration hook intentionally receives raw payload bytes instead of
`StorageObject<T,Ver>` so that migration remains possible even when
`sizeof(T)` or field layout changes between versions.

Unhandled versions return `StorageError::VersionMismatch` and fall back to
factory defaults.

---

## Persistent Type Constraints

Persistent data types used with `RepositoryBase` and `StorageObject` should be:

- trivially copyable
- standard layout
- free of pointers and references
- free of dynamic containers
- free of virtual functions
- based on explicit fixed-width integer types where binary compatibility matters
- designed with explicit reserved/padding fields when long-term layout stability matters

Ordering fields from large to small may reduce compiler-inserted padding, but it
does not guarantee a stable binary layout across toolchains or revisions.
Whenever binary compatibility matters, add explicit reserved fields and back the
layout with `static_assert(sizeof(T) == expected)` checks.

---

## Power-Loss Behavior

The current bootstrap uses a single slot per repository. If power is lost
during a write, the slot may contain a torn header or torn payload. The first
version handles this by detecting invalid magic/object id or CRC mismatch and
falling back to defaults or migration behavior.

For settings or calibration data that require stronger persistence guarantees, a
future evolution should use a two-slot commit scheme with a generation counter
so that one previously valid image remains recoverable after an interrupted
write.

---

## Reset Semantics

`reset()` writes the repository's `defaultValue()` back to persistent storage.
It does not merely return defaults in RAM and it does not invalidate the slot.

---

## Storage Map / Non-Overlapping Regions

Each repository owns a fixed slot `[storageAddress(), storageAddress() + slotSize)`.
The consuming firmware project is responsible for ensuring those regions do not
overlap.

Recommended approaches:

- define per-repository `kSlotSize` constants and derive addresses from a shared map
- add compile-time `static_assert` checks in a central storage-map header
- evolve toward a dedicated slot-descriptor/storage-map facility if the project
  grows to many persistent objects

---

## Testing Strategy

| Test type | Location | Driver |
|-----------|----------|--------|
| Host unit tests | `tests/unit/` | `FakeStorageDriver<N>` |
| Host integration tests | _(add to tests/)_ | `RamStorageDriver<N>` |
| STM32 hardware tests | `stm32-testbench/` | `FramStorageDriver<N>` |

`FakeStorageDriver` extends `IStorageDriver` with:
- `setReadFail()` / `setWriteFail()` — fault injection.
- `readCount()` / `writeCount()` — call-count observability.
- `fill(byte)` — reset state between test cases.

---

## Boundary Rules

The framework enforces a strict separation between reusable infrastructure and
application-specific code.

| Location | Allowed content |
|----------|-----------------|
| `interfaces/` | Pure-abstract interfaces (`IStorageDriver`, `IRepository<T>`, `StorageError`) |
| `core/` | CRC, `ObjectHeader`, `StorageObject` — no application data types |
| `repositories/` | `RepositoryBase` CRTP only — no concrete data models |
| `drivers/ram/` | `RamStorageDriver` — static array, allocation-free |
| `drivers/fram/` | `FramStorageDriver`, `ISpiHal` — FRAM/SPI only, no application logic |
| `examples/` | Example data types (e.g. `ExampleSettings`) and example repositories |
| `tests/` | Host-only test code; `FakeStorageDriver` lives here |

**Application data models** (real settings structs, calibration data, logs, etc.)
belong in the **consuming firmware project**, not in this framework.

---

## Using as a Git Submodule

### Adding to a firmware project

```bash
# Add the framework as a submodule
git submodule add https://github.com/johnnyserup/EmbeddedStorageFramework.git extern/esf
git submodule update --init --recursive
```

### CMake integration

In your firmware project's `CMakeLists.txt`:

```cmake
# Add the framework (examples and tests are excluded when cross-compiling)
add_subdirectory(extern/esf)

# Link only the layers you need
target_link_libraries(my_firmware PRIVATE
    esf::repositories   # RepositoryBase + IRepository<T>
    esf::driver_fram    # FramStorageDriver + ISpiHal
)
```

Available CMake targets:

| Target | Contents |
|--------|----------|
| `esf::interfaces` | `IStorageDriver`, `IRepository<T>`, `StorageError` |
| `esf::core` | `Crc32`, `ObjectHeader`, `StorageObject<T,Ver>` |
| `esf::repositories` | `RepositoryBase<Derived,T,Ver>` |
| `esf::driver_ram` | `RamStorageDriver<N>` |
| `esf::driver_fram` | `FramStorageDriver<N>`, `ISpiHal` |
| `esf::examples` | `ExampleSettings`, `ExampleSettingsRepository` (host/test only) |
| `esf::fake` | `FakeStorageDriver<N>` (host/test only) |

### What to implement in the consuming project

1. **SPI HAL adapter** — implement `ISpiHal` to bridge `FramStorageDriver` to
   your MCU's SPI peripheral (e.g. STM32 HAL `HAL_SPI_TransmitReceive`).

2. **Application data types** — define your own trivially-copyable structs
   (settings, calibration data, log entries, etc.).

3. **Concrete repositories** — derive from
   `RepositoryBase<MyRepo, MyData, Ver, ObjectId>`, supply `storageAddress()`
   and `defaultValue()`, and optionally override `migrate()`.

4. **Driver wiring** — construct a `FramStorageDriver` (or `RamStorageDriver`
   for tests), inject it into your repository, and inject the repository
   into your application layer via `IRepository<T>`.

### Minimal integration example

```cpp
// In your firmware project — NOT in the framework

#include "esf/RepositoryBase.hpp"
#include "esf/drivers/FramStorageDriver.hpp"
#include "MySpiBridge.hpp"   // your ISpiHal implementation

// 1. Define your application data type
struct MySettings {
    uint32_t serialNumber = 0u;
    uint8_t  channel      = 1u;
    uint8_t  _reserved[3] = {};
};

// 2. Define a concrete repository
class MySettingsRepository
    : public esf::RepositoryBase<MySettingsRepository, MySettings, 1u, 0x2001u>
{
public:
    static constexpr uint32_t  kAddress = 0u;
    static constexpr uint32_t  kSlotSize =
        esf::StorageObject<MySettings, 1u, 0x2001u>::kTotalSize;
    static constexpr MySettings kDefault{};

    explicit MySettingsRepository(esf::IStorageDriver& drv) noexcept
        : RepositoryBase(drv) {}

    static constexpr uint32_t   storageAddress() noexcept { return kAddress; }
    static constexpr MySettings defaultValue()   noexcept { return kDefault; }
};

// 3. Wire it all together
MySpiBridge            hal;
esf::drivers::FramStorageDriver<8192> driver{hal};
MySettingsRepository   repo{driver};

MySettings s{};
if (repo.load(s) != esf::StorageError::Ok) {
    repo.reset();
}
```



| Decision | Rationale |
|----------|-----------|
| Header-only templates for drivers | Zero linker friction when targeting different devices |
| CRTP for RepositoryBase | Avoids virtual dispatch overhead on tight embedded loops |
| CRC-32 without lookup table | Saves ~1 KiB of flash; deterministic on all targets |
| `uint32_t` addresses | Consistent with STM32 address space; avoids `size_t` width differences between host and target |
| `bool` return from driver | Simplest non-throwing error propagation compatible with bare-metal |
| Fields ordered large→small in structs | May reduce compiler-inserted padding, but explicit reserved fields and `static_assert` checks are still required when layout stability matters |
