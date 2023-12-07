#ifndef CALA_RESOURCEVIEWER_H
#define CALA_RESOURCEVIEWER_H

#include <Cala/ui/Window.h>
#include <Cala/vulkan/Device.h>

namespace cala::ui {

    class ResourceViewer : public Window {
    public:

        ResourceViewer(ImGuiContext* context, vk::Device* device);

        ~ResourceViewer();

        void render() override;

    private:

        vk::Device* _device;

        i32 _bufferIndex;
        i32 _imageIndex;
        bool _imageDirty;
        VkDescriptorSet _imageSet;

        i32 _mipIndex;
        i32 _layerIndex;

        vk::ImageHandle _imageHandle;
        vk::Image::View _imageView;
        vk::Image::View _deleteView;

        std::vector<std::pair<i32, std::pair<VkDescriptorSet, vk::Image::View>>> _destroyQueue;

    };

}

#endif //CALA_RESOURCEVIEWER_H
