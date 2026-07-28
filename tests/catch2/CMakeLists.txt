find_package(Catch2 3 CONFIG QUIET)

if(Catch2_FOUND AND TARGET Catch2::Catch2)
  message(STATUS "Using system Catch2 ${Catch2_VERSION}")
  set_target_properties(Catch2::Catch2 PROPERTIES IMPORTED_GLOBAL TRUE)
  if(DEFINED Catch2_DIR)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
  endif()
else()
  message(STATUS "System Catch2 not found; fetching v3.15.2")
  include(FetchContent)

  set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.15.2
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(Catch2)

  list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
