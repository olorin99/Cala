#ifndef CALA_SAMPLER_H
#define CALA_SAMPLER_H

#include <vulkan/vulkan.h>
#include <Ende/platform.h>

namespace cala::backend::vulkan {

    class Driver;

    class Sampler {
    public:

        struct CreateInfo {
            VkFilter filter = VK_FILTER_LINEAR;
            VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            bool anisotropy = false;
            f32 maxAnisotropy = 0.f;
            bool compare = false;
            VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
            VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            f32 mipLodBias = 0.f;
            f32 minLod = 0.f;
            f32 maxLod = 0.f;
            VkBorderColor borderColour = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            bool unnormalizedCoordinates = false;
        };

        Sampler(Driver& driver, CreateInfo info);

        ~Sampler();

        VkSampler sampler() const { return _sampler; }

    private:

        Driver& _driver;
        VkSampler _sampler;


    };

}

#endif //CALA_SAMPLER_H
