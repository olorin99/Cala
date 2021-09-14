#ifndef CALA_DRIVER_H
#define CALA_DRIVER_H

#include <Cala/backend/vulkan/Context.h>
#include <Cala/backend/vulkan/Swapchain.h>

namespace cala::backend::vulkan {

    class Driver {
    public:

        Driver(ende::Span<const char*> extensions, void* window, void* display);




//    private:

        Context _context;
        Swapchain _swapchain;


    };

}

#endif //CALA_DRIVER_H
