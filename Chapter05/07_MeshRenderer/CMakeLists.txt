cmake_minimum_required(VERSION 3.12)

project(Chapter05)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch05_Sample07_MeshRenderer "Chapter 05")

target_link_libraries(Ch05_Sample07_MeshRenderer PRIVATE SharedUtils assimp meshoptimizer)
