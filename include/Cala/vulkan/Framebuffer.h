#ifndef CALA_FRAMEBUFFER_H
#define CALA_FRAMEBUFFER_H

#include <volk/volk.h>
#include <Cala/vulkan/RenderPass.h>

namespace cala {
    class MaterialInstance;
}

namespace cala::vk {

    class Framebuffer {
    public:

        Framebuffer(VkDevice device, RenderPass& renderPass, std::span<VkImageView> attachments, u32 width, u32 height);

        Framebuffer(VkDevice device, RenderPass& renderPass, MaterialInstance* instance, u32 width, u32 height);

        ~Framebuffer();


        VkFramebuffer framebuffer() const { return _framebuffer; }

        RenderPass& renderPass() const { return _renderPass; };

        std::pair<u32, u32> extent() const { return { _width, _height }; }

    private:

        VkDevice _device;
        VkFramebuffer _framebuffer;
        RenderPass& _renderPass;
        std::span<VkImageView> _attachments;
        u32 _width;
        u32 _height;


    };

}

#endif //CALA_FRAMEBUFFER_H
