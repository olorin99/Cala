#include "Cala/backend/vulkan/RenderPass.h"

cala::backend::vulkan::RenderPass::RenderPass(VkDevice device, ende::Span<Attachment> attachments)
    : _device(device)
{

    VkAttachmentDescription attachmentDescriptions[4]{};
    VkAttachmentReference attachmentReferences[4]{};
    for (u32 i = 0; i < attachments.size(); i++) {
        attachmentDescriptions[i].format = attachments[i].format;
        attachmentDescriptions[i].samples = attachments[i].samples;
        attachmentDescriptions[i].loadOp = attachments[i].loadOp;
        attachmentDescriptions[i].storeOp = attachments[i].storeOp;
        attachmentDescriptions[i].stencilLoadOp = attachments[i].stencilLoadOp;
        attachmentDescriptions[i].stencilStoreOp = attachments[i].stencilStoreOp;
        attachmentDescriptions[i].initialLayout = attachments[i].initialLayout;
        attachmentDescriptions[i].finalLayout = attachments[i].finalLayout;

        attachmentReferences[i].attachment = i;
        attachmentReferences[i].layout = attachments[i].internalLayout;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = attachments.size();
    subpass.pColorAttachments = attachmentReferences;


    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;



    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = attachments.size();
    createInfo.pAttachments = attachmentDescriptions;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    vkCreateRenderPass(_device, &createInfo, nullptr, &_renderPass);

}

cala::backend::vulkan::RenderPass::~RenderPass() {
    vkDestroyRenderPass(_device, _renderPass, nullptr);
}


VkFramebuffer cala::backend::vulkan::RenderPass::framebuffer(VkImageView view, u32 width, u32 height) {

    VkFramebufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

    createInfo.renderPass = _renderPass;
    createInfo.attachmentCount = 1;
    createInfo.width = width;
    createInfo.height = height;
    createInfo.layers = 1;

    createInfo.pAttachments = &view;

    VkFramebuffer framebuffer;
    vkCreateFramebuffer(_device, &createInfo, nullptr, &framebuffer);

    return framebuffer;
}