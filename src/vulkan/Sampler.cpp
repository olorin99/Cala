#include <Cala/vulkan/Sampler.h>
#include <Cala/vulkan/Device.h>

cala::vk::Sampler::Sampler(Device& driver, CreateInfo info)
    : _driver(driver)
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
    samplerInfo.maxAnisotropy = info.anisotropy && info.maxAnisotropy == 0 ? driver.context().maxAnisotropy() : info.maxAnisotropy;
    samplerInfo.compareEnable = info.compare;
    samplerInfo.compareOp = info.compareOp;
    samplerInfo.minLod = info.minLod;
    samplerInfo.maxLod = info.maxLod;
    samplerInfo.borderColor = info.borderColour;
    samplerInfo.unnormalizedCoordinates = info.unnormalizedCoordinates;

    VK_TRY(vkCreateSampler(_driver.context().device(), &samplerInfo, nullptr, &_sampler));
}

cala::vk::Sampler::~Sampler() {
    if (_sampler != VK_NULL_HANDLE)
        vkDestroySampler(_driver.context().device(), _sampler, nullptr);
}

cala::vk::Sampler::Sampler(Sampler &&rhs) noexcept
    : _driver(rhs._driver),
    _sampler(VK_NULL_HANDLE)
{
    std::swap(_sampler, rhs._sampler);
}

cala::vk::Sampler &cala::vk::Sampler::operator=(Sampler &&rhs) noexcept {
    std::swap(_sampler, rhs._sampler);
    return *this;
}

bool cala::vk::Sampler::CreateInfo::operator==(const cala::vk::Sampler::CreateInfo &rhs) const {
    return filter == rhs.filter &&
            addressMode == rhs.addressMode &&
            anisotropy == rhs.anisotropy &&
            maxAnisotropy == rhs.maxAnisotropy &&
            compare == rhs.compare &&
            compareOp == rhs.compareOp &&
            mipmapMode == rhs.mipmapMode &&
            mipLodBias == rhs.mipLodBias &&
            minLod == rhs.minLod &&
            maxLod == rhs.maxLod &&
            borderColour == rhs.borderColour &&
            unnormalizedCoordinates == rhs.unnormalizedCoordinates;
}