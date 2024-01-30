#include "shared/VulkanApp.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

int main()
{
  VulkanApp app;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  {
    lvk::Holder<lvk::TextureHandle> texture = loadTexture(ctx, "data/rubber_duck/textures/Duck_baseColor.png");

    const uint32_t kNumMeshes = 32 * 1024;

    std::vector<vec4> centers(kNumMeshes);

    for (vec4& p : centers) {
      p = vec4(glm::linearRand(-vec3(500.0f), +vec3(500.0f)), glm::linearRand(0.0f, 3.14159f));
    }

    lvk::Holder<lvk::BufferHandle> bufferPosAngle   = ctx->createBuffer({
          .usage     = lvk::BufferUsageBits_Storage,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(vec4) * kNumMeshes,
          .data      = centers.data(),
          .debugName = "Buffer: angles & positions",
    });
    lvk::Holder<lvk::BufferHandle> bufferMatrices[] = {
      ctx->createBuffer({
          .usage     = lvk::BufferUsageBits_Storage,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(mat4) * kNumMeshes,
          .debugName = "Buffer: matrices 1",
      }),
      ctx->createBuffer({
          .usage     = lvk::BufferUsageBits_Storage,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(mat4) * kNumMeshes,
          .debugName = "Buffer: matrices 2",
      }),
    };

    const aiScene* scene = aiImportFile("data/rubber_duck/scene.gltf", aiProcess_Triangulate);

    if (!scene || !scene->HasMeshes()) {
      printf("Unable to load data/rubber_duck/scene.gltf\n");
      exit(255);
    }

    struct Vertex {
      vec3 pos;
      vec2 uv;
      vec3 n;
    };

    static_assert(sizeof(Vertex) == 8 * sizeof(float));

    const aiMesh* mesh = scene->mMeshes[0];
    std::vector<Vertex> positions;
    std::vector<uint32_t> indices;
    for (unsigned int i = 0; i != mesh->mNumVertices; i++) {
      const aiVector3D v = mesh->mVertices[i];
      const aiVector3D t = mesh->mTextureCoords[0][i];
      const aiVector3D n = mesh->mNormals[i];
      positions.push_back({ .pos = vec3(v.x, v.y, v.z), .uv = vec2(t.x, t.y), .n = vec3(n.x, n.y, n.z) });
    }

    for (unsigned int i = 0; i != mesh->mNumFaces; i++) {
      for (int j = 0; j != 3; j++) {
        indices.push_back(mesh->mFaces[i].mIndices[j]);
      }
    }

    aiReleaseImport(scene);

    lvk::Holder<lvk::BufferHandle> vertexBuffer = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Storage,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(Vertex) * positions.size(),
          .data      = positions.data(),
          .debugName = "Buffer: vertex" },
        nullptr);
    lvk::Holder<lvk::BufferHandle> indexBuffer = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Index,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(uint32_t) * indices.size(),
          .data      = indices.data(),
          .debugName = "Buffer: index" },
        nullptr);

    lvk::Holder<lvk::ShaderModuleHandle> comp = loadShaderModule(ctx, "Chapter04/04_InstancedMeshes/src/main.comp");

    lvk::Holder<lvk::ComputePipelineHandle> pipelineComputeMatrices = ctx->createComputePipeline({
        .smComp = comp,
    });

    LVK_ASSERT(pipelineComputeMatrices.valid());

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter04/04_InstancedMeshes/src/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter04/04_InstancedMeshes/src/main.frag");

    lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid = ctx->createRenderPipeline({
        .smVert      = vert,
        .smFrag      = frag,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
        .cullMode    = lvk::CullMode_Back,
    });

    LVK_ASSERT(pipelineSolid.valid());

    uint32_t frameId = 0;

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
      const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.2f, 1500.0f);

      const lvk::RenderPass renderPass = {
        .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
        .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
      };

      const lvk::Framebuffer framebuffer = {
        .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
        .depthStencil = { .texture = app.getDepthTexture() },
      };

      lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
      {
        const mat4 view = translate(mat4(1.0f), vec3(0.0f, 0.0f, -1000.0f + 500.0f * (1.0f - cos(-glfwGetTime() * 0.5f))));

        const struct {
          mat4 viewproj;
          uint32_t textureId;
          uint64_t bufferPosAngle;
          uint64_t bufferMatrices;
          uint64_t bufferVertices;
          float time;
        } pc{
          .viewproj       = proj * view,
          .textureId      = texture.index(),
          .bufferPosAngle = ctx->gpuAddress(bufferPosAngle),
          .bufferMatrices = ctx->gpuAddress(bufferMatrices[frameId]),
          .bufferVertices = ctx->gpuAddress(vertexBuffer),
          .time           = (float)glfwGetTime(),
        };
        buf.cmdPushConstants(pc);
        buf.cmdBindComputePipeline(pipelineComputeMatrices);
        buf.cmdDispatchThreadGroups({ .width = kNumMeshes / 64 });
        buf.cmdBeginRendering(renderPass, framebuffer, { .buffers = { lvk::BufferHandle(bufferMatrices[frameId]) } });
        buf.cmdPushDebugGroupLabel("Solid cube", 0xff0000ff);
        buf.cmdBindRenderPipeline(pipelineSolid);
        buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
        buf.cmdBindIndexBuffer(indexBuffer, lvk::IndexFormat_UI32);
        buf.cmdDrawIndexed(indices.size(), kNumMeshes);
        buf.cmdPopDebugGroupLabel();
        buf.cmdEndRendering();
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());
      frameId = (frameId + 1) & 1;
    });
  }

  ctx.release();

  return 0;
}
