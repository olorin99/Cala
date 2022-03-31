#ifndef CALA_STATS_H
#define CALA_STATS_H

#include <Ende/platform.h>
#include "ImGuiWindow.h"

namespace cala::imgui {

    class Stats : public Window {
    public:

        virtual void render() override;

        void update(f32 frameTime, f32 averageFrameTime, u32 commandCount, u32 pipelineCount, u32 descriptorCount, u32 setLayoutCount, u32 frame);


    private:

        f32 _frameTime;
        f32 _averageFrameTime;
        u32 _commandBufferCount;
        u32 _pipelineCount;
        u32 _descriptorSetCount;
        u32 _setLayoutCount;
        u32 _frame;

    };

}

#endif //CALA_STATS_H
