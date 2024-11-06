#include "shared/VulkanApp.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

/// should contain at least two elements
std::deque<std::pair<uint32_t, uint32_t>> g_MorphQueue = { { 5, 8 }, { 5, 8 } };

/// morphing between two torus knots 0..1
float g_MorphCoef = 0.0f;
float g_AnimationSpeed = 1.0f;

bool g_UseColoredMesh = false;

constexpr uint32_t kNumU = 768;
constexpr uint32_t kNumV = 768;

void generateIndices(uint32_t* indices)
{
  for (uint32_t j = 0; j < kNumV - 1; j++) {
    for (uint32_t i = 0; i < kNumU - 1; i++) {
      uint32_t ofs = (j * (kNumU - 1) + i) * 6;

      uint32_t i1 = (j + 0) * kNumU + (i + 0);
      uint32_t i2 = (j + 0) * kNumU + (i + 1);
      uint32_t i3 = (j + 1) * kNumU + (i + 1);
      uint32_t i4 = (j + 1) * kNumU + (i + 0);

      indices[ofs + 0] = i1;
      indices[ofs + 1] = i2;
      indices[ofs + 2] = i4;

      indices[ofs + 3] = i2;
      indices[ofs + 4] = i3;
      indices[ofs + 5] = i4;
    }
  }
}

float easing(float x)
{
  return (x < 0.5) ? (4 * x * x * (3 * x - 1)) : (4 * (x - 1) * (x - 1) * (3 * (x - 1) + 1) + 1);
}

