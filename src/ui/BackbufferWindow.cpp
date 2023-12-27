#include <Cala/ui/BackbufferWindow.h>
#include <Cala/vulkan/primitives.h>
#include <imgui.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <Cala/Renderer.h>
#include <SDL_mouse.h>

cala::ui::BackbufferWindow::BackbufferWindow(ImGuiContext* context, vk::Device *device, cala::Renderer* renderer)
        : Window(context),
          _imageView(context, device),
          _device(device),
          _renderer(renderer)
{
    _imageView.setName("Backbuffer");
}

cala::ui::BackbufferWindow::~BackbufferWindow() noexcept {
}


void cala::ui::BackbufferWindow::render() {
    i32 mouseX = 0;
    i32 mouseY = 0;
    auto mouseState = SDL_GetMouseState(&mouseX, &mouseY);
    //TODO: need to transform application window coords to imgui window coords so matches position on image
    _renderer->setCursorPos({ static_cast<u32>(mouseX), static_cast<u32>(mouseY) });
    if (ImGui::Begin("Backbuffer")) {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && SDL_BUTTON_LEFT == mouseState) {
            _selectedMeshletID = _renderer->stats().currentMeshlet;
            _selectedMeshID = _renderer->stats().currentMesh;
        }
        auto availSize = ImGui::GetContentRegionAvail();
        _imageView.setSize(availSize.x, availSize.y);
        _imageView.render();
    }
    ImGui::End();
}