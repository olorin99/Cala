#ifndef CALA_MATERIALINSTANCE_H
#define CALA_MATERIALINSTANCE_H

#include <Ende/Vector.h>
#include <Cala/backend/vulkan/Image.h>

namespace cala {

    class Material;

    class MaterialInstance {
    public:

        Material* material() const { return _material; }

        MaterialInstance& addImage(cala::backend::vulkan::Image::View&& view);

        bool setUniform(const char* name, u8* data, u32 size);

        template <typename T>
        bool setUniform(const char* name, const T& data) {
            return setUniform(name, (u8*)&data, sizeof(data));
        }


//        void setUniform

//    private:
        friend Material;

        MaterialInstance(Material& material, u32 offset);

        Material* _material;
        u32 _offset;
        ende::Vector<cala::backend::vulkan::Image::View> _samplers;

    };

}

#endif //CALA_MATERIALINSTANCE_H
