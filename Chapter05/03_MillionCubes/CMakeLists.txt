cmake_minimum_required(VERSION 3.19)

project(Chapter05)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch05_Sample03_MillionCubes "Chapter 05")

target_link_libraries(Ch05_Sample03_MillionCubes PRIVATE SharedUtils assimp)
