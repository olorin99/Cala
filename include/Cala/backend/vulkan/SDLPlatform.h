#ifndef CALA_SDLPLATFORM_H
#define CALA_SDLPLATFORM_H

#include <Ende/platform.h>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include "Platform.h"

namespace cala::backend::vulkan {

    class SDLPlatform : public cala::backend::Platform {
    public:

        SDLPlatform(const char* windowTitle, u32 width, u32 height, u32 flags = 0);

        ~SDLPlatform() override;

        ende::Vector<const char *> requiredExtensions() override;

        VkSurfaceKHR surface(VkInstance instance) override;

        SDL_Window* window() const { return _window; }

        std::pair<u32, u32> windowSize() const override;

    private:

        SDL_Window* _window;
        VkSurfaceKHR _surface;
        u32 _width, _height;

    };

}

#endif //CALA_SDLPLATFORM_H
