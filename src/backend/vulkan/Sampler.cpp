#include "Cala/backend/vulkan/Sampler.h"

cala::backend::vulkan::Sampler::Sampler(Context& context, CreateInfo info)
    : _context(context)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    samplerInfo.magFilter = info.filter;
    samplerInfo.minFilter = info.filter;
    samplerInfo.mipmapMode = info.mipmapMode;
    samplerInfo.addressModeU = info.addressMode;
    samplerInfo.addressModeV = info.addressMode;
    samplerInfo.addressModeW = info.addressMode;
    samplerInfo.mipLodBias = info.mipLodBias;
    samplerInfo.anisotropyEnable = info.anisotropy;
    samplerInfo.maxAnisotropy = info.maxAnisotropy;
    samplerInfo.compareEnable = info.compare;
    samplerInfo.compareOp = info.compareOp;
    samplerInfo.minLod = info.minLod;
    samplerInfo.maxLod = info.maxLod;
    samplerInfo.borderColor = info.borderColour;
    samplerInfo.unnormalizedCoordinates = info.unnormalizedCoordinates;

    vkCreateSampler(context.device(), &samplerInfo, nullptr, &_sampler);
}

cala::backend::vulkan::Sampler::~Sampler() {
    vkDestroySampler(_context.device(), _sampler, nullptr);
}