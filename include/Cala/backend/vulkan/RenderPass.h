#ifndef CALA_RENDERPASS_H
#define CALA_RENDERPASS_H

#include <vulkan/vulkan.h>
#include <Ende/Vector.h>
#include <Ende/Span.h>
#include <Cala/backend/primitives.h>

namespace cala::backend::vulkan {

    class Driver;

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

        RenderPass(Driver& driver, ende::Span<Attachment> attachments);

        ~RenderPass();

        RenderPass(const RenderPass&) = delete;

        RenderPass& operator==(const RenderPass&) = delete;

        RenderPass(RenderPass&& rhs) noexcept;

        RenderPass& operator==(RenderPass&& rhs) noexcept;

        VkRenderPass renderPass() const { return _renderPass; }

        VkFramebuffer framebuffer(ende::Span<VkImageView> attachments, u32 width, u32 height);

        ende::Span<const VkClearValue> clearValues() const { return _clearValues; }

        u32 colourAttachmentCount() const { return _colourAttachments; }

        ende::Span<Attachment> attachments() { return _attachments; }

        u32 id() const { return _id; }

    private:

        VkDevice _device;
        VkRenderPass _renderPass;
        ende::Vector<VkClearValue> _clearValues;
        u32 _colourAttachments;
        u32 _depthAttachments;

        ende::Vector<Attachment> _attachments;
        const u32 _id;

    };

}

#endif //CALA_RENDERPASS_H
