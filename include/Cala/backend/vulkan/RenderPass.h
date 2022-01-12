#ifndef CALA_RENDERPASS_H
#define CALA_RENDERPASS_H

#include <vulkan/vulkan.h>

#include <Ende/Span.h>

namespace cala::backend::vulkan {

    class RenderPass {
    public:

        struct Attachment {
            VkFormat format;
            VkSampleCountFlagBits samples;
            VkAttachmentLoadOp loadOp;
            VkAttachmentStoreOp storeOp;
            VkAttachmentLoadOp stencilLoadOp;
            VkAttachmentStoreOp stencilStoreOp;
            VkImageLayout initialLayout;
            VkImageLayout finalLayout;
            VkImageLayout internalLayout;
        };

        RenderPass(VkDevice device, ende::Span<Attachment> attachments);

        ~RenderPass();

        VkRenderPass renderPass() const { return _renderPass; }

        VkFramebuffer framebuffer(ende::Span<VkImageView> attachments, u32 width, u32 height);

    private:

        VkDevice _device;
        VkRenderPass _renderPass;

    };

}

#endif //CALA_RENDERPASS_H
