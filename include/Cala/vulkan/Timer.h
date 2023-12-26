#ifndef CALA_TIMER_H
#define CALA_TIMER_H

#include <Cala/vulkan/CommandBuffer.h>

namespace cala::vk {

    class Timer {
    public:

        Timer();

        Timer(Device& driver, u32 index = 0);

        void start(CommandHandle cmd);

        void stop();

        u64 result();

    private:
        friend Device;

        Device* _driver = nullptr;

        u32 _index = 0;
        CommandHandle _cmdBuffer = {};
        u64 _result = 0;

    };

}

#endif //CALA_TIMER_H
