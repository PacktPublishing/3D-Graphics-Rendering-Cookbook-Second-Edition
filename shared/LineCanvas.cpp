#include "LineCanvas.h"

static const char* codeVS = R"(
layout (location = 0) out vec4 out_color;

struct Vertex {
  vec4 pos;
  vec4 rgba;
};

layout(std430, buffer_reference) readonly buffer VertexBuffer {
  Vertex vertices[];
};

layout(push_constant) uniform PushConstants {
  mat4 mvp;
  VertexBuffer vb;
};

void main() {
  // Vertex v = vb.vertices[gl_VertexIndex]; <--- does not work on Snapdragon Adreno
  out_color = vb.vertices[gl_VertexIndex].rgba;
  gl_Position = mvp * vb.vertices[gl_VertexIndex].pos;
})";

static const char* codeFS = R"(
layout (location = 0) in vec4 in_color;
layout (location = 0) out vec4 out_color;

void main() {
  out_color = in_color;
})";

void LineCanvas2D::render(const char* nameImGuiWindow)
{
  LVK_PROFILER_FUNCTION();

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
  ImGui::Begin(
      nameImGuiWindow, nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);

  ImDrawList* drawList = ImGui::GetBackgroundDrawList();

  for (const LineData& l : lines_) {
    drawList->AddLine(ImVec2(l.p1.x, l.p1.y), ImVec2(l.p2.x, l.p2.y), ImColor(l.color.r, l.color.g, l.color.b, l.color.a));
  }

  ImGui::End();
}

void LineCanvas3D::line(const vec3& p1, const vec3& p2, const vec4& c)
{
  lines_.push_back({ .pos = vec4(p1, 1.0f), .color = c });
  lines_.push_back({ .pos = vec4(p2, 1.0f), .color = c });
}

void LineCanvas3D::plane(
    const vec3& o, const vec3& v1, const vec3& v2, int n1, int n2, float s1, float s2, const vec4& color, const vec4& outlineColor)
{
  line(o - s1 / 2.0f * v1 - s2 / 2.0f * v2, o - s1 / 2.0f * v1 + s2 / 2.0f * v2, outlineColor);
  line(o + s1 / 2.0f * v1 - s2 / 2.0f * v2, o + s1 / 2.0f * v1 + s2 / 2.0f * v2, outlineColor);

  line(o - s1 / 2.0f * v1 + s2 / 2.0f * v2, o + s1 / 2.0f * v1 + s2 / 2.0f * v2, outlineColor);
  line(o - s1 / 2.0f * v1 - s2 / 2.0f * v2, o + s1 / 2.0f * v1 - s2 / 2.0f * v2, outlineColor);

  for (int i = 1; i < n1; i++) {
    float t       = ((float)i - (float)n1 / 2.0f) * s1 / (float)n1;
    const vec3 o1 = o + t * v1;
    line(o1 - s2 / 2.0f * v2, o1 + s2 / 2.0f * v2, color);
  }

  for (int i = 1; i < n2; i++) {
    const float t = ((float)i - (float)n2 / 2.0f) * s2 / (float)n2;
    const vec3 o2 = o + t * v2;
    line(o2 - s1 / 2.0f * v1, o2 + s1 / 2.0f * v1, color);
  }
}

void LineCanvas3D::box(const mat4& m, const vec3& size, const vec4& c)
{
  vec3 pts[8] = {
    vec3(+size.x, +size.y, +size.z), vec3(+size.x, +size.y, -size.z), vec3(+size.x, -size.y, +size.z), vec3(+size.x, -size.y, -size.z),
    vec3(-size.x, +size.y, +size.z), vec3(-size.x, +size.y, -size.z), vec3(-size.x, -size.y, +size.z), vec3(-size.x, -size.y, -size.z),
  };

  for (auto& p : pts)
    p = vec3(m * vec4(p, 1.f));

  line(pts[0], pts[1], c);
  line(pts[2], pts[3], c);
  line(pts[4], pts[5], c);
  line(pts[6], pts[7], c);

  line(pts[0], pts[2], c);
  line(pts[1], pts[3], c);
  line(pts[4], pts[6], c);
  line(pts[5], pts[7], c);

  line(pts[0], pts[4], c);
  line(pts[1], pts[5], c);
  line(pts[2], pts[6], c);
  line(pts[3], pts[7], c);
}

void LineCanvas3D::box(const mat4& m, const BoundingBox& box, const glm::vec4& color)
{
  this->box(m * glm::translate(mat4(1.f), .5f * (box.min_ + box.max_)), 0.5f * vec3(box.max_ - box.min_), color);
}

