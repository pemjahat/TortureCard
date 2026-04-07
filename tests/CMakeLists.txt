# ---------------------------------------------------------------------------
# Tests: ptcgp_tests
# ---------------------------------------------------------------------------
include(FetchContent)

# Fix MSVC runtime linkage before making GTest available
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES "*.cpp")

add_executable(ptcgp_tests ${TEST_SOURCES})

target_link_libraries(ptcgp_tests PRIVATE
    ptcgp_sim_lib
    GTest::gtest_main
)

target_include_directories(ptcgp_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(ptcgp_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

include(GoogleTest)
gtest_discover_tests(ptcgp_tests)
