#ifndef CALA_IMGUICONTEXT_H
#define CALA_IMGUICONTEXT_H

#include <vulkan/vulkan.h>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_sdl.h>
//#include "imgui.h"
//#include "imgui_impl_vulkan.h"
//#include "imgui_impl_sdl.h"

#include "Cala/backend/vulkan/RenderPass.h"
#include "Cala/backend/vulkan/Driver.h"

class ImGuiContext {
public:

    ImGuiContext(cala::backend::vulkan::Driver& driver, SDL_Window* window);

    ~ImGuiContext();


    void newFrame();

    void processEvent(void* event);

    void render(cala::backend::vulkan::CommandBuffer& buffer);

private:

    VkDevice _device;
    cala::backend::vulkan::RenderPass* _renderPass;
    VkDescriptorPool _descriptorPool;
    VkCommandPool _commandPool;
    SDL_Window* _window;

};


#endif //CALA_IMGUICONTEXT_H