void renderGUI(lvk::TextureHandle texture)
{
  // Each torus knot is specified by a pair of coprime integers p and q.
  // https://en.wikipedia.org/wiki/Torus_knot
  static const std::vector<std::pair<uint32_t, uint32_t>> PQ = {
    {1, 0},
    {2, 3},
    {2, 5},
    {2, 7},
    {3, 4},
    {2, 9},
    {3, 5},
    {5, 8},
    {8, 9},
  };

  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing);
  ImGui::Begin("Torus Knot params", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  {
    ImGui::Checkbox("Use colored mesh", &g_UseColoredMesh);
    ImGui::SliderFloat("Morph animation speed", &g_AnimationSpeed, 0.0f, 2.0f);

    for (size_t i = 0; i != PQ.size(); i++) {
      const std::string title = std::to_string(PQ[i].first) + ", " + std::to_string(PQ[i].second);
      if (ImGui::Button(title.c_str(), ImVec2(128, 0))) {
        if (PQ[i] != g_MorphQueue.back())
          g_MorphQueue.push_back(PQ[i]);
      }
    }
    ImGui::Text("Morph queue:");
    for (size_t i = 0; i != g_MorphQueue.size(); i++) {
      const bool isLastElement = i == 1;
      if (isLastElement) {
        ImGui::Text("  P = %u, Q = %u  <---  t = %.2f", g_MorphQueue[i].first, g_MorphQueue[i].second, g_MorphCoef);
      } else {
        ImGui::Text("  P = %u, Q = %u", g_MorphQueue[i].first, g_MorphQueue[i].second);
      }
    }
  }
  ImGui::End();

  if (!g_UseColoredMesh) {
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    const float dim   = std::max(size.x, size.y);
    const ImVec2 sizeImg(0.25f * dim, 0.25f * dim);

    ImGui::SetNextWindowPos(ImVec2(size.x - sizeImg.x - 25, 0), ImGuiCond_Appearing);
    ImGui::Begin("Texture", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    {
      ImGui::Image(texture.index(), sizeImg);
    }
    ImGui::End();
  }
}

int main()
{
  VulkanApp app;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  {
    std::vector<uint32_t> indicesGen((kNumU - 1) * (kNumV - 1) * 6);

    generateIndices(indicesGen.data());

    const uint32_t vertexBufferSize = 12 * sizeof(float) * kNumU * kNumV;
    const uint32_t indexBufferSize  = sizeof(uint32_t) * (kNumU - 1) * (kNumV - 1) * 6;

    lvk::Holder<lvk::TextureHandle> texture = ctx->createTexture({
        .type       = lvk::TextureType_2D,
        .format     = lvk::Format_RGBA_UN8,
        .dimensions = {1024, 1024},
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "Texture: compute",
    });

    lvk::Holder<lvk::BufferHandle> bufferVertex = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_Storage,
        .storage   = lvk::StorageType_Device,
        .size      = vertexBufferSize,
        .debugName = "Buffer: vertex",
    });
    lvk::Holder<lvk::BufferHandle> bufferIndex  = ctx->createBuffer({
         .usage     = lvk::BufferUsageBits_Index,
         .storage   = lvk::StorageType_Device,
         .size      = indicesGen.size() * sizeof(uint32_t),
         .data      = indicesGen.data(),
         .debugName = "Buffer: index",
    });

    lvk::Holder<lvk::ShaderModuleHandle> compMesh    = loadShaderModule(ctx, "Chapter05/09_ComputeMesh/src/main_mesh.comp");
    lvk::Holder<lvk::ShaderModuleHandle> compTexture = loadShaderModule(ctx, "Chapter05/09_ComputeMesh/src/main_texture.comp");

    lvk::Holder<lvk::ComputePipelineHandle> pipelineComputeMesh    = ctx->createComputePipeline({
           .smComp = compMesh,
    });
    lvk::Holder<lvk::ComputePipelineHandle> pipelineComputeTexture = ctx->createComputePipeline({
        .smComp = compTexture,
    });

    LVK_ASSERT(pipelineComputeMesh.valid());
    LVK_ASSERT(pipelineComputeTexture.valid());

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter05/09_ComputeMesh/src/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> geom = loadShaderModule(ctx, "Chapter05/09_ComputeMesh/src/main.geom");
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter05/09_ComputeMesh/src/main.frag");

    const lvk::VertexInput vdesc = {
      .attributes    = { { .location = 0, .format = lvk::VertexFormat::Float4, .offset = 0 },
                         { .location = 1, .format = lvk::VertexFormat::Float4, .offset = sizeof(vec4) },
                         { .location = 2, .format = lvk::VertexFormat::Float4, .offset = sizeof(vec4)+sizeof(vec4) }, },
      .inputBindings = { { .stride = sizeof(vec4) + sizeof(vec4) + sizeof(vec4) } },
    };

    const uint32_t specColored = 1;
    const uint32_t specNotColored = 0;

    lvk::Holder<lvk::RenderPipelineHandle> pipelineMeshColored = ctx->createRenderPipeline({
        .vertexInput = vdesc,
        .smVert      = vert,
        .smGeom      = geom,
        .smFrag      = frag,
        .specInfo    = { .entries = { { .constantId = 0, .size = sizeof(uint32_t) } }, .data = &specColored, .dataSize = sizeof(specColored) },
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
    });
    lvk::Holder<lvk::RenderPipelineHandle> pipelineMeshTextured = ctx->createRenderPipeline({
        .vertexInput = vdesc,
        .smVert      = vert,
        .smGeom      = geom,
        .smFrag      = frag,
        .specInfo    = { .entries = { { .constantId = 0, .size = sizeof(uint32_t) } }, .data = &specNotColored, .dataSize = sizeof(specNotColored) },
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
    });

    LVK_ASSERT(pipelineMeshColored.valid());
    LVK_ASSERT(pipelineMeshTextured.valid());

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
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
        const mat4 m = glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -18.f));
        const mat4 p = glm::perspective(45.0f, aspectRatio, 0.1f, 1000.0f);

        auto iter = g_MorphQueue.begin();

        struct PerFrame {
          mat4 mvp;
          uint64_t buffer;
          uint32_t textureId;
          float time;
          uint32_t numU, numV;
          float minU, maxU;
          float minV, maxV;
          uint32_t p1, p2;
          uint32_t q1, q2;
          float morph;
        } pc = {
          .mvp       = p * m,
          .buffer    = ctx->gpuAddress(bufferVertex),
          .textureId = texture.index(),
          .time      = (float)glfwGetTime(),
          .numU      = kNumU,
          .numV      = kNumU,
          .minU      = -1.0f,
          .maxU      = +1.0f,
          .minV      = -1.0f,
          .maxV      = +1.0f,
          .p1        = iter->first,
          .p2        = (iter + 1)->first,
          .q1        = iter->second,
          .q2        = (iter + 1)->second,
          .morph     = easing(g_MorphCoef),
        };

        static_assert(sizeof(PerFrame) <= 128);

        buf.cmdBindComputePipeline(pipelineComputeMesh);
        buf.cmdPushConstants(pc);
        buf.cmdDispatchThreadGroups({ .width = (kNumU * kNumV) / 16 }, { .buffers = { { lvk::BufferHandle(bufferVertex) } } });
        if (!g_UseColoredMesh) {
          buf.cmdBindComputePipeline(pipelineComputeTexture);
          buf.cmdDispatchThreadGroups({ .width = 1024 / 16, .height = 1024 / 16 }, { .textures = { { lvk::TextureHandle(texture) } } });
        }
        buf.cmdBeginRendering(
            renderPass, framebuffer,
            { .textures = { { lvk::TextureHandle(texture) } }, .buffers = { { lvk::BufferHandle(bufferVertex) } } });
        buf.cmdBindRenderPipeline(g_UseColoredMesh ? pipelineMeshColored : pipelineMeshTextured);
        buf.cmdPushConstants(pc);
        buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
        buf.cmdBindVertexBuffer(0, bufferVertex);
        buf.cmdBindIndexBuffer(bufferIndex, lvk::IndexFormat_UI32);
        buf.cmdDrawIndexed(indicesGen.size());
        app.imgui_->beginFrame(framebuffer);
        renderGUI(texture);
        app.drawFPS();
        app.imgui_->endFrame(buf);
        buf.cmdEndRendering();
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());

      g_MorphCoef += g_AnimationSpeed * deltaSeconds;

      if (g_MorphCoef > 1.0) {
        g_MorphCoef = 1.0f;
        if (g_MorphQueue.size() > 2) {
          g_MorphCoef = 0.0f;
          g_MorphQueue.pop_front();
        }
      }
    });
  }

  ctx.release();

  return 0;
}
