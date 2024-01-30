#include "shared/VulkanApp.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "shared/LineCanvas.h"

const vec3 kInitialCameraPos    = vec3(0.0f, 1.0f, -1.5f);
const vec3 kInitialCameraTarget = vec3(0.0f, 0.5f, 0.0f);
const vec3 kInitialCameraAngles = vec3(-18.5f, 180.0f, 0.0f);

CameraPositioner_MoveTo positionerMoveTo(kInitialCameraPos, kInitialCameraAngles);
vec3 cameraPos = kInitialCameraPos;
vec3 cameraAngles = kInitialCameraAngles;

// ImGUI stuff
const char* cameraType          = "FirstPerson";
const char* comboBoxItems[]     = { "FirstPerson", "MoveTo" };
const char* currentComboBoxItem = cameraType;

LinearGraph fpsGraph("##fpsGraph", 2048);
LinearGraph sinGraph("##sinGraph", 2048);

void reinitCamera(VulkanApp& app)
{
  if (!strcmp(cameraType, "FirstPerson")) {
    app.camera_ = Camera(app.positioner_);
  } else {
    if (!strcmp(cameraType, "MoveTo")) {
      cameraPos    = kInitialCameraPos;
      cameraAngles = kInitialCameraAngles;
      positionerMoveTo.setDesiredPosition(kInitialCameraPos);
      positionerMoveTo.setDesiredAngles(kInitialCameraAngles);
      app.camera_ = Camera(positionerMoveTo);
    }
  }
}

