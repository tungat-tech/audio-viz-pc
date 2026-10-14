cmake_minimum_required(VERSION 3.21)
project(AudioViz_PC VERSION 0.1 LANGUAGES C CXX)

# Some pinned dependencies declare policy versions older than CMake 4 accepts.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0" AND NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
endif()

option(BUILD_SIMULATOR "Build the LVGL/SDL application" ON)
option(BUILD_PORTABLE_DEMO "Build the simulator without live capture (use --demo)" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
add_library(audio_dsp STATIC
    audio/Fft512.cpp
    audio/AudioProcessor.cpp)
target_include_directories(audio_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(audio_dsp PUBLIC Threads::Threads)

include(CTest)
if(BUILD_TESTING)
    add_executable(pc_processor_test tests/pc_processor_test.cpp)
    target_link_libraries(pc_processor_test PRIVATE audio_dsp)
    add_test(NAME pc_processor_test COMMAND pc_processor_test)
endif()

# This explicit test-only path needs neither a GUI toolchain nor downloads.
if(NOT BUILD_SIMULATOR)
    return()
endif()

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Use downloaded dependency revisions" FORCE)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_CCACHE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.12
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v8.4.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(lvgl)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_include_directories(lvgl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/config)
if(TARGET lvgl_examples)
    set_target_properties(lvgl_examples PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
if(TARGET lvgl_demos)
    set_target_properties(lvgl_demos PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

if(BUILD_PORTABLE_DEMO)
    # Explicit test/demo adapter: never pretends to capture system audio.
    add_library(audio_capture STATIC audio/AudioCaptureDemo.cpp)
    target_include_directories(audio_capture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(audio_capture PUBLIC audio_dsp)
else()
    FetchContent_Declare(miniaudio
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG 0.11.22
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR _header_only)
    FetchContent_MakeAvailable(miniaudio)

    add_library(audio_capture STATIC
        audio/AudioCapture.cpp
        audio/miniaudio_impl.c)
    target_include_directories(audio_capture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${miniaudio_SOURCE_DIR})
    target_link_libraries(audio_capture PUBLIC audio_dsp)
    target_compile_definitions(audio_capture PRIVATE MA_NO_DECODING MA_NO_ENCODING MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH)
endif()

add_library(simulator STATIC simulator/Simulator.cpp)
target_include_directories(simulator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(simulator PUBLIC lvgl SDL2::SDL2-static)

add_library(visualizer_ui STATIC ui/VisualizerUi.cpp)
target_include_directories(visualizer_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(visualizer_ui PUBLIC lvgl audio_dsp)

add_executable(AudioViz_PC main.cpp)
target_link_libraries(AudioViz_PC PRIVATE
    audio_capture visualizer_ui simulator)
if(MINGW)
    # Ship one viewer executable instead of depending on MinGW runtime DLLs.
    target_link_options(AudioViz_PC PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(BUILD_TESTING AND BUILD_PORTABLE_DEMO)
    add_executable(audio_capture_demo_test tests/audio_capture_demo_test.cpp)
    target_link_libraries(audio_capture_demo_test PRIVATE audio_capture)
    add_test(NAME audio_capture_demo_test COMMAND audio_capture_demo_test)
    foreach(mode RANGE 1 3)
        add_test(NAME gui_demo_mode_${mode}
            COMMAND AudioViz_PC --demo --mode ${mode} --frames 6
                --screenshot ${CMAKE_CURRENT_BINARY_DIR}/gui-demo-mode-${mode}.bmp)
        set_tests_properties(gui_demo_mode_${mode} PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
    endforeach()
endif()
