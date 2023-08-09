#ifndef CALA_HANDLE_H
#define CALA_HANDLE_H

#include <Ende/platform.h>
#include <functional>

namespace cala::backend::vulkan {

    class Device;
    class Buffer;
    class Image;
    class ShaderProgram;

    template <typename T>
    class Handle {
    public:

        struct Counter {
            i32 count = 0;
            std::function<void(i32)> deleter;
        };

        Handle() = default;

        Handle(Device* device, i32 index, Counter* counter)
                : _device(device),
                  _index(index),
                  _counter(counter)
        {}

        ~Handle() {
            release();
        }

        Handle(const Handle& rhs)
            : _device(rhs._device),
            _index(rhs._index),
            _counter(rhs._counter)
        {
            if (_counter)
                ++_counter->count;
        }

        Handle& operator=(const Handle& rhs) {
            if (this == &rhs)
                return *this;
            _device = rhs._device;
            _index = rhs._index;
            _counter = rhs._counter;
            if (_counter)
                ++_counter->count;
            return *this;
        }

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

        bool release() {
            if (_counter) {
                --_counter->count;
                if (_counter->count < 1) {
                    _counter->deleter(_index);
                    return true;
                }
            }
            return false;
        }

    private:
        friend Device;

        Device* _device = nullptr;
        i32 _index = -1;
        Counter *_counter = nullptr;

    };

    using BufferHandle = Handle<Buffer>;
    using ImageHandle = Handle<Image>;
    using ProgramHandle = Handle<ShaderProgram>;

}

#endif //CALA_HANDLE_H
