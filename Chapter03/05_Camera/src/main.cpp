#include <lvk/LVK.h>
#include <lvk/HelpersImGui.h>

#include <GLFW/glfw3.h>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

#include <shared/Bitmap.h>
#include <shared/Camera.h>
#include <shared/Utils.h>
#include <shared/UtilsCubemap.h>

#include <vector>

using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

struct MouseState {
	glm::vec2 pos = glm::vec2(0.0f);
	bool pressedLeft = false;
} mouseState;

const vec3 kInitialCameraPos    = vec3(0.0f, 1.0f, -1.5f);
const vec3 kInitialCameraTarget = vec3(0.0f, 0.5f, 0.0f);

CameraPositioner_FirstPerson positioner(kInitialCameraPos, kInitialCameraTarget, vec3(0.0f, 1.0f, 0.0f));
Camera camera(positioner);

int main(void)
{
  minilog::initialize(nullptr, { .threadNames = false });

  GLFWwindow* window = nullptr;
  SCOPE_EXIT
  {
    glfwDestroyWindow(window);
    glfwTerminate();
  };

  {
    std::unique_ptr<lvk::IContext> ctx;
    lvk::Holder<lvk::TextureHandle> depthTexture;
    {
      int width  = -95;
      int height = -90;

      window = lvk::initWindow("Simple example", width, height);
      ctx    = lvk::createVulkanContextWithSwapchain(window, width, height, {});

      depthTexture = ctx->createTexture({
          .type       = lvk::TextureType_2D,
          .format     = lvk::Format_Z_F32,
          .dimensions = {(uint32_t)width, (uint32_t)height},
          .usage      = lvk::TextureUsageBits_Attachment,
          .debugName  = "Depth buffer",
      });
    }

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
        .depthFormat = ctx->getFormat(depthTexture),
        .cullMode    = lvk::CullMode_Back,
    });

    lvk::Holder<lvk::RenderPipelineHandle> pipelineSkybox = ctx->createRenderPipeline({
        .smVert      = vertSkybox,
        .smFrag      = fragSkybox,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = ctx->getFormat(depthTexture),
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

    glfwSetCursorPosCallback(window, [](auto* window, double x, double y) {
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);
      mouseState.pos.x = static_cast<float>(x / width);
      mouseState.pos.y = 1.0f - static_cast<float>(y / height);
    });

    glfwSetMouseButtonCallback(window, [](auto* window, int button, int action, int mods) {
      if (button == GLFW_MOUSE_BUTTON_LEFT) {
        mouseState.pressedLeft = action == GLFW_PRESS;
      }
      double xpos, ypos;
      glfwGetCursorPos(window, &xpos, &ypos);
      const ImGuiMouseButton_ imguiButton = (button == GLFW_MOUSE_BUTTON_LEFT)
                                                ? ImGuiMouseButton_Left
                                                : (button == GLFW_MOUSE_BUTTON_RIGHT ? ImGuiMouseButton_Right : ImGuiMouseButton_Middle);

      ImGuiIO& io               = ImGui::GetIO();
      io.MousePos               = ImVec2((float)xpos, (float)ypos);
      io.MouseDown[imguiButton] = action == GLFW_PRESS;
    });

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
      const bool pressed = action != GLFW_RELEASE;
      if (key == GLFW_KEY_ESCAPE && pressed)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
      if (key == GLFW_KEY_W)
        positioner.movement_.forward_ = pressed;
      if (key == GLFW_KEY_S)
        positioner.movement_.backward_ = pressed;
      if (key == GLFW_KEY_A)
        positioner.movement_.left_ = pressed;
      if (key == GLFW_KEY_D)
        positioner.movement_.right_ = pressed;
      if (key == GLFW_KEY_1)
        positioner.movement_.up_ = pressed;
      if (key == GLFW_KEY_2)
        positioner.movement_.down_ = pressed;
      if (mods & GLFW_MOD_SHIFT)
        positioner.movement_.fastSpeed_ = pressed;
      if (key == GLFW_KEY_SPACE) {
        positioner.lookAt(kInitialCameraPos, kInitialCameraTarget, vec3(0.0f, 1.0f, 0.0f));
        positioner.setSpeed(vec3(0));
      }
    });

    std::unique_ptr<lvk::ImGuiRenderer> imgui = std::make_unique<lvk::ImGuiRenderer>(*ctx, "data/OpenSans-Light.ttf", 30.0f);

    double timeStamp   = glfwGetTime();
    float deltaSeconds = 0.0f;

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);
      if (!width || !height)
        continue;
      const float ratio = width / (float)height;

      positioner.update(deltaSeconds, mouseState.pos, mouseState.pressedLeft);

      const double newTimeStamp = glfwGetTime();
      deltaSeconds              = static_cast<float>(newTimeStamp - timeStamp);
      timeStamp                 = newTimeStamp;

      const vec4 cameraPos = vec4(camera.getPosition(), 1.0f);

      const mat4 p  = glm::perspective(glm::radians(60.0f), ratio, 0.1f, 1000.0f);
      const mat4 m1 = glm::rotate(mat4(1.0f), glm::radians(-90.0f), vec3(1, 0, 0));
      const mat4 m2 = glm::rotate(mat4(1.0f), (float)glfwGetTime(), vec3(0.0f, 1.0f, 0.0f));
      const mat4 v  = glm::translate(mat4(1.0f), vec3(cameraPos));

      const PerFrameData pc = {
        .model     = m2 * m1,
        .view      = camera.getViewMatrix(),
        .proj      = p,
        .cameraPos = cameraPos,
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
        .depthStencil = { .texture = depthTexture },
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

          imgui->beginFrame(framebuffer);
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
          imgui->endFrame(buf);

          buf.cmdEndRendering();
        }
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());
    }
  }

  return 0;
}
