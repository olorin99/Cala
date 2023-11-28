#ifndef CALA_BACKBUFFERWINDOW_H
#define CALA_BACKBUFFERWINDOW_H

#include "Window.h"
#include "Cala/backend/vulkan/Device.h"
#include "ImageView.h"

namespace cala::ui {

    class BackbufferWindow : public Window {
    public:

        BackbufferWindow(ImGuiContext* context, backend::vulkan::Device* device);

        ~BackbufferWindow();

        void render() override;

        void setBackbufferHandle(backend::vulkan::ImageHandle handle) {
            _imageView.setImageHandle(handle);
        }

    private:

        backend::vulkan::Device* _device;
        ImageView _imageView;

    };

}

#endif //CALA_BACKBUFFERWINDOW_H
