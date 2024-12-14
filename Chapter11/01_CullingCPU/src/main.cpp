#include "shared/VulkanApp.h"

#include "Chapter10/Bistro.h"
#include "Chapter10/Skybox.h"
#include "Chapter11/VKMesh11.h"

#include "shared/LineCanvas.h"

mat4 cullingView       = mat4(1.0f);
bool freezeCullingView = false;
bool drawMeshes        = true;
bool drawBoxes         = true;
bool drawWireframe     = false;

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

  LineCanvas3D canvas3d;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  const lvk::Dimensions sizeFb = ctx->getDimensions(ctx->getCurrentSwapchainTexture());

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

  app.addKeyCallback([](GLFWwindow* window, int key, int scancode, int action, int mods) {
    const bool pressed = action != GLFW_RELEASE;
    if (key == GLFW_KEY_P && pressed && !ImGui::GetIO().WantCaptureKeyboard)
      freezeCullingView = !freezeCullingView;
  });

  const Skybox skyBox(
      ctx, "data/immenstadter_horn_2k_prefilter.ktx", "data/immenstadter_horn_2k_irradiance.ktx", ctx->getSwapchainFormat(),
      app.getDepthFormat(), kNumSamples);

  const VKMesh11 mesh(ctx, meshData, scene, lvk::StorageType_HostVisible);
  const VKPipeline11 pipeline(ctx, meshData.streams, ctx->getSwapchainFormat(), app.getDepthFormat(), kNumSamples);

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 view = app.camera_.getViewMatrix();
    const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.1f, 200.0f);

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      // 0. Cull scene
      if (!freezeCullingView)
        cullingView = app.camera_.getViewMatrix();

      vec4 frustumPlanes[6];
      getFrustumPlanes(proj * cullingView, frustumPlanes);
      vec4 frustumCorners[8];
      getFrustumCorners(proj * cullingView, frustumCorners);

      // cull
      int numVisibleMeshes = 0;
      {
        DrawIndexedIndirectCommand* cmd = mesh.getDrawIndexedIndirectCommandPtr();
        for (auto& p : scene.meshForNode) {
          const BoundingBox box = meshData.boxes[p.second].getTransformed(scene.globalTransform[p.first]);
          cmd->instanceCount    = isBoxInFrustum(frustumPlanes, frustumCorners, box) ? 1 : 0;
          numVisibleMeshes += (cmd++)->instanceCount;
        }
        ctx->flushMappedMemory(mesh.indirectBuffer_.bufferIndirect_, 0, mesh.numMeshes_ * sizeof(DrawIndexedIndirectCommand));
      }

      canvas3d.clear();
      canvas3d.setMatrix(proj * view);

      if (freezeCullingView) {
        canvas3d.frustum(cullingView, proj, vec4(1, 1, 0, 1));
      }
      // render all bounding boxes (red)
      if (drawBoxes) {
        const DrawIndexedIndirectCommand* cmd = mesh.getDrawIndexedIndirectCommandPtr();
        for (auto& p : scene.meshForNode) {
          const BoundingBox box = meshData.boxes[p.second];
          canvas3d.box(scene.globalTransform[p.first], box, (cmd++)->instanceCount ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1));
        }
      }

      // 1. Render scene
      const lvk::Framebuffer framebufferMSAA = {
        .color        = { { .texture = msaaColor, .resolveTexture = ctx->getCurrentSwapchainTexture() } },
        .depthStencil = { .texture = msaaDepth },
      };
      buf.cmdBeginRendering(
          lvk::RenderPass{
              .color = { { .loadOp = lvk::LoadOp_Clear, .storeOp = lvk::StoreOp_MsaaResolve, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
              .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
      },
          framebufferMSAA);
      skyBox.draw(buf, view, proj);
      if (drawMeshes) {
        buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
        mesh.draw(buf, pipeline, view, proj, skyBox.texSkyboxIrradiance, drawWireframe);
        buf.cmdPopDebugGroupLabel();
      }
      app.drawGrid(buf, proj, vec3(0, -1.0f, 0), kNumSamples);
      canvas3d.render(*ctx.get(), framebufferMSAA, buf, kNumSamples);
      buf.cmdEndRendering();

      const lvk::Framebuffer framebufferMain = {
        .color = { { .texture = ctx->getCurrentSwapchainTexture() } },
      };

      buf.cmdBeginRendering(
          lvk::RenderPass{
              .color = { { .loadOp = lvk::LoadOp_Load, .storeOp = lvk::StoreOp_Store } },
          },
          framebufferMain);

      app.imgui_->beginFrame(framebufferMain);
      app.drawFPS();
      app.drawMemo();

      // render UI
      {
        const ImGuiViewport* v  = ImGui::GetMainViewport();
        const float windowWidth = v->WorkSize.x / 5;
        ImGui::SetNextWindowPos(ImVec2(10, 200));
        ImGui::Begin(
            "Controls", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Checkbox("Draw wireframe", &drawWireframe);
        ImGui::Text("Draw:");
        const float indentSize = 16.0f;
        ImGui::Indent(indentSize);
        ImGui::Checkbox("Meshes", &drawMeshes);
        ImGui::Checkbox("Boxes", &drawBoxes);
        ImGui::Unindent(indentSize);
        ImGui::Separator();
        ImGui::Checkbox("Freeze culling frustum (P)", &freezeCullingView);
        ImGui::Separator();
        ImGui::Text("Visible meshes: %i", numVisibleMeshes);
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
