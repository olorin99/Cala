#ifndef CALA_BACKBUFFERVIEW_H
#define CALA_BACKBUFFERVIEW_H

#include <Cala/ui/Window.h>
#include <Cala/backend/vulkan/Device.h>

namespace cala::ui {

    class BackbufferView : public Window {
    public:

        BackbufferView(backend::vulkan::Device* device);

        ~BackbufferView();

        void render() override;

        void setBackbufferHandle(backend::vulkan::ImageHandle handle) {
            if (_backbuffer != handle)
                _dirty = true;
            _backbuffer = handle;
        }

    private:

        backend::vulkan::Device* _device;
        backend::vulkan::ImageHandle _backbuffer;
        bool _dirty;
        VkDescriptorSet _imageSet;
        backend::vulkan::Image::View _imageView;

        u32 _width;
        u32 _height;

        std::vector<std::pair<i32, VkDescriptorSet>> _destroyQueue;

    };

}

#endif //CALA_BACKBUFFERVIEW_H
