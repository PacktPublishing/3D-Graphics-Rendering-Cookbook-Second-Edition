#include "shared/VulkanApp.h"

#include "shared/Utils.h"

#include <math.h>

struct VertexData {
  float pos[3];
};

const float t = (1.0f + sqrtf(5.0f)) / 2.0f;

const VertexData vertices[] = {
  {-1,  t,  0},
  { 1,  t,  0},
  {-1, -t,  0},
  { 1, -t,  0},

  { 0, -1,  t},
  { 0,  1,  t},
  { 0, -1, -t},
  { 0,  1, -t},

  { t,  0, -1},
  { t,  0,  1},
  {-t,  0, -1},
  {-t,  0,  1},
};

const uint16_t indices[] = { 0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4,  11, 10, 2,  10, 7, 6, 7, 1, 8,
                             3, 9,  4, 3, 4, 2, 3, 2, 6, 3, 6, 8,  3, 8,  9,  4, 9, 5, 2, 4,  11, 6,  2,  10, 8,  6, 7, 9, 8, 1 };

int main()
{
  VulkanApp app({
      .initialCameraPos    = vec3(0.0f, 3.0f, -4.5f),
      .initialCameraTarget = vec3(0.0f, t, 0.0f),
  });

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  // 0. Vertices/indices
  lvk::Holder<lvk::BufferHandle> bufferIndices  = ctx->createBuffer({
       .usage     = lvk::BufferUsageBits_Index,
       .storage   = lvk::StorageType_Device,
       .size      = sizeof(indices),
       .data      = indices,
       .debugName = "Buffer: indices",
  });
  lvk::Holder<lvk::BufferHandle> bufferVertices = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Vertex,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(vertices),
      .data      = vertices,
      .debugName = "Buffer: vertices",
  });

  // 1. Shaders & pipeline
  lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter10/01_OffscreenRendering/src/main.vert");
  lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter10/01_OffscreenRendering/src/main.frag");

  const lvk::VertexInput vdesc = {
    .attributes    = { { .location = 0, .format = lvk::VertexFormat::Float3 } },
    .inputBindings = { { .stride = sizeof(VertexData) } },
  };

  lvk::Holder<lvk::RenderPipelineHandle> pipeline = ctx->createRenderPipeline({
      .vertexInput = vdesc,
      .smVert      = vert,
      .smFrag      = frag,
      .color       = { { .format = ctx->getSwapchainFormat() } },
      .depthFormat = app.getDepthFormat(),
  });

  // 2. Textures and texture views
  constexpr uint8_t numMipLevels = lvk::calcNumMipLevels(512, 512);

  lvk::Holder<lvk::TextureHandle> texture = ctx->createTexture({
      .type         = lvk::TextureType_2D,
      .format       = lvk::Format_RGBA_UN8,
      .dimensions   = {512, 512},
      .usage        = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
      .numMipLevels = numMipLevels,
      .debugName    = "Texture",
  });

  lvk::Holder<lvk::TextureHandle> mipViews[numMipLevels];

  for (uint32_t l = 0; l != numMipLevels; l++) {
    mipViews[l] = ctx->createTextureView(texture, { .mipLevel = l });
  }

  const vec3 colors[10] = {
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1},

    {1, 1, 0},
    {0, 1, 1},
    {1, 0, 1},

    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1},

    {0, 0, 0},
  };
  LVK_ASSERT(LVK_ARRAY_NUM_ELEMENTS(colors) == numMipLevels);
  // generate custom mip-pyramid
  lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
  for (uint8_t i = 0; i != numMipLevels; i++) {
    buf.cmdBeginRendering(lvk::RenderPass {
        .color = {
          {.loadOp = lvk::LoadOp_Clear, .level = i, .clearColor = {colors[i].r,colors[i].g, colors[i].b,1 }},
        }
      },
        lvk::Framebuffer{ .color = { { .texture = texture }} });
    buf.cmdEndRendering();
  }
  ctx->submit(buf);

  float modelAngle = 0;
  bool rotateModel = true;

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    if (rotateModel)
      modelAngle = fmodf(modelAngle - deltaSeconds, 2.0f * M_PI);

    const mat4 view  = app.camera_.getViewMatrix();
    const mat4 proj  = glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 1000.0f);
    const mat4 model = glm::rotate(glm::translate(mat4(1.0f), vec3(0, t, 0)), modelAngle, vec3(1.0f, 1.0f, 1.0f));

    const lvk::Framebuffer framebuffer = {
      .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
      .depthStencil = { .texture = app.getDepthTexture() },
    };

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    buf.cmdBindVertexBuffer(0, bufferVertices);
    buf.cmdBindIndexBuffer(bufferIndices, lvk::IndexFormat_UI16);
    // 2. Render scene
    buf.cmdBeginRendering(
        lvk::RenderPass{
            .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
            .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
    },
        framebuffer);
    buf.cmdBindRenderPipeline(pipeline);
    buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
    {
      buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
      const struct PushConstants {
        mat4 mvp;
        uint32_t texture;
      } pc = {
        .mvp     = proj * view * model,
        .texture = texture.index(),
      };
      buf.cmdPushConstants(pc);
      buf.cmdDrawIndexed(LVK_ARRAY_NUM_ELEMENTS(indices));
      buf.cmdPopDebugGroupLabel();
    }
    app.drawGrid(buf, proj, vec3(0, -0.01f, 0));
    app.imgui_->beginFrame(framebuffer);
    app.drawFPS();
    app.drawMemo();

    const ImGuiViewport* v = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(10, 200));
    ImGui::Begin("Control", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Checkbox("Rotate model", &rotateModel);
    ImGui::Separator();
    ImGui::Text("Mip-pyramid 512x512");
    const float windowWidth = v->WorkSize.x / 5;
    for (uint32_t l = 0; l != LVK_ARRAY_NUM_ELEMENTS(mipViews); l++) {
      ImGui::Image(mipViews[l].index(), ImVec2((int)windowWidth >> l, ((int)windowWidth >> l)));
    }
    ImGui::Separator();
    ImGui::End();

    app.imgui_->endFrame(buf);

    buf.cmdEndRendering();
    ctx->submit(buf, ctx->getCurrentSwapchainTexture());
  });

  ctx.release();

  return 0;
}
