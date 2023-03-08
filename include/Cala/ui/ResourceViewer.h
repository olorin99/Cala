#ifndef CALA_RESOURCEVIEWER_H
#define CALA_RESOURCEVIEWER_H

#include <Cala/ui/Window.h>
#include <Cala/backend/vulkan/Device.h>

namespace cala::ui {

    class ResourceViewer : public Window {
    public:

        ResourceViewer(backend::vulkan::Device* device);

        void render() override;

    private:

        backend::vulkan::Device* _device;

        i32 _bufferIndex;
        i32 _imageIndex;

    };

}

#endif //CALA_RESOURCEVIEWER_H
