cmake_minimum_required(VERSION 3.19)

project(Chapter09)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch09_Sample03_Morphing "Chapter 09")

target_link_libraries(Ch09_Sample03_Morphing PRIVATE SharedUtils assimp)