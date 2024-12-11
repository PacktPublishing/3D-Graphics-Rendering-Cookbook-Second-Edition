#include "VulkanApp.h"

#include "UtilsGLTF.h"
#include <unordered_map>

extern std::unordered_map<uint32_t, std::string> debugGLSLSourceCode;

static void shaderModuleCallback(lvk::IContext*, lvk::ShaderModuleHandle handle, int line, int col, const char* debugName)
{
  const auto it = debugGLSLSourceCode.find(handle.index());

  if (it != debugGLSLSourceCode.end()) {
    lvk::logShaderSource(it->second.c_str());
  }
}

VulkanApp::VulkanApp(const VulkanAppConfig& cfg)
: cfg_(cfg)
{
  minilog::initialize(nullptr, { .threadNames = false });

  int width  = -95;
  int height = -90;

  window_ = lvk::initWindow("Simple example", width, height);
  ctx_    = lvk::createVulkanContextWithSwapchain(
      window_, width, height,
      {
             .enableValidation          = true,
             .shaderModuleErrorCallback = &shaderModuleCallback,
      });
  depthTexture_ = ctx_->createTexture({
      .type       = lvk::TextureType_2D,
      .format     = lvk::Format_Z_F32,
      .dimensions = {(uint32_t)width, (uint32_t)height},
      .usage      = lvk::TextureUsageBits_Attachment,
      .debugName  = "Depth buffer",
  });

  imgui_     = std::make_unique<lvk::ImGuiRenderer>(*ctx_, "data/OpenSans-Light.ttf", 30.0f);
  implotCtx_ = ImPlot::CreateContext();

  glfwSetWindowUserPointer(window_, this);

  glfwSetMouseButtonCallback(window_, [](GLFWwindow* window, int button, int action, int mods) {
    VulkanApp* app = (VulkanApp*)glfwGetWindowUserPointer(window);
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
      app->mouseState_.pressedLeft = action == GLFW_PRESS;
    }
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    const ImGuiMouseButton_ imguiButton = (button == GLFW_MOUSE_BUTTON_LEFT)
                                              ? ImGuiMouseButton_Left
                                              : (button == GLFW_MOUSE_BUTTON_RIGHT ? ImGuiMouseButton_Right : ImGuiMouseButton_Middle);
    ImGuiIO& io                         = ImGui::GetIO();
    io.MousePos                         = ImVec2((float)xpos, (float)ypos);
    io.MouseDown[imguiButton]           = action == GLFW_PRESS;
    for (auto& cb : app->callbacksMouseButton) {
      cb(window, button, action, mods);
    }
  });
  glfwSetScrollCallback(window_, [](GLFWwindow* window, double dx, double dy) {
    ImGuiIO& io    = ImGui::GetIO();
    io.MouseWheelH = (float)dx;
    io.MouseWheel  = (float)dy;
  });
  glfwSetCursorPosCallback(window_, [](GLFWwindow* window, double x, double y) {
    VulkanApp* app = (VulkanApp*)glfwGetWindowUserPointer(window);
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    ImGui::GetIO().MousePos = ImVec2(x, y);
    app->mouseState_.pos.x  = static_cast<float>(x / width);
    app->mouseState_.pos.y  = 1.0f - static_cast<float>(y / height);
  });
  glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
    VulkanApp* app     = (VulkanApp*)glfwGetWindowUserPointer(window);
    const bool pressed = action != GLFW_RELEASE;
    if (key == GLFW_KEY_ESCAPE && pressed)
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (key == GLFW_KEY_W)
      app->positioner_.movement_.forward_ = pressed;
    if (key == GLFW_KEY_S)
      app->positioner_.movement_.backward_ = pressed;
    if (key == GLFW_KEY_A)
      app->positioner_.movement_.left_ = pressed;
    if (key == GLFW_KEY_D)
      app->positioner_.movement_.right_ = pressed;
    if (key == GLFW_KEY_1)
      app->positioner_.movement_.up_ = pressed;
    if (key == GLFW_KEY_2)
      app->positioner_.movement_.down_ = pressed;

    app->positioner_.movement_.fastSpeed_ = (mods & GLFW_MOD_SHIFT) != 0;

    if (key == GLFW_KEY_SPACE) {
      app->positioner_.lookAt(app->cfg_.initialCameraPos, app->cfg_.initialCameraTarget, vec3(0.0f, 1.0f, 0.0f));
      app->positioner_.setSpeed(vec3(0));
    }
    for (auto& cb : app->callbacksKey) {
      cb(window, key, scancode, action, mods);
    }
  });
}

