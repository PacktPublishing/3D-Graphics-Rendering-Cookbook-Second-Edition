cmake_minimum_required(VERSION 3.19)

project(SharedUtils CXX C)

include(../CMake/CommonMacros.txt)

file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c??)
file(GLOB_RECURSE HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)


add_library(SharedUtils STATIC ${SRC_FILES} ${HEADER_FILES})

set_property(TARGET SharedUtils PROPERTY CXX_STANDARD 20)
set_property(TARGET SharedUtils PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(SharedUtils PUBLIC assimp)
target_link_libraries(SharedUtils PUBLIC LVKLibrary)
target_link_libraries(SharedUtils PUBLIC LVKstb)
target_link_libraries(SharedUtils PUBLIC ktx)

if(WIN32)
  target_compile_definitions(SharedUtils PUBLIC "NOMINMAX")
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(SharedUtils PUBLIC tbb)
endif()
