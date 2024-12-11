#include "shared/VulkanApp.h"

#include "Chapter10/Bistro.h"

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

  lvk::Holder<lvk::TextureHandle> texSkyboxIrradiance = loadTexture(ctx, "data/immenstadter_horn_2k_irradiance.ktx", lvk::TextureType_Cube);

  const uint32_t kNumSamples = 8;

  const lvk::Dimensions sizeFb = ctx->getDimensions(ctx->getCurrentSwapchainTexture());

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

  bool enableMSAA        = true;
  bool drawWireframe     = false;
  bool drawBoundingBoxes = false;

  const VKMesh mesh(ctx, meshData, scene, ctx->getSwapchainFormat(), app.getDepthFormat());
  const VKMesh meshMSAA(ctx, meshData, scene, ctx->getSwapchainFormat(), app.getDepthFormat(), kNumSamples);

  LineCanvas3D canvas3d;

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 view = app.camera_.getViewMatrix();
    const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.01f, 1000.0f);

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      const lvk::Framebuffer framebufferOffscreen = {
        .color        = { { .texture        = enableMSAA ? msaaColor : ctx->getCurrentSwapchainTexture(),
                            .resolveTexture = enableMSAA ? ctx->getCurrentSwapchainTexture() : lvk::TextureHandle{} } },
        .depthStencil = { .texture = enableMSAA ? msaaDepth : app.getDepthTexture() },
      };
      // 1. Render scene
      buf.cmdBeginRendering(
          lvk::RenderPass{
              .color = { { .loadOp     = lvk::LoadOp_Clear,
                           .storeOp    = enableMSAA ? lvk::StoreOp_MsaaResolve : lvk::StoreOp_Store,
                           .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
              .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
      },
          framebufferOffscreen);
      buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
      (enableMSAA ? meshMSAA : mesh).draw(buf, view, proj, texSkyboxIrradiance, drawWireframe);
      buf.cmdPopDebugGroupLabel();
      app.drawGrid(buf, proj, vec3(0, -1.0f, 0), enableMSAA ? kNumSamples : 1);
      canvas3d.render(*ctx.get(), framebufferOffscreen, buf, enableMSAA ? kNumSamples : 1);
      buf.cmdEndRendering();

      // 2. Render UI
      const lvk::Framebuffer framebufferMain = {
        .color = { { .texture = ctx->getCurrentSwapchainTexture() } },
      };
      buf.cmdBeginRendering(
          lvk::RenderPass{
              .color = { { .loadOp = lvk::LoadOp_Load, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
          },
          framebufferMain);

      app.imgui_->beginFrame(framebufferMain);
      app.drawFPS();
      app.drawMemo();

      canvas3d.clear();
      canvas3d.setMatrix(proj * view);
      // render all bounding boxes (red)
      if (drawBoundingBoxes) {
        for (auto& p : scene.meshForNode) {
          const BoundingBox box = meshData.boxes[p.second];
          canvas3d.box(scene.globalTransform[p.first], box, vec4(1, 0, 0, 1));
        }
      }

      {
        const ImGuiViewport* v = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(10, 200));
        ImGui::Begin(
            "MSAA", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Checkbox("Enable MSAA", &enableMSAA);
        ImGui::Checkbox("Draw wireframe", &drawWireframe);
        ImGui::Checkbox("Draw bounding boxes", &drawBoundingBoxes);
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
