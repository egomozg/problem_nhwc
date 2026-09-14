cmake_minimum_required(VERSION 3.16)
project(caches LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

# оставляем executables в build/${config}/.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(CTest)
if(BUILD_TESTING)
  find_package(Catch2 3 QUIET)
  if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG v3.16.0
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  endif()

  include(Catch)
endif()

add_subdirectory(src/lru)
add_subdirectory(src/two_q)
