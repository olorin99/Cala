#ifndef CALA_RENDERGRAPHVIEWER_H
#define CALA_RENDERGRAPHVIEWER_H

#include <Cala/ui/Window.h>
#include <Cala/RenderGraph.h>
#include "imgui_node_editor.h"

namespace cala::ui {

    class RenderGraphViewer : public Window {
    public:

        RenderGraphViewer(ImGuiContext* context, RenderGraph* graph);

        void render() override;

    private:

        RenderGraph* _graph;
        ax::NodeEditor::Config _config;
        ax::NodeEditor::EditorContext* _context;


    };

}

#endif //CALA_RENDERGRAPHVIEWER_H
