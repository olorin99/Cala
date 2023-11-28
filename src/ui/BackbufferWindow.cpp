#include "Cala/ui/BackbufferWindow.h"
#include "Cala/backend/vulkan/primitives.h"
#include <imgui.h>
#include <imgui/backends/imgui_impl_vulkan.h>

cala::ui::BackbufferWindow::BackbufferWindow(ImGuiContext* context, backend::vulkan::Device *device)
        : Window(context),
          _imageView(context, device),
          _device(device)
{
    _imageView.setName("Backbuffer");
}

cala::ui::BackbufferWindow::~BackbufferWindow() noexcept {
}


void cala::ui::BackbufferWindow::render() {
    if (ImGui::Begin("Backbuffer")) {
        auto availSize = ImGui::GetContentRegionAvail();
        _imageView.setSize(availSize.x, availSize.y);
        _imageView.render();
    }
    ImGui::End();
}