#include <filesystem>

#include "shared/VulkanApp.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <meshoptimizer.h>

#include "shared/Scene/VtxData.h"

constexpr bool g_calculateLODs = false;

void processLods(std::vector<uint32_t>& indices, std::vector<float>& vertices, std::vector<std::vector<uint32_t>>& outLods)
{
  const size_t verticesCountIn = vertices.size() / 2;

  size_t targetIndicesCount = indices.size();

  uint8_t LOD = 1;

  printf("\n   LOD0: %i indices", int(indices.size()));

  outLods.push_back(indices);

  while (targetIndicesCount > 1024 && LOD < 8) {
    targetIndicesCount = indices.size() / 2;

    bool sloppy = false;

    size_t numOptIndices = meshopt_simplify(
        indices.data(), indices.data(), (uint32_t)indices.size(), vertices.data(), verticesCountIn, sizeof(float) * 3, targetIndicesCount,
        0.02f);

    // cannot simplify further
    if (static_cast<size_t>(numOptIndices * 1.1f) > indices.size()) {
      if (LOD > 1) {
        // try harder
        numOptIndices = meshopt_simplifySloppy(
            indices.data(), indices.data(), indices.size(), vertices.data(), verticesCountIn, sizeof(float) * 3, targetIndicesCount, 0.02f);
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
}

Mesh convertAIMesh(const aiMesh* m, MeshData& meshData, uint32_t& indexOffset, uint32_t& vertexOffset)
{
  static_assert(sizeof(aiVector3D) == 3 * sizeof(float));

  const bool hasTexCoords = m->HasTextureCoords(0);

  // Original data for LOD calculation
  std::vector<float> srcVertices;
  std::vector<uint32_t> srcIndices;

  std::vector<std::vector<uint32_t>> outLods;

  std::vector<uint8_t>& vertices = meshData.vertexData;

  for (size_t i = 0; i != m->mNumVertices; i++) {
    const aiVector3D v = m->mVertices[i];
    const aiVector3D n = m->mNormals[i];
    const aiVector2D t = hasTexCoords ? aiVector2D(m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y) : aiVector2D();

    if (g_calculateLODs) {
      srcVertices.push_back(v.x);
      srcVertices.push_back(v.y);
      srcVertices.push_back(v.z);
    }

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

  if (!g_calculateLODs)
    outLods.push_back(srcIndices);
  else
    processLods(srcIndices, srcVertices, outLods);

  printf("\nCalculated LOD count: %u\n", (unsigned)outLods.size());

  Mesh result = {
    .indexOffset  = indexOffset,
    .vertexOffset = vertexOffset,
    .vertexCount  = m->mNumVertices,
  };

  uint32_t numIndices = 0;
  for (size_t l = 0; l < outLods.size(); l++) {
    for (size_t i = 0; i < outLods[l].size(); i++)
      meshData.indexData.push_back(outLods[l][i]);

    result.lodOffset[l] = numIndices;
    numIndices += (int)outLods[l].size();
  }

  result.lodOffset[outLods.size()] = numIndices;
  result.lodCount                  = (uint32_t)outLods.size();

  indexOffset += numIndices;
  vertexOffset += m->mNumVertices;

  return result;
}

void loadMeshFile(const char* fileName, MeshData& meshData)
{
  printf("Loading '%s'...\n", fileName);

  const unsigned int flags = 0 | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                             aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                             aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                             aiProcess_GenUVCoords;

  const aiScene* scene = aiImportFile(fileName, flags);

  if (!scene || !scene->HasMeshes()) {
    printf("Unable to load '%s'\n", fileName);
    exit(255);
  }

  meshData.meshes.reserve(scene->mNumMeshes);
  meshData.boxes.reserve(scene->mNumMeshes);

  uint32_t indexOffset = 0;
  uint32_t vertexOffset = 0;

  for (unsigned int i = 0; i != scene->mNumMeshes; i++) {
    printf("\nConverting meshes %u/%u...", i + 1, scene->mNumMeshes);
    fflush(stdout);
    meshData.meshes.push_back(convertAIMesh(scene->mMeshes[i], meshData, indexOffset, vertexOffset));
  }

  recalculateBoundingBoxes(meshData);
}

struct DrawIndexedIndirectCommand {
  uint32_t count;
  uint32_t instanceCount;
  uint32_t firstIndex;
  uint32_t baseVertex;
  uint32_t baseInstance;
};

class VKMesh final
{
public:
  VKMesh(
      const std::unique_ptr<lvk::IContext>& ctx, const MeshFileHeader& header, const MeshData& meshData, lvk::Format depthFormat)
  : numIndices_(header.indexDataSize / sizeof(uint32_t))
  {
    const uint32_t* indices = meshData.indexData.data();
    const uint8_t* vertexData = meshData.vertexData.data();

    bufferVertices_ = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Vertex,
          .storage   = lvk::StorageType_Device,
          .size      = header.vertexDataSize,
          .data      = vertexData,
          .debugName = "Buffer: vertex" });
    bufferIndices_ = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Index,
          .storage   = lvk::StorageType_Device,
          .size      = header.indexDataSize,
          .data      = indices,
          .debugName = "Buffer: index" });

    std::vector<uint8_t> drawCommands;

    const uint32_t numCommands = header.meshCount;

    drawCommands.resize(sizeof(DrawIndexedIndirectCommand) * numCommands + sizeof(uint32_t));

    // store the number of draw commands in the very beginning of the buffer
    memcpy(drawCommands.data(), &numCommands, sizeof(numCommands));

    DrawIndexedIndirectCommand* cmd = std::launder(reinterpret_cast<DrawIndexedIndirectCommand*>(drawCommands.data() + sizeof(uint32_t)));

    // prepare indirect commands buffer
    for (uint32_t i = 0; i != numCommands; i++) {
      *cmd++ = {
        .count         = meshData.meshes[i].getLODIndicesCount(0),
        .instanceCount = 1,
        .firstIndex    = meshData.meshes[i].indexOffset,
        .baseVertex    = meshData.meshes[i].vertexOffset,
        .baseInstance  = 0,
      };
    }

    bufferIndirect_ = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Indirect,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(DrawIndexedIndirectCommand) * numCommands + sizeof(uint32_t),
          .data      = drawCommands.data(),
          .debugName = "Buffer: indirect" });

    vert_ = loadShaderModule(ctx, "Chapter05/07_MeshRenderer/src/main.vert");
    geom_ = loadShaderModule(ctx, "Chapter05/07_MeshRenderer/src/main.geom");
    frag_ = loadShaderModule(ctx, "Chapter05/07_MeshRenderer/src/main.frag");

    pipeline_ = ctx->createRenderPipeline({
        .vertexInput = meshData.streams,
        .smVert      = vert_,
        .smGeom      = geom_,
        .smFrag      = frag_,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = depthFormat,
        .cullMode    = lvk::CullMode_Back,
    });

    LVK_ASSERT(pipeline_.valid());
  }

  void draw(lvk::ICommandBuffer& buf, const MeshFileHeader& header, const mat4& mvp) const
  {
    buf.cmdBindIndexBuffer(bufferIndices_, lvk::IndexFormat_UI32);
    buf.cmdBindVertexBuffer(0, bufferVertices_);
    buf.cmdBindRenderPipeline(pipeline_);
    buf.cmdPushConstants(mvp);
    buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
    buf.cmdDrawIndexedIndirect(bufferIndirect_, sizeof(uint32_t), header.meshCount);
    // buf.cmdDrawIndexedIndirectCount(bufferIndirect_, sizeof(uint32_t), bufferIndirect_, 0, header.meshCount,
    // sizeof(DrawIndexedIndirectCommand));
  }

