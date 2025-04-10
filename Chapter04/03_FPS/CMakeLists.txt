cmake_minimum_required(VERSION 3.19)

project(Chapter04)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch04_Sample03_FPS "Chapter 04")

target_link_libraries(Ch04_Sample03_FPS PRIVATE SharedUtils)

target_sources(Ch04_Sample03_FPS PRIVATE "${COOKBOOK_ROOT_DIR}/deps/src/lightweightvk/third-party/deps/src/imgui/imgui_demo.cpp")
target_sources(Ch04_Sample03_FPS PRIVATE "${COOKBOOK_ROOT_DIR}/deps/src/lightweightvk/third-party/deps/src/implot/implot_demo.cpp")
