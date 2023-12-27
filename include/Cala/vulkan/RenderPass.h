#ifndef CALA_RENDERPASS_H
#define CALA_RENDERPASS_H

#include <volk/volk.h>
#include <vector>
#include <span>
#include <Cala/vulkan/primitives.h>

namespace cala::vk {

    class Device;

    class RenderPass {
    public:

        struct Attachment {
            Format format;
            VkSampleCountFlagBits samples;
            LoadOp loadOp;
            StoreOp storeOp;
            LoadOp stencilLoadOp;
            StoreOp stencilStoreOp;
            ImageLayout initialLayout;
            ImageLayout finalLayout;
            ImageLayout internalLayout;
            VkClearValue clearValue = { 0.f, 0.f, 0.f, 1.f };
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
