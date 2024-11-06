#include "shared/Scene/MergeUtil.h"
#include "shared/Scene/Scene.h"

#include <unordered_map>

static uint32_t shiftMeshIndices(MeshData& meshData, const std::vector<uint32_t>& meshesToMerge)
{
  uint32_t minVtxOffset = std::numeric_limits<uint32_t>::max();

  for (uint32_t i : meshesToMerge)
    minVtxOffset = std::min(meshData.meshes[i].vertexOffset, minVtxOffset);

  uint32_t mergeCount = 0u; // calculated by summing index counts in meshesToMerge

  // now shift all the indices in individual index blocks [use minVtxOffset]
  for (uint32_t i : meshesToMerge) {
    Mesh& m = meshData.meshes[i];
    // for how much should we shift the indices in mesh [m]
    const uint32_t delta    = m.vertexOffset - minVtxOffset;
    const uint32_t idxCount = m.getLODIndicesCount(0);
    for (uint32_t ii = 0u; ii < idxCount; ii++)
      meshData.indexData[m.indexOffset + ii] += delta;

    m.vertexOffset = minVtxOffset;

    // sum all the deleted meshes' indices
    mergeCount += idxCount;
  }

  return meshData.indexData.size() - mergeCount;
}

// All the meshesToMerge now have the same vertexOffset and individual index values are shifted by appropriate amount
// Here we move all the indices to appropriate places in the new index array
static void mergeIndexArray(MeshData& md, const std::vector<uint32_t>& meshesToMerge, std::unordered_map<uint32_t, uint32_t>& oldToNew)
{
  std::vector<uint32_t> newIndices(md.indexData.size());
  // Two offsets in the new indices array (one begins at the start, the second one after all the copied indices)
  uint32_t copyOffset  = 0;
  uint32_t mergeOffset = shiftMeshIndices(md, meshesToMerge);

  const size_t mergedMeshIndex = md.meshes.size() - meshesToMerge.size();
  uint32_t newIndex            = 0u;
  for (size_t midx = 0u; midx < md.meshes.size(); midx++) {
    const bool shouldMerge = std::binary_search(meshesToMerge.begin(), meshesToMerge.end(), midx);

    oldToNew[midx] = shouldMerge ? mergedMeshIndex : newIndex;
    newIndex += shouldMerge ? 0 : 1;

    Mesh& mesh              = md.meshes[midx];
    const uint32_t idxCount = mesh.getLODIndicesCount(0);
    // move all indices to the new array at mergeOffset
    const auto start          = md.indexData.begin() + mesh.indexOffset;
    mesh.indexOffset          = copyOffset;
    uint32_t* const offsetPtr = shouldMerge ? &mergeOffset : &copyOffset;
    std::copy(start, start + idxCount, newIndices.begin() + *offsetPtr);
    *offsetPtr += idxCount;
  }

  md.indexData = newIndices;

  // all the merged indices are now in lastMesh
  Mesh lastMesh         = md.meshes[meshesToMerge[0]];
  lastMesh.indexOffset  = copyOffset;
  lastMesh.lodOffset[0] = copyOffset;
  lastMesh.lodOffset[1] = mergeOffset;
  lastMesh.lodCount     = 1;
  md.meshes.push_back(lastMesh);
}

void mergeNodesWithMaterial(Scene& scene, MeshData& meshData, const std::string& materialName)
{
  // Find material index
  const int oldMaterial = (int)std::distance(
      std::begin(scene.materialNames), std::find(std::begin(scene.materialNames), std::end(scene.materialNames), materialName));

  std::vector<uint32_t> toDelete;

  for (size_t i = 0u; i < scene.hierarchy.size(); i++)
    if (scene.meshForNode.contains(i) && scene.materialForNode.contains(i) && (scene.materialForNode.at(i) == oldMaterial))
      toDelete.push_back(i);

  std::vector<uint32_t> meshesToMerge(toDelete.size());

  // Convert toDelete indices to mesh indices
  std::transform(toDelete.begin(), toDelete.end(), meshesToMerge.begin(), [&scene](uint32_t i) { return scene.meshForNode.at(i); });

  // TODO: if merged mesh transforms are non-zero, then we should pre-transform individual mesh vertices in meshData using local transform

  // old-to-new mesh indices
  std::unordered_map<uint32_t, uint32_t> oldToNew;

  // now move all the meshesToMerge to the end of array
  mergeIndexArray(meshData, meshesToMerge, oldToNew);

  // cutoff all but one of the merged meshes (insert the last saved mesh from meshesToMerge - they are all the same)
  eraseSelected(meshData.meshes, meshesToMerge);

  for (auto& n : scene.meshForNode)
    n.second = oldToNew[n.second];

  // reattach the node with merged meshes [identity transforms are assumed]
  int newNode                    = addNode(scene, 0, 1);
  scene.meshForNode[newNode]     = (int)meshData.meshes.size() - 1;
  scene.materialForNode[newNode] = (uint32_t)oldMaterial;

  deleteSceneNodes(scene, toDelete);
}

void mergeMaterialLists(
    const std::vector<std::vector<Material>*>& oldMaterials, const std::vector<std::vector<std::string>*>& oldTextures,
    std::vector<Material>& allMaterials, std::vector<std::string>& newTextures)
{
  // map texture names to indices in newTexturesList (calculated as we fill the newTexturesList)
  std::unordered_map<std::string, int> newTextureNames;
  std::unordered_map<size_t, size_t> materialToTextureList; // use the index of Material in the allMaterials array

  // create a combined material list [no hashing of materials, just straightforward merging of all lists]
  for (size_t midx = 0; midx != oldMaterials.size(); midx++) {
    for (const Material& m : *oldMaterials[midx]) {
      allMaterials.push_back(m);
      materialToTextureList[allMaterials.size() - 1] = midx;
    }
  }

  // create one combined texture list
  for (const std::vector<std::string>* tl : oldTextures) {
    for (const std::string& file : *tl) {
      newTextureNames[file] = addUnique(newTextures, file);
    }
  }

  // a lambda to replace textureID by a new "version" (from the global list)
  auto replaceTexture = [&materialToTextureList, &oldTextures, &newTextureNames](int mtlId, int* textureID) {
    if (*textureID == -1)
      return;

    const size_t listIdx                    = materialToTextureList[mtlId];
    const std::vector<std::string>& texList = *oldTextures[listIdx];
    const std::string& texFile              = texList[*textureID];
    *textureID                              = newTextureNames[texFile];
  };

  for (size_t i = 0; i < allMaterials.size(); i++) {
    Material& m = allMaterials[i];
    replaceTexture(i, &m.baseColorTexture);
    replaceTexture(i, &m.emissiveTexture);
    replaceTexture(i, &m.normalTexture);
    replaceTexture(i, &m.opacityTexture);
  }
}
