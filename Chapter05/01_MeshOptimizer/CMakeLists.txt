cmake_minimum_required(VERSION 3.19)

project(Chapter05)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch05_Sample01_MeshOptimizer "Chapter 05")

target_link_libraries(Ch05_Sample01_MeshOptimizer PRIVATE SharedUtils assimp meshoptimizer)