int main()
{
  VulkanApp app({ .initialCameraPos = kInitialCameraPos, .initialCameraTarget = kInitialCameraTarget });

  LineCanvas2D canvas2d;
  LineCanvas3D canvas3d;

  app.fpsCounter_.avgInterval_ = 0.002f;
  app.fpsCounter_.printFPS_    = false;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  {
    struct VertexData {
      vec3 pos;
      vec3 n;
      vec2 tc;
    };

    lvk::Holder<lvk::ShaderModuleHandle> vert       = loadShaderModule(ctx, "Chapter03/04_CubeMap/src/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> frag       = loadShaderModule(ctx, "Chapter03/04_CubeMap/src/main.frag");
    lvk::Holder<lvk::ShaderModuleHandle> vertSkybox = loadShaderModule(ctx, "Chapter03/04_CubeMap/src/skybox.vert");
    lvk::Holder<lvk::ShaderModuleHandle> fragSkybox = loadShaderModule(ctx, "Chapter03/04_CubeMap/src/skybox.frag");

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
        .cullMode    = lvk::CullMode_Back,
    });

    lvk::Holder<lvk::RenderPipelineHandle> pipelineSkybox = ctx->createRenderPipeline({
        .smVert      = vertSkybox,
        .smFrag      = fragSkybox,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
    });

    const lvk::DepthState dState = { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true };

    const aiScene* scene = aiImportFile("data/rubber_duck/scene.gltf", aiProcess_Triangulate);

    if (!scene || !scene->HasMeshes()) {
      printf("Unable to load data/rubber_duck/scene.gltf\n");
      exit(255);
    }

    const aiMesh* mesh = scene->mMeshes[0];
    std::vector<VertexData> vertices;
    for (uint32_t i = 0; i != mesh->mNumVertices; i++) {
      const aiVector3D v = mesh->mVertices[i];
      const aiVector3D n = mesh->mNormals[i];
      const aiVector3D t = mesh->mTextureCoords[0][i];
      vertices.push_back({ .pos = vec3(v.x, v.y, v.z), .n = vec3(n.x, n.y, n.z), .tc = vec2(t.x, t.y) });
    }
    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i != mesh->mNumFaces; i++) {
      for (uint32_t j = 0; j != 3; j++)
        indices.push_back(mesh->mFaces[i].mIndices[j]);
    }
    aiReleaseImport(scene);

    const size_t kSizeIndices  = sizeof(uint32_t) * indices.size();
    const size_t kSizeVertices = sizeof(VertexData) * vertices.size();

    // indices
    lvk::Holder<lvk::BufferHandle> bufferIndices = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Index,
          .storage   = lvk::StorageType_Device,
          .size      = kSizeIndices,
          .data      = indices.data(),
          .debugName = "Buffer: indices" },
        nullptr);

    // vertices
    lvk::Holder<lvk::BufferHandle> bufferVertices = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Vertex,
          .storage   = lvk::StorageType_Device,
          .size      = kSizeVertices,
          .data      = vertices.data(),
          .debugName = "Buffer: vertices" },
        nullptr);

    struct PerFrameData {
      mat4 model;
      mat4 view;
      mat4 proj;
      vec4 cameraPos;
      uint32_t tex     = 0;
      uint32_t texCube = 0;
    };

    lvk::Holder<lvk::BufferHandle> bufferPerFrame = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Uniform,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(PerFrameData),
          .debugName = "Buffer: per-frame" },
        nullptr);

    // texture
    lvk::Holder<lvk::TextureHandle> texture = loadTexture(ctx, "data/rubber_duck/textures/Duck_baseColor.png");

    // cube map
    lvk::Holder<lvk::TextureHandle> cubemapTex;
    {
      int w, h;
      const float* img = stbi_loadf("data/piazza_bologni_1k.hdr", &w, &h, nullptr, 4);
      Bitmap in(w, h, 4, eBitmapFormat_Float, img);
      Bitmap out = convertEquirectangularMapToVerticalCross(in);
      stbi_image_free((void*)img);

      stbi_write_hdr(".cache/screenshot.hdr", out.w_, out.h_, out.comp_, (const float*)out.data_.data());

      Bitmap cubemap = convertVerticalCrossToCubeMapFaces(out);

      cubemapTex = ctx->createTexture({
          .type       = lvk::TextureType_Cube,
          .format     = lvk::Format_RGBA_F32,
          .dimensions = {(uint32_t)cubemap.w_, (uint32_t)cubemap.h_},
          .usage      = lvk::TextureUsageBits_Sampled,
          .data       = cubemap.data_.data(),
          .debugName  = "data/piazza_bologni_1k.hdr",
      });
    }

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
      positionerMoveTo.update(deltaSeconds, app.mouseState_.pos, ImGui::GetIO().WantCaptureMouse ? false : app.mouseState_.pressedLeft);

      const mat4 p  = glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 1000.0f);
      const mat4 m1 = glm::rotate(mat4(1.0f), glm::radians(-90.0f), vec3(1, 0, 0));
      const mat4 m2 = glm::rotate(mat4(1.0f), (float)glfwGetTime(), vec3(0.0f, 1.0f, 0.0f));
      const mat4 v  = glm::translate(mat4(1.0f), app.camera_.getPosition());

      const PerFrameData pc = {
        .model     = m2 * m1,
        .view      = app.camera_.getViewMatrix(),
        .proj      = p,
        .cameraPos = vec4(app.camera_.getPosition(), 1.0f),
        .tex       = texture.index(),
        .texCube   = cubemapTex.index(),
      };

      ctx->upload(bufferPerFrame, &pc, sizeof(pc));

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
          buf.cmdPushConstants(ctx->gpuAddress(bufferPerFrame));
          {
            buf.cmdPushDebugGroupLabel("Skybox", 0xff0000ff);
            buf.cmdBindRenderPipeline(pipelineSkybox);
            buf.cmdDraw(36);
            buf.cmdPopDebugGroupLabel();
          }
          {
            buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
            buf.cmdBindVertexBuffer(0, bufferVertices);
            buf.cmdBindRenderPipeline(pipeline);
            buf.cmdBindDepthState(dState);
            buf.cmdBindIndexBuffer(bufferIndices, lvk::IndexFormat_UI32);
            buf.cmdDrawIndexed(indices.size());
            buf.cmdPopDebugGroupLabel();
          }

          app.imgui_->beginFrame(framebuffer);

          // memo
          ImGui::SetNextWindowPos(ImVec2(10, 10));
          ImGui::Begin(
              "Keyboard hints:", nullptr,
              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs |
                  ImGuiWindowFlags_NoCollapse);
          ImGui::Text("W/S/A/D - camera movement");
          ImGui::Text("1/2 - camera up/down");
          ImGui::Text("Shift - fast movement");
          ImGui::Text("Space - reset view");
          ImGui::End();

          // FPS
          if (const ImGuiViewport* v = ImGui::GetMainViewport()) {
            ImGui::SetNextWindowPos({ v->WorkPos.x + v->WorkSize.x - 15.0f, v->WorkPos.y + 15.0f }, ImGuiCond_Always, { 1.0f, 0.0f });
          }
          ImGui::SetNextWindowBgAlpha(0.30f);
          ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize("FPS : _______").x, 0));
          if (ImGui::Begin(
                  "##FPS", nullptr,
                  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
            ImGui::Text("FPS : %i", (int)app.fpsCounter_.getFPS());
            ImGui::Text("Ms  : %.1f", 1000.0 / app.fpsCounter_.getFPS());
          }
          ImGui::End();

          // camera controls
          ImGui::Begin("Camera Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
          {
            if (ImGui::BeginCombo("##combo", currentComboBoxItem)) // the second parameter is the label previewed before opening the combo.
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

            if (!strcmp(cameraType, "MoveTo")) {
              if (ImGui::SliderFloat3("Position", glm::value_ptr(cameraPos), -10.0f, +10.0f))
                positionerMoveTo.setDesiredPosition(cameraPos);
              if (ImGui::SliderFloat3("Pitch/Pan/Roll", glm::value_ptr(cameraAngles), -180.0f, +180.0f))
                positionerMoveTo.setDesiredAngles(cameraAngles);
            }

            if (currentComboBoxItem && strcmp(currentComboBoxItem, cameraType)) {
              printf("Selected new camera type: %s\n", currentComboBoxItem);
              cameraType = currentComboBoxItem;
              reinitCamera(app);
            }
          }
          ImGui::End();

          // graphs
          sinGraph.renderGraph(0, height * 0.7f, width, height * 0.2f, vec4(0.0f, 1.0f, 0.0f, 1.0f));
          fpsGraph.renderGraph(0, height * 0.8f, width, height * 0.2f);

          canvas2d.clear();
          canvas2d.line({ 100, 300 }, { 100, 400 }, vec4(1, 0, 0, 1));
          canvas2d.line({ 100, 400 }, { 200, 400 }, vec4(0, 1, 0, 1));
          canvas2d.line({ 200, 400 }, { 200, 300 }, vec4(0, 0, 1, 1));
          canvas2d.line({ 200, 300 }, { 100, 300 }, vec4(1, 1, 0, 1));
          canvas2d.render("##plane");

          canvas3d.clear();
          canvas3d.setMatrix(pc.proj * pc.view);
          canvas3d.plane(vec3(0, 0, 0), vec3(1, 0, 0), vec3(0, 0, 1), 40, 40, 10.0f, 10.0f, vec4(1, 0, 0, 1), vec4(0, 1, 0, 1));
          canvas3d.box(mat4(1.0f), BoundingBox(vec3(-2), vec3(+2)), vec4(1, 1, 0, 1));
          canvas3d.frustum(
              glm::lookAt(vec3(cos(glfwGetTime()), kInitialCameraPos.y, sin(glfwGetTime())), kInitialCameraTarget, vec3(0.0f, 1.0f, 0.0f)),
              glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 30.0f), vec4(1, 1, 1, 1));
          canvas3d.render(*ctx.get(), framebuffer, buf, width, height);

          app.imgui_->endFrame(buf);

          buf.cmdEndRendering();
        }
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());

      fpsGraph.addPoint(app.fpsCounter_.getFPS());
      sinGraph.addPoint(sinf(glfwGetTime() * 20.0f));
    });

    ctx.release();
  }

  return 0;
}
