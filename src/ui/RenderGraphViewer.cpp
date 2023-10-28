#include "Cala/ui/RenderGraphViewer.h"
#include <node_editor/imgui_node_editor.h>

cala::ui::RenderGraphViewer::RenderGraphViewer(cala::RenderGraph *graph)
    : _graph(graph),
    _config(),
    _context(nullptr)
{
    _config.CanvasSizeMode = ax::NodeEditor::CanvasSizeMode::CenterOnly;
    _context = ax::NodeEditor::CreateEditor(&_config);
}

void cala::ui::RenderGraphViewer::render() {
    tsl::robin_map<const char*, std::vector<u32>> inputs;
    for (u32 i = 0; i < _graph->_orderedPasses.size(); i++) {
        auto& pass = _graph->_passes[i];
        for (auto& input : pass._inputs)
            inputs[input.label].push_back(i);
    }

    std::vector<tsl::robin_map<const char*, u32>> ids;

    u32 id = 1;

    ax::NodeEditor::SetCurrentEditor(_context);
    ax::NodeEditor::Begin("RenderGraph");

    f32 x = 0;
    f32 y = 0;

    for (auto& pass : _graph->_orderedPasses) {
        ids.emplace_back();

        u32 nodeID = id;

//        ax::NodeEditor::SetNodePosition(id, ImVec2(x, y));
        ax::NodeEditor::BeginNode(id++);
        ImGui::Text("%s", pass->_label);

        ImGui::Separator();

        for (auto& input : pass->_inputs) {
            ids.back()[input.label] = id;
            ax::NodeEditor::BeginPin(id++, ax::NodeEditor::PinKind::Input);
            ImGui::Text("%s", input.label);
            ax::NodeEditor::EndPin();
        }

        ImGui::Separator();

        for (auto& output : pass->_outputs) {
            ids.back()[output.label] = id;
            ax::NodeEditor::BeginPin(id++, ax::NodeEditor::PinKind::Output);
            ImGui::Text("%s", output.label);
            ax::NodeEditor::EndPin();
        }

        ax::NodeEditor::EndNode();

        auto size = ax::NodeEditor::GetNodeSize(nodeID);

        x += size.x + 100;
        y += 50;
    }

    auto findNextAccess = [&](i32 startIndex, u32 resource) -> std::tuple<i32, i32, RenderPass::ResourceAccess> {
        for (i32 passIndex = startIndex + 1; passIndex < _graph->_orderedPasses.size(); passIndex++) {
            auto& pass = _graph->_orderedPasses[passIndex];
            for (i32 outputIndex = 0; outputIndex < pass->_outputs.size(); outputIndex++) {
                if (pass->_outputs[outputIndex].index == resource)
                    return { passIndex, outputIndex, pass->_outputs[outputIndex] };
            }
            for (i32 inputIndex = 0; inputIndex < pass->_inputs.size(); inputIndex++) {
                if (pass->_inputs[inputIndex].index == resource)
                    return { passIndex, inputIndex, pass->_inputs[inputIndex] };
            }
        }
        return { -1, -1, {} };
    };

    for (i32 i = 0; i < (i32)_graph->_orderedPasses.size() - 1; i++) {
        auto& pass = _graph->_orderedPasses[i];
        for (auto& output : pass->_outputs) {
            i32 outputID = ids[i][output.label];

            auto [accessPassIndex, accessIndex, nextAccess] = findNextAccess(i, output.index);
            if (accessPassIndex < 0)
                continue;

            auto& accessPass = _graph->_orderedPasses[accessPassIndex];
            i32 inputId = ids[accessPassIndex][nextAccess.label];

            ax::NodeEditor::Link(id++, outputID, inputId);

        }

    }


    ax::NodeEditor::End();
}