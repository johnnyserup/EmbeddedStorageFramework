# ---------------------------------------------------------------------------
# esf::driver_ram — in-memory storage driver (header-only)
# ---------------------------------------------------------------------------
add_library(esf_driver_ram INTERFACE)
add_library(esf::driver_ram ALIAS esf_driver_ram)

target_include_directories(esf_driver_ram INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(esf_driver_ram INTERFACE esf::interfaces)

target_compile_features(esf_driver_ram INTERFACE cxx_std_20)
