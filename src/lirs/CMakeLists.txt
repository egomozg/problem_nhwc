add_library(lirs INTERFACE)
target_include_directories(lirs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(lirs INTERFACE cxx_std_20)

add_executable(lirs_cli ${PROJECT_SOURCE_DIR}/main.cpp)
target_link_libraries(lirs_cli PRIVATE lirs)
target_compile_definitions(lirs_cli PRIVATE CACHE_HEADER="lirs.hpp")

if(BUILD_TESTING)
    add_executable(lirs_tests tests/lirs_tests.cpp)
    target_link_libraries(lirs_tests PRIVATE lirs Catch2::Catch2WithMain)
    catch_discover_tests(lirs_tests)
endif()
