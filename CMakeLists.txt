cmake_minimum_required(VERSION 3.19)

project(RenderingCookbook2 CXX C)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(COOKBOOK_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COOKBOOK_DEPS_CACHE_FILE "${COOKBOOK_ROOT_DIR}/deps/.bootstrap.json")

if($ENV{CLION_IDE})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${COOKBOOK_ROOT_DIR})
endif()

find_package(Python3 COMPONENTS Interpreter)

execute_process(COMMAND ${Python3_EXECUTABLE} deploy_deps.py WORKING_DIRECTORY ${COOKBOOK_ROOT_DIR} COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND ${Python3_EXECUTABLE} deploy_deps.py WORKING_DIRECTORY ${COOKBOOK_ROOT_DIR}/deps/src/lightweightvk COMMAND_ERROR_IS_FATAL ANY)

add_custom_command(COMMAND ${Python3_EXECUTABLE} deploy_deps.py OUTPUT ${COOKBOOK_DEPS_CACHE_FILE}
                   DEPENDS "${COOKBOOK_ROOT_DIR}/deps/bootstrap.json" WORKING_DIRECTORY ${COOKBOOK_ROOT_DIR})
add_custom_target(Dependencies DEPENDS ${COOKBOOK_DEPS_CACHE_FILE})
set_property(TARGET Dependencies PROPERTY FOLDER "third-party")

if(NOT EXISTS ${COOKBOOK_DEPS_CACHE_FILE})
  message(FATAL_ERROR "Cannot bootstrap dependencies.")
endif()

# cmake-format: off
add_subdirectory(deps/src/lightweightvk/third-party/deps/src/bc7enc)
add_subdirectory(deps/src/lightweightvk/third-party/deps/src/ldrutils)
set(KTX_FEATURE_DOC            OFF CACHE BOOL "")
set(KTX_FEATURE_GL_UPLOAD      OFF CACHE BOOL "")
set(KTX_FEATURE_JNI            OFF CACHE BOOL "")
set(KTX_FEATURE_KTX1            ON CACHE BOOL "")
set(KTX_FEATURE_KTX2            ON CACHE BOOL "")
set(KTX_FEATURE_LOADTEST_APPS  OFF CACHE BOOL "")
set(KTX_FEATURE_STATIC_LIBRARY  ON CACHE BOOL "")
set(KTX_FEATURE_TESTS          OFF CACHE BOOL "")
set(KTX_FEATURE_TOOLS          OFF CACHE BOOL "")
set(KTX_FEATURE_VK_UPLOAD      OFF CACHE BOOL "")
add_subdirectory(deps/src/lightweightvk/third-party/deps/src/ktx-software)
set(LVK_WITH_SAMPLES OFF CACHE BOOL "")
set(LVK_WITH_TRACY   ON  CACHE BOOL "")
add_subdirectory(deps/src/lightweightvk)
lvk_set_folder(ktx "third-party/ktx-software")
lvk_set_folder(ktx_read "third-party/ktx-software")
lvk_set_folder(ktx_version "third-party/ktx-software")
lvk_set_folder(obj_basisu_cbind "third-party/ktx-software")
lvk_set_folder(objUtil "third-party/ktx-software")
if(TARGET astcenc-avx2-static)
  lvk_set_folder(astcenc-avx2-static "third-party/ktx-software")
endif()
# cmake-format: on

set(ASSIMP_NO_EXPORT ON CACHE BOOL "")
set(ASSIMP_BUILD_DRACO OFF CACHE BOOL "")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "")
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "")

add_subdirectory(deps/src/assimp)
if(APPLE)
  target_compile_options(assimp PRIVATE -Wno-deprecated-declarations)
endif()

set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "")
set(MESHOPT_BUILD_TOOLS OFF CACHE BOOL "")
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "")
add_subdirectory(deps/src/meshoptimizer)

if(WIN32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

include_directories(.)
include_directories(deps/src)
include_directories(deps/src/taskflow)
include_directories(deps/src/lightweightvk/third-party/deps/src/glm)
include_directories(deps/src/lightweightvk/third-party/deps/src/imgui)
#include_directories(deps/src/rapidjson/include)
#include_directories(deps/src/bullet/src)

#add_subdirectory(deps/cmake/bullet)
add_subdirectory(deps/cmake/ImGuizmo)

set_property(TARGET assimp          PROPERTY FOLDER "third-party")
set_property(TARGET bc7enc          PROPERTY FOLDER "third-party")
set_property(TARGET LUtils          PROPERTY FOLDER "third-party")
set_property(TARGET meshoptimizer   PROPERTY FOLDER "third-party")
if(WIN32)
  set_property(TARGET zlibstatic    PROPERTY FOLDER "third-party")
  if(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs)
    set_property(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTY FOLDER "third-party")
  endif()
endif()
set_property(TARGET uninstall       PROPERTY FOLDER "third-party")

# shared utils code
add_subdirectory(shared)
add_subdirectory(data)

add_dependencies(SharedUtils Dependencies)

# disable warnings in third-party code
if(MSVC)
	target_compile_options(assimp PRIVATE /wd4267)
	target_compile_options(SPIRV  PRIVATE /wd4267)
endif()

add_subdirectory(Chapter01/01_CMake)
add_subdirectory(Chapter01/02_GLFW)
add_subdirectory(Chapter01/03_Taskflow)
add_subdirectory(Chapter01/04_GLSLang)
add_subdirectory(Chapter01/05_BC7Compression)

add_subdirectory(Chapter02/01_Swapchain)
add_subdirectory(Chapter02/02_HelloTriangle)
add_subdirectory(Chapter02/03_GLM)

add_subdirectory(Chapter03/01_Assimp)
add_subdirectory(Chapter03/02_STB)

add_subdirectory(Chapter04/01_ImGui)
add_subdirectory(Chapter04/02_TracyProfiler)
add_subdirectory(Chapter04/03_FPS)
add_subdirectory(Chapter04/04_CubeMap)
add_subdirectory(Chapter04/05_Camera)
add_subdirectory(Chapter04/06_DemoApp)

add_subdirectory(Chapter05/01_MeshOptimizer)
add_subdirectory(Chapter05/02_VertexPulling)
add_subdirectory(Chapter05/03_MillionCubes)
add_subdirectory(Chapter05/04_InstancedMeshes)
add_subdirectory(Chapter05/05_Grid)
add_subdirectory(Chapter05/06_Tessellation)
add_subdirectory(Chapter05/07_MeshRenderer)
add_subdirectory(Chapter05/08_ComputeTexture)
add_subdirectory(Chapter05/09_ComputeMesh)

add_subdirectory(Chapter06/01_Unlit)
add_subdirectory(Chapter06/02_BRDF_LUT)
add_subdirectory(Chapter06/03_FilterEnvmap)
add_subdirectory(Chapter06/04_MetallicRoughness)
add_subdirectory(Chapter06/05_SpecularGlossiness)

add_subdirectory(Chapter07/01_Clearcoat)
add_subdirectory(Chapter07/02_Sheen)
add_subdirectory(Chapter07/03_Transmission)
add_subdirectory(Chapter07/04_Volume)
add_subdirectory(Chapter07/05_IOR)
add_subdirectory(Chapter07/06_Specular)
add_subdirectory(Chapter07/07_EmissiveStrength)
add_subdirectory(Chapter07/08_AnalyticalLight)

add_subdirectory(Chapter08/01_DescriptorIndexing)
add_subdirectory(Chapter08/02_SceneGraph)
add_subdirectory(Chapter08/03_LargeScene)
