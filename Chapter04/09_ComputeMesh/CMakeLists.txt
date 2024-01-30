cmake_minimum_required(VERSION 3.16)

project(Chapter04)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch04_Sample09_ComputeMesh "Chapter 04")

target_link_libraries(Ch04_Sample09_ComputeMesh PRIVATE SharedUtils assimp)
