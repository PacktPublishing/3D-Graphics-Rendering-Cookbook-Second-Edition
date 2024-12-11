#include "shared/VulkanApp.h"

#include "Chapter10/Bistro.h"
#include "Chapter10/Skybox.h"

int main()
{
  MeshData meshData;
  Scene scene;
  loadBistro(meshData, scene);

  VulkanApp app({
      .initialCameraPos    = vec3(-19.261f, 8.465f, -7.317f),
      .initialCameraTarget = vec3(0, +2.5f, 0),
  });

  app.positioner_.maxSpeed_ = 1.5f;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  lvk::Holder<lvk::ShaderModuleHandle> compSSAO        = loadShaderModule(ctx, "Chapter10/04_SSAO/src/SSAO.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineSSAO = ctx->createComputePipeline({
      .smComp = compSSAO,
  });

  const uint32_t kHorizontal = 1;
  const uint32_t kVertical   = 0;

  lvk::Holder<lvk::ShaderModuleHandle> compBlur         = loadShaderModule(ctx, "data/shaders/Blur.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineBlurX = ctx->createComputePipeline({
      .smComp   = compBlur,
      .specInfo = {.entries = { { .constantId = 0, .size = sizeof(uint32_t) } }, .data = &kHorizontal, .dataSize = sizeof(uint32_t)},
  });
  lvk::Holder<lvk::ComputePipelineHandle> pipelineBlurY = ctx->createComputePipeline({
      .smComp   = compBlur,
      .specInfo = {.entries = { { .constantId = 0, .size = sizeof(uint32_t) } }, .data = &kVertical, .dataSize = sizeof(uint32_t)},
  });

  lvk::Holder<lvk::ShaderModuleHandle> vertCombine       = loadShaderModule(ctx, "data/shaders/QuadFlip.vert");
  lvk::Holder<lvk::ShaderModuleHandle> fragCombine       = loadShaderModule(ctx, "Chapter10/04_SSAO/src/combine.frag");
  lvk::Holder<lvk::RenderPipelineHandle> pipelineCombine = ctx->createRenderPipeline({
      .smVert = vertCombine,
      .smFrag = fragCombine,
      .color  = { { .format = ctx->getSwapchainFormat() } },
  });

  lvk::Holder<lvk::TextureHandle> texSSAO   = ctx->createTexture({
        .format     = ctx->getSwapchainFormat(),
        .dimensions = ctx->getDimensions(ctx->getCurrentSwapchainTexture()),
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "texSSAO",
  });
  lvk::Holder<lvk::TextureHandle> texBlur[] = {
    ctx->createTexture({
        .format     = ctx->getSwapchainFormat(),
        .dimensions = ctx->getDimensions(ctx->getCurrentSwapchainTexture()),
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "texBlur0",
    }),
    ctx->createTexture({
        .format     = ctx->getSwapchainFormat(),
        .dimensions = ctx->getDimensions(ctx->getCurrentSwapchainTexture()),
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "texBlur1",
    }),
  };

  const lvk::Dimensions sizeFb        = ctx->getDimensions(ctx->getCurrentSwapchainTexture());
  const lvk::Dimensions sizeOffscreen = { sizeFb.width, sizeFb.height };

  const uint32_t kNumSamples = 8;

  lvk::Holder<lvk::TextureHandle> msaaColor = ctx->createTexture({
      .format     = ctx->getSwapchainFormat(),
      .dimensions = sizeFb,
      .numSamples = kNumSamples,
      .usage      = lvk::TextureUsageBits_Attachment,
      .storage    = lvk::StorageType_Memoryless,
      .debugName  = "msaaColor",
  });
  lvk::Holder<lvk::TextureHandle> msaaDepth = ctx->createTexture({
      .format     = app.getDepthFormat(),
      .dimensions = sizeFb,
      .numSamples = kNumSamples,
      .usage      = lvk::TextureUsageBits_Attachment,
      .storage    = lvk::StorageType_Memoryless,
      .debugName  = "msaaDepth",
  });

  lvk::Holder<lvk::TextureHandle> offscreenColor = ctx->createTexture({
      .format     = ctx->getSwapchainFormat(),
      .dimensions = sizeOffscreen,
      .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
      .debugName  = "offscreenColor",
  });
  lvk::Holder<lvk::TextureHandle> offscreenDepth = ctx->createTexture({
      .format     = app.getDepthFormat(),
      .dimensions = sizeOffscreen,
      .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
      .debugName  = "offscreenDepth",
  });

  lvk::Holder<lvk::TextureHandle> texRotations = loadTexture(ctx, "data/rot_texture.bmp");

  lvk::Holder<lvk::SamplerHandle> samplerClamp = ctx->createSampler({
      .wrapU = lvk::SamplerWrap_Clamp,
      .wrapV = lvk::SamplerWrap_Clamp,
      .wrapW = lvk::SamplerWrap_Clamp,
  });

  bool drawWireframe = false;
  bool enableBlur    = true;
  int numBlurPasses  = 1;

  enum DrawMode {
    DrawMode_ColorSSAO = 0,
    DrawMode_Color     = 1,
    DrawMode_SSAO      = 2,
  };

  int drawMode         = DrawMode_ColorSSAO;
  float depthThreshold = 30.0f; // bilateral blur

  struct {
    uint32_t texDepth;
    uint32_t texRotation;
    uint32_t texOut;
    uint32_t sampler;
    float zNear;
    float zFar;
    float radius;
    float attScale;
    float distScale;
  } pcSSAO = {
    .texDepth    = offscreenDepth.index(),
    .texRotation = texRotations.index(),
    .texOut      = texSSAO.index(),
    .sampler     = samplerClamp.index(),
    .zNear       = 0.01f,
    .zFar        = 1000.0f,
    .radius      = 0.03f,
    .attScale    = 0.95f,
    .distScale   = 1.7f,
  };

  struct {
    uint32_t texColor;
    uint32_t texSSAO;
    uint32_t sampler;
    float scale;
    float bias;
  } pcCombine = {
    .texColor = offscreenColor.index(),
    .texSSAO  = texSSAO.index(),
    .sampler  = samplerClamp.index(),
    .scale    = 1.5f,
    .bias     = 0.16f,
  };

  const Skybox skyBox(
      ctx, "data/immenstadter_horn_2k_prefilter.ktx", "data/immenstadter_horn_2k_irradiance.ktx", ctx->getSwapchainFormat(),
      app.getDepthFormat(), kNumSamples);
  const VKMesh mesh(ctx, meshData, scene, ctx->getSwapchainFormat(), app.getDepthFormat(), kNumSamples);

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 view = app.camera_.getViewMatrix();
    const mat4 proj = glm::perspective(45.0f, aspectRatio, pcSSAO.zNear, pcSSAO.zFar);

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      // 1. Render scene
      buf.cmdBeginRendering(
          lvk::RenderPass{
              .color = { { .loadOp = lvk::LoadOp_Clear, .storeOp = lvk::StoreOp_MsaaResolve, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
              .depth = { .loadOp = lvk::LoadOp_Clear, .storeOp = lvk::StoreOp_MsaaResolve, .clearDepth = 1.0f }
      },
          lvk::Framebuffer{
              .color        = { { .texture = msaaColor, .resolveTexture = offscreenColor } },
              .depthStencil = { .texture = msaaDepth, .resolveTexture = offscreenDepth },
          });
      skyBox.draw(buf, view, proj);
      {
        buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
        mesh.draw(buf, view, proj, skyBox.texSkyboxIrradiance, drawWireframe);
        buf.cmdPopDebugGroupLabel();
      }
      app.drawGrid(buf, proj, vec3(0, -1.0f, 0), kNumSamples);
      buf.cmdEndRendering();

      // 2. Compute SSAO
      buf.cmdBindComputePipeline(pipelineSSAO);
      buf.cmdPushConstants(pcSSAO);
      buf.cmdDispatchThreadGroups(
          {
              .width  = 1 + (uint32_t)sizeFb.width / 16,
              .height = 1 + (uint32_t)sizeFb.height / 16,
      },
          { .textures = {
                lvk::TextureHandle(offscreenDepth),
                lvk::TextureHandle(texSSAO),
            } });

      // 3. Blur SSAO
      if (enableBlur) {
        const lvk::Dimensions blurDim = {
          .width  = 1 + (uint32_t)sizeFb.width / 16,
          .height = 1 + (uint32_t)sizeFb.height / 16,
        };
        struct BlurPC {
          uint32_t texDepth;
          uint32_t texIn;
          uint32_t texOut;
          float depthThreshold;
        };
        struct BlurPass {
          lvk::TextureHandle texIn;
          lvk::TextureHandle texOut;
        };
        std::vector<BlurPass> passes;
        {
          passes.reserve(2 * numBlurPasses);
          passes.push_back({ texSSAO, texBlur[0] });
          for (int i = 0; i != numBlurPasses - 1; i++) {
            passes.push_back({ texBlur[0], texBlur[1] });
            passes.push_back({ texBlur[1], texBlur[0] });
          }
          passes.push_back({ texBlur[0], texSSAO });
        }
        for (uint32_t i = 0; i != passes.size(); i++) {
          const BlurPass p = passes[i];
          buf.cmdBindComputePipeline(i & 1 ? pipelineBlurX : pipelineBlurY);
          buf.cmdPushConstants(BlurPC{
              .texDepth       = offscreenDepth.index(),
              .texIn          = p.texIn.index(),
              .texOut         = p.texOut.index(),
              .depthThreshold = pcSSAO.zFar * depthThreshold,
          });
          buf.cmdDispatchThreadGroups(
              blurDim, {
                           .textures = {p.texIn, p.texOut, lvk::TextureHandle(offscreenDepth)}
          });
        }
      }

      // 3. Render scene with SSAO into the swapchain image
      if (drawMode == DrawMode_SSAO) {
        buf.cmdCopyImage(texSSAO, ctx->getCurrentSwapchainTexture(), ctx->getDimensions(offscreenColor));
      } else if (drawMode == DrawMode_Color) {
        buf.cmdCopyImage(offscreenColor, ctx->getCurrentSwapchainTexture(), ctx->getDimensions(offscreenColor));
      }

      const lvk::RenderPass renderPassMain = {
        .color = { { .loadOp = lvk::LoadOp_Load, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
      };
      const lvk::Framebuffer framebufferMain = {
        .color = { { .texture = ctx->getCurrentSwapchainTexture() } },
      };

      buf.cmdBeginRendering(renderPassMain, framebufferMain, { .textures = { lvk::TextureHandle(texSSAO) } });

      if (drawMode == DrawMode_ColorSSAO) {
        buf.cmdBindRenderPipeline(pipelineCombine);
        buf.cmdPushConstants(pcCombine);
        buf.cmdBindDepthState({});
        buf.cmdDraw(3);
      }

      app.imgui_->beginFrame(framebufferMain);
      app.drawFPS();
      app.drawMemo();

      // render UI
      {
        const ImGuiViewport* v  = ImGui::GetMainViewport();
        const float windowWidth = v->WorkSize.x / 5;
        ImGui::SetNextWindowPos(ImVec2(10, 200));
        ImGui::Begin(
            "SSAO", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Checkbox("Draw wireframe", &drawWireframe);
        ImGui::Checkbox("Enable blur", &enableBlur);
        ImGui::BeginDisabled(!enableBlur);
        ImGui::SliderFloat("Blur depth threshold", &depthThreshold, 0.0f, 50.0f);
        ImGui::SliderInt("Blur num passes", &numBlurPasses, 1, 5);
        ImGui::EndDisabled();
        ImGui::Text("Draw mode:");
        const float indentSize = 16.0f;
        ImGui::Indent(indentSize);
        ImGui::RadioButton("Color + SSAO", &drawMode, DrawMode_ColorSSAO);
        ImGui::RadioButton("Color only", &drawMode, DrawMode_Color);
        ImGui::RadioButton("SSAO only", &drawMode, DrawMode_SSAO);
        ImGui::Unindent(indentSize);
        ImGui::Separator();
        ImGui::BeginDisabled(drawMode != DrawMode_ColorSSAO);
        ImGui::SliderFloat("SSAO scale", &pcCombine.scale, 0.0f, 2.0f);
        ImGui::SliderFloat("SSAO bias", &pcCombine.bias, 0.0f, 0.3f);
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::BeginDisabled(drawMode == DrawMode_Color);
        ImGui::SliderFloat("SSAO radius", &pcSSAO.radius, 0.01f, 0.1f);
        ImGui::SliderFloat("SSAO attenuation scale", &pcSSAO.attScale, 0.5f, 1.5f);
        ImGui::SliderFloat("SSAO distance scale", &pcSSAO.distScale, 0.0f, 2.0f);
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::Image(texSSAO.index(), ImVec2(windowWidth, windowWidth / aspectRatio));
        ImGui::End();
      }

      app.imgui_->endFrame(buf);

      buf.cmdEndRendering();
    }
    ctx->submit(buf, ctx->getCurrentSwapchainTexture());
  });

  ctx.release();

  return 0;
}