VulkanApp::~VulkanApp()
{
  ImPlot::DestroyContext(implotCtx_);

  gridPipeline  = nullptr;
  gridVert      = nullptr;
  gridFrag      = nullptr;
  imgui_        = nullptr;
  depthTexture_ = nullptr;
  ctx_          = nullptr;

  glfwDestroyWindow(window_);
  glfwTerminate();
}

lvk::Format VulkanApp::getDepthFormat() const
{
  return ctx_->getFormat(depthTexture_);
}

void VulkanApp::run(DrawFrameFunc drawFrame)
{
  double timeStamp   = glfwGetTime();
  float deltaSeconds = 0.0f;

  while (!glfwWindowShouldClose(window_)) {
    fpsCounter_.tick(deltaSeconds);
    const double newTimeStamp = glfwGetTime();
    deltaSeconds              = static_cast<float>(newTimeStamp - timeStamp);
    timeStamp                 = newTimeStamp;

    glfwPollEvents();
    int width, height;
#if defined(__APPLE__)
    // a hacky workaround for retina displays
    glfwGetWindowSize(window_, &width, &height);
#else
    glfwGetFramebufferSize(window_, &width, &height);
#endif
    if (!width || !height)
      continue;
    const float ratio = width / (float)height;

    positioner_.update(deltaSeconds, mouseState_.pos, ImGui::GetIO().WantCaptureMouse ? false : mouseState_.pressedLeft);

    drawFrame((uint32_t)width, (uint32_t)height, ratio, deltaSeconds);
  }
}

void VulkanApp::drawMemo()
{
  ImGui::SetNextWindowPos(ImVec2(10, 10));
  ImGui::Begin(
      "Keyboard hints:", nullptr,
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoCollapse);
  ImGui::Text("W/S/A/D - camera movement");
  ImGui::Text("1/2 - camera up/down");
  ImGui::Text("Shift - fast movement");
  ImGui::Text("Space - reset view");
  ImGui::End();
}

void VulkanApp::drawGTFInspector_Animations(GLTFIntrospective& intro)
{
  if (!intro.showAnimations)
    return;

  if (ImGui::Begin(
          "Animations", nullptr,
          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
              ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoCollapse)) {
    for (uint32_t a = 0; a < intro.animations.size(); ++a) {
      auto it     = std::find(intro.activeAnim.begin(), intro.activeAnim.end(), a);
      bool oState = it != intro.activeAnim.end(); // && selected < anim->size();
      bool state  = oState;
      ImGui::Checkbox(intro.animations[a].c_str(), &state);

      if (state) {
        if (!oState) {
          uint32_t freeSlot = intro.activeAnim.size() - 1;
          if (auto nf = std::find(intro.activeAnim.begin(), intro.activeAnim.end(), ~0u); nf != intro.activeAnim.end()) {
            freeSlot = std::distance(intro.activeAnim.begin(), nf);
          }
          intro.activeAnim[freeSlot] = a;
        }
      } else {
        if (it != intro.activeAnim.end()) {
          *it = ~0;
        }
      }
    }
  }

  if (intro.showAnimationBlend) {
    ImGui::SliderFloat("Blend", &intro.blend, 0, 1.0f);
  }

  ImGui::End();
}

