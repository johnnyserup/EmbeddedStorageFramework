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
│   SettingsRepository  (example)                       │
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
StorageError reset();
```

### `StorageObject<T, Ver>`  _(core/)_

The unit of persistence.  Combines an `ObjectHeader` (magic, version, size, CRC-32)
with the raw bytes of `T`.  The on-storage layout is:

```
[ ObjectHeader (12 bytes) ][ T (sizeof(T) bytes) ]
```

### `ObjectHeader`  _(core/)_

```
Offset  Size  Field
 0       2    magic     (0xCAFE — marks a written slot)
 2       2    version   (incremented when T layout changes)
 4       4    dataSize  (sizeof(T) at write time)
 8       4    crc       (CRC-32 of raw T bytes)
```

### `Crc32`  _(core/)_

Table-free, allocation-free CRC-32 (ISO 3309 / Ethernet / zlib polynomial).
Both a byte-buffer overload and a typed template overload are provided.

### `RepositoryBase<Derived, T, Ver>`  _(repositories/)_

CRTP base that implements `load` / `save` / `reset` on top of any
`IStorageDriver`.  Derived classes supply only:

- `storageAddress()` — byte offset in the driver.
- `defaultValue()` — factory-default T returned when no valid slot exists.
- Optionally `migrate(obj, out)` for version upgrades.

---

## Object Lifecycle

```
save(value)
    │
    ▼
StorageObject<T,Ver>::make(value)
    • header.magic    = 0xCAFE
    • header.version  = Ver
    • header.dataSize = sizeof(T)
    • header.crc      = Crc32::compute(value)
    │
    ▼
driver.write(address, &obj, kTotalSize)

─────────────────────────────────────

load(out)
    │
    ▼
driver.read(address, &obj, kTotalSize)
    │
    ├─ header invalid (wrong magic / wrong size)?
    │       → out = defaultValue(); return InvalidMagic
    │
    ├─ CRC mismatch?
    │       → out = defaultValue(); return CrcMismatch
    │
    ├─ version != Ver?
    │       → derived().migrate(obj, out); return VersionMismatch
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
2. Override `migrate(const Object& oldObj, T& out)` in the derived repository.
3. Map old fields to the new struct; write the new version back with `save()`.

Unhandled versions return `StorageError::VersionMismatch` and fall back to
factory defaults.

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

## Design Decisions

| Decision | Rationale |
|----------|-----------|
| Header-only templates for drivers | Zero linker friction when targeting different devices |
| CRTP for RepositoryBase | Avoids virtual dispatch overhead on tight embedded loops |
| CRC-32 without lookup table | Saves ~1 KiB of flash; deterministic on all targets |
| `uint32_t` addresses | Consistent with STM32 address space; avoids `size_t` width differences between host and target |
| `bool` return from driver | Simplest non-throwing error propagation compatible with bare-metal |
| Fields ordered large→small in structs | Eliminates compiler-inserted padding without `#pragma pack` |
