cmake_minimum_required(VERSION 3.19)

project(Chapter02)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch02_Sample01_Swapchain "Chapter 02")

target_link_libraries(Ch02_Sample01_Swapchain PRIVATE SharedUtils)
