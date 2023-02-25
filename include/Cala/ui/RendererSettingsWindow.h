#ifndef CALA_RENDERERSETTINGSWINDOW_H
#define CALA_RENDERERSETTINGSWINDOW_H

#include <Cala/ui/Window.h>
#include <Cala/Renderer.h>

namespace cala::ui {

    class RendererSettingsWindow : public Window {
    public:

        RendererSettingsWindow(Engine* engine, Renderer* renderer);

        void render() override;

    private:

        Engine* _engine;
        Renderer* _renderer;

    };

}

#endif //CALA_RENDERERSETTINGSWINDOW_H
