# ---------------------------------------------------------------------------
# esf::core — CRC, StorageObject, ObjectHeader
# ---------------------------------------------------------------------------
add_library(esf_core STATIC
    src/Crc32.cpp
)
add_library(esf::core ALIAS esf_core)

target_include_directories(esf_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(esf_core PUBLIC esf::interfaces)

target_compile_features(esf_core PUBLIC cxx_std_20)
