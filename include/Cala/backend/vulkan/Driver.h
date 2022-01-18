#ifndef CALA_DRIVER_H
#define CALA_DRIVER_H

#include <Cala/backend/vulkan/Context.h>
#include <Cala/backend/vulkan/Swapchain.h>
#include <Cala/backend/vulkan/CommandBufferList.h>

namespace cala::backend::vulkan {

    class Driver {
    public:

        Driver(ende::Span<const char*> extensions, void* window, void* display);


        CommandBuffer* beginFrame();

        bool endFrame();

        struct Primitive {
            VkBuffer vertex;
            u32 vertices;
        };

        void draw(CommandBuffer::RasterState state, Primitive primitive);


//    private:

        Context _context;
        Swapchain _swapchain;
        CommandBufferList _commands;

    };

}

#endif //CALA_DRIVER_H
