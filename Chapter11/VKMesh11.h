#pragma once

#include "Chapter08/VKMesh08.h"

class VKIndirectBuffer11 final
{
public:
  VKIndirectBuffer11(
      const std::unique_ptr<lvk::IContext>& ctx, size_t maxDrawCommands, lvk::StorageType indirectBufferStorage = lvk::StorageType_Device)
  : ctx_(ctx)
  , drawCommands_(maxDrawCommands)
  {
    // Indirect buffer layout: | uint32_t: numCommands | DrawIndexedIndirectCommand | DrawIndexedIndirectCommand | ...
    bufferIndirect_ = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Indirect | lvk::BufferUsageBits_Storage,
          .storage   = indirectBufferStorage,
          .size      = sizeof(DrawIndexedIndirectCommand) * maxDrawCommands + sizeof(uint32_t),
          .debugName = "Buffer: indirect" },
        nullptr);
  }

  void uploadIndirectBuffer()
  {
    const uint32_t numCommands = drawCommands_.size();
    // store the number of draw commands in the very beginning of the indirect buffer
    ctx_->upload(bufferIndirect_, &numCommands, sizeof(uint32_t));
    ctx_->upload(bufferIndirect_, drawCommands_.data(), sizeof(VkDrawIndexedIndirectCommand) * numCommands, sizeof(uint32_t));
  };

  void selectTo(VKIndirectBuffer11& buf, const std::function<bool(const DrawIndexedIndirectCommand&)>& pred) const
  {
    buf.drawCommands_.clear();
    for (const auto& c : drawCommands_) {
      if (pred(c))
        buf.drawCommands_.push_back(c);
    }
    buf.uploadIndirectBuffer();
  }

  DrawIndexedIndirectCommand* getDrawIndexedIndirectCommandPtr() const
  {
    LVK_ASSERT(ctx_->getMappedPtr(bufferIndirect_));
    return std::launder(reinterpret_cast<DrawIndexedIndirectCommand*>(ctx_->getMappedPtr(bufferIndirect_) + sizeof(uint32_t)));
  }

public:
  const std::unique_ptr<lvk::IContext>& ctx_;

  lvk::Holder<lvk::BufferHandle> bufferIndirect_;

  std::vector<DrawIndexedIndirectCommand> drawCommands_;
};

class VKPipeline11 final
{
public:
  VKPipeline11(
      const std::unique_ptr<lvk::IContext>& ctx, const lvk::VertexInput& streams, lvk::Format colorFormat, lvk::Format depthFormat,
      uint32_t numSamples = 1, lvk ::Holder<lvk::ShaderModuleHandle>&& vert = {}, lvk::Holder<lvk::ShaderModuleHandle>&& frag = {})
  {
    vert_ = vert.valid() ? std::move(vert) : loadShaderModule(ctx, "Chapter08/02_SceneGraph/src/main.vert");
    frag_ = frag.valid() ? std::move(frag) : loadShaderModule(ctx, "Chapter08/02_SceneGraph/src/main.frag");

    pipeline_ = ctx->createRenderPipeline({
        .vertexInput      = streams,
        .smVert           = vert_,
        .smFrag           = frag_,
        .color            = { { .format = colorFormat } },
        .depthFormat      = depthFormat,
        .cullMode         = lvk::CullMode_None,
        .samplesCount     = numSamples,
        .minSampleShading = numSamples > 1 ? 0.25f : 0.0f,
    });

    pipelineWireframe_ = ctx->createRenderPipeline({
        .vertexInput  = streams,
        .smVert       = vert_,
        .smFrag       = frag_,
        .color        = { { .format = colorFormat } },
        .depthFormat  = depthFormat,
        .cullMode     = lvk::CullMode_None,
        .polygonMode  = lvk::PolygonMode_Line,
        .samplesCount = numSamples,
    });

    LVK_ASSERT(pipeline_.valid());
    LVK_ASSERT(pipelineWireframe_.valid());
  }

public:
  lvk::Holder<lvk::ShaderModuleHandle> vert_;
  lvk::Holder<lvk::ShaderModuleHandle> frag_;

  lvk::Holder<lvk::RenderPipelineHandle> pipeline_;
  lvk::Holder<lvk::RenderPipelineHandle> pipelineWireframe_;
};

