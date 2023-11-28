#include "Cala/ui/GuiWindow.h"
#include <Cala/Renderer.h>

cala::ui::GuiWindow::GuiWindow(cala::Engine &engine, cala::Renderer& renderer, cala::Scene& scene, backend::vulkan::Swapchain &swapchain, SDL_Window* window)
    : _engine(&engine),
      _renderer(&renderer),
      _context(engine.device(), &swapchain, window),

      _renderGraphViewer(&_context, &renderer._graph),
      _lightWindow(&_context, &scene),
      _profileWindow(&_context, &engine, &renderer),
      _rendererSettings(&_context, &engine, &renderer, &swapchain),
      _resourceViewer(&_context, &engine.device()),
      _statisticsWindow(&_context, &engine, &renderer),
      _backbufferView(&_context, &engine.device()),
      _assetManager(&_context, engine.assetManager()),
      _renderGraphWindow(&_context, &renderer._graph),

      _showRenderGraph(false),
      _showLightWindow(true),
      _showProfileWindow(true),
      _showRendererSettings(true),
      _showResourceViewer(true),
      _showStatisticsWindow(true),
      _showBackbufferView(true),
      _showAssetmanager(true),
      _showRenderGraphWindow(true)
{}

void cala::ui::GuiWindow::render() {
    _context.newFrame();

    {
        auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(0.0f);

        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                             ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("Main", nullptr, windowFlags);

        if (ImGui::BeginMainMenuBar()) {

            if (ImGui::BeginMenu("View")) {

                ImGui::MenuItem("Debug");

                ImGui::MenuItem("RenderGraph", nullptr, &_showRenderGraph, true);
                ImGui::MenuItem("LightWindow", nullptr, &_showLightWindow, true);
                ImGui::MenuItem("Profiler", nullptr, &_showProfileWindow, true);
                ImGui::MenuItem("RendererSettings", nullptr, &_showRendererSettings, true);
                ImGui::MenuItem("ResourceViewer", nullptr, &_showResourceViewer, true);
                ImGui::MenuItem("Statistics", nullptr, &_showStatisticsWindow, true);
                ImGui::MenuItem("ImageView", nullptr, &_showBackbufferView, true);

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        ImGuiID dockspaceID = ImGui::GetID("dockspace");
        ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGui::DockSpace(dockspaceID, {0, 0}, dockspaceFlags);
        ImGui::End();
    }

    if (_showRenderGraphWindow)
        _renderGraphWindow.render();

    if (_showRenderGraph)
        _renderGraphViewer.render();

    if (_showStatisticsWindow)
        _statisticsWindow.render();

    if (_showProfileWindow)
        _profileWindow.render();

    if (_showRendererSettings)
        _rendererSettings.render();

    if (_showResourceViewer)
        _resourceViewer.render();

    if (_showBackbufferView) {
        _backbufferView.setBackbufferHandle(_renderer->_graph.getImage("backbuffer"));
        _backbufferView.render();
    }

    if (_showAssetmanager)
        _assetManager.render();

    if (_showLightWindow)
        _lightWindow.render();
}