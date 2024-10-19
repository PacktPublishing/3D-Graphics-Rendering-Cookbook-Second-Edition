#pragma once

#include <stdint.h>

#include <glm/glm.hpp>

#include "shared/Utils.h"
#include "shared/UtilsMath.h"

constexpr const uint32_t kMaxLODs = 7;

// All offsets are relative to the beginning of the data block (excluding headers with a Mesh list)
struct Mesh final {
  // Number of LODs in this mesh. Strictly less than MAX_LODS, last LOD offset is used as a marker only
  uint32_t lodCount = 1;

  // The total count of all previous vertices in this mesh file
  uint32_t indexOffset = 0;

  uint32_t vertexOffset = 0;

  // Vertex count (for all LODs)
  uint32_t vertexCount = 0;

  // Offsets to LOD indices data. The last offset is used as a marker to calculate the size
  uint32_t lodOffset[kMaxLODs + 1] = { 0 };

  uint32_t materialID = 0;

  inline uint32_t getLODIndicesCount(uint32_t lod) const { return lod < lodCount ? lodOffset[lod + 1] - lodOffset[lod] : 0; }

  // Any additional information, such as mesh name, can be added here...
};

struct MeshFileHeader {
  // Unique 64-bit value to check integrity of the file
  uint32_t magicValue = 0x12345678;

  // Number of mesh descriptors following this header
  uint32_t meshCount = 0;

  // How much space index data takes in bytes
  uint32_t indexDataSize = 0;

  // How much space vertex data takes in bytes
  uint32_t vertexDataSize = 0;

  // According to your needs, you may add additional metadata fields...
};

enum MaterialFlags {
  sMaterialFlags_CastShadow    = 0x1,
  sMaterialFlags_ReceiveShadow = 0x2,
  sMaterialFlags_Transparent   = 0x4,
};

struct Material {
  vec4 emissiveFactor      = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  vec4 baseColorFactor     = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  float roughness          = 1.0f;
  float transparencyFactor = 1.0f;
  float alphaTest          = 0.0f;
  float metallicFactor     = 0.0f;
  // index into MeshData::textureFiles
  int baseColorTexture = -1;
  int emissiveTexture  = -1;
  int normalTexture    = -1;
  int opacityTexture   = -1;
  uint32_t flags       = sMaterialFlags_CastShadow | sMaterialFlags_ReceiveShadow;
};

struct MeshData {
  lvk::VertexInput streams = {};
  std::vector<uint32_t> indexData;
  std::vector<uint8_t> vertexData;
  std::vector<Mesh> meshes;
  std::vector<BoundingBox> boxes;
  std::vector<Material> materials;
  std::vector<std::string> textureFiles;
  MeshFileHeader getMeshFileHeader() const
  {
    return {
      .meshCount      = (uint32_t)meshes.size(),
      .indexDataSize  = (uint32_t)(indexData.size() * sizeof(uint32_t)),
      .vertexDataSize = (uint32_t)vertexData.size(),
    };
  }
};

static_assert(sizeof(BoundingBox) == sizeof(float) * 6);

bool isMeshDataValid(const char* fileName);
bool isMeshMaterialsValid(const char* fileName);
bool isMeshHierarchyValid(const char* fileName);
MeshFileHeader loadMeshData(const char* meshFile, MeshData& out);
void loadMeshDataMaterials(const char* meshFile, MeshData& out);
void saveMeshData(const char* fileName, const MeshData& m);
void saveMeshDataMaterials(const char* fileName, const MeshData& m);

void recalculateBoundingBoxes(MeshData& m);

// combine a list of meshes to a single mesh container
MeshFileHeader mergeMeshData(MeshData& m, const std::vector<MeshData*> md);

// use to write values into MeshData::vertexData
template <typename T> inline void put(std::vector<uint8_t>& v, const T& value)
{
  const size_t pos = v.size();
  v.resize(v.size() + sizeof(value));
  memcpy(v.data() + pos, &value, sizeof(value));
}
