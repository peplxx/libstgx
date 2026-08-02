add_subdirectory(catch2)

file(GLOB_RECURSE stgx_test_sources
  CONFIGURE_DEPENDS
  *.cpp
)

add_executable(stgx_tests ${stgx_test_sources})

target_link_libraries(stgx_tests
  PRIVATE
    stgx::stgx
    Catch2::Catch2
)

target_compile_definitions(stgx_tests
  PRIVATE
    STGX_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

include(Catch OPTIONAL RESULT_VARIABLE STGX_CATCH_MODULE)
if(STGX_CATCH_MODULE)
  catch_discover_tests(stgx_tests)
else()
  add_test(NAME stgx_tests COMMAND stgx_tests)
endif()
