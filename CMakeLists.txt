cmake_minimum_required(VERSION 3.18)

# Must be set before find_package: selects the llvm-mos MEGA65 target.
set(LLVM_MOS_PLATFORM mega65)
find_package(llvm-mos-sdk REQUIRED)

project(modplay65 VERSION 0.1.0 LANGUAGES C ASM)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. Use: cmake -B build")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# mega65-libc, pinned to a known commit of its development branch.
# SOURCE_SUBDIR points at a directory that does not exist so that
# FetchContent only downloads; the library is then added EXCLUDE_FROM_ALL
# so its tests and docs are not built with this project.
include(FetchContent)
FetchContent_Declare(mega65libc
    GIT_REPOSITORY https://github.com/MEGA65/mega65-libc.git
    GIT_TAG 49f2f115d3d45da9475f00a34250587df472fe3a
    SOURCE_SUBDIR download-only)
FetchContent_MakeAvailable(mega65libc)
add_subdirectory(${mega65libc_SOURCE_DIR} ${mega65libc_BINARY_DIR}
    EXCLUDE_FROM_ALL)

add_subdirectory(src)
