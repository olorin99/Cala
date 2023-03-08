#include "Cala/ui/ResourceViewer.h"
#include <imgui.h>
#include <Cala/backend/vulkan/CommandBuffer.h>

cala::ui::ResourceViewer::ResourceViewer(backend::vulkan::Device *device)
    : _device(device),
    _bufferIndex(0),
    _imageIndex(0)
{}


void cala::ui::ResourceViewer::render() {

    ImGui::Begin("Resources");

    auto stats = _device->stats();

    ImGui::SliderInt("BufferIndex", &_bufferIndex, 0, stats.allocatedBuffers - 1);
    ImGui::Text("Buffer: %d", _bufferIndex);

    auto buffer = _device->_buffers[_bufferIndex];
    if (buffer) {
        u32 size = buffer->size();
        ImGui::Text("\tSize: %d", size);

    } else {
        ImGui::Text("Buffer has been deallocated");
    }

    ImGui::SliderInt("ImageIndex", &_imageIndex, 0, stats.allocatedImages - 1);
    ImGui::Text("Image: %d", _imageIndex);

    auto image = _device->_images[_imageIndex];
    if (image) {
        u32 size = image->height() * image->width() * image->depth();
        ImGui::Text("\tSize: %d", size);
        ImGui::Text("\tDimensions: { %d, %d }", image->width(), image->height());
        ImGui::Text("\tFormat: %s", backend::formatToString(image->format()));
//        ImGui::Image((void*)image->image(), {(f32)image->width(), (f32)image->height()}, {0, 0}, {1, 1});
    } else {
        ImGui::Text("Image has been deallocated");
    }

    ImGui::End();

}