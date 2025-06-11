#include "shared/VulkanApp.h"

#define DEMO_TEXTURE_MAX_SIZE 2048
#define DEMO_TEXTURE_CACHE_FOLDER ".cache/out_textures_11/"
#define fileNameCachedMeshes ".cache/ch11_bistro.meshes"
#define fileNameCachedMaterials ".cache/ch11_bistro.materials"
#define fileNameCachedHierarchy ".cache/ch11_bistro.scene"

#include "Chapter10/Bistro.h"
#include "Chapter10/Skybox.h"
#include "Chapter11/VKMesh11Lazy.h"

#include "shared/LineCanvas.h"

bool drawMeshes    = true;
bool drawBoxes     = false;
bool drawWireframe = false;
struct LightParams {
  float theta          = +90.0f;
  float phi            = -26.0f;
  float depthBiasConst = 1.1f;
  float depthBiasSlope = 2.0f;

  bool operator==(const LightParams&) const = default;
} light;

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

  lvk::Holder<lvk::TextureHandle> shadowMap = ctx->createTexture({
      .type       = lvk::TextureType_2D,
      .format     = lvk::Format_Z_UN16,
      .dimensions = { 4096, 4096 },
      .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
      .swizzle    = { .r = lvk::Swizzle_R, .g = lvk::Swizzle_R, .b = lvk::Swizzle_R, .a = lvk::Swizzle_1 },
      .debugName  = "Shadow map",
  });

  lvk::Holder<lvk::SamplerHandle> samplerShadow = ctx->createSampler({
      .wrapU               = lvk::SamplerWrap_Clamp,
      .wrapV               = lvk::SamplerWrap_Clamp,
      .depthCompareOp      = lvk::CompareOp_LessEqual,
      .depthCompareEnabled = true,
      .debugName           = "Sampler: shadow",
  });

  struct LightData {
    mat4 viewProjBias;
    vec4 lightDir;
    uint32_t shadowTexture;
    uint32_t shadowSampler;
  };
  lvk::Holder<lvk::BufferHandle> bufferLight = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(LightData),
      .debugName = "Buffer: light",
  });

  const Skybox skyBox(
      ctx, "data/immenstadter_horn_2k_prefilter.ktx", "data/immenstadter_horn_2k_irradiance.ktx", ctx->getSwapchainFormat(),
      app.getDepthFormat(), kNumSamples);

  VKMesh11Lazy mesh(ctx, meshData, scene);
  const VKPipeline11 pipelineMesh(
      ctx, meshData.streams, ctx->getSwapchainFormat(), app.getDepthFormat(), kNumSamples,
      loadShaderModule(ctx, "Chapter11/03_DirectionalShadows/src/main.vert"),
      loadShaderModule(ctx, "Chapter11/03_DirectionalShadows/src/main.frag"));
  const VKPipeline11 pipelineShadow(
      ctx, meshData.streams, lvk::Format_Invalid, ctx->getFormat(shadowMap), 1,
      loadShaderModule(ctx, "Chapter11/03_DirectionalShadows/src/shadow.vert"),
      loadShaderModule(ctx, "Chapter11/03_DirectionalShadows/src/shadow.frag"));

  // pretransform bounding boxes to world space
  std::vector<BoundingBox> reorderedBoxes;
  reorderedBoxes.resize(scene.globalTransform.size());
  for (auto& p : scene.meshForNode) {
    reorderedBoxes[p.first] = meshData.boxes[p.second].getTransformed(scene.globalTransform[p.first]);
  }

  // create the scene AABB in world space
  BoundingBox bigBoxWS = reorderedBoxes.front();
  for (const auto& b : reorderedBoxes) {
    bigBoxWS.combinePoint(b.min_);
    bigBoxWS.combinePoint(b.max_);
  }

  // update shadow map
  LightParams prevLight = { .depthBiasConst = 0 };

  // clang-format off
  const mat4 scaleBias = mat4(0.5, 0.0, 0.0, 0.0,
                              0.0, 0.5, 0.0, 0.0,
                              0.0, 0.0, 1.0, 0.0,
                              0.5, 0.5, 0.0, 1.0);
  // clang-format on

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 view = app.camera_.getViewMatrix();
    const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.1f, 200.0f);

    const glm::mat4 rot1 = glm::rotate(mat4(1.f), glm::radians(light.theta), glm::vec3(0, 1, 0));
    const glm::mat4 rot2 = glm::rotate(rot1, glm::radians(light.phi), glm::vec3(1, 0, 0));
    const vec3 lightDir  = glm::normalize(vec3(rot2 * vec4(0.0f, -1.0f, 0.0f, 1.0f)));
    const mat4 lightView = glm::lookAt(glm::vec3(0.0f), lightDir, vec3(0, 0, 1));

	 // transform scene AABB to light space
    const BoundingBox boxLS = bigBoxWS.getTransformed(lightView);
    const mat4 lightProj = glm::orthoLH_ZO(boxLS.min_.x, boxLS.max_.x, boxLS.min_.y, boxLS.max_.y, boxLS.max_.z, boxLS.min_.z);

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      mesh.processLoadedTextures(buf);

      // update shadow map
      if (prevLight != light) {
        prevLight = light;
        buf.cmdBeginRendering(
            lvk::RenderPass{
                .depth = {.loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f}
        },
            lvk::Framebuffer{ .depthStencil = { .texture = shadowMap } });
        buf.cmdPushDebugGroupLabel("Shadow map", 0xff0000ff);
        buf.cmdSetDepthBias(light.depthBiasConst, light.depthBiasSlope);
        buf.cmdSetDepthBiasEnable(true);
        mesh.draw(buf, pipelineShadow, lightView, lightProj);
        buf.cmdSetDepthBiasEnable(false);
        buf.cmdPopDebugGroupLabel();
        buf.cmdEndRendering();
        buf.cmdUpdateBuffer(
            bufferLight, LightData{
                             .viewProjBias  = scaleBias * lightProj * lightView,
                             .lightDir      = vec4(lightDir, 0.0f),
                             .shadowTexture = shadowMap.index(),
                             .shadowSampler = samplerShadow.index(),
                         });
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
          framebufferMSAA, { .textures = { lvk::TextureHandle(shadowMap) } });
      skyBox.draw(buf, view, proj);
      if (drawMeshes) {
        const struct {
          mat4 viewProj;
          uint64_t bufferTransforms;
          uint64_t bufferDrawData;
          uint64_t bufferMaterials;
          uint64_t bufferLight;
          uint32_t texSkyboxIrradiance;
        } pc = {
          .viewProj            = proj * view,
          .bufferTransforms    = ctx->gpuAddress(mesh.bufferTransforms_),
          .bufferDrawData      = ctx->gpuAddress(mesh.bufferDrawData_),
          .bufferMaterials     = ctx->gpuAddress(mesh.bufferMaterials_),
          .bufferLight         = ctx->gpuAddress(bufferLight),
          .texSkyboxIrradiance = skyBox.texSkyboxIrradiance.index(),
        };
        buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
        mesh.draw(buf, pipelineMesh, &pc, sizeof(pc), { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true }, drawWireframe);
        buf.cmdPopDebugGroupLabel();
      }
      app.drawGrid(buf, proj, vec3(0, -1.0f, 0), kNumSamples);
      canvas3d.clear();
      canvas3d.setMatrix(proj * view);
      canvas3d.frustum(lightView, lightProj, vec4(1, 1, 0, 1));
      // render all bounding boxes (red)
      if (drawBoxes) {
        for (auto& p : scene.meshForNode) {
          const BoundingBox box = meshData.boxes[p.second];
          canvas3d.box(scene.globalTransform[p.first], box, vec4(1, 0, 0, 1));
        }
      }
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
        ImGui::Text("Depth bias factor:");
        ImGui::Indent(indentSize);
        ImGui::SliderFloat("Constant", &light.depthBiasConst, 0.0f, 5.0f);
        ImGui::SliderFloat("Slope", &light.depthBiasSlope, 0.0f, 5.0f);
        ImGui::Unindent(indentSize);
        ImGui::Separator();
        ImGui::Text("Light angles:");
        ImGui::Indent(indentSize);
        ImGui::SliderFloat("Theta", &light.theta, -180.0f, +180.0f);
        ImGui::SliderFloat("Phi", &light.phi, -85.0f, +85.0f);
        ImGui::Unindent(indentSize);
        ImGui::Image(shadowMap.index(), ImVec2(512, 512));
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
