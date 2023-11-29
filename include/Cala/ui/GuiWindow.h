#ifndef CALA_GUIWINDOW_H
#define CALA_GUIWINDOW_H

#include <Cala/Engine.h>
#include <Cala/backend/vulkan/Swapchain.h>
#include "Cala/ImGuiContext.h"

#include <Cala/ui/RenderGraphViewer.h>
#include <Cala/ui/LightWindow.h>
#include <Cala/ui/ProfileWindow.h>
#include <Cala/ui/RendererSettingsWindow.h>
#include <Cala/ui/ResourceViewer.h>
#include <Cala/ui/StatisticsWindow.h>
#include <Cala/ui/BackbufferWindow.h>
#include <Cala/ui/AssetManagerWindow.h>
#include <Cala/ui/RenderGraphWindow.h>
#include <Cala/ui/SceneGraphWindow.h>

namespace cala::ui {

    class GuiWindow {
    public:

        GuiWindow(Engine& engine, Renderer& renderer, Scene& scene, backend::vulkan::Swapchain& swapchain, SDL_Window* window);

        void render();

        ImGuiContext& context() { return _context; }


    private:

        Engine* _engine;
        Renderer* _renderer;
        ImGuiContext _context;

        bool _showRenderGraph;
        RenderGraphViewer _renderGraphViewer;

        bool _showLightWindow;
        LightWindow _lightWindow;

        bool _showProfileWindow;
        ProfileWindow _profileWindow;

        bool _showRendererSettings;
        RendererSettingsWindow _rendererSettings;

        bool _showResourceViewer;
        ResourceViewer _resourceViewer;

        bool _showStatisticsWindow;
        StatisticsWindow _statisticsWindow;

        bool _showBackbufferView;
        BackbufferWindow _backbufferView;

        bool _showAssetmanager;
        AssetManagerWindow _assetManager;

        bool _showRenderGraphWindow;
        RenderGraphWindow _renderGraphWindow;

        bool _showSceneGraphWindow;
        SceneGraphWindow _sceneGraphWindow;

    };

}

#endif //CALA_GUIWINDOW_H
