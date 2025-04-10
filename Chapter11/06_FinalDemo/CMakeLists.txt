cmake_minimum_required(VERSION 3.19)

project(Chapter11)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch11_Sample06_FinalDemo "Chapter 11")

target_link_libraries(Ch11_Sample06_FinalDemo PRIVATE SharedUtils assimp meshoptimizer)
