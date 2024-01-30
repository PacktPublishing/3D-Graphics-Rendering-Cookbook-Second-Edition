#include "shared/scene/VtxData.h"

#include <algorithm>
#include <assert.h>
#include <stdio.h>

bool isMeshDataValid(const char* meshFile)
{
  FILE* f = fopen(meshFile, "rb");

  if (!f)
    return false;

  SCOPE_EXIT
  {
    fclose(f);
  };

  MeshFileHeader header;

  if (fread(&header, 1, sizeof(header), f) != sizeof(header))
    return false;

  if (fseek(f, sizeof(Mesh) * header.meshCount, SEEK_CUR))
    return false;

  if (fseek(f, sizeof(BoundingBox) * header.meshCount, SEEK_CUR))
    return false;

  if (fseek(f, header.indexDataSize, SEEK_CUR))
    return false;

  if (fseek(f, header.vertexDataSize, SEEK_CUR))
    return false;

  return true;
}

MeshFileHeader loadMeshData(const char* meshFile, MeshData& out)
{
  FILE* f = fopen(meshFile, "rb");

  assert(f);

  if (!f) {
    printf("Cannot open '%s'.\n", meshFile);
    assert(false);
    exit(EXIT_FAILURE);
  }

  SCOPE_EXIT
  {
    fclose(f);
  };

  MeshFileHeader header;

  if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
    printf("Unable to read mesh file header.\n");
    assert(false);
    exit(EXIT_FAILURE);
  }

  if (fread(&out.streams, 1, sizeof(out.streams), f) != sizeof(out.streams)) {
    printf("Unable to read vertex streams description.\n");
    assert(false);
    exit(EXIT_FAILURE);
  }

  out.meshes.resize(header.meshCount);
  if (fread(out.meshes.data(), sizeof(Mesh), header.meshCount, f) != header.meshCount) {
    printf("Could not read mesh descriptors.\n");
    assert(false);
    exit(EXIT_FAILURE);
  }
  out.boxes.resize(header.meshCount);
  if (fread(out.boxes.data(), sizeof(BoundingBox), header.meshCount, f) != header.meshCount) {
    printf("Could not read bounding boxes.\n");
    assert(false);
    exit(EXIT_FAILURE);
  }

  out.indexData.resize(header.indexDataSize / sizeof(uint32_t));
  out.vertexData.resize(header.vertexDataSize);

  if (fread(out.indexData.data(), 1, header.indexDataSize, f) != header.indexDataSize) {
    printf("Unable to read index data.\n");
    assert(false);
    exit(EXIT_FAILURE);
  }

  if (fread(out.vertexData.data(), 1, header.vertexDataSize, f) != header.vertexDataSize) {
    printf("Unable to read vertex data.\n");
    assert(false);
    exit(EXIT_FAILURE);
  }

  return header;
}

void saveMeshData(const char* fileName, const MeshData& m)
{
  FILE* f = fopen(fileName, "wb");

  if (!f) {
    printf("Error opening file '%s' for writing.\n", fileName);
    assert(false);
    exit(EXIT_FAILURE);
  }

  const MeshFileHeader header = {
    .magicValue           = 0x12345678,
    .meshCount            = (uint32_t)m.meshes.size(),
    .indexDataSize        = (uint32_t)(m.indexData.size() * sizeof(uint32_t)),
    .vertexDataSize       = (uint32_t)(m.vertexData.size()),
  };

  fwrite(&header, 1, sizeof(header), f);
  fwrite(&m.streams, 1, sizeof(m.streams), f);
  fwrite(m.meshes.data(), sizeof(Mesh), header.meshCount, f);
  fwrite(m.boxes.data(), sizeof(BoundingBox), header.meshCount, f);
  fwrite(m.indexData.data(), 1, header.indexDataSize, f);
  fwrite(m.vertexData.data(), 1, header.vertexDataSize, f);

  fclose(f);
}

