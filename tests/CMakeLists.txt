add_subdirectory(catch2)

file(GLOB_RECURSE stgx_test_sources
  CONFIGURE_DEPENDS
  *.cpp
)

add_executable(stgx_tests ${stgx_test_sources})

target_link_libraries(stgx_tests
  PRIVATE
    stgx::stgx
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(stgx_tests)
