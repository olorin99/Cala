#include <Ende/Vector.h>

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#include "Cala/backend/vulkan/SDLPlatform.h"
#include <SDL2/SDL_vulkan.h>

cala::backend::vulkan::SDLPlatform::SDLPlatform(const char *windowTitle, u32 width, u32 height, u32 flags)
    : _window(nullptr),
    _surface(VK_NULL_HANDLE),
    _width(width),
    _height(height)
{
//    SDL_VideoInit("wayland");
    _window = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags | SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
    if (!_window) throw std::runtime_error("Unable to create window");
}

cala::backend::vulkan::SDLPlatform::~SDLPlatform() {
    SDL_DestroyWindow(_window);
}

ende::Vector<const char *> cala::backend::vulkan::SDLPlatform::requiredExtensions() {
    u32 extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(_window, &extensionCount, nullptr);
    ende::Vector<const char*> extensionNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(_window, &extensionCount, extensionNames.data());
    return std::move(extensionNames);
}

VkSurfaceKHR cala::backend::vulkan::SDLPlatform::surface(VkInstance instance) {
    if (_surface != VK_NULL_HANDLE)
        return _surface;
    if (!SDL_Vulkan_CreateSurface(_window, instance, &_surface))
        throw std::runtime_error("Unable to create surface");
    return _surface;
}

std::pair<u32, u32> cala::backend::vulkan::SDLPlatform::windowSize() const {
    i32 width, height;
    SDL_GetWindowSize(_window, &width, &height);
    return { width, height };
}