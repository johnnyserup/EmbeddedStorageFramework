# ---------------------------------------------------------------------------
# esf::examples — Example data types and repositories (header-only)
#
# This target is included in host builds so that the example-based unit tests
# can link against it.  It must NOT be a dependency of any core framework
# library (interfaces, core, repositories, drivers).
# ---------------------------------------------------------------------------
add_library(esf_examples INTERFACE)
add_library(esf::examples ALIAS esf_examples)

target_include_directories(esf_examples INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(esf_examples INTERFACE esf::repositories)

target_compile_features(esf_examples INTERFACE cxx_std_20)
