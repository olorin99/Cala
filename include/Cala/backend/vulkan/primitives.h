#ifndef CALA_VK_PRIMITIVES_H
#define CALA_VK_PRIMITIVES_H

#include <vulkan/vulkan.h>
#include <Cala/backend/primitives.h>

namespace cala::backend::vulkan {

    constexpr inline VkFormat getFormat(Format format) {
        return static_cast<VkFormat>(format);
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

    constexpr inline VkShaderStageFlagBits getShaderStage(ShaderStage stage) {
        return static_cast<VkShaderStageFlagBits>(stage);
    }

}

#endif //CALA_VK_PRIMITIVES_H
