#ifndef CALA_BACKBUFFERWINDOW_H
#define CALA_BACKBUFFERWINDOW_H

#include "Window.h"
#include <Cala/vulkan/Device.h>
#include "ImageView.h"

namespace cala {
    class Renderer;
}

namespace cala::ui {

    class BackbufferWindow : public Window {
    public:

        BackbufferWindow(ImGuiContext* context, vk::Device* device, Renderer* renderer);

        ~BackbufferWindow();

        void render() override;

        void setBackbufferHandle(vk::ImageHandle handle) {
            _imageView.setImageHandle(handle);
        }

        u32 getSelectedMeshID() const { return _selectedMeshID; }

    private:

        vk::Device* _device;
        ImageView _imageView;

        Renderer* _renderer;
        u32 _selectedMeshletID = 0;
        u32 _selectedMeshID = 0;

    };

}

#endif //CALA_BACKBUFFERWINDOW_H
