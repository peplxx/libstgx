cmake_minimum_required(VERSION 3.20)

add_executable(stgx_gfp_example gfp/main.cpp)

target_link_libraries(stgx_gfp_example
  PRIVATE
    stgx::stgx
)

target_compile_options(stgx_gfp_example PRIVATE -Wall -Wextra -Wpedantic)

add_executable(stgx_dominants_example dominants/main.cpp)

target_link_libraries(stgx_dominants_example
  PRIVATE
    stgx::stgx
)

target_compile_options(stgx_dominants_example PRIVATE -Wall -Wextra -Wpedantic)
