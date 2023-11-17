#ifndef CALA_HANDLE_H
#define CALA_HANDLE_H

#include <Ende/platform.h>
#include <functional>

namespace cala::backend::vulkan {

    class Device;
    class Buffer;
    class Image;
    class ShaderProgram;
    class ShaderModule;
    class PipelineLayout;
    class Sampler;
    class CommandBuffer;
    class CommandPool;

    template <typename T, typename O>
    class Handle {
    public:

        struct Counter {
            i32 count = 0;
            std::function<void(i32)> deleter;
        };

        Handle() = default;

        Handle(O* owner, i32 index, Counter* counter)
                : _owner(owner),
                  _index(index),
                  _counter(counter)
        {}

        ~Handle() {
            release();
        }

        Handle(const Handle& rhs)
            : _owner(rhs._owner),
            _index(rhs._index),
            _counter(rhs._counter)
        {
            if (_counter)
                ++_counter->count;
        }

        Handle& operator=(const Handle& rhs) {
            if (this == &rhs)
                return *this;
            if (_index != rhs._index)
                release();
            _owner = rhs._owner;
            _index = rhs._index;
            _counter = rhs._counter;
            if (_counter)
                ++_counter->count;
            return *this;
        }

        T& operator*() noexcept;

        T* operator->() noexcept;

        T* operator->() const noexcept;

        explicit operator bool() const noexcept {
            return _owner && _index >= 0 && isValid();
        }

        bool operator==(Handle<T, O> rhs) const noexcept {
            return _owner == rhs._owner && _index == rhs._index;
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
        friend O;

        O* _owner = nullptr;
        i32 _index = -1;
        Counter *_counter = nullptr;

    };

    using BufferHandle = Handle<Buffer, Device>;
    using ImageHandle = Handle<Image, Device>;
    using ProgramHandle = Handle<ShaderProgram, Device>;
    using ShaderModuleHandle = Handle<ShaderModule, Device>;
    using PipelineLayoutHandle = Handle<PipelineLayout, Device>;
    using SamplerHandle = Handle<Sampler, Device>;
    using CommandHandle = Handle<CommandBuffer, CommandPool>;

}

#endif //CALA_HANDLE_H
