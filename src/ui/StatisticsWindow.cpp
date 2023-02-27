#include "Cala/ui/StatisticsWindow.h"
#include <imgui.h>

cala::ui::StatisticsWindow::StatisticsWindow(Engine *engine, Renderer *renderer)
    : _engine(engine),
    _renderer(renderer)
{}

void cala::ui::StatisticsWindow::render() {
    ImGui::Begin("Statistics");
    ImGui::Text("%s", _engine->device().context().deviceName().data());

    auto rendererStats = _renderer->stats();

    ImGui::Text("Draw Calls: %d", rendererStats.drawCallCount);

    auto pipelineStats = _engine->device().context().getPipelineStatistics();

    ImGui::Text("Input Assembly Vertices: %lu", pipelineStats.inputAssemblyVertices);
    ImGui::Text("Input Assembly Primitives: %lu", pipelineStats.inputAssemblyPrimitives);
    ImGui::Text("Vertex Shader Invocations: %lu", pipelineStats.vertexShaderInvocations);
    ImGui::Text("Clipping Invocations: %lu", pipelineStats.clippingInvocations);
    ImGui::Text("Clipping Primitives: %lu", pipelineStats.clippingPrimitives);
    ImGui::Text("Fragment Shader Invocations: %lu", pipelineStats.fragmentShaderInvocations);

    ImGui::Text("Memory Usage: ");
    VmaBudget budgets[10]{};
    vmaGetHeapBudgets(_engine->device().context().allocator(), budgets);
    for (auto & budget : budgets) {
        if (budget.usage == 0)
            break;
        ImGui::Text("\tUsed: %lu mb", budget.usage / 1000000);
        ImGui::Text("\tAvailable: %lu mb", budget.budget / 1000000);
    }

    auto engineStats = _engine->device().stats();
    ImGui::Text("Allocated Buffers: %d", engineStats.allocatedBuffers);
    ImGui::Text("Buffers In Use: %d", engineStats.buffersInUse);
    ImGui::Text("Allocated Images: %d", engineStats.allocatedImages);
    ImGui::Text("Images In Use: %d", engineStats.imagesInUse);
    ImGui::Text("Allocated DescriptorSets: %d", engineStats.descriptorSetCount);
    ImGui::Text("Allocated Pipelines: %d", engineStats.pipelineCount);

    ImGui::End();
}