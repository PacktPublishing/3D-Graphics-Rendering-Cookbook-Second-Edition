#pragma once

#include <meshoptimizer.h>

#include "shared/UtilsGLTF.h"

struct DrawIndexedIndirectCommand {
  uint32_t count;
  uint32_t instanceCount;
  uint32_t firstIndex;
  uint32_t baseVertex;
  uint32_t baseInstance;
};

struct DrawData {
  uint32_t transformId;
  uint32_t materialId;
};

// textureId -> TextureHandle
using TextureCache = std::vector<lvk::Holder<lvk::TextureHandle>>;
// textureId -> FileName
using TextureFiles = std::vector<std::string>;

GLTFMaterialDataGPU convertToGPUMaterial(
    const std::unique_ptr<lvk::IContext>& ctx, const Material& mat, const TextureFiles& files, TextureCache& cache)
{
  GLTFMaterialDataGPU result = {
    .baseColorFactor                  = mat.baseColorFactor,
    .metallicRoughnessNormalOcclusion = vec4(mat.metallicFactor, mat.roughness, 1.0f, 1.0f),
    .emissiveFactorAlphaCutoff        = vec4(vec3(mat.emissiveFactor), mat.alphaTest),
  };

  auto getTextureFromCache = [&cache, &ctx, &files](int textureId) -> uint32_t {
    if (textureId == -1)
      return 0;

    if (cache.size() <= textureId) {
      cache.resize(textureId + 1);
    }
    if (cache[textureId].empty()) {
      cache[textureId] = loadTexture(ctx, files[textureId].c_str());
    }
    return cache[textureId].index();
  };

  result.baseColorTexture    = getTextureFromCache(mat.baseColorTexture);
  result.emissiveTexture     = getTextureFromCache(mat.emissiveTexture);
  result.normalTexture       = getTextureFromCache(mat.normalTexture);
  result.transmissionTexture = getTextureFromCache(mat.opacityTexture);

  return result;
}

