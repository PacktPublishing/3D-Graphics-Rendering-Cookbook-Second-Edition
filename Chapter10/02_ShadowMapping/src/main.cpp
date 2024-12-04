#include "shared/VulkanApp.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "shared/LineCanvas.h"
#include "shared/Utils.h"

#include "shared/Scene/Scene.h"
#include "shared/Scene/VtxData.h"

#include "Chapter08/VKMesh08.h"

#include <math.h>

int main()
{
  VulkanApp app({
      .initialCameraPos    = vec3(0.0f, 3.0f, -4.5f),
      .initialCameraTarget = vec3(0.0f, 0.5f, 0.0f),
  });

  LineCanvas3D canvas3d;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  struct VertexData {
    vec3 pos;
    vec3 n;
    vec2 tc;
  };

  // 0. Scene vertices/indices
  std::vector<VertexData> vertices;
  std::vector<uint32_t> indices;

  // 1. Duck
  {
    const aiScene* scene = aiImportFile("data/rubber_duck/scene.gltf", aiProcess_Triangulate);

    if (!scene || !scene->HasMeshes()) {
      printf("Unable to load data/rubber_duck/scene.gltf\n");
      exit(255);
    }

    const aiMesh* mesh = scene->mMeshes[0];
    for (uint32_t i = 0; i != mesh->mNumVertices; i++) {
      const aiVector3D v = mesh->mVertices[i];
      const aiVector3D n = mesh->mNormals[i];
      const aiVector3D t = mesh->mTextureCoords[0][i];
      vertices.push_back({ .pos = vec3(v.x, v.y, v.z), .n = vec3(n.x, n.y, n.z), .tc = vec2(t.x, t.y) });
    }
    for (uint32_t i = 0; i != mesh->mNumFaces; i++) {
      for (uint32_t j = 0; j != 3; j++)
        indices.push_back(mesh->mFaces[i].mIndices[j]);
    }
    aiReleaseImport(scene);
  }

  const uint32_t duckNumIndices    = (uint32_t)indices.size();
  const uint32_t planeVertexOffset = (uint32_t)vertices.size();

  // 2. Plane
  mergeVectors(indices, { 0, 1, 2, 2, 3, 0 });
  mergeVectors(
      vertices, {
                    {vec3(-4, -4, 0), vec3(0, 0, 1), vec2(0, 0)},
                    {vec3(-4, +4, 0), vec3(0, 0, 1), vec2(0, 1)},
                    {vec3(+4, +4, 0), vec3(0, 0, 1), vec2(1, 1)},
                    {vec3(+4, -4, 0), vec3(0, 0, 1), vec2(1, 0)},
  });

  lvk::Holder<lvk::BufferHandle> bufferIndices = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Index,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(uint32_t) * indices.size(),
      .data      = indices.data(),
      .debugName = "Buffer: indices",
  });

  lvk::Holder<lvk::BufferHandle> bufferVertices = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Vertex,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(VertexData) * vertices.size(),
      .data      = vertices.data(),
      .debugName = "Buffer: vertices",
  });

  // Textures
  lvk::Holder<lvk::TextureHandle> duckTexture  = loadTexture(ctx, "data/rubber_duck/textures/Duck_baseColor.png");
  lvk::Holder<lvk::TextureHandle> planeTexture = loadTexture(ctx, "data/wood.jpg");

  struct PerFrameData {
    mat4 view;
    mat4 proj;
    mat4 light;
    vec4 lightAngles; // cos(inner), cos(outer)
    vec4 lightPos;
    uint32_t shadowTexture;
    uint32_t shadowSampler;
  };
  lvk::Holder<lvk::BufferHandle> bufferPerFrame = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Uniform,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(PerFrameData),
      .debugName = "Buffer: per-frame",
  });

  lvk::Holder<lvk::TextureHandle> shadowMap = ctx->createTexture({
      .type       = lvk::TextureType_2D,
      .format     = lvk::Format_Z_UN16,
      .dimensions = {1024, 1024},
      .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
      .debugName  = "Shadow map",
  });

  lvk::Holder<lvk::SamplerHandle> samplerShadow = ctx->createSampler({
      .wrapU               = lvk::SamplerWrap_Clamp,
      .wrapV               = lvk::SamplerWrap_Clamp,
      .depthCompareOp      = lvk::CompareOp_LessEqual,
      .depthCompareEnabled = true,
      .debugName           = "Sampler: shadow",
  });

  lvk::Holder<lvk::ShaderModuleHandle> vert       = loadShaderModule(ctx, "Chapter10/02_ShadowMapping/src/main.vert");
  lvk::Holder<lvk::ShaderModuleHandle> frag       = loadShaderModule(ctx, "Chapter10/02_ShadowMapping/src/main.frag");
  lvk::Holder<lvk::ShaderModuleHandle> vertShadow = loadShaderModule(ctx, "Chapter10/02_ShadowMapping/src/shadow.vert");
  lvk::Holder<lvk::ShaderModuleHandle> fragShadow = loadShaderModule(ctx, "Chapter10/02_ShadowMapping/src/shadow.frag");

  const lvk::VertexInput vdesc = {
      .attributes    = {{ .location = 0, .format = lvk::VertexFormat::Float3, .offset = offsetof(VertexData, pos) },
                        { .location = 1, .format = lvk::VertexFormat::Float3, .offset = offsetof(VertexData, n) },
                        { .location = 2, .format = lvk::VertexFormat::Float2, .offset = offsetof(VertexData, tc) }, },
      .inputBindings = { { .stride = sizeof(VertexData) } },
    };

  lvk::Holder<lvk::RenderPipelineHandle> pipeline = ctx->createRenderPipeline({
      .vertexInput = vdesc,
      .smVert      = vert,
      .smFrag      = frag,
      .color       = { { .format = ctx->getSwapchainFormat() } },
      .depthFormat = app.getDepthFormat(),
  });

  lvk::Holder<lvk::RenderPipelineHandle> pipelineShadow = ctx->createRenderPipeline({
      .vertexInput =
          lvk::VertexInput{
                           .attributes    = { { .location = 0, .format = lvk::VertexFormat::Float3, .offset = offsetof(VertexData, pos) } },
                           .inputBindings = { { .stride = sizeof(VertexData) } },
                           },
      .smVert      = vertShadow,
      .smFrag      = fragShadow,
      .depthFormat = ctx->getFormat(shadowMap),
  });

  float g_LightFOV        = 45.0f;
  float g_LightInnerAngle = 10.0f;
  float g_LightNear       = 0.8f;
  float g_LightFar        = 8.0f;

  float g_LightDist   = 4.0f;
  float g_LightXAngle = 240.0f;
  float g_LightYAngle = 0.0f;

  float g_LightDepthBiasConst = 1.5f;
  float g_LightDepthBiasSlope = 3.0f;

  bool g_RotateModel = true;
  bool g_RotateLight = true;
  bool g_DrawFrustum = true;

  float g_ModelAngle = 0;
  float g_LightAngle = 0;

  const char* comboBoxItems[]     = { "First person", "Light source" };
  const char* cameraType          = comboBoxItems[0];
  const char* currentComboBoxItem = cameraType;

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    if (g_RotateModel)
      g_ModelAngle = fmodf(g_ModelAngle - 50.0f * deltaSeconds, 360.0f);
    if (g_RotateLight)
      g_LightYAngle = fmodf(g_LightYAngle + 50.0f * deltaSeconds, 360.0f);

    // 0. Calculate light and camera parameters
    const mat4 rotY     = glm::rotate(mat4(1.f), glm::radians(g_LightYAngle), vec3(0, 1, 0));
    const mat4 rotX     = glm::rotate(rotY, glm::radians(g_LightXAngle), vec3(1, 0, 0));
    const vec4 lightPos = rotX * vec4(0, 0, g_LightDist, 1.0f);

    const mat4 lightProj = glm::perspective(glm::radians(g_LightFOV), 1.0f, g_LightNear, g_LightFar);
    const mat4 lightView = glm::lookAt(vec3(lightPos), vec3(0), vec3(0, 1, 0));

    const bool showLightCamera = cameraType == comboBoxItems[1];

    const mat4 view = showLightCamera ? lightView : app.camera_.getViewMatrix();
    const mat4 proj = showLightCamera ? glm::perspective(glm::radians(g_LightFOV), aspectRatio, g_LightNear, g_LightFar)
                                      : glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 1000.0f);
    const mat4 m1   = glm::rotate(mat4(1.0f), glm::radians(-90.0f), vec3(1, 0, 0));
    const mat4 m2   = glm::rotate(mat4(1.0f), glm::radians(g_ModelAngle), vec3(0.0f, 1.0f, 0.0f));

    const lvk::Framebuffer framebuffer = {
      .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
      .depthStencil = { .texture = app.getDepthTexture() },
    };

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    buf.cmdBindVertexBuffer(0, bufferVertices);
    buf.cmdBindIndexBuffer(bufferIndices, lvk::IndexFormat_UI32);
    struct PushConstants {
      mat4 model;
      uint64_t perFrameBuffer;
      uint32_t texture;
    };
    // 1. Render shadow map
    buf.cmdUpdateBuffer(
        bufferPerFrame, PerFrameData{
                            .view = lightView,
                            .proj = lightProj,
                        });
    buf.cmdBeginRendering(
        lvk::RenderPass{
            .depth = {.loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f}
    },
        lvk::Framebuffer{ .depthStencil = { .texture = shadowMap } });

    buf.cmdBindRenderPipeline(pipelineShadow);
    buf.cmdPushConstants(PushConstants{
        .model          = m2 * m1,
        .perFrameBuffer = ctx->gpuAddress(bufferPerFrame),
    });
    buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
    buf.cmdSetDepthBias(g_LightDepthBiasConst, g_LightDepthBiasSlope);
    buf.cmdSetDepthBiasEnable(true);
    buf.cmdDrawIndexed(duckNumIndices);
    buf.cmdSetDepthBiasEnable(false);
    buf.cmdEndRendering();
    // 2. Render scene
    const mat4 scaleBias = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.5, 0.5, 0.0, 1.0);
    buf.cmdUpdateBuffer(
        bufferPerFrame,
        PerFrameData{
            .view  = view,
            .proj  = proj,
            .light = scaleBias * lightProj * lightView,
            .lightAngles =
                vec4(cosf(glm::radians(0.5f * g_LightFOV)), cosf(glm::radians(0.5f * (g_LightFOV - g_LightInnerAngle))), 1.0f, 1.0f),
            .lightPos      = lightPos,
            .shadowTexture = shadowMap.index(),
            .shadowSampler = samplerShadow.index(),
        });

    buf.cmdBeginRendering(
        lvk::RenderPass{
            .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
            .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
    },
        framebuffer, { .textures = { { lvk::TextureHandle(shadowMap) } } });
    buf.cmdBindRenderPipeline(pipeline);
    buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
    {
      buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
      buf.cmdPushConstants(PushConstants{
          .model          = m2 * m1,
          .perFrameBuffer = ctx->gpuAddress(bufferPerFrame),
          .texture        = duckTexture.index(),
      });
      buf.cmdDrawIndexed(duckNumIndices);
      buf.cmdPopDebugGroupLabel();
    }
    {
      buf.cmdPushDebugGroupLabel("Plane", 0xff0000ff);
      buf.cmdPushConstants(PushConstants{
          .model          = m1,
          .perFrameBuffer = ctx->gpuAddress(bufferPerFrame),
          .texture        = planeTexture.index(),
      });
      buf.cmdDrawIndexed(6, 1, 0, planeVertexOffset);
      buf.cmdPopDebugGroupLabel();
    }
    if (!showLightCamera)
      app.drawGrid(buf, proj, vec3(0, -0.01f, 0));
    app.imgui_->beginFrame(framebuffer);
    app.drawFPS();
    app.drawMemo();

    const ImGuiViewport* v = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(10, 200));
    ImGui::Begin("Control", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Checkbox("Rotate model", &g_RotateModel);
    ImGui::Separator();
    ImGui::Text("Light parameters", nullptr);
    const float indentSize = 16.0f;
    ImGui::Indent(indentSize);
    ImGui::Checkbox("Rotate light", &g_RotateLight);
    ImGui::Checkbox("Draw light frustum", &g_DrawFrustum);
    ImGui::Separator();
    ImGui::Text("Depth bias factor", nullptr);
    ImGui::Indent(indentSize);
    ImGui::SliderFloat("Constant", &g_LightDepthBiasConst, 0.0f, 5.0f);
    ImGui::SliderFloat("Slope", &g_LightDepthBiasSlope, 0.0f, 5.0f);
    ImGui::Unindent(indentSize);
    ImGui::Separator();
    ImGui::SliderFloat("Proj::Light FOV", &g_LightFOV, 15.0f, 120.0f);
    ImGui::SliderFloat("Proj::Light inner angle", &g_LightInnerAngle, 1.0f, 15.0f);
    ImGui::SliderFloat("Proj::Near", &g_LightNear, 0.1f, 3.0f);
    ImGui::SliderFloat("Proj::Far", &g_LightFar, 1.0f, 20.0f);
    ImGui::SliderFloat("Pos::Dist", &g_LightDist, 1.0f, 10.0f);
    ImGui::SliderFloat("Pos::AngleX", &g_LightXAngle, 0, 360.0f);
    ImGui::BeginDisabled(g_RotateLight);
    ImGui::SliderFloat("Pos::AngleY", &g_LightYAngle, 0, 360.0f);
    ImGui::EndDisabled();
    ImGui::Unindent(indentSize);
    ImGui::Separator();
    ImGui::Image(shadowMap.index(), ImVec2(512, 512));
    ImGui::Separator();
    // camera controls
    {
      if (ImGui::BeginCombo("Camera", currentComboBoxItem)) // the second parameter is the label previewed before opening the combo.
      {
        for (int n = 0; n < IM_ARRAYSIZE(comboBoxItems); n++) {
          const bool isSelected = (currentComboBoxItem == comboBoxItems[n]);
          if (ImGui::Selectable(comboBoxItems[n], isSelected))
            currentComboBoxItem = comboBoxItems[n];
          if (isSelected)
            ImGui::SetItemDefaultFocus(); // initial focus when opening the combo (scrolling + for keyboard navigation support)
        }
        ImGui::EndCombo();
      }
      if (currentComboBoxItem && strcmp(currentComboBoxItem, cameraType)) {
        printf("Selected new camera type: %s\n", currentComboBoxItem);
        cameraType = currentComboBoxItem;
      }
    }

    ImGui::End();

    if (!showLightCamera && g_DrawFrustum) {
      canvas3d.clear();
      canvas3d.setMatrix(proj * view);
      canvas3d.frustum(lightView, lightProj, vec4(1, 0, 0, 1));
      canvas3d.render(*ctx.get(), framebuffer, buf);
    }

    app.imgui_->endFrame(buf);

    buf.cmdEndRendering();
    ctx->submit(buf, ctx->getCurrentSwapchainTexture());
  });

  ctx.release();

  return 0;
}
