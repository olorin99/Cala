#include "Cala/backend/vulkan/Framebuffer.h"

cala::backend::vulkan::Framebuffer::Framebuffer(VkDevice device, RenderPass &renderPass, ende::Span<VkImageView> attachments, u32 width, u32 height)
    : _device(device),
    _framebuffer(VK_NULL_HANDLE),
    _renderPass(renderPass),
    _attachments(attachments),
    _width(width),
    _height(height)
{
    VkFramebufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

    createInfo.renderPass = _renderPass.renderPass();
    createInfo.attachmentCount = attachments.size();
    createInfo.width = width;
    createInfo.height = height;
    createInfo.layers = 1;

    createInfo.pAttachments = attachments.data();

    vkCreateFramebuffer(_device, &createInfo, nullptr, &_framebuffer);
}

cala::backend::vulkan::Framebuffer::~Framebuffer() {
    vkDestroyFramebuffer(_device, _framebuffer, nullptr);
}