class VKMesh11
{
public:
  VKMesh11(
      const std::unique_ptr<lvk::IContext>& ctx, const MeshData& meshData, const Scene& scene,
      lvk::StorageType indirectBufferStorage = lvk::StorageType_Device, bool preloadMaterials = true)
  : ctx(ctx)
  , numIndices_((uint32_t)meshData.indexData.size())
  , numMeshes_((uint32_t)meshData.meshes.size())
  , indirectBuffer_(ctx, meshData.getMeshFileHeader().meshCount, indirectBufferStorage)
  , textureFiles_(meshData.textureFiles)
  {
    const MeshFileHeader header = meshData.getMeshFileHeader();

    const uint32_t* indices   = meshData.indexData.data();
    const uint8_t* vertexData = meshData.vertexData.data();

    materialsCPU_ = meshData.materials;
    materialsGPU_.reserve(meshData.materials.size());

    for (const auto& mat : meshData.materials) {
      materialsGPU_.push_back(preloadMaterials ? convertToGPUMaterial(ctx, mat, textureFiles_, textureCache_) : GLTFMaterialDataGPU{});
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
          .size      = meshData.materials.size() * sizeof(decltype(materialsGPU_)::value_type),
          .data      = materialsGPU_.data(),
          .debugName = "Buffer: materials" },
        nullptr);

    const uint32_t numCommands = header.meshCount;

    indirectBuffer_.drawCommands_.resize(numCommands);
    drawData_.resize(numCommands);

    DrawIndexedIndirectCommand* cmd = indirectBuffer_.drawCommands_.data();
    DrawData* dd                    = drawData_.data();

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
    indirectBuffer_.uploadIndirectBuffer();

    bufferDrawData_ = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Storage,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(DrawData) * numCommands,
          .data      = drawData_.data(),
          .debugName = "Buffer: drawData" },
        nullptr);
  }

  void draw(
      lvk::ICommandBuffer& buf, const VKPipeline11& pipeline, const mat4& view, const mat4& proj,
      lvk::TextureHandle texSkyboxIrradiance = {}, bool wireframe = false, const VKIndirectBuffer11* indirectBuffer = nullptr) const
  {
    buf.cmdBindIndexBuffer(bufferIndices_, lvk::IndexFormat_UI32);
    buf.cmdBindVertexBuffer(0, bufferVertices_);
    buf.cmdBindRenderPipeline(wireframe ? pipeline.pipelineWireframe_ : pipeline.pipeline_);
    buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
    const struct {
      mat4 viewProj;
      uint64_t bufferTransforms;
      uint64_t bufferDrawData;
      uint64_t bufferMaterials;
      uint32_t texSkyboxIrradiance;
    } pc = {
      .viewProj            = proj * view,
      .bufferTransforms    = ctx->gpuAddress(bufferTransforms_),
      .bufferDrawData      = ctx->gpuAddress(bufferDrawData_),
      .bufferMaterials     = ctx->gpuAddress(bufferMaterials_),
      .texSkyboxIrradiance = texSkyboxIrradiance.index(),
    };
    static_assert(sizeof(pc) <= 128);
    buf.cmdPushConstants(pc);
    if (!indirectBuffer)
      indirectBuffer = &indirectBuffer_;
    buf.cmdDrawIndexedIndirectCount(
        indirectBuffer->bufferIndirect_, sizeof(uint32_t), indirectBuffer->bufferIndirect_, 0, numMeshes_,
        sizeof(DrawIndexedIndirectCommand));
  }

  void draw(
      lvk::ICommandBuffer& buf, const VKPipeline11& pipeline, const void* pushConstants, size_t pcSize,
      const lvk::DepthState depthState = { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true }, bool wireframe = false,
      const VKIndirectBuffer11* indirectBuffer = nullptr) const
  {
    buf.cmdBindIndexBuffer(bufferIndices_, lvk::IndexFormat_UI32);
    buf.cmdBindVertexBuffer(0, bufferVertices_);
    buf.cmdBindRenderPipeline(wireframe ? pipeline.pipelineWireframe_ : pipeline.pipeline_);
    buf.cmdBindDepthState(depthState);
    buf.cmdPushConstants(pushConstants, pcSize);
    if (!indirectBuffer)
      indirectBuffer = &indirectBuffer_;
    buf.cmdDrawIndexedIndirectCount(
        indirectBuffer->bufferIndirect_, sizeof(uint32_t), indirectBuffer->bufferIndirect_, 0, numMeshes_,
        sizeof(DrawIndexedIndirectCommand));
  }

  DrawIndexedIndirectCommand* getDrawIndexedIndirectCommandPtr() const { return indirectBuffer_.getDrawIndexedIndirectCommandPtr(); };

public:
  const std::unique_ptr<lvk::IContext>& ctx;

  uint32_t numIndices_ = 0;
  uint32_t numMeshes_  = 0;

  lvk::Holder<lvk::BufferHandle> bufferIndices_;
  lvk::Holder<lvk::BufferHandle> bufferVertices_;
  lvk::Holder<lvk::BufferHandle> bufferTransforms_;
  lvk::Holder<lvk::BufferHandle> bufferDrawData_;
  lvk::Holder<lvk::BufferHandle> bufferMaterials_;

  std::vector<DrawData> drawData_;

  VKIndirectBuffer11 indirectBuffer_;

  TextureFiles textureFiles_;
  mutable TextureCache textureCache_;

  std::vector<Material> materialsCPU_;
  std::vector<GLTFMaterialDataGPU> materialsGPU_;
};
