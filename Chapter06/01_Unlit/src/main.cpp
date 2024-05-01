#include "shared/VulkanApp.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

int main()
{
  VulkanApp app;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  {
    const aiScene* scene = aiImportFile("deps/src/glTF-Sample-Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", aiProcess_Triangulate);

    if (!scene || !scene->HasMeshes()) {
      printf("Unable to load DamagedHelmet.gltf\n");
      exit(255);
    }

    const aiMesh* mesh = scene->mMeshes[0];

    struct Vertex {
      vec3 position;
      vec4 color;
      vec2 uv;
    };

    std::vector<Vertex> vertices;
	 vertices.reserve(mesh->mNumVertices);
    for (unsigned int i = 0; i != mesh->mNumVertices; i++) {
      const aiVector3D v = mesh->mVertices[i];
      const aiColor4D  c = mesh->mColors[0] ? mesh->mColors[0][i] : aiColor4D(1, 1, 1, 1);
      const aiVector3D t = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);
      vertices.push_back({
          .position = vec3(v.x, v.y, v.z),
          .color    = vec4(c.r, c.g, c.b, c.a),
          .uv       = vec2(t.x, 1.0f - t.y),
      });
    }
    std::vector<uint32_t> indices;
	 indices.reserve(3 * mesh->mNumFaces);
    for (unsigned int i = 0; i != mesh->mNumFaces; i++) {
      for (int j = 0; j != 3; j++) {
        indices.push_back(mesh->mFaces[i].mIndices[j]);
      }
    }

    aiReleaseImport(scene);

    lvk::Holder<lvk::TextureHandle> baseColorTexture =
        loadTexture(ctx, "deps/src/glTF-Sample-Assets/Models/DamagedHelmet/glTF/Default_albedo.jpg");
    if (baseColorTexture.empty()) {
      exit(255);
    }

    lvk::Holder<lvk::BufferHandle> vertexBuffer = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Vertex,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(Vertex) * vertices.size(),
          .data      = vertices.data(),
          .debugName = "Buffer: vertex" });
    lvk::Holder<lvk::BufferHandle> indexBuffer = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Index,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(uint32_t) * indices.size(),
          .data      = indices.data(),
          .debugName = "Buffer: index" });

    const lvk::VertexInput vdesc = {
      .attributes    = { { .location = 0, .format = lvk::VertexFormat::Float3, .offset = 0  },
                       {   .location = 1, .format = lvk::VertexFormat::Float4, .offset = sizeof(vec3) },
							  {   .location = 2, .format = lvk::VertexFormat::Float2, .offset = sizeof(vec3) + sizeof(vec4) }, },
      .inputBindings = { { .stride = sizeof(Vertex) }, },
    };

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter06/01_Unlit/src/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter06/01_Unlit/src/main.frag");

    lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid = ctx->createRenderPipeline({
        .vertexInput = vdesc,
        .smVert      = vert,
        .smFrag      = frag,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
        .cullMode    = lvk::CullMode_Back,
    });

    LVK_ASSERT(pipelineSolid.valid());

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
      const mat4 m1 = glm::rotate(mat4(1.0f), glm::radians(+90.0f), vec3(1, 0, 0));
      const mat4 m2 = glm::rotate(mat4(1.0f), (float)glfwGetTime(), vec3(0.0f, 1.0f, 0.0f));
      const mat4 v = app.camera_.getViewMatrix();
      const mat4 p = glm::perspective(45.0f, aspectRatio, 0.1f, 1000.0f);

      struct PerFrameData {
        mat4 mvp;
        vec4 baseColor;
        uint32_t baseTextureId;
      } perFrameData = {
        .mvp           = p * v * m2 * m1,
        .baseColor     = vec4(1, 1, 1, 1),
        .baseTextureId = baseColorTexture.index(),
      };

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
            buf.cmdBindIndexBuffer(indexBuffer, lvk::IndexFormat_UI32);
            buf.cmdBindRenderPipeline(pipelineSolid);
            buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
            buf.cmdPushConstants(perFrameData);
            buf.cmdDrawIndexed(indices.size());
          }
          buf.cmdPopDebugGroupLabel();
          app.drawGrid(buf, p, vec3(0, -1.0f, 0));
          app.imgui_->beginFrame(framebuffer);
          app.drawFPS();
          app.drawMemo();
          app.imgui_->endFrame(buf);
          buf.cmdEndRendering();
        }
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());
    });
  }

  ctx.release();

  return 0;
}
