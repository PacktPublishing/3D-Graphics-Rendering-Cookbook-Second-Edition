cmake_minimum_required(VERSION 3.19)

project(Chapter09)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch09_Sample01_AnimationPlayer "Chapter 09")

target_link_libraries(Ch09_Sample01_AnimationPlayer PRIVATE SharedUtils assimp)