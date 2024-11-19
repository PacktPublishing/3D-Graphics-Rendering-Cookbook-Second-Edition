#include "shared/VulkanApp.h"

#include "Chapter10/Bistro.h"
#include "Chapter10/Skybox.h"

#include <implot/implot.h>

// Uchimura 2017, "HDR theory and practice"
// Math: https://www.desmos.com/calculator/gslcdxvipg
// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
float uchimura(float x, float P, float a, float m, float l, float c, float b)
{
  float l0 = ((P - m) * l) / a;
  float L0 = m - m / a;
  float L1 = m + (1.0f - m) / a;
  float S0 = m + l0;
  float S1 = m + a * l0;
  float C2 = (a * P) / (P - S1);
  float CP = -C2 / P;

  float w0 = float(1.0f - glm::smoothstep(0.0f, m, x));
  float w2 = float(glm::step(m + l0, x));
  float w1 = float(1.0f - w0 - w2);

  float T = float(m * pow(x / m, float(c)) + b);
  float S = float(P - (P - S1) * exp(CP * (x - S0)));
  float L = float(m + a * (x - m));

  return T * w0 + L * w1 + S * w2;
}

float reinhard2(float v, float maxWhite)
{
  return v * (1.0f + (v / (maxWhite * maxWhite))) / (1.0f + v);
}

