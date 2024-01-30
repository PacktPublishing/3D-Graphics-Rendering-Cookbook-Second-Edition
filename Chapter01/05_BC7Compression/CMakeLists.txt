cmake_minimum_required(VERSION 3.16)

project(Chapter02)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch01_Sample05_BC7Compression "Chapter 01")

target_link_libraries(Ch01_Sample05_BC7Compression PRIVATE SharedUtils bc7enc)
