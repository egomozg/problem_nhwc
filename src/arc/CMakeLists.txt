add_library(arc INTERFACE)
target_include_directories(arc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(arc INTERFACE cxx_std_20)

add_executable(arc_cli ${PROJECT_SOURCE_DIR}/main.cpp)
target_link_libraries(arc_cli PRIVATE arc)
target_compile_definitions(arc_cli PRIVATE CACHE_HEADER="arc.hpp")

if(BUILD_TESTING)
    add_executable(arc_tests tests/arc_tests.cpp)
    target_link_libraries(arc_tests PRIVATE arc Catch2::Catch2WithMain)
    catch_discover_tests(arc_tests)
endif()
