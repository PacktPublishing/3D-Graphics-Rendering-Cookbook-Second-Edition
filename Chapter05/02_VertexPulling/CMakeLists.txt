cmake_minimum_required(VERSION 3.12)

project(Chapter05)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch05_Sample02_VertexPulling "Chapter 05")

target_link_libraries(Ch05_Sample02_VertexPulling PRIVATE SharedUtils assimp)
