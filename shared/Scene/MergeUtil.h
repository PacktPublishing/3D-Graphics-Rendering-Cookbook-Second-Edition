#pragma once

#include "shared/Scene/Scene.h"
#include "shared/Scene/VtxData.h"

void mergeNodesWithMaterial(Scene& scene, MeshData& meshData, const std::string& materialName);

// Merge material lists from multiple scenes (follows the logic of merging in mergeScenes)
void mergeMaterialLists(
    // Input:
    const std::vector<std::vector<Material>*>& oldMaterials,   // all materials
    const std::vector<std::vector<std::string>*>& oldTextures, // all textures from all material lists
    // Output:
    std::vector<Material>& allMaterials,
    std::vector<std::string>& newTextures // all textures (merged from oldTextures, only unique items)
);
