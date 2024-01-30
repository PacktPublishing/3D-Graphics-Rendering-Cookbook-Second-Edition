#include "shared/VulkanApp.h"

int main()
{
  VulkanApp app({
      .initialCameraPos    = {0.0f, 1.0f, 1.0f},
      .initialCameraTarget = {0.0f, 0.0f, 0.0f},
  });

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "data/shaders/Grid.vert");
  lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "data/shaders/Grid.frag");

  lvk::Holder<lvk::RenderPipelineHandle> pipeline = ctx->createRenderPipeline({
      .smVert      = vert,
      .smFrag      = frag,
      .color       = { {
                .format            = ctx->getSwapchainFormat(),
                .blendEnabled      = true,
                .srcRGBBlendFactor = lvk::BlendFactor_SrcAlpha,
                .dstRGBBlendFactor = lvk::BlendFactor_OneMinusSrcAlpha,
      } },
      .depthFormat = app.getDepthFormat(),
  });

  LVK_ASSERT(pipeline.valid());

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
      buf.cmdBeginRendering(renderPass, framebuffer);
      {
        buf.cmdPushDebugGroupLabel("Grid", 0xff0000ff);
        {
          buf.cmdBindRenderPipeline(pipeline);
          buf.cmdBindDepthState({});
          struct {
            mat4 mvp;
            vec4 camPos;
				vec4 origin;
          } pc = {
            .mvp    = glm::perspective(45.0f, aspectRatio, 0.1f, 1000.0f) * app.camera_.getViewMatrix(),
            .camPos = vec4(app.camera_.getPosition(), 1.0f),
            .origin = vec4(0.0f),
          };
          buf.cmdPushConstants(pc);
          buf.cmdDraw(6);
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