void saveBoundingBoxes(const char* fileName, const std::vector<BoundingBox>& boxes)
{
  FILE* f = fopen(fileName, "wb");

  if (!f) {
    printf("Error opening bounding boxes file '%s' for writing.\n", fileName);
    assert(false);
    exit(EXIT_FAILURE);
  }

  const uint32_t sz = (uint32_t)boxes.size();
  fwrite(&sz, 1, sizeof(sz), f);
  fwrite(boxes.data(), sz, sizeof(BoundingBox), f);

  fclose(f);
}

void loadBoundingBoxes(const char* fileName, std::vector<BoundingBox>& boxes)
{
  FILE* f = fopen(fileName, "rb");

  if (!f) {
    printf("Error opening bounding boxes file '%s'\n", fileName);
    assert(false);
    exit(EXIT_FAILURE);
  }

  uint32_t sz;
  fread(&sz, 1, sizeof(sz), f);

  // TODO: check file size, divide by bounding box size
  boxes.resize(sz);
  fread(boxes.data(), sz, sizeof(BoundingBox), f);

  fclose(f);
}

// combine a collection of meshes into a single MeshData container
MeshFileHeader mergeMeshData(MeshData& m, const std::vector<MeshData*> md)
{
  uint32_t totalVertexDataSize = 0;
  uint32_t totalIndexDataSize  = 0;

  uint32_t offs = 0;
  for (const MeshData* i : md) {
    mergeVectors(m.indexData, i->indexData);
    mergeVectors(m.vertexData, i->vertexData);
    mergeVectors(m.meshes, i->meshes);
    mergeVectors(m.boxes, i->boxes);

    uint32_t vtxOffset = totalVertexDataSize / 8; // 8 is the number of components in per-vertex attributes: vec3 position, vec3 normal, vec2 UV

    for (size_t j = 0; j != i->meshes.size(); j++)
      // m.vertexCount, m.lodCount and m.streamCount do not change
      // m.vertexOffset also does not change, because vertex offsets are local (i.e., baked into the indices)
      m.meshes[offs + j].indexOffset += totalIndexDataSize;

    // shift individual indices
    for (size_t j = 0; j != i->indexData.size(); j++)
      m.indexData[totalIndexDataSize + j] += vtxOffset;

    offs += (uint32_t)i->meshes.size();

    totalIndexDataSize += (uint32_t)i->indexData.size();
    totalVertexDataSize += (uint32_t)i->vertexData.size();
  }

  return MeshFileHeader{
    .magicValue           = 0x12345678,
    .meshCount            = (uint32_t)offs,
    .indexDataSize        = static_cast<uint32_t>(totalIndexDataSize * sizeof(uint32_t)),
    .vertexDataSize       = static_cast<uint32_t>(totalVertexDataSize),
  };
}

void recalculateBoundingBoxes(MeshData& m)
{
  LVK_ASSERT(m.streams.attributes[0].format == lvk::VertexFormat::Float3);

  m.boxes.clear();
  m.boxes.reserve(m.meshes.size());

  for (const Mesh& mesh : m.meshes) {
    const uint32_t numIndices = mesh.getLODIndicesCount(0);

    glm::vec3 vmin(std::numeric_limits<float>::max());
    glm::vec3 vmax(std::numeric_limits<float>::lowest());

    for (uint32_t i = 0; i != numIndices; i++) {
      const uint32_t vtxOffset = m.indexData[mesh.indexOffset + i] + mesh.vertexOffset;
      const float* vf          = (const float*)&m.vertexData[vtxOffset * LVK_ARRAY_NUM_ELEMENTS(m.streams.attributes)];

      vmin = glm::min(vmin, vec3(vf[0], vf[1], vf[2]));
      vmax = glm::max(vmax, vec3(vf[0], vf[1], vf[2]));
    }

    m.boxes.emplace_back(vmin, vmax);
  }
}
