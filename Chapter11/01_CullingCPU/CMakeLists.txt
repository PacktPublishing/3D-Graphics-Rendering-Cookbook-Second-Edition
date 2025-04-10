cmake_minimum_required(VERSION 3.19)

project(Chapter11)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch11_Sample01_CullingCPU "Chapter 11")

target_link_libraries(Ch11_Sample01_CullingCPU PRIVATE SharedUtils assimp meshoptimizer)
