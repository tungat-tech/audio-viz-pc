cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(AudioVizUiCore VERSION 0.1.0 LANGUAGES C CXX)
endif()

if(NOT TARGET lvgl)
    message(FATAL_ERROR
        "AudioViz UI Core expects the parent application to provide an LVGL target")
endif()

add_library(audioviz_ui_core STATIC
    ui/lv_font_montserrat_vietnamese_12.c
    ui/lv_font_montserrat_vietnamese_14.c
    ui/VisualizerUi.cpp
    ui/UtilityModes.cpp)

add_library(AudioViz::UiCore ALIAS audioviz_ui_core)

target_compile_features(audioviz_ui_core PUBLIC cxx_std_17)
target_include_directories(audioviz_ui_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(audioviz_ui_core PUBLIC lvgl)

