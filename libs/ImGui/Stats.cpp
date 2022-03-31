#include "Stats.h"

#include <imgui.h>

void cala::imgui::Stats::render() {
    ImGui::Begin("Stats");

    ImGui::Text("FrameTime: %f", _frameTime);
    ImGui::Text("AverageFrameTime: %f", _averageFrameTime);
    ImGui::Text("FPS: %f", 1000.f / _frameTime);
    ImGui::Text("Average FPS: %f", 1000.f / _averageFrameTime);
    ImGui::Text("Command Buffers: %u", _commandBufferCount);
    ImGui::Text("Pipelines: %u", _pipelineCount);
    ImGui::Text("Descriptors: %u", _descriptorSetCount);
    ImGui::Text("Set Layouts: %u", _setLayoutCount);
    ImGui::Text("Frame: %u", _frame);

    ImGui::End();
}

void cala::imgui::Stats::update(f32 frameTime, f32 averageFrameTime, u32 commandCount, u32 pipelineCount,
                                u32 descriptorCount, u32 setLayoutCount, u32 frame) {
    _frameTime = frameTime;
    _averageFrameTime = averageFrameTime;
    _commandBufferCount = commandCount;
    _pipelineCount = pipelineCount;
    _descriptorSetCount = descriptorCount;
    _setLayoutCount = setLayoutCount;
    _frame = frame;
}