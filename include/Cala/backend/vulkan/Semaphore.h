#ifndef CALA_SEMAPHORE_H
#define CALA_SEMAPHORE_H

#include <Ende/platform.h>
#include <volk.h>

namespace cala::backend::vulkan {

    class Device;

    class Semaphore {
    public:

        Semaphore(Device* device, i64 initialValue = -1);

        ~Semaphore();

        Semaphore(Semaphore&& rhs) noexcept;

        Semaphore& operator=(Semaphore&& rhs) noexcept;

        bool valid() const { return _device && _semaphore != VK_NULL_HANDLE; }

        bool isTimeline() const { return _isTimeline; }

        u64 value() const { return _value; }

        u64 increment() { return ++_value; }

        VkSemaphore semaphore() const { return _semaphore; }

        bool wait(u64 value, u64 timeout = 1000000000);

        bool signal(u64 value);

        u64 queryGPUValue();

    private:

        Device* _device;
        VkSemaphore _semaphore;
        u64 _value;
        bool _isTimeline;

    };

}

#endif //CALA_SEMAPHORE_H