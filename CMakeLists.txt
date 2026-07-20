cmake_minimum_required(VERSION 3.20)

project(EmbeddedStorageFramework
    VERSION     0.1.0
    DESCRIPTION "Reusable embedded persistence framework for STM32 systems"
    LANGUAGES   CXX
)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDEs / static analysers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Compiler warnings (host builds only — cross-compiler flags differ)
# ---------------------------------------------------------------------------
if(NOT CMAKE_CROSSCOMPILING)
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Woverloaded-virtual
        -Wnull-dereference
    )
endif()

# ---------------------------------------------------------------------------
# Sub-components
# ---------------------------------------------------------------------------
add_subdirectory(interfaces)
add_subdirectory(core)
add_subdirectory(repositories)
add_subdirectory(drivers/ram)
add_subdirectory(drivers/fram)

# ---------------------------------------------------------------------------
# Tests (host-only)
# ---------------------------------------------------------------------------
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(tests)
endif()
