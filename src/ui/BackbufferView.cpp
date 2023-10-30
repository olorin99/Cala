#include "Cala/ui/BackbufferView.h"
#include "Cala/backend/vulkan/primitives.h"
#include <imgui.h>
#include <imgui/backends/imgui_impl_vulkan.h>

cala::ui::BackbufferView::BackbufferView(backend::vulkan::Device *device)
    : _device(device),
    _dirty(true),
    _imageSet(VK_NULL_HANDLE)
{}

cala::ui::BackbufferView::~BackbufferView() noexcept {
    for (auto& handle : _destroyQueue) {
        ImGui_ImplVulkan_RemoveTexture(handle.second);
    }
}

void cala::ui::BackbufferView::render() {
    for (auto it = _destroyQueue.begin(); it != _destroyQueue.end(); it++) {
        auto& frame = it->first;
        auto& handle = it->second;
        if (frame <= 0) {
            ImGui_ImplVulkan_RemoveTexture(handle);
            _destroyQueue.erase(it--);
        } else
            --frame;
    }

    if (_dirty) {
        if (_imageSet != VK_NULL_HANDLE)
            _destroyQueue.push({ 5, _imageSet, });

        if (_backbuffer) {
            _imageSet = ImGui_ImplVulkan_AddTexture(_device->defaultSampler().sampler(), _device->getImageView(_backbuffer).view, backend::vulkan::getImageLayout(_backbuffer->layout()));
        }
        _dirty = false;
    }

    if (ImGui::Begin("BackbufferView")) {

        auto availSize = ImGui::GetContentRegionAvail();

        if (_imageSet != VK_NULL_HANDLE) {
            f32 width = _backbuffer->width();
            f32 height = _backbuffer->height();

            f32 u0 = ((width / 2) - (availSize.x / 2)) / width;
            f32 u1 = ((width / 2) + (availSize.x / 2)) / width;

            f32 v0 = ((height / 2) - (availSize.y / 2)) / height;
            f32 v1 = ((height / 2) + (availSize.y / 2)) / height;

            ImGui::Image(_imageSet, { availSize.x, availSize.y }, { u0, v0 }, { u1, v1 });
        }

        ImGui::End();
    }
}