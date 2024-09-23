#include "shared/VulkanApp.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

const double kAnimationFPS        = 50.0;
const uint32_t kNumFlipbooks      = 3;
const uint32_t kNumFlipbookFrames = 100;

struct AnimationState {
  vec2 position           = vec2(0);
  double startTime        = 0;
  float time              = 0;
  uint32_t textureIndex   = 0;
  uint32_t firstFrame = 0;
};

std::vector<AnimationState> g_Animations;
std::vector<AnimationState> g_AnimationsKeyframe;

float timelineOffset = 0.0f;
bool showTimeline = false;

void updateAnimations(float deltaSeconds)
{
  for (size_t i = 0; i < g_Animations.size();) {
    g_Animations[i].time += deltaSeconds;
    g_Animations[i].textureIndex = g_Animations[i].firstFrame + (uint32_t)(kAnimationFPS * g_Animations[i].time);

    if (g_Animations[i].textureIndex >= kNumFlipbookFrames + g_Animations[i].firstFrame)
      g_Animations.erase(g_Animations.begin() + i);
    else
      i++;
  }
}

void setAnimationsOffset(float offset)
{
  for (size_t i = 0; i < g_Animations.size(); i++) {
    g_Animations[i].time         = std::max(g_AnimationsKeyframe[i].time + offset, 0.0f);
    g_Animations[i].textureIndex = g_Animations[i].firstFrame + (uint32_t)(kAnimationFPS * g_Animations[i].time);
    g_Animations[i].textureIndex = std::min(g_Animations[i].textureIndex, kNumFlipbookFrames + g_Animations[i].firstFrame - 1);
  }
}

int main()
{
  VulkanApp app;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  {
    std::vector<lvk::Holder<lvk::TextureHandle>> textures;
    textures.reserve(kNumFlipbooks * kNumFlipbookFrames);

    for (uint32_t book = 0; book != kNumFlipbooks; book++) {
      for (uint32_t frame = 0; frame != kNumFlipbookFrames; frame++) {
        char fname[1024];
        snprintf(fname, sizeof(fname), "deps/src/explosion%01u/explosion%02u-frame%03u.tga", book, book, frame + 1);
        textures.emplace_back(loadTexture(ctx, fname));
      }
    }

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter08/01_DescriptorIndexing/src/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter08/01_DescriptorIndexing/src/main.frag");

    lvk::Holder<lvk::RenderPipelineHandle> pipelineQuad = ctx->createRenderPipeline({
        .topology = lvk::Topology_TriangleStrip,
        .smVert   = vert,
        .smFrag   = frag,
        .color    = { {
               .format            = ctx->getSwapchainFormat(),
               .blendEnabled      = true,
               .srcRGBBlendFactor = lvk::BlendFactor_SrcAlpha,
               .dstRGBBlendFactor = lvk::BlendFactor_OneMinusSrcAlpha,
        } },
    });

    LVK_ASSERT(pipelineQuad.valid());

    app.addMouseButtonCallback([](GLFWwindow* window, int button, int action, int mods) {
      VulkanApp* app = (VulkanApp*)glfwGetWindowUserPointer(window);
      ImGuiIO& io    = ImGui::GetIO();
      if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !io.WantCaptureMouse && !showTimeline) {
        g_Animations.push_back(AnimationState{
            .position     = app->mouseState_.pos,
            .startTime    = glfwGetTime(),
            .textureIndex = 0,
            .firstFrame   = kNumFlipbookFrames * (uint32_t)(rand() % 3),
        });
      }
    });
    app.addKeyCallback([](GLFWwindow* window, int key, int scancode, int action, int mods) {
      ImGuiIO& io        = ImGui::GetIO();
      const bool pressed = action != GLFW_RELEASE;
      if (key == GLFW_KEY_SPACE && pressed && !io.WantCaptureKeyboard)
        showTimeline = !showTimeline;
      // save the current state as a keyframe
      if (showTimeline) {
        timelineOffset       = 0.0f;
        g_AnimationsKeyframe = g_Animations;
      }
      if (g_Animations.empty())
        showTimeline = false;
    });

    // put the first explosion in the screen center
    g_Animations.push_back(AnimationState{
        .position     = vec2(0.5f, 0.5f),
        .startTime    = glfwGetTime(),
        .textureIndex = 0,
        .firstFrame   = kNumFlipbookFrames * (uint32_t)(rand() % 3),
    });

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
      if (!showTimeline) {
        updateAnimations(deltaSeconds);
      }

      const lvk::RenderPass renderPass = {
        .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
      };

      const lvk::Framebuffer framebuffer = {
        .color = { { .texture = ctx->getCurrentSwapchainTexture() } },
      };

      lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
      {
        auto easing = [](float t) -> float {
          const float p1 = 0.1f;
          const float p2 = 0.8f;
          if (t <= p1)
            return glm::smoothstep(0.0f, 1.0f, t / p1);
          if (t >= p2)
            return glm::smoothstep(1.0f, 0.0f, (t - p2) / (1.0f - p2));
          return 1.0f;
        };
        buf.cmdBeginRendering(renderPass, framebuffer);
        for (const AnimationState& s : g_Animations) {
          const float t = s.time / (kNumFlipbookFrames / kAnimationFPS);
          const struct {
            mat4 proj;
            uint32_t textureId;
            vec2 pos;
            vec2 size;
            float alphaScale;
          } pc{
            .proj       = glm::ortho(0.0f, float(width), 0.0f, float(height)),
            .textureId  = textures[s.textureIndex].index(),
            .pos        = s.position * vec2(width, height),
            .size       = vec2(height * 0.5f),
            .alphaScale = easing(t),
          };
          buf.cmdBindRenderPipeline(pipelineQuad);
          buf.cmdPushConstants(pc);
          buf.cmdDraw(4);
        }
        app.imgui_->beginFrame(framebuffer);
        app.drawFPS();
        // hints
        {
          ImGui::SetNextWindowPos(ImVec2(10, 10));
          ImGui::Begin(
              "Hints:", nullptr,
              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs |
                  ImGuiWindowFlags_NoCollapse);
          if (showTimeline) {
            ImGui::Text("SPACE - toggle timeline");
          } else {
            ImGui::Text("SPACE - toggle timeline");
            ImGui::Text("Left click - set an explosion");
          }
          ImGui::End();
        }
        if (showTimeline) {
          const ImGuiViewport* v = ImGui::GetMainViewport();
          LVK_ASSERT(v);
          ImGui::SetNextWindowContentSize({ v->Size.x - 520, 0 });
          ImGui::SetNextWindowPos(ImVec2(350, 10), ImGuiCond_Always);
          ImGui::Begin("Timeline:", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
			 if (ImGui::SliderFloat("Time offset", &timelineOffset, -2.0f, +2.0f)) {
            setAnimationsOffset(timelineOffset);
			 }
          ImGui::End();
        }
        app.imgui_->endFrame(buf);
        buf.cmdEndRendering();
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());
    });
  }

  ctx.release();

  return 0;
}
