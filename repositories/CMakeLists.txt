# ---------------------------------------------------------------------------
# esf::repositories — RepositoryBase CRTP base (header-only)
# ---------------------------------------------------------------------------
add_library(esf_repositories INTERFACE)
add_library(esf::repositories ALIAS esf_repositories)

target_include_directories(esf_repositories INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(esf_repositories INTERFACE esf::core)

target_compile_features(esf_repositories INTERFACE cxx_std_20)
