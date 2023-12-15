#ifndef CALA_HANDLE_H
#define CALA_HANDLE_H

#include <Ende/platform.h>
#include <functional>

#include <cstdio>

namespace cala::vk {

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

        struct Data {
            i32 index = -1;
            i32 count = 0;
            std::function<void(i32)> deleter;
        };

        Handle() = default;

        Handle(O* owner, i32 index, Data* counter)
                : _owner(owner),
                  _index(index),
                  _data(counter)
        {}

        ~Handle() {
            release();
        }

        Handle(const Handle& rhs)
            : _owner(rhs._owner),
              _index(rhs._index),
              _data(rhs._data)
        {
            if (_data)
                ++_data->count;
        }

        Handle& operator=(const Handle& rhs) {
            if (this == &rhs)
                return *this;
            auto newData = rhs._data;
            if (newData)
                newData->count++;
            release();
            _owner = rhs._owner;
            _index = rhs._index;
            _data = rhs._data;
            return *this;
        }

        T& operator*() noexcept;

        T& operator*() const noexcept;

        T* operator->() noexcept;

        T* operator->() const noexcept;

        explicit operator bool() const noexcept {
            return _owner && ((!_data && _index >= 0) || _data->index >= 0) && isValid();
        }

        bool operator==(Handle<T, O> rhs) const noexcept {
            return _owner == rhs._owner && _data == rhs._data && _index == rhs._index && (!_data || _data->index == rhs._data->index);
        }

        i32 index() const {
            if (_data)
                return _data->index;
            return _index;
        }

        i32 count() const {
            if (_data)
                return _data->count;
            return 0;
        }

        bool isValid() const;

        bool release() {
            if (_data) {
                --_data->count;
                if (_data->count < 1) {
                    _data->deleter(_data->index);
                    return true;
                }
            }
            return false;
        }

    private:
        friend O;

        O* _owner = nullptr;
        i32 _index = -1;
        Data *_data = nullptr;

    };

    using BufferHandle = Handle<Buffer, Device>;
    using ImageHandle = Handle<Image, Device>;
    using ShaderModuleHandle = Handle<ShaderModule, Device>;
    using PipelineLayoutHandle = Handle<PipelineLayout, Device>;
    using SamplerHandle = Handle<Sampler, Device>;
    using CommandHandle = Handle<CommandBuffer, CommandPool>;

}

#endif //CALA_HANDLE_H
