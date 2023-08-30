#include "Cala/ui/RendererSettingsWindow.h"
#include <imgui.h>

cala::ui::RendererSettingsWindow::RendererSettingsWindow(Engine* engine, Renderer *renderer, backend::vulkan::Swapchain* swapchain)
    : _engine(engine),
    _renderer(renderer),
    _swapchain(swapchain)
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
    bool vsync = _swapchain->getVsync();
    if (ImGui::Checkbox("Vsync", &vsync)) {
        _engine->device().wait();
        _swapchain->setVsync(vsync);
    }
    ImGui::Text("Debug Settings");
    ImGui::Checkbox("\tDebug Clusters", &rendererSettings.debugClusters);
    ImGui::Checkbox("\tNormals", &rendererSettings.debugNormals);
//        rendererSettings.forward = false;
    ImGui::Checkbox("\tRoughness", &rendererSettings.debugRoughness);
    ImGui::Checkbox("\tMetallic", &rendererSettings.debugMetallic);
    ImGui::Checkbox("\tWorldPos", &rendererSettings.debugWorldPos);
    ImGui::Checkbox("\tUnlit", &rendererSettings.debugUnlit);
    ImGui::Checkbox("\tWireframe", &rendererSettings.debugWireframe);
    if (rendererSettings.debugWireframe) {
        ImGui::SliderFloat("\tWireframe Thickness", &rendererSettings.wireframeThickness, 1, 20);
        auto colour = rendererSettings.wireframeColour;
        ImGui::ColorEdit3("\tWireframe Colour", &colour[0]);
        ImGui::Text("Colour: { %f, %f, %fm %f }", colour[0], colour[1], colour[2], colour[3]);
        rendererSettings.wireframeColour = colour;
    }
    ImGui::Checkbox("\tNormal Lines", &rendererSettings.debugNormalLines);
    if (rendererSettings.debugNormalLines) {
        ImGui::SliderFloat("\tNormal Length", &rendererSettings.normalLength, 0.01, 1);
        auto colour = rendererSettings.wireframeColour;
        ImGui::ColorEdit3("\tWireframe Colour", &colour[0]);
        ImGui::Text("Colour: { %f, %f, %fm %f }", colour[0], colour[1], colour[2], colour[3]);
        rendererSettings.wireframeColour = colour;
    }

    f32 gamma = _renderer->getGamma();
    if (ImGui::SliderFloat("Gamma", &gamma, 0, 5))
        _renderer->setGamma(gamma);

    ImGui::End();
}