#ifndef CALA_SAMPLERARRAY_H
#define CALA_SAMPLERARRAY_H

#include <Cala/backend/vulkan/Sampler.h>
#include <Cala/backend/vulkan/Image.h>

namespace cala::backend::vulkan {

    class SamplerArray {
    public:

        SamplerArray(Device& driver);

        SamplerArray(SamplerArray&& rhs) noexcept;

        SamplerArray& operator=(SamplerArray&& rhs) noexcept;

        void set(u32 index, Image::View&& view, Sampler&& sampler);

        std::pair<Image::View&, Sampler&> get(u32 i);

        std::array<VkImageView, 5> views() const;

        u32 size() const { return _count; }

    private:

        Device& _driver;
        std::array<Image::View, 5> _views;
        std::array<Sampler, 5> _samplers;
        u32 _count;

    };

}

#endif //CALA_SAMPLERARRAY_H