void VulkanApp::drawGTFInspector_Materials(GLTFIntrospective& intro) {
  if (!intro.showMaterials || intro.materials.empty())
    return;

  if (ImGui::Begin(
          "Materials", nullptr,
          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
              ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoCollapse)) {
    for (uint32_t m = 0; m < intro.materials.size(); ++m) {
      GLTFMaterialIntro& mat      = intro.materials[m];
      const uint32_t& currentMask = intro.materials[m].currentMaterialMask;

      auto setMaterialMask = [&m = intro.materials[m]](uint32_t flag, bool active) {
        m.modified = true;
        if (active) {
          m.currentMaterialMask |= flag;
        } else {
          m.currentMaterialMask &= ~flag;
        }
      };

      const bool isUnlit = (currentMask & MaterialType_Unlit) == MaterialType_Unlit;

      bool state = false;

      ImGui::Text("%s", mat.name.c_str());
      ImGui::PushID(m);
      state = isUnlit;

      if (ImGui::RadioButton("Unlit", state)) {
        mat.currentMaterialMask = 0;
        setMaterialMask(MaterialType_Unlit, true);
      }

      state = (currentMask & MaterialType_MetallicRoughness) == MaterialType_MetallicRoughness;
      if ((mat.materialMask & MaterialType_MetallicRoughness) == MaterialType_MetallicRoughness) {
        if (ImGui::RadioButton("MetallicRoughness", state)) {
          setMaterialMask(MaterialType_Unlit, false);
          setMaterialMask(MaterialType_SpecularGlossiness, false);
          setMaterialMask(MaterialType_MetallicRoughness, true);
        }
      }

      state = (currentMask & MaterialType_SpecularGlossiness) == MaterialType_SpecularGlossiness;
      if ((mat.materialMask & MaterialType_SpecularGlossiness) == MaterialType_SpecularGlossiness) {
        if (ImGui::RadioButton("SpecularGlossiness", state)) {
          setMaterialMask(MaterialType_Unlit, false);
          setMaterialMask(MaterialType_SpecularGlossiness, true);
          setMaterialMask(MaterialType_MetallicRoughness, false);
        }
      }

      state = (currentMask & MaterialType_Sheen) == MaterialType_Sheen;
      if ((mat.materialMask & MaterialType_Sheen) == MaterialType_Sheen) {
        ImGui::BeginDisabled(isUnlit);
        if (ImGui::Checkbox("Sheen", &state)) {
          setMaterialMask(MaterialType_Sheen, state);
        }
        ImGui::EndDisabled();
      }

      state = (mat.currentMaterialMask & MaterialType_ClearCoat) == MaterialType_ClearCoat;
      if ((mat.materialMask & MaterialType_ClearCoat) == MaterialType_ClearCoat) {
        ImGui::BeginDisabled(isUnlit);
        if (ImGui::Checkbox("ClearCoat", &state)) {
          setMaterialMask(MaterialType_ClearCoat, state);
        }
        ImGui::EndDisabled();
      }

      state = (mat.currentMaterialMask & MaterialType_Specular) == MaterialType_Specular;
      if ((mat.materialMask & MaterialType_Specular) == MaterialType_Specular) {
        ImGui::BeginDisabled(isUnlit);
        if (ImGui::Checkbox("Specular", &state)) {
          setMaterialMask(MaterialType_Specular, state);
        }
        ImGui::EndDisabled();
      }

      state = (mat.currentMaterialMask & MaterialType_Transmission) == MaterialType_Transmission;
      if ((mat.materialMask & MaterialType_Transmission) == MaterialType_Transmission) {
        ImGui::BeginDisabled(isUnlit);
        if (ImGui::Checkbox("Transmission", &state)) {
          if (!state) {
            setMaterialMask(MaterialType_Volume, false);
          }
          setMaterialMask(MaterialType_Transmission, state);
        }
        ImGui::EndDisabled();
      }

      state = (mat.currentMaterialMask & MaterialType_Volume) == MaterialType_Volume;
      if ((mat.materialMask & MaterialType_Volume) == MaterialType_Volume) {
        ImGui::BeginDisabled(isUnlit);
        if (ImGui::Checkbox("Volume", &state)) {
          setMaterialMask(MaterialType_Volume, state);
          if (state) {
            setMaterialMask(MaterialType_Transmission, true);
          }
        }
        ImGui::EndDisabled();
      }

      ImGui::PopID();
    }
  }

  ImGui::End();
}

