#ifndef CALA_MATERIALINSTANCE_H
#define CALA_MATERIALINSTANCE_H

#include <Ende/Vector.h>
#include <Cala/backend/vulkan/Image.h>
#include <Cala/backend/vulkan/SamplerArray.h>

namespace cala {

    namespace backend::vulkan {
        class CommandBuffer;
    }

    class Material;

    class MaterialInstance {
    public:

        MaterialInstance(MaterialInstance&& rhs) noexcept;

        Material* material() const { return _material; }

        bool setUniform(u32 set, const char* name, u8* data, u32 size);

        bool setUniform(const char* name, u8* data, u32 size);

        template <typename T>
        bool setUniform(u32 set, const char* name, const T& data) {
            return setUniform(set, name, (u8*)&data, sizeof(data));
        }

        template <typename T>
        bool setUniform(const char* name, const T& data) {
            return setUniform(name, (u8*)&data, sizeof(data));
        }

        bool setSampler(u32 set, const char* name, cala::backend::vulkan::Image& view, backend::vulkan::Sampler&& sampler);

        bool setSampler(const char* name, cala::backend::vulkan::Image& view, backend::vulkan::Sampler&& sampler);

        bool setSampler(u32 set, const char* name, cala::backend::vulkan::Image::View&& view, backend::vulkan::Sampler&& sampler);

        bool setSampler(const char* name, cala::backend::vulkan::Image::View&& view, backend::vulkan::Sampler&& sampler);

        Material* getMaterial() const { return _material; }

        void bind(backend::vulkan::CommandBuffer& cmd, u32 set = 2, u32 first = 0);

        const backend::vulkan::SamplerArray& samplers() const { return _samplers; }

    private:
        friend Material;

        MaterialInstance(Material& material, u32 offset);

        Material* _material;
        u32 _offset;
        backend::vulkan::SamplerArray _samplers;

    };

}

#endif //CALA_MATERIALINSTANCE_H
