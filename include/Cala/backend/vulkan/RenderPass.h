#ifndef CALA_RENDERPASS_H
#define CALA_RENDERPASS_H

#include <vulkan/vulkan.h>
#include <Ende/Vector.h>
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

        ende::Span<const VkClearValue> clearValues() const { return _clearValues; }

        u32 colourAttachmentCount() const { return _colourAttachments; }

    private:

        VkDevice _device;
        VkRenderPass _renderPass;
        ende::Vector<VkClearValue> _clearValues;
        u32 _colourAttachments;
        u32 _depthAttachments;

    };

}

#endif //CALA_RENDERPASS_H