void VulkanApp::drawGTFInspector_Cameras(GLTFIntrospective& intro)
{
  if (!intro.showCameras)
    return;

  ImGui::Begin("Cameras:", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoCollapse);
  std::string current_item = intro.activeCamera != ~0u ? intro.cameras[intro.activeCamera] : "";
  if (ImGui::BeginCombo("##combo", current_item.c_str())) {
    for (uint32_t n = 0; n < intro.cameras.size(); n++) {
      bool is_selected = (current_item == intro.cameras[n]);
      if (ImGui::Selectable(intro.cameras[n].c_str(), is_selected)) {
        intro.activeCamera = n;
        current_item       = intro.cameras[n];
      }
      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::End();
}

void VulkanApp::drawGTFInspector(GLTFIntrospective& intro)
{
  if (!cfg_.showGLTFInspector) {
    return;
  }

  ImGui::SetNextWindowPos(ImVec2(10, 300));

  drawGTFInspector_Animations(intro);
  drawGTFInspector_Materials(intro);
  drawGTFInspector_Cameras(intro);
}

void VulkanApp::drawFPS()
{
  if (const ImGuiViewport* v = ImGui::GetMainViewport()) {
    ImGui::SetNextWindowPos({ v->WorkPos.x + v->WorkSize.x - 15.0f, v->WorkPos.y + 15.0f }, ImGuiCond_Always, { 1.0f, 0.0f });
  }
  ImGui::SetNextWindowBgAlpha(0.30f);
  ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize("FPS : _______").x, 0));
  if (ImGui::Begin(
          "##FPS", nullptr,
          ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
              ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
    ImGui::Text("FPS : %i", (int)fpsCounter_.getFPS());
    ImGui::Text("Ms  : %.1f", fpsCounter_.getFPS() > 0 ? 1000.0 / fpsCounter_.getFPS() : 0);
  }
  ImGui::End();
}

void VulkanApp::drawGrid(lvk::ICommandBuffer& buf, const mat4& proj, const vec3& origin, uint32_t numSamples, lvk::Format colorFormat)
{
  drawGrid(buf, proj * camera_.getViewMatrix(), origin, camera_.getPosition(), numSamples, colorFormat);
}

void VulkanApp::drawGrid(
    lvk::ICommandBuffer& buf, const mat4& mvp, const vec3& origin, const vec3& camPos, uint32_t numSamples, lvk::Format colorFormat)
{
  if (gridPipeline.empty() || pipelineSamples != numSamples) {
    gridVert = loadShaderModule(ctx_, "data/shaders/Grid.vert");
    gridFrag = loadShaderModule(ctx_, "data/shaders/Grid.frag");

    pipelineSamples = numSamples;

    gridPipeline = ctx_->createRenderPipeline({
        .smVert       = gridVert,
        .smFrag       = gridFrag,
        .color        = { {
                   .format            = colorFormat != lvk::Format_Invalid ? colorFormat : ctx_->getSwapchainFormat(),
                   .blendEnabled      = true,
                   .srcRGBBlendFactor = lvk::BlendFactor_SrcAlpha,
                   .dstRGBBlendFactor = lvk::BlendFactor_OneMinusSrcAlpha,
        } },
        .depthFormat  = this->getDepthFormat(),
        .samplesCount = numSamples,
        .debugName    = "Pipeline: drawGrid()",
    });
  }

  const struct {
    mat4 mvp;
    vec4 camPos;
    vec4 origin;
  } pc = {
    .mvp    = mvp,
    .camPos = vec4(camPos, 1.0f),
    .origin = vec4(origin, 1.0f),
  };

  buf.cmdPushDebugGroupLabel("Grid", 0xff0000ff);
  buf.cmdBindRenderPipeline(gridPipeline);
  buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = false });
  buf.cmdPushConstants(pc);
  buf.cmdDraw(6);
  buf.cmdPopDebugGroupLabel();
}
