#ifndef CALA_RENDERGRAPHWINDOW_H
#define CALA_RENDERGRAPHWINDOW_H

#include <Cala/ui/Window.h>
#include <Cala/RenderGraph.h>
#include <Cala/ui/ImageView.h>

namespace cala::ui {

    class RenderGraphWindow : public Window {
    public:

        RenderGraphWindow(ImGuiContext* context, RenderGraph* graph);

        ~RenderGraphWindow() noexcept override;

        void render() override;

    private:

        RenderGraph* _graph;
        std::vector<ui::ImageView> _assetViews;

    };

}

#endif //CALA_RENDERGRAPHWINDOW_H
