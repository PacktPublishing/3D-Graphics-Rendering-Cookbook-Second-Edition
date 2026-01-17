#include <lvk/LVK.h>
#include <lvk/HelpersImGui.h>

#include <GLFW/glfw3.h>

#include <stb/stb_image.h>

#include <stdio.h>
#include <stdlib.h>

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

  std::unique_ptr<lvk::ImGuiRenderer> imgui = std::make_unique<lvk::ImGuiRenderer>(*ctx, window, "data/OpenSans-Light.ttf", 30.0f);

  int w, h, comp;
  const uint8_t* img = stbi_load("data/wood.jpg", &w, &h, &comp, 4);

  assert(img);

  lvk::Holder<lvk::TextureHandle> texture = ctx->createTexture({
      .type       = lvk::TextureType_2D,
      .format     = lvk::Format_RGBA_UN8,
      .dimensions = {(uint32_t)w, (uint32_t)h},
      .usage      = lvk::TextureUsageBits_Sampled,
      .data       = img,
      .debugName  = "03_STB.jpg",
  });

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if (!width || !height)
      continue;
    const float ratio = width / (float)height;

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      const lvk::Framebuffer framebuffer = {
        .color = { { .texture = ctx->getCurrentSwapchainTexture() } },
      };
      buf.cmdBeginRendering({ .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } } }, framebuffer);
      imgui->beginFrame(framebuffer);
      ImGui::Begin("Texture Viewer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::Image(texture.index(), ImVec2(512, 512));
      ImGui::ShowDemoWindow();
      ImGui::End();
      imgui->endFrame(buf);
      buf.cmdEndRendering();
    }
    ctx->submit(buf, ctx->getCurrentSwapchainTexture());
  }

  imgui   = nullptr;
  texture = nullptr;

  ctx.reset();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
