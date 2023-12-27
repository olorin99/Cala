#include "Cala/ui/StatisticsWindow.h"
#include <imgui.h>

cala::ui::StatisticsWindow::StatisticsWindow(ImGuiContext* context, Engine *engine, Renderer *renderer)
    : Window(context),
    _engine(engine),
    _renderer(renderer),
    _detailedMemoryStats(false),
    _numericalStats(true)
{}

std::string numberToWord(u32 number) {
    if (number > 1000000000) {
        return std::to_string(number / 1000000000) + " billion";
    } else if (number > 1000000) {
        return std::to_string(number / 1000000) + " million";
    } else if (number > 1000) {
        return std::to_string(number / 1000) + " thousand";
    }
    return std::to_string(number);
}

void cala::ui::StatisticsWindow::render() {
    if (ImGui::Begin("Statistics")) {
        ImGui::Text("%s", _engine->device().context().deviceName().data());

        ImGui::Checkbox("Numerical Stats", &_numericalStats);

        auto rendererStats = _renderer->stats();

        if (_numericalStats) {
            ImGui::Text("Total Meshlets: %d", rendererStats.sceneMeshlets);
            ImGui::Text("Total Indices: %d", rendererStats.sceneIndices);

            ImGui::Text("Draw Calls: %d", rendererStats.drawCallCount);
            ImGui::Text("Drawn Meshes: %d", rendererStats.drawnMeshes);
            ImGui::Text("Culled Meshes: %d", rendererStats.culledMeshes);
            f32 culledMeshRatio = (static_cast<f32>(rendererStats.culledMeshes) / (static_cast<f32>(rendererStats.drawnMeshes) + static_cast<f32>(rendererStats.culledMeshes))) * 100;
            if (rendererStats.drawnMeshes + rendererStats.culledMeshes == 0)
                culledMeshRatio = 0;
            ImGui::Text("Culled mesh ratio: %.0f%%", culledMeshRatio);
            ImGui::Text("Drawn Meshlets: %d", rendererStats.drawnMeshlets);
            ImGui::Text("Culled Meshlets: %d", rendererStats.culledMeshlets);
            f32 culledMeshletRatio = (static_cast<f32>(rendererStats.culledMeshlets) / (static_cast<f32>(rendererStats.drawnMeshlets) + static_cast<f32>(rendererStats.culledMeshlets))) * 100;
            if (rendererStats.drawnMeshlets + rendererStats.culledMeshlets == 0)
                culledMeshletRatio = 0;
            ImGui::Text("Culled meshlet ratio: %.0f%%", culledMeshletRatio);
            ImGui::Text("Drawn Triangles %d", rendererStats.drawnTriangles);
        } else {
            ImGui::Text("Total Meshlets: %s", numberToWord(rendererStats.sceneMeshlets).c_str());
            ImGui::Text("Total Indices: %s", numberToWord(rendererStats.sceneIndices).c_str());

            ImGui::Text("Draw Calls: %s", numberToWord(rendererStats.drawCallCount).c_str());
            ImGui::Text("Drawn Meshes: %s", numberToWord(rendererStats.drawnMeshes).c_str());
            ImGui::Text("Culled Meshes: %s", numberToWord(rendererStats.culledMeshes).c_str());
            f32 culledMeshRatio = (static_cast<f32>(rendererStats.culledMeshes) / (static_cast<f32>(rendererStats.drawnMeshes) + static_cast<f32>(rendererStats.culledMeshes))) * 100;
            if (rendererStats.drawnMeshes + rendererStats.culledMeshes == 0)
                culledMeshRatio = 0;
            ImGui::Text("Culled mesh ratio: %.0f%%", culledMeshRatio);
            ImGui::Text("Drawn Meshlets: %s", numberToWord(rendererStats.drawnMeshlets).c_str());
            ImGui::Text("Culled Meshlets: %s", numberToWord(rendererStats.culledMeshlets).c_str());
            f32 culledMeshletRatio = (static_cast<f32>(rendererStats.culledMeshlets) / (static_cast<f32>(rendererStats.drawnMeshlets) + static_cast<f32>(rendererStats.culledMeshlets))) * 100;
            if (rendererStats.drawnMeshlets + rendererStats.culledMeshlets == 0)
                culledMeshletRatio = 0;
            ImGui::Text("Culled meshlet ratio: %.0f%%", culledMeshletRatio);
            ImGui::Text("Drawn Triangles %s", numberToWord(rendererStats.drawnTriangles).c_str());
        }

        auto pipelineStats = _engine->device().context().getPipelineStatistics();

        ImGui::Text("Input Assembly Vertices: %lu", pipelineStats.inputAssemblyVertices);
        ImGui::Text("Input Assembly Primitives: %lu", pipelineStats.inputAssemblyPrimitives);
        ImGui::Text("Vertex Shader Invocations: %lu", pipelineStats.vertexShaderInvocations);
        ImGui::Text("Clipping Invocations: %lu", pipelineStats.clippingInvocations);
        ImGui::Text("Clipping Primitives: %lu", pipelineStats.clippingPrimitives);
        ImGui::Text("Fragment Shader Invocations: %lu", pipelineStats.fragmentShaderInvocations);

        ImGui::Text("Memory Usage: ");
        ImGui::Checkbox("Detailed Memory Statistics", &_detailedMemoryStats);
        if (_detailedMemoryStats) {
            VmaTotalStatistics statistics{};
            vmaCalculateStatistics(_engine->device().context().allocator(), &statistics);
            for (auto& stats : statistics.memoryHeap) {
                if (stats.unusedRangeSizeMax == 0 && stats.allocationSizeMax == 0)
                    break;
                ImGui::Separator();
                ImGui::Text("\tAllocations: %u", stats.statistics.allocationCount);
                ImGui::Text("\tAllocations size: %lu", stats.statistics.allocationBytes / 1000000);
                ImGui::Text("\tAllocated blocks: %u", stats.statistics.blockCount);
                ImGui::Text("\tBlock size: %lu mb", stats.statistics.blockBytes / 1000000);

                ImGui::Text("\tLargest allocation: %lu mb", stats.allocationSizeMax / 1000000);
                ImGui::Text("\tSmallest allocation: %lu b", stats.allocationSizeMin);
                ImGui::Text("\tUnused range count: %u", stats.unusedRangeCount);
                ImGui::Text("\tUnused range max: %lu mb", stats.unusedRangeSizeMax / 1000000);
                ImGui::Text("\tUnused range min: %lu b", stats.unusedRangeSizeMin);
            }
        } else {
            VmaBudget budgets[10]{};
            vmaGetHeapBudgets(_engine->device().context().allocator(), budgets);
            for (auto & budget : budgets) {
                if (budget.usage == 0)
                    break;
                ImGui::Separator();
                ImGui::Text("\tUsed: %lu mb", budget.usage / 1000000);
                ImGui::Text("\tAvailable: %lu mb", budget.budget / 1000000);
                ImGui::Text("\tAllocations: %u", budget.statistics.allocationCount);
                ImGui::Text("\tAllocations size: %lu mb", budget.statistics.allocationBytes / 1000000);
                ImGui::Text("\tAllocated blocks: %u", budget.statistics.blockCount);
                ImGui::Text("\tBlock size: %lu mb", budget.statistics.blockBytes / 1000000);
            }
        }

        auto engineStats = _engine->device().stats();
        ImGui::Text("Allocated Buffers: %d", engineStats.allocatedBuffers);
        ImGui::Text("Buffers In Use: %d", engineStats.buffersInUse);

        ImGui::Text("Allocated Images: %d", engineStats.allocatedImages);
        ImGui::Text("Images In Use: %d", engineStats.imagesInUse);

        ImGui::Text("Allocated ShaderModules: %d", engineStats.allocatedShaderModules);
        ImGui::Text("ShaderModules In Use: %d", engineStats.shaderModulesInUse);

        ImGui::Text("Allocated PipelineLayouts: %d", engineStats.allocatedPipelineLayouts);
        ImGui::Text("PipelineLayouts In Use: %d", engineStats.pipelineLayoutsInUse);

        ImGui::Text("Allocated DescriptorSets: %d", engineStats.descriptorSetCount);
        ImGui::Text("Allocated Pipelines: %d", engineStats.pipelineCount);
        ImGui::Text("Bytes Allocated Per Frame: %d", engineStats.perFrameAllocated);
        ImGui::Text("Bytes Uploaded Per Frame: %d", engineStats.perFrameUploaded);
        ImGui::Text("Bytes Allocated: %d mb", engineStats.totalAllocated / 1000000);
        ImGui::Text("Bytes Deallocated: %d mb", engineStats.totalDeallocated / 1000000);

    }
    ImGui::End();
}