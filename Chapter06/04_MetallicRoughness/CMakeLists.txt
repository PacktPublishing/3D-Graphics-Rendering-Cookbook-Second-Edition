cmake_minimum_required(VERSION 3.16)

project(Chapter06)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch06_Sample04_MetallicRoughness "Chapter 06")

target_link_libraries(Ch06_Sample04_MetallicRoughness PRIVATE SharedUtils assimp)
