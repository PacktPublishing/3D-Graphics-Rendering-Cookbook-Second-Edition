cmake_minimum_required(VERSION 3.16)

project(Chapter03)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch03_Sample04_CubeMap "Chapter 03")

target_link_libraries(Ch03_Sample04_CubeMap PRIVATE SharedUtils assimp)
