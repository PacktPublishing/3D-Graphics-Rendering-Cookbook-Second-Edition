#include "shared/VulkanApp.h"

#include "Chapter10/Bistro.h"
#include "Chapter10/Skybox.h"

#include "Chapter11/VKMesh11.h"

bool drawMeshesOpaque      = true;
bool drawMeshesTransparent = true;
bool drawWireframe         = false;
bool showHeatmap           = false;
float opacityBoost         = 0.0f;

int main()
{
  MeshData meshData;
  Scene scene;
  loadBistro(meshData, scene);

  VulkanApp app({
      .initialCameraPos    = vec3(1.835f, 1.922f, 6.412f),
      .initialCameraTarget = vec3(2.0f, 1.9f, 6.0f),
  });

  app.positioner_.maxSpeed_ = 1.5f;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  const lvk::Dimensions sizeFb = ctx->getDimensions(ctx->getCurrentSwapchainTexture());

  const uint32_t kNumSamples = 8;

  lvk::Holder<lvk::TextureHandle> msaaColor      = ctx->createTexture({
           .format     = ctx->getSwapchainFormat(),
           .dimensions = sizeFb,
           .numSamples = kNumSamples,
           .usage      = lvk::TextureUsageBits_Attachment,
           .storage    = lvk::StorageType_Memoryless,
           .debugName  = "msaaColor",
  });
  lvk::Holder<lvk::TextureHandle> msaaDepth      = ctx->createTexture({
           .format     = app.getDepthFormat(),
           .dimensions = sizeFb,
           .numSamples = kNumSamples,
           .usage      = lvk::TextureUsageBits_Attachment,
           .storage    = lvk::StorageType_Memoryless,
           .debugName  = "msaaDepth",
  });
  lvk::Holder<lvk::TextureHandle> offscreenColor = ctx->createTexture({
      .format     = ctx->getSwapchainFormat(),
      .dimensions = sizeFb,
      .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
      .debugName  = "offscreenColor",
  });

  const Skybox skyBox(
      ctx, "data/immenstadter_horn_2k_prefilter.ktx", "data/immenstadter_horn_2k_irradiance.ktx", ctx->getSwapchainFormat(),
      app.getDepthFormat(), kNumSamples);
  const VKMesh11 mesh(ctx, meshData, scene);
  const VKPipeline11 pipelineOpaque(
      ctx, meshData.streams, ctx->getSwapchainFormat(), app.getDepthFormat(), kNumSamples,
      loadShaderModule(ctx, "Chapter11/04_OIT/src/main.vert"), loadShaderModule(ctx, "Chapter11/04_OIT/src/opaque.frag"));
  const VKPipeline11 pipelineTransparent(
      ctx, meshData.streams, ctx->getSwapchainFormat(), app.getDepthFormat(), kNumSamples,
      loadShaderModule(ctx, "Chapter11/04_OIT/src/main.vert"), loadShaderModule(ctx, "Chapter11/04_OIT/src/transparent.frag"));

  lvk::Holder<lvk::ShaderModuleHandle> vertOIT       = loadShaderModule(ctx, "data/shaders/QuadFlip.vert");
  lvk::Holder<lvk::ShaderModuleHandle> fragOIT       = loadShaderModule(ctx, "Chapter11/04_OIT/src/oit.frag");
  lvk::Holder<lvk::RenderPipelineHandle> pipelineOIT = ctx->createRenderPipeline({
      .smVert = vertOIT,
      .smFrag = fragOIT,
      .color  = { { .format = ctx->getSwapchainFormat() } },
  });

  VKIndirectBuffer11 meshesOpaque(ctx, mesh.numMeshes_);
  VKIndirectBuffer11 meshesTransparent(ctx, mesh.numMeshes_);

  auto isTransparent = [&meshData, &mesh](const DrawIndexedIndirectCommand& c) -> bool {
    const uint32_t mtlIndex = mesh.drawData_[c.baseInstance].materialId;
    const Material& mtl     = meshData.materials[mtlIndex];
    return (mtl.flags & sMaterialFlags_Transparent) > 0;
  };

  mesh.indirectBuffer_.selectTo(meshesOpaque, [&isTransparent](const DrawIndexedIndirectCommand& c) -> bool { return !isTransparent(c); });
  mesh.indirectBuffer_.selectTo(
      meshesTransparent, [&isTransparent](const DrawIndexedIndirectCommand& c) -> bool { return isTransparent(c); });

  struct TransparentFragment {
    uint64_t rgba; // f16vec4
    float depth;
    uint32_t next;
  };

  const uint32_t kMaxOITFragments = sizeFb.width * sizeFb.height * kNumSamples;

  lvk::Holder<lvk::BufferHandle> bufferTransparencyLists = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(TransparentFragment) * kMaxOITFragments,
      .debugName = "Buffer: transparency lists",
  });

  lvk::Holder<lvk::TextureHandle> textureHeadsOIT = ctx->createTexture({
      .format     = lvk::Format_R_UI32,
      .dimensions = sizeFb,
      .usage      = lvk::TextureUsageBits_Storage,
      .debugName  = "oitHeads",
  });

  lvk::Holder<lvk::BufferHandle> bufferAtomicCounter = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(uint32_t),
      .debugName = "Buffer: atomic counter",
  });

  const struct OITBuffer {
    uint64_t bufferAtomicCounter;
    uint64_t bufferTransparencyLists;
    uint32_t texHeadsOIT;
    uint32_t maxOITFragments;
  } oitBufferData = {
    .bufferAtomicCounter     = ctx->gpuAddress(bufferAtomicCounter),
    .bufferTransparencyLists = ctx->gpuAddress(bufferTransparencyLists),
    .texHeadsOIT             = textureHeadsOIT.index(),
    .maxOITFragments         = kMaxOITFragments,
  };

  lvk::Holder<lvk::BufferHandle> bufferOIT = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(oitBufferData),
      .data      = &oitBufferData,
      .debugName = "Buffer: OIT",
  });

  auto clearTransparencyBuffers = [&bufferAtomicCounter, &textureHeadsOIT, sizeFb](lvk::ICommandBuffer& buf) {
    buf.cmdClearColorImage(textureHeadsOIT, { .uint32 = { 0xffffffff } });
    buf.cmdFillBuffer(bufferAtomicCounter, 0, sizeof(uint32_t), 0);
  };

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 view = app.camera_.getViewMatrix();
    const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.1f, 200.0f);

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      clearTransparencyBuffers(buf);
      // 1. Render scene
      const lvk::Framebuffer framebufferMSAA = {
        .color        = { { .texture = msaaColor, .resolveTexture = offscreenColor } },
        .depthStencil = { .texture = msaaDepth },
      };
      buf.cmdBeginRendering(
          lvk::RenderPass{
              .color = { { .loadOp = lvk::LoadOp_Clear, .storeOp = lvk::StoreOp_MsaaResolve, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
              .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
      },
          framebufferMSAA);
      skyBox.draw(buf, view, proj);
      const struct {
        mat4 viewProj;
        vec4 cameraPos;
        uint64_t bufferTransforms;
        uint64_t bufferDrawData;
        uint64_t bufferMaterials;
        uint64_t bufferOIT;
        uint32_t texSkybox;
        uint32_t texSkyboxIrradiance;
      } pc = {
        .viewProj            = proj * view,
        .cameraPos           = vec4(app.camera_.getPosition(), 1.0f),
        .bufferTransforms    = ctx->gpuAddress(mesh.bufferTransforms_),
        .bufferDrawData      = ctx->gpuAddress(mesh.bufferDrawData_),
        .bufferMaterials     = ctx->gpuAddress(mesh.bufferMaterials_),
        .bufferOIT           = ctx->gpuAddress(bufferOIT),
        .texSkybox           = skyBox.texSkybox.index(),
        .texSkyboxIrradiance = skyBox.texSkyboxIrradiance.index(),
      };
      if (drawMeshesOpaque) {
        buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
        mesh.draw(
            buf, pipelineOpaque, &pc, sizeof(pc), { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true }, drawWireframe,
            &meshesOpaque);
        buf.cmdPopDebugGroupLabel();
      }
      if (drawMeshesTransparent) {
        buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
        mesh.draw(
            buf, pipelineTransparent, &pc, sizeof(pc), { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = false }, drawWireframe,
            &meshesTransparent);
        buf.cmdPopDebugGroupLabel();
      }
      app.drawGrid(buf, proj, vec3(0, -1.0f, 0), kNumSamples);
      buf.cmdEndRendering();

      const lvk::Framebuffer framebufferMain = {
        .color = { { .texture = ctx->getCurrentSwapchainTexture() } },
      };

      buf.cmdBeginRendering(
          lvk::RenderPass{
              .color = {{ .loadOp = lvk::LoadOp_Load, .storeOp = lvk::StoreOp_Store }},
      },
          framebufferMain,
          { .textures = { lvk::TextureHandle(textureHeadsOIT), lvk::TextureHandle(offscreenColor) },
            .buffers  = { lvk::BufferHandle(bufferTransparencyLists) } });

      // combine OIT
      const struct {
        uint64_t bufferTransparencyLists;
        uint32_t texColor;
        uint32_t texHeadsOIT;
        float time;
        float opacityBoost;
        uint32_t showHeatmap;
      } pcOIT = {
        .bufferTransparencyLists = ctx->gpuAddress(bufferTransparencyLists),
        .texColor                = offscreenColor.index(),
        .texHeadsOIT             = textureHeadsOIT.index(),
        .time                    = static_cast<float>(glfwGetTime()),
        .opacityBoost            = opacityBoost,
        .showHeatmap             = showHeatmap ? 1u : 0u,
      };
      buf.cmdBindRenderPipeline(pipelineOIT);
      buf.cmdPushConstants(pcOIT);
      buf.cmdBindDepthState({});
      buf.cmdDraw(3);

      app.imgui_->beginFrame(framebufferMain);
      app.drawFPS();
      app.drawMemo();

      // render UI
      {
        const ImGuiViewport* v  = ImGui::GetMainViewport();
        const float windowWidth = v->WorkSize.x / 5;
        ImGui::SetNextWindowPos(ImVec2(10, 200));
        ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Checkbox("Draw wireframe", &drawWireframe);
        ImGui::Text("Draw:");
        const float indentSize = 16.0f;
        ImGui::Indent(indentSize);
        ImGui::Checkbox("Opaque meshes", &drawMeshesOpaque);
        ImGui::Checkbox("Transparent meshes", &drawMeshesTransparent);
        ImGui::Unindent(indentSize);
        ImGui::Separator();
        ImGui::SliderFloat("Opacity boost", &opacityBoost, -1.0f, +1.0f);
        ImGui::Checkbox("Show transparency heat map", &showHeatmap);
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
