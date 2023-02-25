#include "Cala/ui/StatisticsWindow.h"
#include <imgui.h>

cala::ui::StatisticsWindow::StatisticsWindow(Engine *engine, Renderer *renderer)
    : _engine(engine),
    _renderer(renderer)
{}

void cala::ui::StatisticsWindow::render() {
    ImGui::Begin("Statistics");
    auto rendererStats = _renderer->stats();

    ImGui::Text("Descriptors: %d", rendererStats.descriptorCount);
    ImGui::Text("Pipelines: %d", rendererStats.pipelineCount);
    ImGui::Text("Draw Calls: %d", rendererStats.drawCallCount);

    auto pipelineStats = _engine->driver().context().getPipelineStatistics();

    ImGui::Text("Input Assembly Vertices: %lu", pipelineStats.inputAssemblyVertices);
    ImGui::Text("Input Assembly Primitives: %lu", pipelineStats.inputAssemblyPrimitives);
    ImGui::Text("Vertex Shader Invocations: %lu", pipelineStats.vertexShaderInvocations);
    ImGui::Text("Clipping Invocations: %lu", pipelineStats.clippingInvocations);
    ImGui::Text("Clipping Primitives: %lu", pipelineStats.clippingPrimitives);
    ImGui::Text("Fragment Shader Invocations: %lu", pipelineStats.fragmentShaderInvocations);

    ImGui::Text("Memory Usage: ");
    VmaBudget budgets[10]{};
    vmaGetHeapBudgets(_engine->driver().context().allocator(), budgets);
    for (auto & budget : budgets) {
        if (budget.usage == 0)
            break;
        ImGui::Text("\tUsed: %lu mb", budget.usage / 1000000);
        ImGui::Text("\tAvailable: %lu mb", budget.budget / 1000000);
    }

    auto engineStats = _engine->stats();
    ImGui::Text("Allocated Buffers: %d", engineStats.allocatedBuffers);
    ImGui::Text("Buffers In Use: %d", engineStats.buffersInUse);
    ImGui::Text("Allocated Images: %d", engineStats.allocatedImages);
    ImGui::Text("Images In Use: %d", engineStats.imagesInUse);

    ImGui::End();
}