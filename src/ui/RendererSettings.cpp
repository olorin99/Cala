#include "Cala/ui/RendererSettingsWindow.h"
#include <imgui.h>

cala::ui::RendererSettingsWindow::RendererSettingsWindow(Engine* engine, Renderer *renderer)
    : _engine(engine),
    _renderer(renderer)
{}

void cala::ui::RendererSettingsWindow::render() {
    auto& rendererSettings = _renderer->settings();
    ImGui::Begin("Render Settings");
    ImGui::Checkbox("Forward Pass", &rendererSettings.forward);
    ImGui::Checkbox("Depth Pre Pass", &rendererSettings.depthPre);
    ImGui::Checkbox("Skybox Pass", &rendererSettings.skybox);
    ImGui::Checkbox("Tonemap Pass", &rendererSettings.tonemap);
    ImGui::Checkbox("Freeze Frustum,", &rendererSettings.freezeFrustum);
    ImGui::Checkbox("IBL,", &rendererSettings.ibl);
    bool vsync = _engine->driver().swapchain().getVsync();
    if (ImGui::Checkbox("Vsync", &vsync)) {
        _engine->driver().wait();
        _engine->driver().swapchain().setVsync(vsync);
    }
    ImGui::Checkbox("Debug Clusters", &rendererSettings.debugClusters);

    f32 gamma = _renderer->getGamma();
    if (ImGui::SliderFloat("Gamma", &gamma, 0, 5))
        _renderer->setGamma(gamma);

    ImGui::End();
}