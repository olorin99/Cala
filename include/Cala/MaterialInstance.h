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

        MaterialInstance& addImage(cala::backend::vulkan::Image::View&& view);

        bool setUniform(const char* name, u8* data, u32 size);

        template <typename T>
        bool setUniform(const char* name, const T& data) {
            return setUniform(name, (u8*)&data, sizeof(data));
        }

        bool setSampler(u32 set, const char* name, cala::backend::vulkan::Image::View&& view, backend::vulkan::Sampler&& sampler);

        bool setSampler(const char* name, cala::backend::vulkan::Image::View&& view, backend::vulkan::Sampler&& sampler);

        void bind(backend::vulkan::CommandBuffer& cmd, u32 set);


//        void setUniform

//    private:
        friend Material;

        MaterialInstance(Material& material, u32 offset);

        Material* _material;
        u32 _offset;
        backend::vulkan::SamplerArray _samplers;
//        ende::Vector<cala::backend::vulkan::Image::View> _samplers;

    };

}

#endif //CALA_MATERIALINSTANCE_H
