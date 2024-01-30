#include <lvk/LVK.h>
#include <lvk/HelpersImGui.h>

#include <GLFW/glfw3.h>
#include <implot/implot.h>

#include <stdio.h>
#include <stdlib.h>
#include <shared/UtilsFPS.h>

int main()
{
  minilog::initialize(nullptr, { .threadNames = false });

  GLFWwindow* window = nullptr;
  std::unique_ptr<lvk::IContext> ctx;
  {
    int width  = -95;
    int height = -90;

    window = lvk::initWindow("Simple example", width, height);
    ctx    = lvk::createVulkanContextWithSwapchain(window, width, height, {});
  }

  std::unique_ptr<lvk::ImGuiRenderer> imgui = std::make_unique<lvk::ImGuiRenderer>(*ctx, "data/OpenSans-Light.ttf", 30.0f);

  glfwSetCursorPosCallback(window, [](auto* window, double x, double y) { ImGui::GetIO().MousePos = ImVec2(x, y); });
  glfwSetMouseButtonCallback(window, [](auto* window, int button, int action, int mods) {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    const ImGuiMouseButton_ imguiButton = (button == GLFW_MOUSE_BUTTON_LEFT)
                                              ? ImGuiMouseButton_Left
                                              : (button == GLFW_MOUSE_BUTTON_RIGHT ? ImGuiMouseButton_Right : ImGuiMouseButton_Middle);
    ImGuiIO& io                         = ImGui::GetIO();
    io.MousePos                         = ImVec2((float)xpos, (float)ypos);
    io.MouseDown[imguiButton]           = action == GLFW_PRESS;
  });

  double timeStamp   = glfwGetTime();
  float deltaSeconds = 0.0f;

  FramesPerSecondCounter fpsCounter(0.5f);

  ImPlotContext* implotCtx = ImPlot::CreateContext();

  while (!glfwWindowShouldClose(window)) {
    fpsCounter.tick(deltaSeconds);
    const double newTimeStamp = glfwGetTime();
    deltaSeconds              = static_cast<float>(newTimeStamp - timeStamp);
    timeStamp                 = newTimeStamp;

    glfwPollEvents();
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if (!width || !height)
      continue;
    const float ratio = width / (float)height;

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      const lvk::Framebuffer framebuffer = { .color = { { .texture = ctx->getCurrentSwapchainTexture() } } };
      buf.cmdBeginRendering({ .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } } }, framebuffer);
      imgui->beginFrame(framebuffer);
      if (const ImGuiViewport* v = ImGui::GetMainViewport()) {
        ImGui::SetNextWindowPos({ v->WorkPos.x + v->WorkSize.x - 15.0f, v->WorkPos.y + 15.0f }, ImGuiCond_Always, { 1.0f, 0.0f });
      }
      ImGui::SetNextWindowBgAlpha(0.30f);
      ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize("FPS : _______").x, 0));
      if (ImGui::Begin("##FPS", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("FPS : %i", (int)fpsCounter.getFPS());
        ImGui::Text("Ms  : %.1f", 1000.0 / fpsCounter.getFPS());
      }
      ImGui::End();
      ImPlot::ShowDemoWindow();
      ImGui::ShowDemoWindow();
      imgui->endFrame(buf);
      buf.cmdEndRendering();
    }
    ctx->submit(buf, ctx->getCurrentSwapchainTexture());
  }

  ImPlot::DestroyContext(implotCtx);

  imgui = nullptr;

  ctx.reset();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
