#include "VulkanApp.h"

extern std::unordered_map<void*, std::string> debugGLSLSourceCode;

static void shaderModuleCallback(lvk::IContext*, lvk::ShaderModuleHandle handle, int line, int col, const char* debugName)
{
  const auto it = debugGLSLSourceCode.find(handle.indexAsVoid());

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

  window_       = lvk::initWindow("Simple example", width, height);
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
    if (mods & GLFW_MOD_SHIFT)
      app->positioner_.movement_.fastSpeed_ = pressed;
    if (key == GLFW_KEY_SPACE) {
      app->positioner_.lookAt(app->cfg_.initialCameraPos, app->cfg_.initialCameraTarget, vec3(0.0f, 1.0f, 0.0f));
      app->positioner_.setSpeed(vec3(0));
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

void VulkanApp::drawGrid(lvk::ICommandBuffer& buf, const mat4& proj, const vec3& origin)
{
  if (gridPipeline.empty()) {
    gridVert = loadShaderModule(ctx_, "data/shaders/Grid.vert");
    gridFrag = loadShaderModule(ctx_, "data/shaders/Grid.frag");

    gridPipeline = ctx_->createRenderPipeline({
        .smVert      = gridVert,
        .smFrag      = gridFrag,
        .color       = { {
                  .format            = ctx_->getSwapchainFormat(),
                  .blendEnabled      = true,
                  .srcRGBBlendFactor = lvk::BlendFactor_SrcAlpha,
                  .dstRGBBlendFactor = lvk::BlendFactor_OneMinusSrcAlpha,
        } },
        .depthFormat = this->getDepthFormat(),
    });
  }

  const struct {
    mat4 mvp;
    vec4 camPos;
    vec4 origin;
  } pc = {
    .mvp    = proj * camera_.getViewMatrix(),
    .camPos = vec4(camera_.getPosition(), 1.0f),
    .origin = vec4(origin, 1.0f),
  };

  buf.cmdPushDebugGroupLabel("Grid", 0xff0000ff);
  buf.cmdBindRenderPipeline(gridPipeline);
  buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = false });
  buf.cmdPushConstants(pc);
  buf.cmdDraw(6);
  buf.cmdPopDebugGroupLabel();
}
