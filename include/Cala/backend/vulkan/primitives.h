#ifndef CALA_VK_PRIMITIVES_H
#define CALA_VK_PRIMITIVES_H

#include <volk.h>
#include <Cala/backend/primitives.h>

namespace cala::backend::vulkan {

    constexpr inline VkPresentModeKHR getPresentMode(PresentMode mode) {
        return static_cast<VkPresentModeKHR>(mode);
    }

    constexpr inline VkFormat getFormat(Format format) {
        return static_cast<VkFormat>(format);
    }

    constexpr inline VkImageLayout getImageLayout(ImageLayout layout) {
        return static_cast<VkImageLayout>(layout);
    }

    constexpr inline VkMemoryPropertyFlagBits getMemoryProperties(MemoryProperties flags) {
        return static_cast<VkMemoryPropertyFlagBits>(flags);
    }

    constexpr inline VkBufferUsageFlagBits getBufferUsage(BufferUsage usage) {
        return static_cast<VkBufferUsageFlagBits>(usage);
    }

    constexpr inline VkImageUsageFlagBits getImageUsage(ImageUsage usage) {
        return static_cast<VkImageUsageFlagBits>(usage);
    }

    constexpr inline VkImageType getImageType(ImageType type) {
        return static_cast<VkImageType>(type);
    }

    constexpr inline VkImageViewType getImageViewType(ImageViewType type) {
        return static_cast<VkImageViewType>(type);
    }

    constexpr inline VkAttachmentLoadOp getLoadOp(LoadOp op) {
        return static_cast<VkAttachmentLoadOp>(op);
    }

    constexpr inline VkAttachmentStoreOp getStoreOp(StoreOp op) {
        return static_cast<VkAttachmentStoreOp>(op);
    }

    constexpr inline VkPolygonMode getPolygonMode(PolygonMode mode) {
        return static_cast<VkPolygonMode>(mode);
    }

    constexpr inline VkCullModeFlags getCullMode(CullMode mode) {
        return static_cast<VkCullModeFlags>(mode);
    }

    constexpr inline VkFrontFace getFrontFace(FrontFace face) {
        return static_cast<VkFrontFace>(face);
    }

    constexpr inline VkCompareOp getCompareOp(CompareOp op) {
        return static_cast<VkCompareOp>(op);
    }

    constexpr inline VkBlendFactor getBlendFactor(BlendFactor factor) {
        return static_cast<VkBlendFactor>(factor);
    }

    constexpr inline VkShaderStageFlagBits getShaderStage(ShaderStage stage) {
        return static_cast<VkShaderStageFlagBits>(stage);
    }

    constexpr inline VkAccessFlagBits2 getAccessFlags(Access flags) {
        return static_cast<VkAccessFlagBits2>(flags);
    }

    constexpr inline VkPipelineStageFlagBits2 getPipelineStage(PipelineStage stage) {
        return static_cast<VkPipelineStageFlagBits2>(stage);
    }

}

#endif //CALA_VK_PRIMITIVES_H
