cmake_minimum_required(VERSION 3.19)

project(Chapter07)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch07_Sample02_Sheen "Chapter 07")

target_link_libraries(Ch07_Sample02_Sheen PRIVATE SharedUtils assimp) # Shaders)
