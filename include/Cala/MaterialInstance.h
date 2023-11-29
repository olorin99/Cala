#ifndef CALA_MATERIALINSTANCE_H
#define CALA_MATERIALINSTANCE_H

#include <Cala/backend/vulkan/Image.h>
#include <Cala/backend/vulkan/SamplerArray.h>
#include <string_view>

namespace cala {

    namespace backend::vulkan {
        class CommandBuffer;
    }

    class Material;

    class MaterialInstance {
    public:

        MaterialInstance(MaterialInstance&& rhs) noexcept;

        Material* material() const { return _material; }


        bool setParameter(const std::string& name, u8* data, u32 size);

        template <typename T>
        bool setParameter(const std::string& name, const T& data) {
            return setParameter(name, (u8*)&data, sizeof(data));
        }

        void* getParameter(const std::string& name, u32 size);

        template <typename T>
        T getParameter(const std::string& name) {
            void* data = getParameter(name, sizeof(T));
            if (!data)
                return T();
            return *reinterpret_cast<T*>(data);
        }


        void setData(u8* data, u32 size, u32 offset = 0);

        template <typename T>
        void setData(const T& data, u32 offset = 0) {
            setData((u8*)&data, sizeof(data), offset);
        }

        u32 getOffset() const { return _offset; }

        u32 getIndex() const;

        void bind(backend::vulkan::CommandBuffer& cmd, u32 set = 2, u32 first = 0);


    private:
        friend Material;

        MaterialInstance(Material* material, u32 offset);

        Material* _material;
        u32 _offset;

    };

}

#endif //CALA_MATERIALINSTANCE_H
