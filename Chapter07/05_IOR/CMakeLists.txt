cmake_minimum_required(VERSION 3.16)

project(Chapter07)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch07_Sample05_IOR "Chapter 07")

target_link_libraries(Ch07_Sample05_IOR PRIVATE SharedUtils assimp)
