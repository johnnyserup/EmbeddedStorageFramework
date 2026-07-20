# ---------------------------------------------------------------------------
# esf::driver_fram — SPI FRAM driver (header-only, requires ISpiHal injection)
# ---------------------------------------------------------------------------
add_library(esf_driver_fram INTERFACE)
add_library(esf::driver_fram ALIAS esf_driver_fram)

target_include_directories(esf_driver_fram INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(esf_driver_fram INTERFACE esf::interfaces)

target_compile_features(esf_driver_fram INTERFACE cxx_std_20)