void LineCanvas3D::frustum(const mat4& camView, const mat4& camProj, const vec4& color)
{
  const vec3 corners[] = { vec3(-1, -1, -1), vec3(+1, -1, -1), vec3(+1, +1, -1), vec3(-1, +1, -1),
                           vec3(-1, -1, +1), vec3(+1, -1, +1), vec3(+1, +1, +1), vec3(-1, +1, +1) };

  vec3 pp[8];

  for (int i = 0; i < 8; i++) {
    vec4 q = glm::inverse(camView) * glm::inverse(camProj) * vec4(corners[i], 1.0f);
    pp[i]  = vec3(q.x / q.w, q.y / q.w, q.z / q.w);
  }
  line(pp[0], pp[4], color);
  line(pp[1], pp[5], color);
  line(pp[2], pp[6], color);
  line(pp[3], pp[7], color);
  // near
  line(pp[0], pp[1], color);
  line(pp[1], pp[2], color);
  line(pp[2], pp[3], color);
  line(pp[3], pp[0], color);
  // x
  line(pp[0], pp[2], color);
  line(pp[1], pp[3], color);
  // far
  line(pp[4], pp[5], color);
  line(pp[5], pp[6], color);
  line(pp[6], pp[7], color);
  line(pp[7], pp[4], color);
  // x
  line(pp[4], pp[6], color);
  line(pp[5], pp[7], color);

  const vec4 gridColor = color * 0.7f;
  const int gridLines  = 100;

  // bottom
  {
    vec3 p1       = pp[0];
    vec3 p2       = pp[1];
    const vec3 s1 = (pp[4] - pp[0]) / float(gridLines);
    const vec3 s2 = (pp[5] - pp[1]) / float(gridLines);
    for (int i = 0; i != gridLines; i++, p1 += s1, p2 += s2)
      line(p1, p2, gridColor);
  }
  // top
  {
    vec3 p1       = pp[2];
    vec3 p2       = pp[3];
    const vec3 s1 = (pp[6] - pp[2]) / float(gridLines);
    const vec3 s2 = (pp[7] - pp[3]) / float(gridLines);
    for (int i = 0; i != gridLines; i++, p1 += s1, p2 += s2)
      line(p1, p2, gridColor);
  }
  // left
  {
    vec3 p1       = pp[0];
    vec3 p2       = pp[3];
    const vec3 s1 = (pp[4] - pp[0]) / float(gridLines);
    const vec3 s2 = (pp[7] - pp[3]) / float(gridLines);
    for (int i = 0; i != gridLines; i++, p1 += s1, p2 += s2)
      line(p1, p2, gridColor);
  }
  // right
  {
    vec3 p1       = pp[1];
    vec3 p2       = pp[2];
    const vec3 s1 = (pp[5] - pp[1]) / float(gridLines);
    const vec3 s2 = (pp[6] - pp[2]) / float(gridLines);
    for (int i = 0; i != gridLines; i++, p1 += s1, p2 += s2)
      line(p1, p2, gridColor);
  }
}

void LineCanvas3D::render(lvk::IContext& ctx, const lvk::Framebuffer& desc, lvk::ICommandBuffer& buf, uint32_t numSamples)
{
  LVK_PROFILER_FUNCTION();

  if (lines_.empty()) {
    return;
  }

  const uint32_t requiredSize = lines_.size() * sizeof(LineData);

  if (currentBufferSize_[currentFrame_] < requiredSize) {
    linesBuffer_[currentFrame_] = ctx.createBuffer(
        { .usage = lvk::BufferUsageBits_Storage, .storage = lvk::StorageType_HostVisible, .size = requiredSize, .data = lines_.data() });
    currentBufferSize_[currentFrame_] = requiredSize;
  } else {
    ctx.upload(linesBuffer_[currentFrame_], lines_.data(), requiredSize);
  }

  if (pipeline_.empty() || pipelineSamples != numSamples) {
    pipelineSamples = numSamples;

    vert_     = ctx.createShaderModule({ codeVS, lvk::Stage_Vert, "Shader Module: imgui (vert)" });
    frag_     = ctx.createShaderModule({ codeFS, lvk::Stage_Frag, "Shader Module: imgui (frag)" });
    pipeline_ = ctx.createRenderPipeline(
        {
            .topology     = lvk::Topology_Line,
            .smVert       = vert_,
            .smFrag       = frag_,
            .color        = { {
                       .format            = ctx.getFormat(desc.color[0].texture),
                       .blendEnabled      = true,
                       .srcRGBBlendFactor = lvk::BlendFactor_SrcAlpha,
                       .dstRGBBlendFactor = lvk::BlendFactor_OneMinusSrcAlpha,
            } },
            .depthFormat  = desc.depthStencil.texture ? ctx.getFormat(desc.depthStencil.texture) : lvk::Format_Invalid,
            .cullMode     = lvk::CullMode_None,
            .samplesCount = numSamples,
        },
        nullptr);
  }

  struct {
    mat4 mvp;
    uint64_t addr;
  } pc{
    .mvp  = mvp_,
    .addr = ctx.gpuAddress(linesBuffer_[currentFrame_]),
  };
  buf.cmdBindRenderPipeline(pipeline_);
  buf.cmdPushConstants(pc);
  buf.cmdDraw(lines_.size());

  currentFrame_ = (currentFrame_ + 1) % LVK_ARRAY_NUM_ELEMENTS(linesBuffer_);
}
