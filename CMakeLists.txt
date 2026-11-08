cmake_minimum_required(VERSION 3.16)
project(ProjectDivaRecomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The generated code relies on MIPS arithmetic: wrapping integers and memory
# accessed through any type. A trap for undefined behaviour would stop it.
add_compile_options(-fno-sanitize=undefined -fwrapv -fno-strict-aliasing)

set(PSPRECOMP_REAL_THREADS ON CACHE BOOL "The game needs real guest threads" FORCE)
add_subdirectory(third_party/psprecomp)

# Written by Tools/build.ps1. It is derived from the game, so it stays out of the
# repository. On the first configure it does not exist yet: only the recompiler
# gets built, which is what generates it.
set(DIVA_GEN_DIR "${CMAKE_BINARY_DIR}/generated" CACHE PATH "Generated C")
file(GLOB DIVA_GEN CONFIGURE_DEPENDS "${DIVA_GEN_DIR}/*.c")

if(DIVA_GEN)
    add_executable(diva src/host/main.c src/host/audio_out.c src/host/mods.c src/host/settings.c src/host/watch.c src/host/lifesearch.c src/host/controls.c src/host/options.c src/host/language_screen.c src/host/screen_kit.c src/host/translation.c src/host/letters.c src/host/hooks.c src/host/extras.c src/host/language.c src/host/timing.c src/host/screenshot.c src/host/presence.c ${DIVA_GEN})
    target_include_directories(diva PRIVATE "${DIVA_GEN_DIR}")
    target_link_libraries(diva PRIVATE psprecomp winmm gdi32 user32)
    set_source_files_properties(${DIVA_GEN} PROPERTIES
        COMPILE_OPTIONS "-O2;-w"
        COMPILE_DEFINITIONS PSPRECOMP_FAST_MEMORY)
    # Guest calls nest as host calls, so the boot thread needs the stack that
    # guest threads get.
    target_link_options(diva PRIVATE -Wl,--stack,67108864)
endif()
