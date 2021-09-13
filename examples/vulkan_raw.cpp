
#include <iostream>

#include <Ende/platform.h>
#include <Ende/Vector.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_syswm.h>

#include <vulkan/vulkan.h>

#include <Cala/backend/vulkan/Context.h>
#include <Cala/backend/vulkan/Swapchain.h>

using namespace cala::backend::vulkan;

int main() {

    auto window = SDL_CreateWindow("hello", 0, 0, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

    u32 extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    ende::Vector<const char*> extensionNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data());

    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    ende::Vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    for (const auto& extension : extensions)
        std::cout << '\t' << extension.extensionName << '\n';


    Context context(extensionNames);
    Swapchain swapchain(context, &wmInfo.info.x11.window, wmInfo.info.x11.display);



    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
            }
        }



    }


    SDL_DestroyWindow(window);

    return 0;
}