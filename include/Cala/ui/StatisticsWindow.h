#ifndef CALA_STATISTICSWINDOW_H
#define CALA_STATISTICSWINDOW_H

#include <Cala/ui/Window.h>
#include <Cala/Renderer.h>

namespace cala::ui {

    class StatisticsWindow : public Window {
    public:

        StatisticsWindow(Engine* engine, Renderer* renderer);

        void render() override;

    private:

        Engine* _engine;
        Renderer* _renderer;
        bool _detailedMemoryStats;

    };

}

#endif //CALA_STATISTICSWINDOW_H
