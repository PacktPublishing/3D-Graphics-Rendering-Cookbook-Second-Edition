#pragma once

#include <lvk/LVK.h>

#include <deque>
#include <vector>

class LinearGraph
{
public:
  explicit LinearGraph(const char* name, size_t maxGraphPoints = 256)
  : name_(name)
  , maxPoints_(maxGraphPoints)
  {
  }

  void addPoint(float value)
  {
    graph_.push_back(value);
    if (graph_.size() > maxPoints_)
      graph_.erase(graph_.begin());
  }

  void renderGraph(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const glm::vec4& color = vec4(1.0)) const
  {
    LVK_PROFILER_FUNCTION();

    float minVal = std::numeric_limits<float>::max();
    float maxVal = std::numeric_limits<float>::min();

    for (float f : graph_) {
      if (f < minVal)
        minVal = f;
      if (f > maxVal)
        maxVal = f;
    }

    const float range = maxVal - minVal;

    float valX = 0.0;

    std::vector<float> dataX_;
    std::vector<float> dataY_;
    dataX_.reserve(graph_.size());
    dataY_.reserve(graph_.size());

    for (float f : graph_) {
      const float valY = (f - minVal) / range;
      valX += 1.0f / maxPoints_;
      dataX_.push_back(valX);
      dataY_.push_back(valY);
    }

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin(
        name_, nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);

    if (ImPlot::BeginPlot(name_, ImVec2(width, height), ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs)) {
      ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
      ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(color.r, color.g, color.b, color.a));
      ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(0, 0, 0, 0));
      ImPlot::PlotLine("#line", dataX_.data(), dataY_.data(), (int)graph_.size(), ImPlotLineFlags_None);
      ImPlot::PopStyleColor(2);
      ImPlot::EndPlot();
    }

    ImGui::End();
  }

private:
  const char* name_ = nullptr;
  const size_t maxPoints_;
  std::deque<float> graph_;
};
