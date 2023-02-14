#ifndef CALA_TIMER_H
#define CALA_TIMER_H

#include <Cala/backend/vulkan/CommandBuffer.h>

namespace cala::backend::vulkan {

    class Timer {
    public:

        Timer(Device& driver, u32 index = 0);

        void start(CommandBuffer& cmd);

        void stop();

        u64 result();

    private:

        Device* _driver;

        u32 _index;
        CommandBuffer* _cmdBuffer;
        u64 _result;

    };

}

#endif //CALA_TIMER_H
