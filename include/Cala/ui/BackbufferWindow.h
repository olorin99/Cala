#ifndef CALA_BACKBUFFERWINDOW_H
#define CALA_BACKBUFFERWINDOW_H

#include "Window.h"
#include <Cala/vulkan/Device.h>
#include "ImageView.h"

namespace cala::ui {

    class BackbufferWindow : public Window {
    public:

        BackbufferWindow(ImGuiContext* context, vk::Device* device);

        ~BackbufferWindow();

        void render() override;

        void setBackbufferHandle(vk::ImageHandle handle) {
            _imageView.setImageHandle(handle);
        }

    private:

        vk::Device* _device;
        ImageView _imageView;

    };

}

#endif //CALA_BACKBUFFERWINDOW_H
