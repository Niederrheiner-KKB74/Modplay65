# Host-side unit tests for the portable code. This is a separate CMake
# project built with the normal PC compiler:
#   cmake -S tests -B build-tests && cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure
cmake_minimum_required(VERSION 3.18)
project(modplay65_tests LANGUAGES C)
enable_testing()

add_executable(test_mod_load
    test_mod_load.c
    ../src/mod/mod_load.c)
target_include_directories(test_mod_load PRIVATE ../src)
target_compile_options(test_mod_load PRIVATE -Wall -Wextra -Wshadow)
add_test(NAME mod_load COMMAND test_mod_load)
