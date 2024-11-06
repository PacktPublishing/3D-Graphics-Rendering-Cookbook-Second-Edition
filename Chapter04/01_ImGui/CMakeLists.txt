cmake_minimum_required(VERSION 3.16)

project(Chapter04)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch04_Sample01_ImGui "Chapter 04")

target_link_libraries(Ch04_Sample01_ImGui PRIVATE SharedUtils)

target_sources(Ch04_Sample01_ImGui PRIVATE "${COOKBOOK_ROOT_DIR}/deps/src/lightweightvk/third-party/deps/src/imgui/imgui_demo.cpp")
