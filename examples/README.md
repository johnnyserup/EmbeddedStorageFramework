# Examples

This directory contains self-contained usage examples for EmbeddedStorageFramework.

## Contents

- `include/esf/examples/ExampleSettingsRepository.hpp` — `ExampleSettings` data
  type and `ExampleSettingsRepository` concrete repository.  These demonstrate
  how to derive from `RepositoryBase` and are used by the framework's own unit
  tests.

> **Important:** These are framework examples only.  Do **not** add real
> application data models here.  In your firmware project define your own
> settings structs and repositories alongside your application code, outside
> of this submodule.

## Planned examples

- `basic_usage/` — standalone CMake project: save and load with `RamStorageDriver`
- `migration/` — implementing a version migration step
