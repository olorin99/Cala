#ifndef CALA_TIMER_H
#define CALA_TIMER_H

#include "CommandBuffer.h"

namespace cala::vk {

    class Timer {
    public:

        Timer(Device& driver, u32 index = 0);

        void start(CommandHandle cmd);

        void stop();

        u64 result();

    private:

        Device* _driver;

        u32 _index;
        CommandHandle _cmdBuffer;
        u64 _result;

    };

}

#endif //CALA_TIMER_H
