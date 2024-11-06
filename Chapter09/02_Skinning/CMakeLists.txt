cmake_minimum_required(VERSION 3.16)

project(Chapter09)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch09_Sample02_Skinning "Chapter 09")

target_link_libraries(Ch09_Sample02_Skinning PRIVATE SharedUtils assimp)