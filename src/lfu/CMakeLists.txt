add_library(lfu INTERFACE)
target_include_directories(lfu INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(lfu INTERFACE cxx_std_20)

add_executable(lfu_cli ${PROJECT_SOURCE_DIR}/main.cpp)
target_link_libraries(lfu_cli PRIVATE lfu)
target_compile_definitions(lfu_cli PRIVATE CACHE_HEADER="lfu.hpp")

if(BUILD_TESTING)
  add_executable(lfu_tests tests/lfu_tests.cpp)
  target_link_libraries(lfu_tests PRIVATE lfu Catch2::Catch2WithMain)
  catch_discover_tests(lfu_tests)
endif()