// NOTE: this function was manually tweaked to load Bistro materials from .obj - use UtilsGLTF.h for anything else
Material convertAIMaterial(const aiMaterial* M, std::vector<std::string>& files, std::vector<std::string>& opacityMaps)
{
  Material D;

  aiColor4D Color;

  if (aiGetMaterialColor(M, AI_MATKEY_COLOR_AMBIENT, &Color) == AI_SUCCESS) {
    D.emissiveFactor = { Color.r, Color.g, Color.b, Color.a };
    if (D.emissiveFactor.w > 1.0f)
      D.emissiveFactor.w = 1.0f;
  }
  if (aiGetMaterialColor(M, AI_MATKEY_COLOR_DIFFUSE, &Color) == AI_SUCCESS) {
    D.baseColorFactor = { Color.r, Color.g, Color.b, Color.a };
    if (D.baseColorFactor.w > 1.0f)
      D.baseColorFactor.w = 1.0f;
  }
  if (aiGetMaterialColor(M, AI_MATKEY_COLOR_EMISSIVE, &Color) == AI_SUCCESS) {
    D.emissiveFactor += vec4(Color.r, Color.g, Color.b, Color.a);
    if (D.emissiveFactor.w > 1.0f)
      D.emissiveFactor.w = 1.0f;
  }

  const float opaquenessThreshold = 0.05f;
  float Opacity                   = 1.0f;

  if (aiGetMaterialFloat(M, AI_MATKEY_OPACITY, &Opacity) == AI_SUCCESS) {
    D.transparencyFactor = glm::clamp(1.0f - Opacity, 0.0f, 1.0f);
    if (D.transparencyFactor >= 1.0f - opaquenessThreshold)
      D.transparencyFactor = 0.0f;
  }

  if (aiGetMaterialColor(M, AI_MATKEY_COLOR_TRANSPARENT, &Color) == AI_SUCCESS) {
    const float Opacity  = std::max(std::max(Color.r, Color.g), Color.b);
    D.transparencyFactor = glm::clamp(Opacity, 0.0f, 1.0f);
    if (D.transparencyFactor >= 1.0f - opaquenessThreshold)
      D.transparencyFactor = 0.0f;
    D.alphaTest = 0.5f;
  }

  float tmp = 1.0f;
  if (aiGetMaterialFloat(M, AI_MATKEY_METALLIC_FACTOR, &tmp) == AI_SUCCESS)
    D.metallicFactor = tmp;

  if (aiGetMaterialFloat(M, AI_MATKEY_ROUGHNESS_FACTOR, &tmp) == AI_SUCCESS)
    D.roughness = tmp;

  aiString path;
  aiTextureMapping mapping;
  unsigned int uvIndex               = 0;
  float blend                        = 1.0f;
  aiTextureOp textureOp              = aiTextureOp_Add;
  aiTextureMapMode textureMapMode[2] = { aiTextureMapMode_Wrap, aiTextureMapMode_Wrap };
  unsigned int textureFlags          = 0;

  if (aiGetMaterialTexture(M, aiTextureType_EMISSIVE, 0, &path, &mapping, &uvIndex, &blend, &textureOp, textureMapMode, &textureFlags) ==
      AI_SUCCESS) {
    D.emissiveTexture = addUnique(files, path.C_Str());
  }

  if (aiGetMaterialTexture(M, aiTextureType_DIFFUSE, 0, &path, &mapping, &uvIndex, &blend, &textureOp, textureMapMode, &textureFlags) ==
      AI_SUCCESS) {
    D.baseColorTexture          = addUnique(files, path.C_Str());
    const std::string albedoMap = std::string(path.C_Str());
    if (albedoMap.find("grey_30") != albedoMap.npos)
      D.flags |= sMaterialFlags_Transparent;
  }

  // first try tangent space normal map
  if (aiGetMaterialTexture(M, aiTextureType_NORMALS, 0, &path, &mapping, &uvIndex, &blend, &textureOp, textureMapMode, &textureFlags) ==
      AI_SUCCESS) {
    D.normalTexture = addUnique(files, path.C_Str());
  }
  // then height map
  if (D.normalTexture == -1)
    if (aiGetMaterialTexture(M, aiTextureType_HEIGHT, 0, &path, &mapping, &uvIndex, &blend, &textureOp, textureMapMode, &textureFlags) ==
        AI_SUCCESS)
      D.normalTexture = addUnique(files, path.C_Str());

  if (aiGetMaterialTexture(M, aiTextureType_OPACITY, 0, &path, &mapping, &uvIndex, &blend, &textureOp, textureMapMode, &textureFlags) ==
      AI_SUCCESS) {
    D.opacityTexture = addUnique(opacityMaps, path.C_Str());
    D.alphaTest      = 0.5f;
  }

  // patch materials
  aiString Name;
  std::string materialName;
  if (aiGetMaterialString(M, AI_MATKEY_NAME, &Name) == AI_SUCCESS) {
    materialName = Name.C_Str();
  }
  // apply heuristics
  if ((materialName.find("MASTER_Glass_Clean") != std::string::npos) || (materialName.find("MenuSign_02_Glass") != std::string::npos) ||
      (materialName.find("Vespa_Headlight") != std::string::npos)) {
    D.alphaTest          = 0.75f;
    D.transparencyFactor = 0.1f;
    D.flags |= sMaterialFlags_Transparent;
  } else if ((materialName.find("Glass") != std::string::npos)) {
    D.alphaTest          = 0.56f;
    D.transparencyFactor = 0.1f;
    D.flags |= sMaterialFlags_Transparent;
  } else if (materialName.find("Bottle") != std::string::npos) {
    D.alphaTest          = 0.56f;
    D.transparencyFactor = 0.4f;
    D.flags |= sMaterialFlags_Transparent;
  } else if (materialName.find("Metal") != std::string::npos) {
    D.metallicFactor = 1.0f;
    D.roughness      = 0.1f;
  }

  return D;
}

void processLODs(
    std::vector<uint32_t>& indices, std::vector<uint8_t>& vertices, size_t vertexStride, std::vector<std::vector<uint32_t>>& outLods, bool generateLods)
{
  size_t verticesCountIn    = vertices.size() / vertexStride;
  size_t targetIndicesCount = indices.size();

  printf("\n   LOD0: %i indices", int(indices.size()));

  outLods.push_back(indices);

  if (!generateLods)
    return;

  uint8_t LOD = 1;

  while (targetIndicesCount > 1024 && LOD < kMaxLODs) {
    targetIndicesCount /= 2;

    bool sloppy = false;

    size_t numOptIndices = meshopt_simplify(
        indices.data(), indices.data(), (uint32_t)indices.size(), (const float*)vertices.data(), verticesCountIn, vertexStride,
        targetIndicesCount, 0.02f, 0, nullptr);

    // cannot simplify further
    if (static_cast<size_t>(numOptIndices * 1.1f) > indices.size()) {
      if (LOD > 1) {
        // try harder
        numOptIndices = meshopt_simplifySloppy(
            indices.data(), indices.data(), indices.size(), (const float*)vertices.data(), verticesCountIn, vertexStride,
            targetIndicesCount, 0.02f, nullptr);
        sloppy = true;
        if (numOptIndices == indices.size())
          break;
      } else
        break;
    }

    indices.resize(numOptIndices);

    meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), verticesCountIn);

    printf("\n   LOD%i: %i indices %s", int(LOD), int(numOptIndices), sloppy ? "[sloppy]" : "");

    LOD++;

    outLods.push_back(indices);
  }

  printf("\n");
}

