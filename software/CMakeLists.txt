cmake_minimum_required(VERSION 3.10)

project(SC1000BUILD VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(NATIVE "Build for native host instead of ARM" OFF)

# Add DEBUG define for Debug builds (enables debug() macro in src/util/debug.h)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
endif()

# Source files organized by directory
set(CORE_SOURCES
        src/core/sc1000.cpp
        src/core/sc_settings.cpp
        src/core/sc_input.cpp
        src/core/global.cpp
)

set(CONTROL_SOURCES
        src/control/actions.cpp
        src/control/mapping_registry.cpp
)

set(ENGINE_SOURCES
        src/engine/audio_engine.cpp
        src/engine/cv_engine.cpp
        src/engine/loop_buffer.cpp
)

set(PLATFORM_SOURCES
        src/platform/alsa.cpp
        src/platform/crossfader.cpp
        src/platform/midi.cpp
        src/platform/gpio.cpp
        src/platform/i2c.cpp
        src/platform/encoder.cpp
        src/platform/pic.cpp
        src/platform/sc_hardware.cpp
)

set(INPUT_SOURCES
        src/input/controller.cpp
        src/input/midi_controller.cpp
        src/input/midi_event.cpp
        src/input/midi_input.cpp
)

set(PLAYER_SOURCES
        src/player/cues.cpp
        src/player/deck.cpp
        src/player/player.cpp
        src/player/playlist.cpp
        src/player/track.cpp
)

set(THREAD_SOURCES
        src/thread/realtime.cpp
        src/thread/rig.cpp
        src/thread/thread.cpp
)

set(UTIL_SOURCES
        src/util/external.cpp
        src/util/log.cpp
        src/util/status.cpp
)

add_executable(sc1000
        src/main.cpp
        ${CORE_SOURCES}
        ${CONTROL_SOURCES}
        ${ENGINE_SOURCES}
        ${PLATFORM_SOURCES}
        ${INPUT_SOURCES}
        ${PLAYER_SOURCES}
        ${THREAD_SOURCES}
        ${UTIL_SOURCES}
)

# Release build optimization (CMake default is -O3, but be explicit)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# Compiler flags
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Building for ARM (SC1000 device) via toolchain")
    # ARM flags are set by toolchain file, just add optimization flags
    target_compile_options(sc1000 PRIVATE -flto -ftree-vectorize)
elseif(NATIVE)
    message(STATUS "Building for NATIVE host")
    target_compile_options(sc1000 PRIVATE -flto -ftree-vectorize)
else()
    message(STATUS "Building for ARM (SC1000 device) - direct")
    target_compile_options(sc1000 PRIVATE -mcpu=cortex-a8 -mfpu=neon -flto -ftree-vectorize)
endif()

# Common warning flags
target_compile_options(sc1000 PRIVATE
        -Wall
        -Wextra
        -Wconversion
        -Wno-unused-parameter
)

# Include directories
target_include_directories(sc1000 PRIVATE
        deps
        src
)

# Link libraries
# Static linking for C/C++ runtime, dynamic for ALSA (needs dlopen for plugins)
# Using set_target_properties for CMake 3.10 compatibility (target_link_options requires 3.13)
set_target_properties(sc1000 PROPERTIES
        LINK_FLAGS "-flto -static-libstdc++ -static-libgcc"
)

target_link_libraries(sc1000
        asound
        m
        pthread
)

# Desktop test application (only for native builds)
if(NATIVE)
    option(BUILD_DESKTOP_TEST "Build desktop test application" OFF)

    if(BUILD_DESKTOP_TEST)
        add_executable(sc1000-desktop
                src/desktop/main_desktop.cpp
                src/desktop/platform_desktop.cpp
        )

        target_compile_options(sc1000-desktop PRIVATE
                -Wall
                -Wextra
                -Wno-unused-parameter
        )

        target_include_directories(sc1000-desktop PRIVATE
                deps
                src
        )

        target_compile_definitions(sc1000-desktop PRIVATE SC1000_DESKTOP)

        message(STATUS "Building desktop test application")
    endif()
endif()

# Resampler quality test (header-only, no dependencies)
option(BUILD_RESAMPLER_TEST "Build resampler quality test" OFF)

if(BUILD_RESAMPLER_TEST)
    add_executable(resampler-test
            tools/resampler_test.cpp
    )

    target_compile_options(resampler-test PRIVATE
            -Wall
            -Wextra
            -O2
    )

    target_include_directories(resampler-test PRIVATE
            src
    )

    message(STATUS "Building resampler quality test")
endif()

# Audio engine test harness
option(BUILD_TESTS "Build audio engine tests" OFF)

if(BUILD_TESTS)
    set(TEST_SOURCES
            test/src/test_main.cpp
            test/src/test_harness.cpp
            test/src/test_audio_backend.cpp
            test/src/test_samples.cpp
            test/src/input_sequence.cpp
            test/src/test_stubs.cpp
    )

    # Subset of sources needed for testing (no hardware dependencies)
    set(TEST_CORE_SOURCES
            src/core/sc1000.cpp
            src/core/sc_settings.cpp
            src/core/global.cpp
            src/control/actions.cpp
            src/control/mapping_registry.cpp
            src/engine/audio_engine.cpp
            src/engine/cv_engine.cpp
            src/engine/loop_buffer.cpp
            src/player/cues.cpp
            src/player/deck.cpp
            src/player/player.cpp
            src/player/playlist.cpp
            src/player/track.cpp
            src/input/midi_event.cpp
            src/util/log.cpp
    )

    add_executable(sc1000-test
            ${TEST_SOURCES}
            ${TEST_CORE_SOURCES}
    )

    target_compile_options(sc1000-test PRIVATE
            -Wall
            -Wextra
            -Wno-unused-parameter
            -O2
    )

    target_compile_definitions(sc1000-test PRIVATE
            SC1000_TEST_MODE
    )

    target_include_directories(sc1000-test PRIVATE
            deps
            src
            test/src
    )

    target_link_libraries(sc1000-test
            m
            pthread
    )

    message(STATUS "Building audio engine tests")
endif()