private:
  uint32_t numIndices_;

  lvk::Holder<lvk::BufferHandle> bufferIndices_;
  lvk::Holder<lvk::BufferHandle> bufferVertices_;
  lvk::Holder<lvk::BufferHandle> bufferIndirect_;

  lvk::Holder<lvk::ShaderModuleHandle> vert_;
  lvk::Holder<lvk::ShaderModuleHandle> geom_;
  lvk::Holder<lvk::ShaderModuleHandle> frag_;

  lvk::Holder<lvk::RenderPipelineHandle> pipeline_;
};

const char* meshMeshes = ".cache/ch05_bistro.meshes";

int main()
{
  if (!isMeshDataValid(meshMeshes)) {
    printf("No cached mesh data found. Precaching...\n\n");

    MeshData meshData;

    loadMeshFile("deps/src/bistro/Exterior/exterior.obj", meshData);

    saveMeshData(meshMeshes, meshData);
  }

  MeshData meshData;
  const MeshFileHeader header = loadMeshData(meshMeshes, meshData);

  VulkanApp app({
      .initialCameraPos    = vec3(-14.894f, 5.743f, -5.527f),
      .initialCameraTarget = vec3(0, 2.5f, 0),
  });

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  {
    const VKMesh mesh(ctx, header, meshData, app.getDepthFormat());

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
      const mat4 p = glm::perspective(45.0f, aspectRatio, 0.1f, 1000.0f);

      const lvk::RenderPass renderPass = {
        .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
        .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
      };

      const lvk::Framebuffer framebuffer = {
        .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
        .depthStencil = { .texture = app.getDepthTexture() },
      };

      const mat4 mvp = p * app.camera_.getViewMatrix() * glm::scale(mat4(1.0f), vec3(0.01f));

      lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
      {
        buf.cmdBeginRendering(renderPass, framebuffer);
        buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
        mesh.draw(buf, header, mvp);
        buf.cmdPopDebugGroupLabel();
        app.drawGrid(buf, p, vec3(0, -0.0f, 0));
        app.imgui_->beginFrame(framebuffer);
        app.drawFPS();
        app.imgui_->endFrame(buf);
        buf.cmdEndRendering();
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());
    });
  }

  ctx.release();

  return 0;
}
