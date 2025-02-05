#include "shared/VulkanApp.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

int main()
{
  VulkanApp app;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  {
    lvk::Holder<lvk::TextureHandle> texture = ctx->createTexture({
        .type       = lvk::TextureType_2D,
        .format     = lvk::Format_RGBA_UN8,
        .dimensions = {1280, 720},
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "Texture: compute",
    });

    lvk::Holder<lvk::ShaderModuleHandle> comp = loadShaderModule(ctx, "Chapter05/08_ComputeTexture/src/main.comp");

    lvk::Holder<lvk::ComputePipelineHandle> pipelineComputeMatrices = ctx->createComputePipeline({
        .smComp = comp,
    });

    LVK_ASSERT(pipelineComputeMatrices.valid());

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "data/shaders/Quad.vert");
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "data/shaders/Quad.frag");

    lvk::Holder<lvk::RenderPipelineHandle> pipelineFullScreenQuad = ctx->createRenderPipeline({
        .smVert = vert,
        .smFrag = frag,
        .color  = { { .format = ctx->getSwapchainFormat() } },
    });

    LVK_ASSERT(pipelineFullScreenQuad.valid());

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
      const lvk::RenderPass renderPass = {
        .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
      };

      const lvk::Framebuffer framebuffer = {
        .color = { { .texture = ctx->getCurrentSwapchainTexture() } },
      };

      lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
      {
        const struct {
          uint32_t textureId;
          float time;
        } pc{
          .textureId = texture.index(),
          .time      = (float)glfwGetTime(),
        };        
        buf.cmdBindComputePipeline(pipelineComputeMatrices);
        buf.cmdPushConstants(pc);
        buf.cmdDispatchThreadGroups({ .width = 1280 / 16, .height = 720 / 16 });
        buf.cmdBeginRendering(renderPass, framebuffer, { .textures = { { lvk::TextureHandle(texture) } } });
        buf.cmdBindRenderPipeline(pipelineFullScreenQuad);
        buf.cmdPushConstants(pc);
        buf.cmdDraw(3);
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
