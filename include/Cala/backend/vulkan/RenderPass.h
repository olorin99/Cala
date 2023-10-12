#ifndef CALA_RENDERPASS_H
#define CALA_RENDERPASS_H

#include <volk.h>
#include <vector>
#include <span>
#include <Cala/backend/primitives.h>

namespace cala::backend::vulkan {

    class Device;

    class RenderPass {
    public:

        struct Attachment {
            Format format;
            VkSampleCountFlagBits samples;
            VkAttachmentLoadOp loadOp;
            VkAttachmentStoreOp storeOp;
            VkAttachmentLoadOp stencilLoadOp;
            VkAttachmentStoreOp stencilStoreOp;
            VkImageLayout initialLayout;
            VkImageLayout finalLayout;
            VkImageLayout internalLayout;
        };

        RenderPass(Device& driver, std::span<Attachment> attachments);

        ~RenderPass();

        RenderPass(const RenderPass&) = delete;

        RenderPass& operator==(const RenderPass&) = delete;

        RenderPass(RenderPass&& rhs) noexcept;

        RenderPass& operator==(RenderPass&& rhs) noexcept;

        VkRenderPass renderPass() const { return _renderPass; }

        VkFramebuffer framebuffer(std::span<VkImageView> attachments, u32 width, u32 height);

        std::span<const VkClearValue> clearValues() const { return _clearValues; }

        u32 colourAttachmentCount() const { return _colourAttachments; }

        std::span<Attachment> attachments() { return _attachments; }

        u32 id() const { return _id; }

    private:

        VkDevice _device;
        VkRenderPass _renderPass;
        std::vector<VkClearValue> _clearValues;
        u32 _colourAttachments;
        u32 _depthAttachments;

        std::vector<Attachment> _attachments;
        const u32 _id;

    };

}

#endif //CALA_RENDERPASS_H
