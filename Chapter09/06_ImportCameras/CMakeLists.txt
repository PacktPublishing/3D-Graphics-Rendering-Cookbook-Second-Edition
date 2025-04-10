cmake_minimum_required(VERSION 3.19)

project(Chapter09)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch09_Sample06_ImportCameras "Chapter 09")

target_link_libraries(Ch09_Sample06_ImportCameras PRIVATE SharedUtils assimp)