// Khronos PBR Neutral Tone Mapper
// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/README.md#pbr-neutral-specification
// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/pbrNeutral.glsl
float PBRNeutralToneMapping(float color, float startCompression, float desaturation)
{
  startCompression -= 0.04f;

  float x      = color;
  float offset = x < 0.08f ? x - 6.25f * x * x : 0.04f;
  color -= offset;

  float peak = color;
  if (peak < startCompression)
    return color;

  const float d = 1. - startCompression;
  float newPeak = 1. - d * d / (peak + d - startCompression);
  color *= newPeak / peak;

  float g = 1.0f - 1.0f / (desaturation * (peak - newPeak) + 1.0f);
  return glm::mix(color, newPeak, g);
}

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

  const uint32_t kNumSamples         = 8;
  const lvk::Format kOffscreenFormat = lvk::Format_RGBA_F16;

  const Skybox skyBox(
      ctx, "data/immenstadter_horn_2k_prefilter.ktx", "data/immenstadter_horn_2k_irradiance.ktx", kOffscreenFormat, app.getDepthFormat(),
      kNumSamples);
  const VKMesh mesh(ctx, meshData, scene, kOffscreenFormat, app.getDepthFormat(), kNumSamples);

  lvk::Holder<lvk::ShaderModuleHandle> compBrightPass        = loadShaderModule(ctx, "Chapter10/05_HDR/src/BrightPass.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineBrightPass = ctx->createComputePipeline({ .smComp = compBrightPass });

  lvk::Holder<lvk::ShaderModuleHandle> compAdaptationPass        = loadShaderModule(ctx, "Chapter10/06_HDR_Adaptation/src/Adaptation.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineAdaptationPass = ctx->createComputePipeline({ .smComp = compAdaptationPass });

  const uint32_t kHorizontal = 1;
  const uint32_t kVertical   = 0;

  lvk::Holder<lvk::ShaderModuleHandle> compBloomPass     = loadShaderModule(ctx, "Chapter10/05_HDR/src/Bloom.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineBloomX = ctx->createComputePipeline({
      .smComp   = compBloomPass,
      .specInfo = {.entries = { { .constantId = 0, .size = sizeof(uint32_t) } }, .data = &kHorizontal, .dataSize = sizeof(uint32_t)},
  });
  lvk::Holder<lvk::ComputePipelineHandle> pipelineBloomY = ctx->createComputePipeline({
      .smComp   = compBloomPass,
      .specInfo = {.entries = { { .constantId = 0, .size = sizeof(uint32_t) } }, .data = &kVertical, .dataSize = sizeof(uint32_t)},
  });

  lvk::Holder<lvk::ShaderModuleHandle> vertToneMap = loadShaderModule(ctx, "data/shaders/QuadFlip.vert");
  lvk::Holder<lvk::ShaderModuleHandle> fragToneMap = loadShaderModule(ctx, "Chapter10/05_HDR/src/ToneMap.frag");

  lvk::Holder<lvk::RenderPipelineHandle> pipelineToneMap = ctx->createRenderPipeline({
      .smVert = vertToneMap,
      .smFrag = fragToneMap,
      .color  = { { .format = ctx->getSwapchainFormat() } },
  });

  lvk::Holder<lvk::SamplerHandle> samplerClamp = ctx->createSampler({
      .wrapU = lvk::SamplerWrap_Clamp,
      .wrapV = lvk::SamplerWrap_Clamp,
      .wrapW = lvk::SamplerWrap_Clamp,
  });

  const lvk::Dimensions sizeFb = ctx->getDimensions(ctx->getCurrentSwapchainTexture());

  lvk::Holder<lvk::TextureHandle> msaaColor = ctx->createTexture({
      .format     = kOffscreenFormat,
      .dimensions = sizeFb,
      .numSamples = kNumSamples,
      .usage      = lvk::TextureUsageBits_Attachment,
      .debugName  = "msaaColor",
  });
  lvk::Holder<lvk::TextureHandle> msaaDepth = ctx->createTexture({
      .format     = app.getDepthFormat(),
      .dimensions = sizeFb,
      .numSamples = kNumSamples,
      .usage      = lvk::TextureUsageBits_Attachment,
      .debugName  = "msaaDepth",
  });

  const lvk::Dimensions sizeBloom = { 512, 512 };

  lvk::Holder<lvk::TextureHandle> texBrightPass = ctx->createTexture({
      .format     = kOffscreenFormat,
      .dimensions = sizeBloom,
      .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
      .debugName  = "texBrightPass",
  });
  lvk::Holder<lvk::TextureHandle> texBloomPass  = ctx->createTexture({
       .format     = kOffscreenFormat,
       .dimensions = sizeBloom,
       .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
       .debugName  = "texBloomPass",
  });

  const lvk::ComponentMapping swizzle = { .r = lvk::Swizzle_R, .g = lvk::Swizzle_R, .b = lvk::Swizzle_R, .a = lvk::Swizzle_1 };

  lvk::Holder<lvk::TextureHandle> texLuminanceViews[10] = { ctx->createTexture({
      .format       = lvk::Format_R_F16,
      .dimensions   = sizeBloom,
      .usage        = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
      .numMipLevels = lvk::calcNumMipLevels(sizeBloom.width, sizeBloom.height),
      .swizzle      = swizzle,
      .debugName    = "texLuminance",
  }) };

  for (uint32_t l = 1; l != LVK_ARRAY_NUM_ELEMENTS(texLuminanceViews); l++) {
    texLuminanceViews[l] = ctx->createTextureView(texLuminanceViews[0], { .mipLevel = l, .swizzle = swizzle });
  }

  const uint16_t brightPixel = glm::packHalf1x16(50.0f);

  // ping-pong textures for iterative luminance adaptation
  const lvk::TextureDesc luminanceTextureDesc{
    .format     = lvk::Format_R_F16,
    .dimensions = {1, 1},
    .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
    .swizzle    = swizzle,
    .data       = &brightPixel,
  };
  lvk::Holder<lvk::TextureHandle> texAdaptedLuminance[2] = {
    ctx->createTexture(luminanceTextureDesc, "texAdaptedLuminance0"),
    ctx->createTexture(luminanceTextureDesc, "texAdaptedLuminance1"),
  };

  lvk::Holder<lvk::TextureHandle> offscreenColor = ctx->createTexture({
      .format     = kOffscreenFormat,
      .dimensions = sizeFb,
      .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
      .debugName  = "offscreenColor",
  });

  lvk::Holder<lvk::TextureHandle> texBloom[] = {
    ctx->createTexture({
        .format     = kOffscreenFormat,
        .dimensions = sizeBloom,
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "texBloom0",
    }),
    ctx->createTexture({
        .format     = kOffscreenFormat,
        .dimensions = sizeBloom,
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "texBloom1",
    }),
  };

  bool drawWireframe  = false;
  bool drawCurves     = false;
  bool enableBloom    = true;
  float bloomStrength = 0.01f;
  int numBloomPasses  = 2;
  float adaptationSpeed = 1.0f;

  enum ToneMappingMode {
    ToneMapping_None       = 0,
    ToneMapping_Reinhard   = 1,
    ToneMapping_Uchimura   = 2,
    ToneMapping_KhronosPBR = 3,
  };

  ImPlotContext* implotCtx = ImPlot::CreateContext();

  struct {
    uint32_t texColor;
    uint32_t texLuminance;
    uint32_t texBloom;
    uint32_t sampler;
    int drawMode = ToneMapping_Uchimura;

    float exposure      = 1.0f;
    float bloomStrength = 0.1f;

    // Reinhard
    float maxWhite = 1.0f;

    // Uchimura
    float P = 1.0f;  // max display brightness
    float a = 1.05f; // contrast
    float m = 0.1f;  // linear section start
    float l = 0.8f;  // linear section length
    float c = 3.0f;  // black tightness
    float b = 0.0f;  // pedestal

    // Khronos PBR
    float startCompression = 0.8f;  // highlight compression start
    float desaturation     = 0.15f; // desaturation speed
  } pcHDR = {
    .texColor     = offscreenColor.index(),
    .texLuminance = texAdaptedLuminance[0].index(), // 1x1
    .texBloom     = texBloomPass.index(),
    .sampler      = samplerClamp.index(),
  };

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 view = app.camera_.getViewMatrix();
    const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.01f, 1000.0f);

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      // 1. Render scene
      buf.cmdBeginRendering(
          lvk::RenderPass{
              .color = { { .loadOp = lvk::LoadOp_Clear, .storeOp = lvk::StoreOp_MsaaResolve, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
              .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
      },
          lvk::Framebuffer{
              .color        = { { .texture = msaaColor, .resolveTexture = offscreenColor } },
              .depthStencil = { .texture = msaaDepth },
          });
      skyBox.draw(buf, view, proj);
      {
        buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
        mesh.draw(*ctx.get(), buf, view, proj, skyBox.texSkyboxIrradiance, drawWireframe);
        buf.cmdPopDebugGroupLabel();
      }
      app.drawGrid(buf, proj, vec3(0, -1.0f, 0), kNumSamples, kOffscreenFormat);
      buf.cmdEndRendering();

      // 2. Bright pass - extract luminance and bright areas
      const struct {
        uint32_t texColor;
        uint32_t texOut;
        uint32_t texLuminance;
        uint32_t sampler;
        float exposure;
      } pcBrightPass = {
        .texColor     = offscreenColor.index(),
        .texOut       = texBrightPass.index(),
        .texLuminance = texLuminanceViews[0].index(),
        .sampler      = samplerClamp.index(),
        .exposure     = pcHDR.exposure,
      };
      buf.cmdBindComputePipeline(pipelineBrightPass);
      buf.cmdPushConstants(pcBrightPass);
      buf.cmdDispatchThreadGroups(
          sizeBloom.divide2D(16), {
                                      .textures = {lvk::TextureHandle(offscreenColor), lvk::TextureHandle(texLuminanceViews[0])}
      });
      buf.cmdGenerateMipmap(texLuminanceViews[0]);

      // 2.1. Bloom
      struct BlurPC {
        uint32_t texIn;
        uint32_t texOut;
        uint32_t sampler;
      };
      struct StreaksPC {
        uint32_t texIn;
        uint32_t texOut;
        uint32_t texRotationPattern;
        uint32_t sampler;
      };
      struct BlurPass {
        lvk::TextureHandle texIn;
        lvk::TextureHandle texOut;
      };
      std::vector<BlurPass> passes;
      {
        passes.reserve(2 * numBloomPasses);
        passes.push_back({ texBrightPass, texBloom[0] });
        for (int i = 0; i != numBloomPasses - 1; i++) {
          passes.push_back({ texBloom[0], texBloom[1] });
          passes.push_back({ texBloom[1], texBloom[0] });
        }
        passes.push_back({ texBloom[0], texBloomPass });
      }
      for (uint32_t i = 0; i != passes.size(); i++) {
        const BlurPass p = passes[i];
        buf.cmdBindComputePipeline(i & 1 ? pipelineBloomX : pipelineBloomY);
        buf.cmdPushConstants(BlurPC{
            .texIn   = p.texIn.index(),
            .texOut  = p.texOut.index(),
            .sampler = samplerClamp.index(),
        });
        if (enableBloom)
          buf.cmdDispatchThreadGroups(
              sizeBloom.divide2D(16), {
                                          .textures = {p.texIn, p.texOut, lvk::TextureHandle(texBrightPass)}
          });
      }

      // 3. Light adaptation pass
      const struct {
        uint32_t texCurrSceneLuminance;
        uint32_t texPrevAdaptedLuminance;
        uint32_t texNewAdaptedLuminance;
        float adaptationSpeed;
      } pcAdaptationPass = {
        .texCurrSceneLuminance   = texLuminanceViews[LVK_ARRAY_NUM_ELEMENTS(texLuminanceViews) - 1].index(), // 1x1,
        .texPrevAdaptedLuminance = texAdaptedLuminance[0].index(),
        .texNewAdaptedLuminance  = texAdaptedLuminance[1].index(),
        .adaptationSpeed         = 100.0f * deltaSeconds * adaptationSpeed,
      };
      buf.cmdBindComputePipeline(pipelineAdaptationPass);
      buf.cmdPushConstants(pcAdaptationPass);
      buf.cmdDispatchThreadGroups(
          {
              1, 1, 1
      },
          { .textures = {
                lvk::TextureHandle(texLuminanceViews[0]), // transition the entire mip-pyramid
                lvk::TextureHandle(texAdaptedLuminance[0]),
                lvk::TextureHandle(texAdaptedLuminance[1]),
            } });

      // 4. Render tone-mapped scene into a swapchain image
      const lvk::RenderPass renderPassMain = {
        .color = { { .loadOp = lvk::LoadOp_Load, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
      };
      const lvk::Framebuffer framebufferMain = {
        .color = { { .texture = ctx->getCurrentSwapchainTexture() } },
      };

      // transition the entire mip-pyramid
      buf.cmdBeginRendering(renderPassMain, framebufferMain, { .textures = { lvk::TextureHandle(texAdaptedLuminance[1]) } });

      buf.cmdBindRenderPipeline(pipelineToneMap);
      buf.cmdPushConstants(pcHDR);
      buf.cmdBindDepthState({});
      buf.cmdDraw(3); // fullscreen triangle

      app.imgui_->beginFrame(framebufferMain);
      app.drawFPS();
      app.drawMemo();

      // render UI
      {
        const ImGuiViewport* v  = ImGui::GetMainViewport();
        const float windowWidth = v->WorkSize.x / 5;
        ImGui::SetNextWindowPos(ImVec2(10, 200));
        ImGui::SetNextWindowSize(ImVec2(windowWidth, v->WorkSize.y - 210));
        ImGui::Begin("HDR", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Checkbox("Draw wireframe", &drawWireframe);
        ImGui::Checkbox("Draw tone mapping curves", &drawCurves);
        ImGui::Separator();
        const float indentSize = 32.0f;
        ImGui::Text("Tone mapping params:");
        ImGui::SliderFloat("Exposure", &pcHDR.exposure, 0.1f, 2.0f);
        ImGui::SliderFloat("Adaptation speed", &adaptationSpeed, 0.1f, 2.0f);
        ImGui::Checkbox("Enable bloom", &enableBloom);
        pcHDR.bloomStrength = enableBloom ? bloomStrength : 0.0f;
        ImGui::BeginDisabled(!enableBloom);
        ImGui::Indent(indentSize);
        ImGui::SliderFloat("Bloom strength", &bloomStrength, 0.0f, 1.0f);
        ImGui::SliderInt("Bloom num passes", &numBloomPasses, 1, 5);
        ImGui::Unindent(indentSize);
        ImGui::EndDisabled();
        ImGui::Text("Tone mapping mode:");
        ImGui::RadioButton("None", &pcHDR.drawMode, ToneMapping_None);
        ImGui::RadioButton("Reinhard", &pcHDR.drawMode, ToneMapping_Reinhard);
        if (pcHDR.drawMode == ToneMapping_Reinhard) {
          ImGui::Indent(indentSize);
          ImGui::BeginDisabled(pcHDR.drawMode != ToneMapping_Reinhard);
          ImGui::SliderFloat("Max white", &pcHDR.maxWhite, 0.5f, 2.0f);
          ImGui::EndDisabled();
          ImGui::Unindent(indentSize);
        }
        ImGui::RadioButton("Uchimura", &pcHDR.drawMode, ToneMapping_Uchimura);
        if (pcHDR.drawMode == ToneMapping_Uchimura) {
          ImGui::Indent(indentSize);
          ImGui::BeginDisabled(pcHDR.drawMode != ToneMapping_Uchimura);
          ImGui::SliderFloat("Max brightness", &pcHDR.P, 1.0f, 2.0f);
          ImGui::SliderFloat("Contrast", &pcHDR.a, 0.0f, 5.0f);
          ImGui::SliderFloat("Linear section start", &pcHDR.m, 0.0f, 1.0f);
          ImGui::SliderFloat("Linear section length", &pcHDR.l, 0.0f, 1.0f);
          ImGui::SliderFloat("Black tightness", &pcHDR.c, 1.0f, 3.0f);
          ImGui::SliderFloat("Pedestal", &pcHDR.b, 0.0f, 1.0f);
          ImGui::EndDisabled();
          ImGui::Unindent(indentSize);
        }
        ImGui::RadioButton("Khronos PBR Neutral", &pcHDR.drawMode, ToneMapping_KhronosPBR);
        if (pcHDR.drawMode == ToneMapping_KhronosPBR) {
          ImGui::Indent(indentSize);
          ImGui::SliderFloat("Highlight compression start", &pcHDR.startCompression, 0.0f, 1.0f);
          ImGui::SliderFloat("Desaturation speed", &pcHDR.desaturation, 0.0f, 1.0f);
          ImGui::Unindent(indentSize);
        }
        ImGui::Separator();

        ImGui::Text("Average luminance 1x1:");
        ImGui::Image(texLuminanceViews[LVK_ARRAY_NUM_ELEMENTS(texLuminanceViews) - 1].index(), ImVec2(128, 128));
        ImGui::Text("Adapted luminance 1x1:");
        ImGui::Image(texAdaptedLuminance[0].index(), ImVec2(128, 128));
        ImGui::Separator();
        ImGui::Text("Bright pass:");
        ImGui::Image(texBrightPass.index(), ImVec2(windowWidth, windowWidth / aspectRatio));
        ImGui::Text("Bloom pass:");
        ImGui::Image(texBloomPass.index(), ImVec2(windowWidth, windowWidth / aspectRatio));
        ImGui::Separator();
        ImGui::Text("Luminance pyramid 512x512");
        for (uint32_t l = 0; l != LVK_ARRAY_NUM_ELEMENTS(texLuminanceViews); l++) {
          ImGui::Image(texLuminanceViews[l].index(), ImVec2((int)windowWidth >> l, ((int)windowWidth >> l)));
        }
        ImGui::Separator();
        ImGui::End();

        if (drawCurves) {
          const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
          ImGui::SetNextWindowBgAlpha(0.8f);
          ImGui::SetNextWindowPos({ width * 0.6f, height * 0.7f }, ImGuiCond_Appearing);
          ImGui::SetNextWindowSize({ width * 0.4f, height * 0.3f });
          ImGui::Begin("Tone mapping curve", nullptr, flags);
          const int kNumGraphPoints = 1001;
          float xs[kNumGraphPoints];
          float ysUnchimura[kNumGraphPoints];
          float ysReinhard2[kNumGraphPoints];
          float ysKhronosPBR[kNumGraphPoints];
          for (int i = 0; i != kNumGraphPoints; i++) {
            xs[i]           = float(i) / kNumGraphPoints;
            ysUnchimura[i]  = uchimura(xs[i], pcHDR.P, pcHDR.a, pcHDR.m, pcHDR.l, pcHDR.c, pcHDR.b);
            ysReinhard2[i]  = reinhard2(xs[i], pcHDR.maxWhite);
            ysKhronosPBR[i] = PBRNeutralToneMapping(xs[i], pcHDR.startCompression, pcHDR.desaturation);
          }
          if (ImPlot::BeginPlot("Tone mapping curves", { width * 0.4f, height * 0.3f }, ImPlotFlags_NoInputs)) {
            ImPlot::SetupAxes("Input", "Output");
            ImPlot::PlotLine("Uchimura", xs, ysUnchimura, kNumGraphPoints);
            ImPlot::PlotLine("Reinhard", xs, ysReinhard2, kNumGraphPoints);
            ImPlot::PlotLine("Khronos PBR", xs, ysKhronosPBR, kNumGraphPoints);
            ImPlot::EndPlot();
          }
          ImGui::End();
        }
      }

      app.imgui_->endFrame(buf);

      buf.cmdEndRendering();
    }
    ctx->submit(buf, ctx->getCurrentSwapchainTexture());

    // swap ping-bong textures
    std::swap(texAdaptedLuminance[0], texAdaptedLuminance[1]);
  });

  ImPlot::DestroyContext(implotCtx);

  ctx.release();

  return 0;
}
