#ifndef CALA_PROFILEWINDOW_H
#define CALA_PROFILEWINDOW_H

#include <Cala/ui/Window.h>
#include <Cala/Renderer.h>

namespace cala::ui {

    class ProfileWindow : public Window {
    public:

        ProfileWindow(ImGuiContext* context, Engine* engine, Renderer* renderer);

        void render() override;

    private:

        Engine* _engine;
        Renderer* _renderer;

        static const u32 MAX_FRAME_COUNT = 60 * 5;

        std::array<std::pair<f32, f32>, MAX_FRAME_COUNT> _globalTime;
        std::array<f32, MAX_FRAME_COUNT> _cpuTimes;
        std::array<f32, MAX_FRAME_COUNT> _gpuTimes;
        u32 _frameOffset;
        tsl::robin_map<const char*, std::array<f32, MAX_FRAME_COUNT>> _times;

        f32 _cpuAvg;
        f32 _gpuAvg;

    };

}

#endif //CALA_PROFILEWINDOW_H
