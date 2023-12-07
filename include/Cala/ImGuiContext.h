#ifndef CALA_IMGUICONTEXT_H
#define CALA_IMGUICONTEXT_H

#include <backends/imgui_impl_sdl2.h>

#include <Cala/vulkan/RenderPass.h>
#include <Cala/vulkan/Device.h>

class ImGuiContext {
public:

    ImGuiContext(cala::vk::Device& driver, cala::vk::Swapchain* swapchain, SDL_Window* window);

    ~ImGuiContext();


    void newFrame();

    void processEvent(void* event);

    void render(cala::vk::CommandHandle buffer);

    void destroySet(VkDescriptorSet set);

private:

    VkDevice _device;
    cala::vk::RenderPass* _renderPass;
    VkDescriptorPool _descriptorPool;
    VkCommandPool _commandPool;
    SDL_Window* _window;

    std::vector<std::pair<i32, VkDescriptorSet>> _descriptorDestroyQueue;

};


#endif //CALA_IMGUICONTEXT_H