Mesh convertAIMesh(const aiMesh* m, MeshData& meshData, uint32_t& indexOffset, uint32_t& vertexOffset, bool generateLODs)
{
  static_assert(sizeof(aiVector3D) == 3 * sizeof(float));

  const bool hasTexCoords = m->HasTextureCoords(0);

  // Original data for LOD calculation
  std::vector<uint32_t> srcIndices;
  std::vector<uint8_t> vertices;

  for (size_t i = 0; i != m->mNumVertices; i++) {
    const aiVector3D v = m->mVertices[i];
    const aiVector3D n = m->mNormals[i];
    const aiVector2D t = hasTexCoords ? aiVector2D(m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y) : aiVector2D();
    put(vertices, v);                                              // pos   : vec3
    put(vertices, glm::packHalf2x16(vec2(t.x, t.y)));              // uv    : half2
    put(vertices, glm::packSnorm3x10_1x2(vec4(n.x, n.y, n.z, 0))); // normal: 2_10_10_10_REV
  }

  // pos, uv, normal
  meshData.streams = {
    .attributes    = { { .location = 0, .format = lvk::VertexFormat::Float3, .offset = 0 },                // pos
                       { .location = 1, .format = lvk::VertexFormat::HalfFloat2, .offset = sizeof(vec3) }, // uv
                       { .location = 2, .format = lvk::VertexFormat::Int_2_10_10_10_REV, .offset = sizeof(vec3) + sizeof(uint32_t) } }, // n
    .inputBindings = { { .stride = sizeof(vec3) + sizeof(uint32_t) + sizeof(uint32_t) } },
  };

  for (unsigned int i = 0; i != m->mNumFaces; i++) {
    if (m->mFaces[i].mNumIndices != 3)
      continue;
    for (unsigned j = 0; j != m->mFaces[i].mNumIndices; j++)
      srcIndices.push_back(m->mFaces[i].mIndices[j]);
  }

  const uint32_t vertexStride = meshData.streams.getVertexSize();

  // optimize the entire mesh
  {
    const uint32_t vertexCountIn = vertices.size() / vertexStride;
    std::vector<uint32_t> remap(vertexCountIn);
    const size_t vertexCountOut =
        meshopt_generateVertexRemap(remap.data(), srcIndices.data(), srcIndices.size(), vertices.data(), vertexCountIn, vertexStride);

    std::vector<uint32_t> remappedIndices(srcIndices.size());
    std::vector<uint8_t> remappedVertices(vertexCountOut * vertexStride);

    meshopt_remapIndexBuffer(remappedIndices.data(), srcIndices.data(), srcIndices.size(), remap.data());
    meshopt_remapVertexBuffer(remappedVertices.data(), vertices.data(), vertexCountIn, vertexStride, remap.data());

    meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), srcIndices.size(), vertexCountOut);
    meshopt_optimizeOverdraw(
        remappedIndices.data(), remappedIndices.data(), srcIndices.size(), (const float*)remappedVertices.data(), vertexCountOut,
        vertexStride, 1.05f);
    meshopt_optimizeVertexFetch(
        remappedVertices.data(), remappedIndices.data(), srcIndices.size(), remappedVertices.data(), vertexCountOut, vertexStride);

    srcIndices = remappedIndices;
    vertices   = remappedVertices;
    LVK_ASSERT(vertexCountOut == vertices.size() / vertexStride);
  }

  const uint32_t numVertices = static_cast<uint32_t>(vertices.size() / vertexStride);

  std::vector<std::vector<uint32_t>> outLods;
  processLODs(srcIndices, vertices, vertexStride, outLods, generateLODs);

  Mesh result = {
    .indexOffset  = indexOffset,
    .vertexOffset = vertexOffset,
    .vertexCount  = numVertices,
  };

  uint32_t numIndices = 0;
  for (size_t l = 0; l < outLods.size(); l++) {
    mergeVectors(meshData.indexData, outLods[l]);
    result.lodOffset[l] = numIndices;
    numIndices += (uint32_t)outLods[l].size();
  }

  mergeVectors(meshData.vertexData, vertices);

  result.lodOffset[outLods.size()] = numIndices;
  result.lodCount                  = (uint32_t)outLods.size();
  result.materialID                = m->mMaterialIndex;

  indexOffset += numIndices;
  vertexOffset += numVertices;

  return result;
}

