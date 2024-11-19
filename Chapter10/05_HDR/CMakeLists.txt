cmake_minimum_required(VERSION 3.16)

project(Chapter10)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch10_Sample05_HDR "Chapter 10")

target_link_libraries(Ch10_Sample05_HDR PRIVATE SharedUtils assimp bc7enc meshoptimizer)
