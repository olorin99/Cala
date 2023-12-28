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

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    if (ImGui::Begin("Backbuffer")) {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
            auto windowPos = ImGui::GetWindowPos();
            auto cursorPos = ImGui::GetCursorPos();
            auto availSize = ImGui::GetContentRegionAvail();
            auto handle = _imageView.handle();
            auto imageWidth = 0;
            auto imageHeight = 0;
            if (handle) {
                imageWidth = handle->width();
                imageHeight = handle->height();
            }

            f32 u0 = ((imageWidth / 2) - (availSize.x / 2)) / imageWidth;
            f32 v0 = ((imageHeight / 2) - (availSize.y / 2)) / imageHeight;

            f32 imageOffsetWidth = imageWidth * u0;
            f32 imageOffsetHeight = imageHeight * v0;

            i32 adjustedMouseX = mouseX - windowPos.x - cursorPos.x + imageOffsetWidth;
            i32 adjustedMouseY = mouseY - windowPos.y - cursorPos.y + imageOffsetHeight;

            _renderer->setCursorPos({ static_cast<u32>(adjustedMouseX), static_cast<u32>(adjustedMouseY) });
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && SDL_BUTTON_LEFT == mouseState) {
                _selectedMeshletID = _renderer->stats().currentMeshlet;
                _selectedMeshID = _renderer->stats().currentMesh;
            }
        }

        auto availSize = ImGui::GetContentRegionAvail();

        _imageView.setSize(availSize.x, availSize.y);
        _imageView.render();
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
}