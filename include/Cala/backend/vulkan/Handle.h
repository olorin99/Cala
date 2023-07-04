#ifndef CALA_HANDLE_H
#define CALA_HANDLE_H

#include <Ende/platform.h>

namespace cala::backend::vulkan {

    class Device;
    class Buffer;
    class Image;
    class ShaderProgram;

    template <typename T>
    class Handle {
    public:

        Handle() = default;

        Handle(Device* device, i32 index)
                : _device(device),
                  _index(index)
        {}

        T& operator*() noexcept;

        T* operator->() noexcept;

        explicit operator bool() const noexcept {
            return _device && _index >= 0 && isValid();
        }

        bool operator==(Handle<T> rhs) const noexcept {
            return _device == rhs._device && _index == rhs._index;
        }

        i32 index() const { return _index; }

        bool isValid() const;

    private:

        Device* _device = nullptr;
        i32 _index = -1;

    };

    using BufferHandle = Handle<Buffer>;
    using ImageHandle = Handle<Image>;
    using ProgramHandle = Handle<ShaderProgram>;

}

#endif //CALA_HANDLE_H
