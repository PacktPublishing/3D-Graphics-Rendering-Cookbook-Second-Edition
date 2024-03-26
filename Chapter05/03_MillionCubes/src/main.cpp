#include "shared/VulkanApp.h"

int main()
{
  VulkanApp app;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  {
    const uint32_t texWidth  = 256;
    const uint32_t texHeight = 256;
    std::vector<uint32_t> pixels(texWidth * texHeight);
    for (uint32_t y = 0; y != texHeight; y++) {
      for (uint32_t x = 0; x != texWidth; x++) {
        // create a XOR pattern
        pixels[y * texWidth + x] = 0xFF000000 + ((x ^ y) << 16) + ((x ^ y) << 8) + (x ^ y);
      }
    }

    lvk::Holder<lvk::TextureHandle> texture = ctx->createTexture({
        .type       = lvk::TextureType_2D,
        .format     = lvk::Format_BGRA_UN8,
        .dimensions = {texWidth, texHeight},
        .usage      = lvk::TextureUsageBits_Sampled,
        .data       = pixels.data(),
        .debugName  = "XOR pattern",
    });

    const uint32_t kNumCubes = 1 * 1024 * 1024;

    std::vector<vec4> centers(kNumCubes);

    for (vec4& p : centers) {
      p = vec4(glm::linearRand(-vec3(500.0f), +vec3(500.0f)), glm::linearRand(0.0f, 3.14159f));
    }

    lvk::Holder<lvk::BufferHandle> bufferPosAngle = ctx->createBuffer({
        .usage   = lvk::BufferUsageBits_Storage,
        .storage = lvk::StorageType_Device,
        .size    = sizeof(vec4) * kNumCubes,
        .data    = centers.data(),
    });

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter05/03_MillionCubes/src/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter05/03_MillionCubes/src/main.frag");

    lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid = ctx->createRenderPipeline({
        .smVert      = vert,
        .smFrag      = frag,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
    });

    LVK_ASSERT(pipelineSolid.valid());

    const lvk::DepthState dState = { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true };

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
        buf.cmdBeginRendering(renderPass, framebuffer);

        const mat4 view = translate(mat4(1.0f), vec3(0.0f, 0.0f, -1000.0f + 500.0f * (1.0f - cos(-glfwGetTime() * 0.5f))));

        const struct {
          mat4 viewproj;
          uint32_t textureId;
          uint64_t bufferPosAngle;
          float time;
        } pc{
          .viewproj       = proj * view,
          .textureId      = texture.index(),
          .bufferPosAngle = ctx->gpuAddress(bufferPosAngle),
          .time           = (float)glfwGetTime(),
        };
        buf.cmdPushDebugGroupLabel("Solid cube", 0xff0000ff);
        buf.cmdBindRenderPipeline(pipelineSolid);
        buf.cmdPushConstants(pc);
        buf.cmdBindDepthState(dState);
        buf.cmdDraw(36, kNumCubes);
        buf.cmdPopDebugGroupLabel();
        buf.cmdEndRendering();
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());
    });
  }

  ctx.release();

  return 0;
}