class VKMesh final
{
public:
  VKMesh(
      const std::unique_ptr<lvk::IContext>& ctx, const MeshData& meshData, const Scene& scene,
	  lvk::Format colorFormat,
      lvk::Format depthFormat, uint32_t numSamples = 1,lvk ::Holder<lvk::ShaderModuleHandle>&& vert = {},
      lvk::Holder<lvk::ShaderModuleHandle>&& frag = {})
  : ctx(ctx)
  , numIndices_((uint32_t)meshData.indexData.size())
  , numMeshes_((uint32_t)meshData.meshes.size())
  , textureFiles_(meshData.textureFiles)
  {
    const MeshFileHeader header = meshData.getMeshFileHeader();

    const uint32_t* indices   = meshData.indexData.data();
    const uint8_t* vertexData = meshData.vertexData.data();

    std::vector<GLTFMaterialDataGPU> materials;

    materials.reserve(meshData.materials.size());

    for (const auto& mat : meshData.materials) {
      materials.push_back(convertToGPUMaterial(ctx, mat, textureFiles_, textureCache_));
    }

    bufferVertices_ = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Vertex,
          .storage   = lvk::StorageType_Device,
          .size      = header.vertexDataSize,
          .data      = vertexData,
          .debugName = "Buffer: vertex" },
        nullptr);
    bufferIndices_ = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Index,
          .storage   = lvk::StorageType_Device,
          .size      = header.indexDataSize,
          .data      = indices,
          .debugName = "Buffer: index" },
        nullptr);
    bufferTransforms_ = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Storage,
          .storage   = lvk::StorageType_Device,
          .size      = scene.globalTransform.size() * sizeof(glm::mat4),
          .data      = scene.globalTransform.data(),
          .debugName = "Buffer: transforms" },
        nullptr);
    bufferMaterials_ = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Storage,
          .storage   = lvk::StorageType_Device,
          .size      = materials.size() * sizeof(decltype(materials)::value_type),
          .data      = materials.data(),
          .debugName = "Buffer: materials" },
        nullptr);

    std::vector<uint8_t> drawCommands;
    std::vector<DrawData> drawData;

    const uint32_t numCommands = header.meshCount;

    drawCommands.resize(sizeof(DrawIndexedIndirectCommand) * numCommands + sizeof(uint32_t));
    drawData.resize(numCommands);

    // store the number of draw commands in the very beginning of the buffer
    memcpy(drawCommands.data(), &numCommands, sizeof(numCommands));

    DrawIndexedIndirectCommand* cmd = std::launder(reinterpret_cast<DrawIndexedIndirectCommand*>(drawCommands.data() + sizeof(uint32_t)));
    DrawData* dd                    = drawData.data();

    LVK_ASSERT(scene.meshForNode.size() == numCommands);

    uint32_t ddIndex = 0;

    // prepare indirect commands buffer
    for (auto& i : scene.meshForNode) {
      const Mesh& mesh = meshData.meshes[i.second];

      const uint32_t lod = std::min(0u, mesh.lodCount - 1); // TODO: implement dynamic lod

      *cmd++ = {
        .count         = mesh.getLODIndicesCount(lod),
        .instanceCount = 1,
        .firstIndex    = mesh.indexOffset, // + mesh.lodOffset[lod],
        .baseVertex    = mesh.vertexOffset,
        .baseInstance  = ddIndex++,
      };
      *dd++ = {
        .transformId = i.first,
        .materialId  = mesh.materialID,
      };
    }

    bufferIndirect_ = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Indirect,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(DrawIndexedIndirectCommand) * numCommands + sizeof(uint32_t),
          .data      = drawCommands.data(),
          .debugName = "Buffer: indirect" },
        nullptr);

    bufferDrawData_ = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Storage,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(DrawData) * numCommands,
          .data      = drawData.data(),
          .debugName = "Buffer: drawData" },
        nullptr);

    vert_ = vert.valid() ? std::move(vert) : loadShaderModule(ctx, "Chapter08/02_SceneGraph/src/main.vert");
    frag_ = frag.valid() ? std::move(frag) : loadShaderModule(ctx, "Chapter08/02_SceneGraph/src/main.frag");

    pipeline_ = ctx->createRenderPipeline({
        .vertexInput = meshData.streams,
        .smVert      = vert_,
        .smFrag      = frag_,
        .color       = { { .format = colorFormat } },
        .depthFormat = depthFormat,
        .cullMode    = lvk::CullMode_None,
        .samplesCount = numSamples,
    });

    pipelineWireframe_ = ctx->createRenderPipeline({
        .vertexInput = meshData.streams,
        .smVert      = vert_,
        .smFrag      = frag_,
        .color       = { { .format = colorFormat } },
        .depthFormat = depthFormat,
        .cullMode    = lvk::CullMode_None,
        .polygonMode = lvk::PolygonMode_Line,
        .samplesCount = numSamples,
    });

    LVK_ASSERT(pipeline_.valid());
  }

  void draw(
      lvk::IContext& ctx, lvk::ICommandBuffer& buf, const mat4& view, const mat4& proj,
      lvk::TextureHandle texSkyboxIrradiance = {}, bool wireframe = false) const
  {
    buf.cmdBindIndexBuffer(bufferIndices_, lvk::IndexFormat_UI32);
    buf.cmdBindVertexBuffer(0, bufferVertices_);
    buf.cmdBindRenderPipeline(wireframe ? pipelineWireframe_ : pipeline_);
    buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
    const struct {
      mat4 viewProj;
      uint64_t bufferTransforms;
      uint64_t bufferDrawData;
      uint64_t bufferMaterials;
      uint32_t texSkyboxIrradiance;
    } pc = {
      .viewProj            = proj * view,
      .bufferTransforms    = ctx.gpuAddress(bufferTransforms_),
      .bufferDrawData      = ctx.gpuAddress(bufferDrawData_),
      .bufferMaterials     = ctx.gpuAddress(bufferMaterials_),
      .texSkyboxIrradiance = texSkyboxIrradiance.index(),
    };
    static_assert(sizeof(pc) <= 128);
    buf.cmdPushConstants(pc);
    buf.cmdDrawIndexedIndirect(bufferIndirect_, sizeof(uint32_t), numMeshes_);
    // buf.cmdDrawIndexedIndirectCount(bufferIndirect_, sizeof(uint32_t), bufferIndirect_, 0, numMeshes_,
    // sizeof(DrawIndexedIndirectCommand));
  }
  void updateGlobalTransforms(const mat4* data, size_t numMatrices) const
  {
    ctx->upload(bufferTransforms_, data, numMatrices * sizeof(mat4));
  }

  void updateMaterial(const Material* materials, int updateMaterialIndex) const
  {
    if (updateMaterialIndex < 0)
      return;
    const GLTFMaterialDataGPU mat = convertToGPUMaterial(ctx, materials[updateMaterialIndex], textureFiles_, textureCache_);
    ctx->upload(bufferMaterials_, &mat, sizeof(mat), sizeof(mat) * updateMaterialIndex);
  }

public:
  const std::unique_ptr<lvk::IContext>& ctx;

  uint32_t numIndices_ = 0;
  uint32_t numMeshes_  = 0;

  lvk::Holder<lvk::BufferHandle> bufferIndices_;
  lvk::Holder<lvk::BufferHandle> bufferVertices_;
  lvk::Holder<lvk::BufferHandle> bufferIndirect_;
  lvk::Holder<lvk::BufferHandle> bufferTransforms_;
  lvk::Holder<lvk::BufferHandle> bufferDrawData_;
  lvk::Holder<lvk::BufferHandle> bufferMaterials_;

  lvk::Holder<lvk::ShaderModuleHandle> vert_;
  lvk::Holder<lvk::ShaderModuleHandle> frag_;

  lvk::Holder<lvk::RenderPipelineHandle> pipeline_;
  lvk::Holder<lvk::RenderPipelineHandle> pipelineWireframe_;

  TextureFiles textureFiles_;
  mutable TextureCache textureCache_;
};
