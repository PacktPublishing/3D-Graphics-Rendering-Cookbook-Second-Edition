cmake_minimum_required(VERSION 3.16)

project(Chapter02)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch02_Sample04_AssImp "Chapter 02")

target_link_libraries(Ch02_Sample04_AssImp PRIVATE SharedUtils assimp)
