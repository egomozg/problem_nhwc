add_library(lru INTERFACE)
target_include_directories(lru INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(lru INTERFACE cxx_std_20)

add_executable(lru_cli ${PROJECT_SOURCE_DIR}/main.cpp)
target_link_libraries(lru_cli PRIVATE lru)
target_compile_definitions(lru_cli PRIVATE CACHE_HEADER="lru.hpp")

if(BUILD_TESTING)
  add_executable(lru_tests tests/lru_tests.cpp)
  target_link_libraries(lru_tests PRIVATE lru Catch2::Catch2WithMain)
  catch_discover_tests(lru_tests)
endif()
