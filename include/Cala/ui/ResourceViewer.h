#ifndef CALA_RESOURCEVIEWER_H
#define CALA_RESOURCEVIEWER_H

#include <Cala/ui/Window.h>
#include <Cala/backend/vulkan/Device.h>

namespace cala::ui {

    class ResourceViewer : public Window {
    public:

        ResourceViewer(backend::vulkan::Device* device);

        ~ResourceViewer();

        void render() override;

    private:

        backend::vulkan::Device* _device;

        i32 _bufferIndex;
        i32 _imageIndex;
        bool _imageDirty;
        VkDescriptorSet _imageSet;

        i32 _mipIndex;
        i32 _layerIndex;

        backend::vulkan::ImageHandle _imageHandle;
        backend::vulkan::Image::View _imageView;
        backend::vulkan::Image::View _deleteView;

        ende::Vector<std::pair<i32, std::pair<VkDescriptorSet, backend::vulkan::Image::View>>> _destroyQueue;

    };

}

#endif //CALA_RESOURCEVIEWER_H
