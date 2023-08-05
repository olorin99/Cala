#ifndef CALA_SAMPLER_H
#define CALA_SAMPLER_H

#include <vulkan/vulkan.h>
#include <Ende/platform.h>

namespace cala::backend::vulkan {

    class Device;

    class Sampler {
    public:

        struct CreateInfo {
            VkFilter filter = VK_FILTER_LINEAR;
            VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            bool anisotropy = true;
            f32 maxAnisotropy = 0.f;
            bool compare = false;
            VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
            VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            f32 mipLodBias = 0.f;
            f32 minLod = 0.f;
            f32 maxLod = 10.f;
            VkBorderColor borderColour = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            bool unnormalizedCoordinates = false;
        };

        Sampler(Device& driver, CreateInfo info);

        ~Sampler();

        Sampler(Sampler&& rhs) noexcept;

        Sampler& operator=(Sampler&& rhs) noexcept;

        VkSampler sampler() const { return _sampler; }

    private:

        Device& _driver;
        VkSampler _sampler;


    };

}

#endif //CALA_SAMPLER_H
