#include "Cala/ui/GuiWindow.h"
#include <Cala/Renderer.h>

cala::ui::GuiWindow::GuiWindow(cala::Engine &engine, cala::Renderer& renderer, cala::Scene& scene, vk::Swapchain &swapchain, SDL_Window* window)
    : _engine(&engine),
      _renderer(&renderer),
      _context(engine.device(), &swapchain, window),

      _renderGraphViewer(&_context, &renderer._graph),
      _profileWindow(&_context, &engine, &renderer),
      _rendererSettings(&_context, &engine, &renderer, &swapchain),
      _resourceViewer(&_context, &engine.device()),
      _statisticsWindow(&_context, &engine, &renderer),
      _backbufferView(&_context, &engine.device(), &renderer),
      _assetManager(&_context, engine.assetManager()),
      _renderGraphWindow(&_context, &renderer._graph),
      _sceneGraphWindow(&_context, &scene),

      _showRenderGraph(false),
      _showProfileWindow(true),
      _showRendererSettings(true),
      _showResourceViewer(true),
      _showStatisticsWindow(true),
      _showBackbufferView(true),
      _showAssetmanager(true),
      _showRenderGraphWindow(true),
      _showSceneGraphWindow(true)
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

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("Main", nullptr, windowFlags);

        if (ImGui::BeginMainMenuBar()) {

            if (ImGui::BeginMenu("View")) {

                ImGui::MenuItem("Debug");

                ImGui::MenuItem("RenderGraph", nullptr, &_showRenderGraph, true);
                ImGui::MenuItem("Profiler", nullptr, &_showProfileWindow, true);
                ImGui::MenuItem("RendererSettings", nullptr, &_showRendererSettings, true);
                ImGui::MenuItem("ResourceViewer", nullptr, &_showResourceViewer, true);
                ImGui::MenuItem("Statistics", nullptr, &_showStatisticsWindow, true);
                ImGui::MenuItem("ImageView", nullptr, &_showBackbufferView, true);

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        ImGui::PopStyleVar(3);

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

    if (_showSceneGraphWindow) {
        _sceneGraphWindow.setSelectedMeshID(_backbufferView.getSelectedMeshID());
        _sceneGraphWindow.render();
    }


    ImGui::Render();
}