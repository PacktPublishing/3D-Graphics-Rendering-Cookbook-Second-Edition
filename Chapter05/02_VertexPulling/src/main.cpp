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

    const aiScene* scene = aiImportFile("data/rubber_duck/scene.gltf", aiProcess_Triangulate);

    if (!scene || !scene->HasMeshes()) {
      printf("Unable to load data/rubber_duck/scene.gltf\n");
      exit(255);
    }

    struct Vertex {
      vec3 pos;
      vec2 uv;
    };

    static_assert(sizeof(Vertex) == 5 * sizeof(float));

    const aiMesh* mesh = scene->mMeshes[0];
    std::vector<Vertex> positions;
    std::vector<uint32_t> indices;
    for (unsigned int i = 0; i != mesh->mNumVertices; i++) {
      const aiVector3D v = mesh->mVertices[i];
      const aiVector3D t = mesh->mTextureCoords[0][i];
      positions.push_back({ .pos = vec3(v.x, v.y, v.z), .uv = vec2(t.x, t.y) });
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
          .debugName = "Buffer: vertex" });
    lvk::Holder<lvk::BufferHandle> indexBuffer = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Index,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(uint32_t) * indices.size(),
          .data      = indices.data(),
          .debugName = "Buffer: index" });

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter05/02_VertexPulling/src/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> geom = loadShaderModule(ctx, "Chapter05/02_VertexPulling/src/main.geom");
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter05/02_VertexPulling/src/main.frag");

    lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid = ctx->createRenderPipeline({
        .smVert      = vert,
        .smGeom      = geom,
        .smFrag      = frag,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
        .cullMode    = lvk::CullMode_Back,
    });

    LVK_ASSERT(pipelineSolid.valid());

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
      const mat4 m = glm::rotate(mat4(1.0f), glm::radians(-90.0f), vec3(1, 0, 0));
      const mat4 v = glm::rotate(glm::translate(mat4(1.0f), vec3(0.0f, -0.5f, -1.5f)), (float)glfwGetTime(), vec3(0.0f, 1.0f, 0.0f));
      const mat4 p = glm::perspective(45.0f, aspectRatio, 0.1f, 1000.0f);

      const lvk::RenderPass renderPass = {
        .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
        .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
      };

      const lvk::Framebuffer framebuffer = {
        .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
        .depthStencil = { .texture = app.getDepthTexture() },
      };

      const struct PushConstants {
        mat4 mvp;
        uint64_t vertices;
        uint32_t texture;
      } pc{
        .mvp      = p * v * m,
        .vertices = ctx->gpuAddress(vertexBuffer),
        .texture  = texture.index(),
      };

      lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
      {
        buf.cmdBeginRendering(renderPass, framebuffer);
        buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
        {
          buf.cmdBindIndexBuffer(indexBuffer, lvk::IndexFormat_UI32);
          buf.cmdBindRenderPipeline(pipelineSolid);
          buf.cmdPushConstants(pc);
          buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
          buf.cmdDrawIndexed(indices.size());
        }
        buf.cmdPopDebugGroupLabel();
        buf.cmdEndRendering();
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());
    });
  }

  ctx.release();

  return 0;
}
