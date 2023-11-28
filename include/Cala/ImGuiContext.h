#ifndef CALA_IMGUICONTEXT_H
#define CALA_IMGUICONTEXT_H

#include <backends/imgui_impl_sdl2.h>

#include "Cala/backend/vulkan/RenderPass.h"
#include "Cala/backend/vulkan/Device.h"

class ImGuiContext {
public:

    ImGuiContext(cala::backend::vulkan::Device& driver, cala::backend::vulkan::Swapchain* swapchain, SDL_Window* window);

    ~ImGuiContext();


    void newFrame();

    void processEvent(void* event);

    void render(cala::backend::vulkan::CommandHandle buffer);

    void destroySet(VkDescriptorSet set);

private:

    VkDevice _device;
    cala::backend::vulkan::RenderPass* _renderPass;
    VkDescriptorPool _descriptorPool;
    VkCommandPool _commandPool;
    SDL_Window* _window;

    std::vector<std::pair<i32, VkDescriptorSet>> _descriptorDestroyQueue;

};


#endif //CALA_IMGUICONTEXT_H
