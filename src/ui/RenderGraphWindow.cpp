#include "Cala/ui/RenderGraphWindow.h"

cala::ui::RenderGraphWindow::RenderGraphWindow(ImGuiContext *context, cala::RenderGraph *graph)
    : Window(context),
    _graph(graph)
{}

cala::ui::RenderGraphWindow::~RenderGraphWindow() noexcept {}

void cala::ui::RenderGraphWindow::render() {
    if (ImGui::Begin("RenderGraph Window")) {
        _assetViews.resize(_graph->_images.size(), ImageView(_context, &_graph->_engine->device()));
        for (u32 i = 0; i < _graph->_resources.size(); i++) {
            auto& resource = _graph->_resources[i];
            if (auto imageResource = dynamic_cast<ImageResource*>(resource.get()); imageResource) {
                auto& image = _graph->_images[imageResource->index];
                ImGui::Text(imageResource->label);
                ImGui::Text("Handle: %i", image.index());
                std::string buttonLabel = std::format("Save##{}", i);
                if (ImGui::Button(buttonLabel.c_str())) {
                    _graph->_engine->saveImageToDisk(std::format("{}.jpg", imageResource->label), image);
                }
                std::string label = std::format("Image##{}", i);
                if (ImGui::TreeNode(label.c_str())) {
                    auto availSize = ImGui::GetContentRegionAvail();
                    _assetViews[i].setSize(availSize.x, availSize.x);
                    _assetViews[i].setMode(1);
                    _assetViews[i].setImageHandle(image);
                    _assetViews[i].render();
                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::End();
}