add_executable(modplay65
    main.c
    hw/audio_dma.c
    hw/fileload.c
    hw/screen.c
    hw/system.c
    hw/timebase.c
    mod/mod_file.c
    mod/mod_load.c
    mod/mod_out.c
    mod/mod_play.c
    mod/mod_tables.c
    sid/psid.c
    sid/sid_play.c
    ui/dir.c
    vis/pattern.c
    vis/scope.c)

target_include_directories(modplay65 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(modplay65 PRIVATE -Oz -Wall -Wextra -Wshadow)
# The SDK links with LTO; the link step needs the size setting as well.
target_link_options(modplay65 PRIVATE -Oz)
# Code that runs on every tick or every video frame is optimised for speed
# as well as size; everything else only for size.
set_source_files_properties(
    hw/audio_dma.c
    hw/screen.c
    hw/timebase.c
    mod/mod_out.c
    mod/mod_play.c
    vis/scope.c
    PROPERTIES COMPILE_OPTIONS "-Os")
target_compile_definitions(modplay65 PRIVATE
    MODPLAY65_VERSION="${PROJECT_VERSION}")
target_link_libraries(modplay65 PRIVATE mega65libc)
# Own memory layout: keeps $B600-$CFFF free for the screen's glyph block.
target_link_options(modplay65 PRIVATE
    -T ${CMAKE_CURRENT_SOURCE_DIR}/modplay65.ld)
set_target_properties(modplay65 PROPERTIES
    LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/modplay65.ld)

# The linker script already produces a loadable PRG with a BASIC SYS line.
set_target_properties(modplay65 PROPERTIES
    OUTPUT_NAME modplay65.prg
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
