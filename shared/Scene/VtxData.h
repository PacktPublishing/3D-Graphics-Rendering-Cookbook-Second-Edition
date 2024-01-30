#pragma once

#include <stdint.h>

#include <glm/glm.hpp>

#include "shared/Utils.h"
#include "shared/UtilsMath.h"

constexpr const uint32_t kMaxLODs = 8;

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
  uint32_t lodOffset[kMaxLODs] = { 0 };

  uint32_t materialID = 0;

  inline uint32_t getLODIndicesCount(uint32_t lod) const { return lod < lodCount ? lodOffset[lod + 1] - lodOffset[lod] : 0; }

  // Any additional information, such as mesh name, can be added here...
};

struct MeshFileHeader {
  // Unique 64-bit value to check integrity of the file
  uint32_t magicValue;

  // Number of mesh descriptors following this header
  uint32_t meshCount;

  // How much space index data takes in bytes
  uint32_t indexDataSize;

  // How much space vertex data takes in bytes
  uint32_t vertexDataSize;

  // According to your needs, you may add additional metadata fields...
};

struct MeshData {
  lvk::VertexInput streams = {};
  std::vector<uint32_t> indexData;
  std::vector<uint8_t> vertexData;
  std::vector<Mesh> meshes;
  std::vector<BoundingBox> boxes;
};

static_assert(sizeof(BoundingBox) == sizeof(float) * 6);

bool isMeshDataValid(const char* meshFile);
MeshFileHeader loadMeshData(const char* meshFile, MeshData& out);
void saveMeshData(const char* fileName, const MeshData& m);

void recalculateBoundingBoxes(MeshData& m);

// combine a list of meshes to a single mesh container
MeshFileHeader mergeMeshData(MeshData& m, const std::vector<MeshData*> md);

template <typename T> inline void mergeVectors(std::vector<T>& v1, const std::vector<T>& v2)
{
  v1.insert(v1.end(), v2.begin(), v2.end());
}

// use to write values into MeshData::vertexData
template <typename T> inline void put(std::vector<uint8_t>& v, const T& value)
{
  const size_t pos = v.size();
  v.resize(v.size() + sizeof(value));
  memcpy(v.data() + pos, &value, sizeof(value));
}
