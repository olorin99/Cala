#ifndef CALA_PROFILEWINDOW_H
#define CALA_PROFILEWINDOW_H

#include <Cala/ui/Window.h>
#include <Cala/Renderer.h>

namespace cala::ui {

    class ProfileWindow : public Window {
    public:

        ProfileWindow(Engine* engine, Renderer* renderer);

        void render() override;

    private:

        Engine* _engine;
        Renderer* _renderer;

        std::array<f32, 60> _globalTime;
        tsl::robin_map<const char*, std::array<f32, 60>> _times;

    };

}

#endif //CALA_PROFILEWINDOW_H
