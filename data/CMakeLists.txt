cmake_minimum_required(VERSION 3.16)

project(SharedUtils CXX C)

include(../CMake/CommonMacros.txt)

file(GLOB_RECURSE SHADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} shaders/gltf/*.vert shaders/gltf/*.frag shaders/gltf/*.geom shaders/gltf/*.sp src/*.comp)

add_library(Shaders INTERFACE ${SHADER_FILES})
