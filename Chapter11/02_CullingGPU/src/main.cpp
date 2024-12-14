#include "shared/VulkanApp.h"

#include "Chapter10/Bistro.h"
#include "Chapter10/Skybox.h"
#include "Chapter11/VKMesh11.h"

#include "shared/LineCanvas.h"

enum CullingMode {
  CullingMode_None = 0,
  CullingMode_CPU  = 1,
  CullingMode_GPU  = 2,
};

mat4 cullingView       = mat4(1.0f);
int cullingMode        = CullingMode_GPU;
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

  lvk::Holder<lvk::ShaderModuleHandle> compCulling        = loadShaderModule(ctx, "Chapter11/02_CullingGPU/src/FrustumCulling.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineCulling = ctx->createComputePipeline({
      .smComp = compCulling,
  });

  app.addKeyCallback([](GLFWwindow* window, int key, int scancode, int action, int mods) {
    const bool pressed = action != GLFW_RELEASE;
    if (!pressed || ImGui::GetIO().WantCaptureKeyboard)
      return;
    if (key == GLFW_KEY_P)
      freezeCullingView = !freezeCullingView;
    if (key == GLFW_KEY_N)
      cullingMode = CullingMode_None;
    if (key == GLFW_KEY_C)
      cullingMode = CullingMode_CPU;
    if (key == GLFW_KEY_G)
      cullingMode = CullingMode_GPU;
  });

  const Skybox skyBox(
      ctx, "data/immenstadter_horn_2k_prefilter.ktx", "data/immenstadter_horn_2k_irradiance.ktx", ctx->getSwapchainFormat(),
      app.getDepthFormat(), kNumSamples);

  const VKMesh11 mesh(ctx, meshData, scene, lvk::StorageType_HostVisible);
  const VKPipeline11 pipeline(ctx, meshData.streams, ctx->getSwapchainFormat(), app.getDepthFormat(), kNumSamples);

  std::vector<BoundingBox> reorderedBoxes;
  reorderedBoxes.resize(scene.globalTransform.size());

  // pretransform bounding boxes to world space
  for (auto& p : scene.meshForNode) {
    reorderedBoxes[p.first] = meshData.boxes[p.second].getTransformed(scene.globalTransform[p.first]);
  }

  lvk::Holder<lvk::BufferHandle> bufferAABBs = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = reorderedBoxes.size() * sizeof(BoundingBox),
      .data      = reorderedBoxes.data(),
      .debugName = "Buffer: AABBs",
  });

  struct CullingData {
    vec4 frustumPlanes[6];
    vec4 frustumCorners[8];
    uint32_t numMeshesToCull  = 0;
    uint32_t numVisibleMeshes = 0; // GPU
  } emptyCullingData;

  int numVisibleMeshes = 0; // CPU

  // round-robin
  const lvk::BufferDesc cullingDataDesc = {
    .usage     = lvk::BufferUsageBits_Storage,
    .storage   = lvk::StorageType_HostVisible,
    .size      = sizeof(CullingData),
    .data      = &emptyCullingData,
    .debugName = "Buffer: CullingData 0",
  };
  lvk::Holder<lvk::BufferHandle> bufferCullingData[] = {
    ctx->createBuffer(cullingDataDesc, "Buffer: CullingData 0"),
    ctx->createBuffer(cullingDataDesc, "Buffer: CullingData 1"),
  };
  lvk::SubmitHandle submitHandle[LVK_ARRAY_NUM_ELEMENTS(bufferCullingData)] = {};
  uint32_t currentBufferId = 0;

  struct {
    uint64_t commands;
    uint64_t drawData;
    uint64_t AABBs;
    uint64_t meshes;
  } pcCulling = {
    .commands = ctx->gpuAddress(mesh.indirectBuffer_.bufferIndirect_),
    .drawData = ctx->gpuAddress(mesh.bufferDrawData_),
    .AABBs    = ctx->gpuAddress(bufferAABBs),
  };

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 view = app.camera_.getViewMatrix();
    const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.1f, 200.0f);

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      // 0. Cull scene
      if (!freezeCullingView)
        cullingView = app.camera_.getViewMatrix();

      CullingData cullingData = {
        .numMeshesToCull = static_cast<uint32_t>(scene.meshForNode.size()),
      };

      getFrustumPlanes(proj * cullingView, cullingData.frustumPlanes);
      getFrustumCorners(proj * cullingView, cullingData.frustumCorners);

      // cull
      if (cullingMode == CullingMode_None) {
        numVisibleMeshes                = static_cast<uint32_t>(scene.meshForNode.size());
        DrawIndexedIndirectCommand* cmd = mesh.getDrawIndexedIndirectCommandPtr();
        for (auto& p : scene.meshForNode) {
          (cmd++)->instanceCount = 1;
        }
        ctx->flushMappedMemory(mesh.indirectBuffer_.bufferIndirect_, 0, mesh.numMeshes_ * sizeof(DrawIndexedIndirectCommand));
      } else if (cullingMode == CullingMode_CPU) {
        numVisibleMeshes = 0;

        DrawIndexedIndirectCommand* cmd = mesh.getDrawIndexedIndirectCommandPtr();
        for (auto& p : scene.meshForNode) {
          const BoundingBox box = reorderedBoxes[p.first];
          cmd->instanceCount    = isBoxInFrustum(cullingData.frustumPlanes, cullingData.frustumCorners, box) ? 1 : 0;
          numVisibleMeshes += (cmd++)->instanceCount;
        }
        ctx->flushMappedMemory(mesh.indirectBuffer_.bufferIndirect_, 0, mesh.numMeshes_ * sizeof(DrawIndexedIndirectCommand));
      } else if (cullingMode == CullingMode_GPU) {
        buf.cmdBindComputePipeline(pipelineCulling);
        pcCulling.meshes = ctx->gpuAddress(bufferCullingData[currentBufferId]);
        buf.cmdPushConstants(pcCulling);
        buf.cmdUpdateBuffer(bufferCullingData[currentBufferId], cullingData);
        buf.cmdDispatchThreadGroups(
            { 1 + cullingData.numMeshesToCull / 64 }, { .buffers = { lvk::BufferHandle(mesh.indirectBuffer_.bufferIndirect_) } });
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
          framebufferMSAA, { .buffers = { lvk::BufferHandle(mesh.indirectBuffer_.bufferIndirect_) } });
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

        const float indentSize = 16.0f;
        ImGui::Text("Draw:");
        ImGui::Indent(indentSize);
        ImGui::Checkbox("Meshes", &drawMeshes);
        ImGui::Checkbox("Boxes", &drawBoxes);
        ImGui::Unindent(indentSize);
        ImGui::Separator();
        ImGui::Text("Culling:");
        ImGui::Indent(indentSize);
        ImGui::RadioButton("None (N)", &cullingMode, CullingMode_None);
        ImGui::RadioButton("CPU  (C)", &cullingMode, CullingMode_CPU);
        ImGui::RadioButton("GPU  (G)", &cullingMode, CullingMode_GPU);
        ImGui::Unindent(indentSize);
        ImGui::Checkbox("Freeze culling frustum (P)", &freezeCullingView);
        ImGui::Separator();
        ImGui::Text("Visible meshes: %i", numVisibleMeshes);
        ImGui::End();
      }

      app.imgui_->endFrame(buf);

      buf.cmdEndRendering();
    }
    submitHandle[currentBufferId] = ctx->submit(buf, ctx->getCurrentSwapchainTexture());

    currentBufferId = (currentBufferId + 1) % LVK_ARRAY_NUM_ELEMENTS(bufferCullingData);

    if (cullingMode == CullingMode_GPU && app.fpsCounter_.numFrames_ > 1) {
      ctx->wait(submitHandle[currentBufferId]);
      ctx->download(bufferCullingData[currentBufferId], &numVisibleMeshes, sizeof(uint32_t), offsetof(CullingData, numVisibleMeshes));
    }
  });

  ctx.release();

  return 0;
}
