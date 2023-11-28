#ifndef CALA_RENDERERSETTINGSWINDOW_H
#define CALA_RENDERERSETTINGSWINDOW_H

#include <Cala/ui/Window.h>
#include <Cala/Renderer.h>

namespace cala::ui {

    class RendererSettingsWindow : public Window {
    public:

        RendererSettingsWindow(ImGuiContext* context, Engine* engine, Renderer* renderer, backend::vulkan::Swapchain* swapchain);

        void render() override;

    private:

        Engine* _engine;
        Renderer* _renderer;
        backend::vulkan::Swapchain* _swapchain;
        i32 _targetFPS;

    };

}

#endif //CALA_RENDERERSETTINGSWINDOW_H
