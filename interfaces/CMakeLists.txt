# ---------------------------------------------------------------------------
# esf::interfaces — pure-abstract interface headers (header-only library)
# ---------------------------------------------------------------------------
add_library(esf_interfaces INTERFACE)
add_library(esf::interfaces ALIAS esf_interfaces)

target_include_directories(esf_interfaces INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(esf_interfaces INTERFACE cxx_std_20)
