cmake_minimum_required(VERSION 3.16)

project(Chapter11)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch11_Sample03_DirectionalShadows "Chapter 11")

target_link_libraries(Ch11_Sample03_DirectionalShadows PRIVATE SharedUtils assimp meshoptimizer)
