cmake_minimum_required(VERSION 3.12)

project(Chapter06)

include(../../CMake/CommonMacros.txt)

include_directories(../../deps/src/vulkan/include)
include_directories(../../shared)

SETUP_APP(Ch06_Sample03_FilterEnvmap "Chapter 06")

target_link_libraries(Ch06_Sample03_FilterEnvmap PRIVATE SharedUtils)
