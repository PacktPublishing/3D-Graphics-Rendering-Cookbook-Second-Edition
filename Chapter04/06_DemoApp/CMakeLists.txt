cmake_minimum_required(VERSION 3.19)

project(Chapter04)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch04_Sample06_DemoApp "Chapter 04")

target_link_libraries(Ch04_Sample06_DemoApp PRIVATE SharedUtils assimp)
