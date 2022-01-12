#include "Cala/backend/vulkan/RenderPass.h"

cala::backend::vulkan::RenderPass::RenderPass(VkDevice device, ende::Span<Attachment> attachments)
    : _device(device)
{

    u32 colourAttachmentCount = 0;
    VkAttachmentDescription attachmentDescriptions[5]{}; // 4 colour attachments + depth
    VkAttachmentReference colourReferences[4]{};
    VkAttachmentDescription depthAttachment{};
    VkAttachmentReference depthReference{};
    bool depthPresent = false;

    for (u32 i = 0; i < attachments.size(); i++) {
        if (attachments[i].format != VK_FORMAT_D32_SFLOAT) {
            attachmentDescriptions[colourAttachmentCount].format = attachments[i].format;
            attachmentDescriptions[colourAttachmentCount].samples = attachments[i].samples;
            attachmentDescriptions[colourAttachmentCount].loadOp = attachments[i].loadOp;
            attachmentDescriptions[colourAttachmentCount].storeOp = attachments[i].storeOp;
            attachmentDescriptions[colourAttachmentCount].stencilLoadOp = attachments[i].stencilLoadOp;
            attachmentDescriptions[colourAttachmentCount].stencilStoreOp = attachments[i].stencilStoreOp;
            attachmentDescriptions[colourAttachmentCount].initialLayout = attachments[i].initialLayout;
            attachmentDescriptions[colourAttachmentCount].finalLayout = attachments[i].finalLayout;
            colourReferences[colourAttachmentCount].attachment = i;
            colourReferences[colourAttachmentCount].layout = attachments[i].internalLayout;
            colourAttachmentCount++;
        } else {
            depthAttachment.format = attachments[i].format;
            depthAttachment.samples = attachments[i].samples;
            depthAttachment.loadOp = attachments[i].loadOp;
            depthAttachment.storeOp = attachments[i].storeOp;
            depthAttachment.stencilLoadOp = attachments[i].stencilLoadOp;
            depthAttachment.stencilStoreOp = attachments[i].stencilStoreOp;
            depthAttachment.initialLayout = attachments[i].initialLayout;
            depthAttachment.finalLayout = attachments[i].finalLayout;
            depthReference.attachment = i;
            depthReference.layout = attachments[i].internalLayout;
            depthPresent = true;
        }

    }

    attachmentDescriptions[colourAttachmentCount] = depthAttachment;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = colourAttachmentCount;
    subpass.pColorAttachments = colourReferences;
    if (depthPresent)
        subpass.pDepthStencilAttachment = &depthReference;



//    VkSubpassDependency dependency{};
//    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
//    dependency.dstSubpass = 0;
//    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
//    dependency.srcAccessMask = 0;
//    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
//    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency dependencies[5]{};
    for (u32 i = 0; i < attachments.size(); i++) {
        dependencies[i].srcSubpass = VK_SUBPASS_EXTERNAL;
        if (depthPresent && i == attachments.size() - 1) {
            dependencies[i].dstSubpass = 0;
            dependencies[i].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependencies[i].srcAccessMask = 0;
            dependencies[i].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;;
            dependencies[i].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        } else {
            dependencies[i].dstSubpass = 0;
            dependencies[i].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[i].srcAccessMask = 0;
            dependencies[i].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[i].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
    }



    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = attachments.size();
    createInfo.pAttachments = attachmentDescriptions;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = attachments.size();
    createInfo.pDependencies = dependencies;

    vkCreateRenderPass(_device, &createInfo, nullptr, &_renderPass);

}

cala::backend::vulkan::RenderPass::~RenderPass() {
    vkDestroyRenderPass(_device, _renderPass, nullptr);
}


VkFramebuffer cala::backend::vulkan::RenderPass::framebuffer(ende::Span<VkImageView> attachments, u32 width, u32 height) {

    VkFramebufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

    createInfo.renderPass = _renderPass;
    createInfo.attachmentCount = attachments.size();
    createInfo.width = width;
    createInfo.height = height;
    createInfo.layers = 1;

    createInfo.pAttachments = attachments.data();

    VkFramebuffer framebuffer;
    vkCreateFramebuffer(_device, &createInfo, nullptr, &framebuffer);

    return framebuffer;
}