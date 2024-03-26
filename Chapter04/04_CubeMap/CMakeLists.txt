cmake_minimum_required(VERSION 3.16)

project(Chapter04)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch04_Sample04_CubeMap "Chapter 04")

target_link_libraries(Ch04_Sample04_CubeMap PRIVATE SharedUtils assimp)
