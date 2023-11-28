#ifndef CALA_IMAGEVIEW_H
#define CALA_IMAGEVIEW_H

#include <Cala/ui/Window.h>
#include <Cala/backend/vulkan/Device.h>

namespace cala::ui {

    class ImageView : public Window {
    public:

        ImageView(ImGuiContext* context, backend::vulkan::Device* device);

        ~ImageView() noexcept override;

        ImageView(const ImageView& rhs);

        void render() override;

        void setImageHandle(backend::vulkan::ImageHandle handle) {
            if (_image != handle) {
                _dirty = true;
                _image = handle;
            }
        }

        void setName(std::string_view name) {
            _name = name;
        }

        void setSize(u32 width, u32 height) {
            _width = width;
            _height = height;
        }

        void setMode(u32 mode) {
            _mode = mode; // 0 = fill, 1 == fit
        }

    private:

        backend::vulkan::Device* _device;
        backend::vulkan::ImageHandle _image;
        bool _dirty;
        VkDescriptorSet _imageSet;
        std::string _name;

        u32 _width;
        u32 _height;
        u32 _mode;

    };

}

#endif //CALA_IMAGEVIEW_H
