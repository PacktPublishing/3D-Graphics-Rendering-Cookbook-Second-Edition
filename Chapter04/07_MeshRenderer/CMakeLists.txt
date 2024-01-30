cmake_minimum_required(VERSION 3.12)

project(Chapter04)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch04_Sample07_MeshRenderer "Chapter 04")

target_link_libraries(Ch04_Sample07_MeshRenderer PRIVATE SharedUtils assimp meshoptimizer)
