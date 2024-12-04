cmake_minimum_required(VERSION 3.16)

project(Chapter08)

include(../../CMake/CommonMacros.txt)

SETUP_APP(Ch08_Sample02_SceneGraph "Chapter 08")

target_link_libraries(Ch08_Sample02_SceneGraph PRIVATE SharedUtils assimp ImGuizmo meshoptimizer)
