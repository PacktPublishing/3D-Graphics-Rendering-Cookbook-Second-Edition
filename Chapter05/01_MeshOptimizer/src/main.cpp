#include "shared/VulkanApp.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <meshoptimizer.h>

int main()
{
  VulkanApp app;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  const lvk::VertexInput vdesc = {
    .attributes    = { { .location = 0, .format = lvk::VertexFormat::Float3, .offset = 0 } },
    .inputBindings = { { .stride = sizeof(vec3) } },
  };

  lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter05/01_MeshOptimizer/src/main.vert");
  lvk::Holder<lvk::ShaderModuleHandle> geom = loadShaderModule(ctx, "Chapter05/01_MeshOptimizer/src/main.geom");
  lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter05/01_MeshOptimizer/src/main.frag");

  lvk::Holder<lvk::RenderPipelineHandle> pipeline = ctx->createRenderPipeline({
      .vertexInput = vdesc,
      .smVert      = vert,
      .smGeom      = geom,
      .smFrag      = frag,
      .color       = { { .format = ctx->getSwapchainFormat() } },
      .depthFormat = app.getDepthFormat(),
      .cullMode    = lvk::CullMode_Back,
  });

  const lvk::DepthState dState = { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true };

  LVK_ASSERT(pipeline.valid());

  const aiScene* scene = aiImportFile("data/rubber_duck/scene.gltf", aiProcess_Triangulate);

  if (!scene || !scene->HasMeshes()) {
    printf("Unable to load data/rubber_duck/scene.gltf\n");
    exit(255);
  }

  const aiMesh* mesh = scene->mMeshes[0];
  std::vector<vec3> positions;
  std::vector<uint32_t> indices;
  for (unsigned int i = 0; i != mesh->mNumVertices; i++) {
    const aiVector3D v = mesh->mVertices[i];
    positions.push_back(vec3(v.x, v.y, v.z));
  }

  for (unsigned int i = 0; i != mesh->mNumFaces; i++) {
    for (int j = 0; j != 3; j++) {
      indices.push_back(mesh->mFaces[i].mIndices[j]);
    }
  }

  aiReleaseImport(scene);

  std::vector<uint32_t> indicesLod;
  {
    std::vector<uint32_t> remap(indices.size());
    const size_t vertexCount =
        meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), positions.data(), indices.size(), sizeof(vec3));

    std::vector<uint32_t> remappedIndices(indices.size());
    std::vector<vec3> remappedVertices(vertexCount);

    meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(), remap.data());
    meshopt_remapVertexBuffer(remappedVertices.data(), positions.data(), positions.size(), sizeof(vec3), remap.data());

    meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), indices.size(), vertexCount);
    meshopt_optimizeOverdraw(
        remappedIndices.data(), remappedIndices.data(), indices.size(), glm::value_ptr(remappedVertices[0]), vertexCount, sizeof(vec3),
        1.05f);
    meshopt_optimizeVertexFetch(
        remappedVertices.data(), remappedIndices.data(), indices.size(), remappedVertices.data(), vertexCount, sizeof(vec3));

    const float threshold           = 0.2f;
    const size_t target_index_count = size_t(remappedIndices.size() * threshold);
    const float target_error        = 1e-2f;

    indicesLod.resize(remappedIndices.size());
    indicesLod.resize(meshopt_simplify(
        &indicesLod[0], remappedIndices.data(), remappedIndices.size(), &remappedVertices[0].x, vertexCount, sizeof(vec3),
        target_index_count, target_error));

    indices   = remappedIndices;
    positions = remappedVertices;
  }

  lvk::Holder<lvk::BufferHandle> vertexBuffer = ctx->createBuffer(
      { .usage     = lvk::BufferUsageBits_Vertex,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(vec3) * positions.size(),
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
  lvk::Holder<lvk::BufferHandle> indexBufferLod = ctx->createBuffer(
      { .usage     = lvk::BufferUsageBits_Index,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(uint32_t) * indicesLod.size(),
        .data      = indicesLod.data(),
        .debugName = "Buffer: index LOD" },
      nullptr);

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 m  = glm::rotate(mat4(1.0f), glm::radians(-90.0f), vec3(1, 0, 0));
    const mat4 v1 = glm::rotate(glm::translate(mat4(1.0f), vec3(-0.5f, -0.5f, -1.5f)), (float)glfwGetTime(), vec3(0.0f, 1.0f, 0.0f));
    const mat4 v2 = glm::rotate(glm::translate(mat4(1.0f), vec3(+0.5f, -0.5f, -1.5f)), (float)glfwGetTime(), vec3(0.0f, 1.0f, 0.0f));
    const mat4 p  = glm::perspective(45.0f, aspectRatio, 0.1f, 1000.0f);

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
      buf.cmdBeginRendering(renderPass, framebuffer);
      {
        buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
        {
          buf.cmdBindVertexBuffer(0, vertexBuffer, 0);
          buf.cmdBindRenderPipeline(pipeline);
          buf.cmdBindDepthState(dState);
          buf.cmdPushConstants(p * v1 * m);
          buf.cmdBindIndexBuffer(indexBuffer, lvk::IndexFormat_UI32);
          buf.cmdDrawIndexed(indices.size());
          buf.cmdPushConstants(p * v2 * m);
          buf.cmdBindIndexBuffer(indexBufferLod, lvk::IndexFormat_UI32);
          buf.cmdDrawIndexed(indicesLod.size());
        }
        buf.cmdPopDebugGroupLabel();
        buf.cmdEndRendering();
      }
    }
    ctx->submit(buf, ctx->getCurrentSwapchainTexture());
  });

  ctx.release();

  return 0;
}
