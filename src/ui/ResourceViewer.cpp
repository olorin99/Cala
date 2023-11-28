#include "Cala/ui/ResourceViewer.h"
#include <Cala/backend/vulkan/primitives.h>
#include <imgui.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <Cala/backend/vulkan/CommandBuffer.h>

cala::ui::ResourceViewer::ResourceViewer(ImGuiContext* context, backend::vulkan::Device *device)
    : Window(context),
    _device(device),
    _bufferIndex(0),
    _imageIndex(0),
    _imageDirty(true),
    _imageSet(VK_NULL_HANDLE),
    _mipIndex(0),
    _layerIndex(0)
{}

cala::ui::ResourceViewer::~ResourceViewer() {
    while (!_destroyQueue.empty()) {
        auto set = std::move(_destroyQueue.back());
        _destroyQueue.pop_back();
        set.second.second = backend::vulkan::Image::View();
        ImGui_ImplVulkan_RemoveTexture(set.second.first);
    }
}

void cala::ui::ResourceViewer::render() {
    for (auto it = _destroyQueue.begin(); it != _destroyQueue.end(); it++) {
        auto& frame = it->first;
        auto& handle = it->second;
        if (frame <= 0) {
            handle.second = backend::vulkan::Image::View();
            ImGui_ImplVulkan_RemoveTexture(handle.first);
            _destroyQueue.erase(it--);
        } else
            --frame;
    }
    if (_imageSet == VK_NULL_HANDLE && _imageIndex == 0) {
        _imageHandle = _device->getImageHandle(_imageIndex);
    }

    if (ImGui::Begin("Resources")) {
        auto stats = _device->stats();

        ImGui::SliderInt("BufferIndex", &_bufferIndex, 0, stats.allocatedBuffers - 1);
        ImGui::Text("Buffer: %d", _bufferIndex);

        auto buffer = _device->_bufferList.getResource(_bufferIndex);
        if (buffer->buffer() != VK_NULL_HANDLE) {
            u32 bytes = buffer->size();
            u32 kb = bytes / 1000;
            u32 mb = bytes / 1000000;
            u32 gb = bytes / 1000000000;

            ImGui::Text("\tSize: %d", bytes);
            ImGui::Text("\tKB: %d", kb);
            ImGui::Text("\tMB: %d", mb);
            ImGui::Text("\tGB: %d", gb);

        } else {
            ImGui::Text("Buffer has been deallocated");
        }

        if (ImGui::SliderInt("ImageIndex", &_imageIndex, 0, stats.allocatedImages - 1))
            _imageDirty = true;
        if (ImGui::SliderInt("Mip Level", &_mipIndex, 0, _imageHandle->mips() - 1))
            _imageDirty = true;
        if (ImGui::SliderInt("Layer Level", &_layerIndex, 0, _imageHandle->layers() - 1))
            _imageDirty = true;
        ImGui::Text("Image: %d", _imageIndex);

        if (_imageDirty) {
            if (_imageSet != VK_NULL_HANDLE) {
                _destroyQueue.push_back({ 5, std::make_pair( _imageSet, std::move(_imageView)) });
            }
            _imageHandle = _device->getImageHandle(_imageIndex);
            if (_imageHandle && _imageHandle->type() == backend::ImageType::IMAGE2D) {
                _imageView = _imageHandle->newView(_mipIndex, 1, _layerIndex, 1, backend::ImageViewType::VIEW2D);
                _imageSet = ImGui_ImplVulkan_AddTexture(_device->defaultSampler()->sampler(), _imageView.view, backend::vulkan::getImageLayout(_imageHandle->layout()));
            }
            _imageDirty = false;
        }

        if (_imageHandle) {
            u32 size = _imageHandle->height() * _imageHandle->width() * _imageHandle->depth();
            ImGui::Text("\tSize: %d", size);
            ImGui::Text("\tDimensions: { %d, %d }", _imageHandle->width(), _imageHandle->height());
            ImGui::Text("\tFormat: %s", backend::formatToString(_imageHandle->format()));

            auto availSize = ImGui::GetContentRegionAvail();

            f32 ratio = (f32)_imageHandle->height() / (f32)_imageHandle->width();
            ImGui::Image(_imageSet, { availSize.x, availSize.x * ratio });
        } else {
            ImGui::Text("Image has been deallocated");
        }

    }
    ImGui::End();


}