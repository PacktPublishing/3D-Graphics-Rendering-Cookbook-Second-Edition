#include <lvk/LVK.h>

#include <GLFW/glfw3.h>

#include <shared/Utils.h>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/version.h>

#include <stdio.h>
#include <stdlib.h>

#include <vector>

using glm::mat4;
using glm::vec3;

int main()
{
  minilog::initialize(nullptr, { .threadNames = false });

  GLFWwindow* window = nullptr;
  std::unique_ptr<lvk::IContext> ctx;
  lvk::Holder<lvk::TextureHandle> depthTexture;
  {
    int width  = -95;
    int height = -90;

    window       = lvk::initWindow("Simple example", width, height);
    ctx          = lvk::createVulkanContextWithSwapchain(window, width, height, {});
    depthTexture = ctx->createTexture({
        .type       = lvk::TextureType_2D,
        .format     = lvk::Format_Z_F32,
        .dimensions = {(uint32_t)width, (uint32_t)height},
        .usage      = lvk::TextureUsageBits_Attachment,
        .debugName  = "Depth buffer",
    });
  }

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

  const lvk::VertexInput vdesc = {
    .attributes    = { { .location = 0, .format = lvk::VertexFormat::Float3, .offset = 0 } },
    .inputBindings = { { .stride = sizeof(vec3) } },
  };

  lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter03/01_Assimp/src/main.vert");
  lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter03/01_Assimp/src/main.frag");

  lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid = ctx->createRenderPipeline({
      .vertexInput = vdesc,
      .smVert      = vert,
      .smFrag      = frag,
      .color       = { { .format = ctx->getSwapchainFormat() } },
      .depthFormat = ctx->getFormat(depthTexture),
      .cullMode    = lvk::CullMode_Back,
  });

   // the storage must be alive until the pipeline exists
  const uint32_t isWireframe = 1;

  lvk::Holder<lvk::RenderPipelineHandle> pipelineWireframe = ctx->createRenderPipeline({
      .vertexInput = vdesc,
      .smVert      = vert,
      .smFrag      = frag,
      .specInfo = { .entries = { { .constantId = 0, .size = sizeof(uint32_t) } }, .data = &isWireframe, .dataSize = sizeof(isWireframe) },
      .color       = { { .format = ctx->getSwapchainFormat() } },
      .depthFormat = ctx->getFormat(depthTexture),
      .cullMode    = lvk::CullMode_Back,
      .polygonMode = lvk::PolygonMode_Line,
  });

  const lvk::DepthState dState = { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true };

  LVK_ASSERT(pipelineSolid.valid());
  LVK_ASSERT(pipelineWireframe.valid());

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if (!width || !height)
      continue;
    const float ratio = width / (float)height;

    const mat4 m = glm::rotate(mat4(1.0f), glm::radians(-90.0f), vec3(1, 0, 0));
    const mat4 v = glm::rotate(glm::translate(mat4(1.0f), vec3(0.0f, -0.5f, -1.5f)), (float)glfwGetTime(), vec3(0.0f, 1.0f, 0.0f));
    const mat4 p = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);

    const lvk::RenderPass renderPass = {
      .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
      .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
    };

    const lvk::Framebuffer framebuffer = {
      .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
      .depthStencil = { .texture = depthTexture },
    };

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      buf.cmdBeginRendering(renderPass, framebuffer);
      {
        buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
        {
          buf.cmdBindVertexBuffer(0, vertexBuffer);
          buf.cmdBindIndexBuffer(indexBuffer, lvk::IndexFormat_UI32);
          buf.cmdBindRenderPipeline(pipelineSolid);
          buf.cmdBindDepthState(dState);
          buf.cmdPushConstants(p * v * m);
          buf.cmdDrawIndexed(indices.size());
          buf.cmdBindRenderPipeline(pipelineWireframe);
          buf.cmdSetDepthBias(0.0f, -1.0f, 0.0f);
          buf.cmdDrawIndexed(indices.size());
        }
        buf.cmdPopDebugGroupLabel();
        buf.cmdEndRendering();
      }
    }
    ctx->submit(buf, ctx->getCurrentSwapchainTexture());
  }

  vert.reset();
  frag.reset();
  depthTexture.reset();
  pipelineSolid.reset();
  pipelineWireframe.reset();
  vertexBuffer.reset();
  indexBuffer.reset();
  ctx.reset();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}

