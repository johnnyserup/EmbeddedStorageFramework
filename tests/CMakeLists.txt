# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# Prevent GoogleTest from overriding our compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
# Fake storage driver (test support library)
# ---------------------------------------------------------------------------
add_library(esf_fake INTERFACE)
add_library(esf::fake ALIAS esf_fake)

target_include_directories(esf_fake INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/fake/include
)
target_link_libraries(esf_fake INTERFACE esf::interfaces)
target_compile_features(esf_fake INTERFACE cxx_std_20)

# ---------------------------------------------------------------------------
# Unit test executable
# ---------------------------------------------------------------------------
add_executable(esf_tests
    unit/test_Crc32.cpp
    unit/test_StorageObject.cpp
    unit/test_FakeStorageDriver.cpp
    unit/test_SettingsRepository.cpp
)

target_link_libraries(esf_tests PRIVATE
    esf::core
    esf::repositories
    esf::fake
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(esf_tests)
