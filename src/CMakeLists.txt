add_executable(modplay65
    main.c
    hw/audio_dma.c
    hw/screen.c
    mod/mod_file.c
    mod/mod_load.c)

target_include_directories(modplay65 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(modplay65 PRIVATE -Os -Wall -Wextra -Wshadow)
target_compile_definitions(modplay65 PRIVATE
    MODPLAY65_VERSION="${PROJECT_VERSION}")
target_link_libraries(modplay65 PRIVATE mega65libc)

# The linker script already produces a loadable PRG with a BASIC SYS line.
set_target_properties(modplay65 PROPERTIES
    OUTPUT_NAME modplay65.prg
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
