#pragma once

#include "shared/VulkanApp.h"

class LineCanvas2D
{
public:
  void clear() { lines_.clear(); }
  void line(const vec2& p1, const vec2& p2, const vec4& c) { lines_.push_back({ .p1 = p1, .p2 = p2, .color = c }); }
  void render(const char* nameImGuiWindow);

private:
  struct LineData {
    vec2 p1, p2;
    vec4 color;
  };
  std::vector<LineData> lines_;
};

class LineCanvas3D
{
public:
  void clear() { lines_.clear(); }
  void line(const vec3& p1, const vec3& p2, const vec4& c);
  void plane(
      const vec3& orig, const vec3& v1, const vec3& v2, int n1, int n2, float s1, float s2, const vec4& color, const vec4& outlineColor);
  void box(const mat4& m, const BoundingBox& box, const vec4& color);
  void box(const mat4& m, const vec3& size, const vec4& color);
  void frustum(const mat4& camView, const mat4& camProj, const vec4& color);

  void setMatrix(const mat4& mvp) { mvp_ = mvp; }
  void render(lvk::IContext& ctx, const lvk::Framebuffer& desc, lvk::ICommandBuffer& buf, uint32_t numSamples = 1);

private:
  mat4 mvp_ = mat4(1.0f);

  struct LineData {
    vec4 pos;
    vec4 color;
  };
  std::vector<LineData> lines_;

  lvk::Holder<lvk::ShaderModuleHandle> vert_;
  lvk::Holder<lvk::ShaderModuleHandle> frag_;
  lvk::Holder<lvk::RenderPipelineHandle> pipeline_;
  lvk::Holder<lvk::BufferHandle> linesBuffer_[3] = {};

  uint32_t pipelineSamples = 1;

  uint32_t currentBufferSize_[3] = {};
  uint32_t currentFrame_         = 0;

  static_assert(LVK_ARRAY_NUM_ELEMENTS(linesBuffer_) == LVK_ARRAY_NUM_ELEMENTS(currentBufferSize_));
};
