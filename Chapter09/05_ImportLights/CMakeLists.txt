cmake_minimum_required(VERSION 3.16)

project(Chapter09)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch09_Sample05_ImportLights "Chapter 09")

target_link_libraries(Ch09_Sample05_ImportLights PRIVATE SharedUtils assimp)
