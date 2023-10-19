#include "Cala/ui/RenderGraphViewer.h"
#include <node_editor/imgui_node_editor.h>

cala::ui::RenderGraphViewer::RenderGraphViewer(cala::RenderGraph *graph)
    : _graph(graph),
    _context(ax::NodeEditor::CreateEditor())
{}

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

        ax::NodeEditor::SetNodePosition(id, ImVec2(x, y));
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

    for (i32 i = 0; i < (i32)_graph->_orderedPasses.size() - 1; i++) {
        auto& pass = _graph->_orderedPasses[i];
        for (auto& output : pass->_outputs) {
            i32 outputID = ids[i][output.label];

            for (i32 j = i; j < _graph->_orderedPasses.size(); j++) {
                auto& nextPass = _graph->_orderedPasses[j];
                for (auto& input : nextPass->_inputs) {
                    if (strcmp(output.label, input.label) == 0) {
                        i32 inputID = ids[j][input.label];
                        ax::NodeEditor::Link(id++, outputID, inputID);
                    }
                }
                for (auto& output2 : nextPass->_outputs) {
                    if (strcmp(output.label, output2.label) == 0) {
                        i32 inputID = ids[j][output2.label];
                        ax::NodeEditor::Link(id++, outputID, inputID);
                    }
                }
            }

        }

//            for (u32 i = 0; i < renderer._graph._orderedPasses.size(); i++) {
//                auto& pass = renderer._graph._orderedPasses[i];
//                for (auto& output : pass->_outputs) {
//                    i32 outputID = ids[i][output];
//                    auto& in = inputs[output];
//                    for (auto& input : in) {
//                        i32 inputID = ids[input][output];
//                        ax::NodeEditor::Link(id++, outputID, inputID);
//                    }
//                }

    }


    ax::NodeEditor::End();
}