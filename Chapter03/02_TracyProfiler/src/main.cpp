#include <lvk/LVK.h>

#include <GLFW/glfw3.h>

#include <shared/Utils.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <stdio.h>
#include <stdlib.h>

using glm::mat4;
using glm::vec3;

int main()
{
  minilog::initialize(nullptr, { .threadNames = false });

  GLFWwindow* window = nullptr;
  std::unique_ptr<lvk::IContext> ctx;
  {
    LVK_PROFILER_ZONE("Initialization", LVK_PROFILER_COLOR_CREATE);
    int width  = -95;
    int height = -90;

    window = lvk::initWindow("Simple example", width, height);
    ctx    = lvk::createVulkanContextWithSwapchain(window, width, height, {});
    LVK_PROFILER_ZONE_END();
  }

  lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter03/02_TracyProfiler/src/main.vert");
  lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter03/02_TracyProfiler/src/main.frag");

  lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid = ctx->createRenderPipeline({
      .smVert = vert,
      .smFrag = frag,
      .color    = { { .format = ctx->getSwapchainFormat() } },
      .cullMode = lvk::CullMode_Back,
  });

  lvk::Holder<lvk::RenderPipelineHandle> pipelineWireframe = ctx->createRenderPipeline({
      .smVert = vert,
      .smFrag = frag,
      .color       = { { .format = ctx->getSwapchainFormat() } },
      .cullMode    = lvk::CullMode_Back,
      .polygonMode = lvk::PolygonMode_Line,
  });

  LVK_ASSERT(pipelineSolid.valid());
  LVK_ASSERT(pipelineWireframe.valid());

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if (!width || !height)
      continue;
    const float ratio = width / (float)height;

    const mat4 m = glm::rotate(glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -3.5f)), (float)glfwGetTime(), vec3(1.0f, 1.0f, 1.0f));
    const mat4 p = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);

    struct PerFrameData {
      mat4 mvp;
      int isWireframe;
    } perFrameData = {
      .mvp         = p * m,
      .isWireframe = false,
    };

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      LVK_PROFILER_ZONE("Fill command buffer", 0xffffff);
      buf.cmdBeginRendering(
          { .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } } },
          { .color = { { .texture = ctx->getCurrentSwapchainTexture() } } });
      buf.cmdPushDebugGroupLabel("Solid cube", 0xff0000ff);
      {
        buf.cmdBindRenderPipeline(pipelineSolid);
        buf.cmdPushConstants(perFrameData);
        buf.cmdDraw(36);
      }
      buf.cmdPopDebugGroupLabel();
      buf.cmdPushDebugGroupLabel("Wireframe cube", 0xff0000ff);
      {
        buf.cmdBindRenderPipeline(pipelineWireframe);
        perFrameData.isWireframe = true;
        buf.cmdPushConstants(perFrameData);
        buf.cmdDraw(36);
      }
      buf.cmdPopDebugGroupLabel();
      buf.cmdEndRendering();
      LVK_PROFILER_ZONE_END();
    }
    ctx->submit(buf, ctx->getCurrentSwapchainTexture());
  }

  vert.reset();
  frag.reset();
  pipelineSolid.reset();
  pipelineWireframe.reset();
  ctx.reset();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
