cmake_minimum_required(VERSION 3.16)

project(Chapter04)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch04_Sample03_MillionCubes "Chapter 04")

target_link_libraries(Ch04_Sample03_MillionCubes PRIVATE SharedUtils assimp)
