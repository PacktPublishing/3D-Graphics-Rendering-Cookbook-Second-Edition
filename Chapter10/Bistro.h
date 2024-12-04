#pragma once

#include "shared/Scene/MergeUtil.h"
#include "shared/Scene/Scene.h"
#include "shared/Scene/VtxData.h"

#include "Chapter08/SceneUtils.h"

const char* fileNameCachedMeshes    = ".cache/ch08_bistro.meshes";
const char* fileNameCachedMaterials = ".cache/ch08_bistro.materials";
const char* fileNameCachedHierarchy = ".cache/ch08_bistro.scene";

void loadBistro(MeshData& meshData, Scene& scene) {
  if (!isMeshDataValid(fileNameCachedMeshes) || !isMeshHierarchyValid(fileNameCachedHierarchy) ||
      !isMeshMaterialsValid(fileNameCachedMaterials)) {
    printf("No cached mesh data found. Precaching...\n\n");

    MeshData meshData_Exterior;
    MeshData meshData_Interior;
    Scene ourScene_Exterior;
    Scene ourScene_Interior;

    // don't generate LODs because meshoptimizer fails on the Bistro mesh
    loadMeshFile("deps/src/bistro/Exterior/exterior.obj", meshData_Exterior, ourScene_Exterior, false);
    loadMeshFile("deps/src/bistro/Interior/interior.obj", meshData_Interior, ourScene_Interior, false);

    // merge some meshes
    printf("[Unmerged] scene items: %u\n", (uint32_t)ourScene_Exterior.hierarchy.size());
    mergeNodesWithMaterial(ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Orange_Leaves");
    printf("[Merged orange leaves] scene items: %u\n", (uint32_t)ourScene_Exterior.hierarchy.size());
    mergeNodesWithMaterial(ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Green_Leaves");
    printf("[Merged green leaves]  scene items: %u\n", (uint32_t)ourScene_Exterior.hierarchy.size());
    mergeNodesWithMaterial(ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Trunk");
    printf("[Merged trunk]  scene items: %u\n", (uint32_t)ourScene_Exterior.hierarchy.size());

    // merge everything into one big scene
    MeshData meshData;
    Scene ourScene;

    mergeScenes(
        ourScene,
        {
            &ourScene_Exterior,
            &ourScene_Interior,
        },
        {},
        {
            static_cast<uint32_t>(meshData_Exterior.meshes.size()),
            static_cast<uint32_t>(meshData_Interior.meshes.size()),
        });
    mergeMeshData(meshData, { &meshData_Exterior, &meshData_Interior });
    mergeMaterialLists(
        {
            &meshData_Exterior.materials,
            &meshData_Interior.materials,
        },
        {
            &meshData_Exterior.textureFiles,
            &meshData_Interior.textureFiles,
        },
        meshData.materials, meshData.textureFiles);

    ourScene.localTransform[0] = glm::scale(vec3(0.01f)); // scale the Bistro
    markAsChanged(ourScene, 0);

    recalculateBoundingBoxes(meshData);

    saveMeshData(fileNameCachedMeshes, meshData);
    saveMeshDataMaterials(fileNameCachedMaterials, meshData);
    saveScene(fileNameCachedHierarchy, ourScene);
  }

  const MeshFileHeader header = loadMeshData(fileNameCachedMeshes, meshData);
  loadMeshDataMaterials(fileNameCachedMaterials, meshData);

  loadScene(fileNameCachedHierarchy, scene);
}
