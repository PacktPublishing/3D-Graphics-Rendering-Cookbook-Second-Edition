cmake_minimum_required(VERSION 3.19)

project(Chapter02)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch01_Sample03_TaskFlow "Chapter 01")

target_link_libraries(Ch01_Sample03_TaskFlow SharedUtils)
