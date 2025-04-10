cmake_minimum_required(VERSION 3.19)

project(Chapter08)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch08_Sample03_LargeScene "Chapter 08")

target_link_libraries(Ch08_Sample03_LargeScene PRIVATE SharedUtils assimp meshoptimizer)
