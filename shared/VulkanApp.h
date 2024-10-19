#pragma once

#include <lvk/HelpersImGui.h>
#include <lvk/LVK.h>

#include <GLFW/glfw3.h>
#include <implot/implot.h>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include "shared/UtilsFPS.h"
#include <shared/Bitmap.h>
#include <shared/Camera.h>
#include <shared/Graph.h>
#include <shared/Utils.h>
#include <shared/UtilsCubemap.h>

#include <functional>

using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

using DrawFrameFunc = std::function<void(uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds)>;

struct VulkanAppConfig {
  vec3 initialCameraPos    = vec3(0.0f, 0.0f, -2.5f);
  vec3 initialCameraTarget = vec3(0.0f, 0.0f, 0.0f);
  bool showCamerasUI       = false;
  bool showAnimationsUI    = false;
};

class VulkanApp
{
public:
  explicit VulkanApp(const VulkanAppConfig& cfg = {});
  virtual ~VulkanApp();

  virtual void run(DrawFrameFunc drawFrame);
  virtual void drawGrid(lvk::ICommandBuffer& buf, const mat4& proj, const vec3& origin = vec3(0.0f), uint32_t numSamples = 1);
  virtual void drawGrid(lvk::ICommandBuffer& buf, const mat4& mvp, const vec3& origin, const vec3& camPos, uint32_t numSamples = 1);
  virtual void drawFPS();
  virtual void drawMemo();
  virtual void drawCameras(const std::vector<std::string>& cameras, uint32_t& activeCamera);
  virtual void drawAnimations(const std::vector<std::string>& animations, std::vector<uint32_t>* anim = nullptr, float* blend = nullptr);

  lvk::Format getDepthFormat() const;
  lvk::TextureHandle getDepthTexture() const { return depthTexture_; }

  void addMouseButtonCallback(GLFWmousebuttonfun cb) { callbacksMouseButton.push_back(cb); }
  void addKeyCallback(GLFWkeyfun cb) { callbacksKey.push_back(cb); }

public:
  GLFWwindow* window_ = nullptr;
  std::unique_ptr<lvk::IContext> ctx_;
  lvk::Holder<lvk::TextureHandle> depthTexture_;
  FramesPerSecondCounter fpsCounter_ = FramesPerSecondCounter(0.5f);
  std::unique_ptr<lvk::ImGuiRenderer> imgui_;
  ImPlotContext* implotCtx_ = nullptr;

  const VulkanAppConfig cfg_ = {};

  CameraPositioner_FirstPerson positioner_ = { cfg_.initialCameraPos, cfg_.initialCameraTarget, vec3(0.0f, 1.0f, 0.0f) };
  Camera camera_                           = Camera(positioner_);

  struct MouseState {
    vec2 pos         = vec2(0.0f);
    bool pressedLeft = false;
  } mouseState_;

protected:
  lvk::Holder<lvk::ShaderModuleHandle> gridVert       = {};
  lvk::Holder<lvk::ShaderModuleHandle> gridFrag       = {};
  lvk::Holder<lvk::RenderPipelineHandle> gridPipeline = {};

  uint32_t pipelineSamples = 1;

  std::vector<GLFWmousebuttonfun> callbacksMouseButton;
  std::vector<GLFWkeyfun> callbacksKey;
};
