#include "Cala/ui/RendererSettingsWindow.h"
#include <imgui.h>

cala::ui::RendererSettingsWindow::RendererSettingsWindow(Engine* engine, Renderer *renderer, backend::vulkan::Swapchain* swapchain)
    : _engine(engine),
    _renderer(renderer),
    _swapchain(swapchain),
    _targetFPS(240)
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
    ImGui::Checkbox("VXGI", &rendererSettings.vxgi);
    if (rendererSettings.vxgi) {
        auto minBounds = rendererSettings.voxelBounds.first;
        ImGui::SliderInt3("minBounds", &minBounds[0], -100, 0);
        auto maxBounds = rendererSettings.voxelBounds.second;
        ImGui::SliderInt3("maxBounds", &maxBounds[0], 0, 100);

        rendererSettings.voxelBounds.first = minBounds;
        rendererSettings.voxelBounds.second = maxBounds;
    }
    ImGui::Checkbox("GPU Culling", &rendererSettings.gpuCulling);
    ImGui::Checkbox("Bounded FrameTime", &rendererSettings.boundedFrameTime);
    ImGui::SliderInt("Target FPS", &_targetFPS, 5, 240);
    rendererSettings.millisecondTarget = 1000.f / _targetFPS;

    backend::PresentMode presentMode = _swapchain->getPresentMode();
    const char* modes[] = { "FIFO", "MAILBOX", "IMMEDIATE" };
    static int modeIndex = 0;
    if (ImGui::Combo("PresentMode", &modeIndex, modes, 3)) {
        _engine->device().wait();
        switch (modeIndex) {
            case 0:
                _swapchain->setPresentMode(backend::PresentMode::FIFO);
                break;
            case 1:
                _swapchain->setPresentMode(backend::PresentMode::MAILBOX);
                break;
            case 2:
                _swapchain->setPresentMode(backend::PresentMode::IMMEDIATE);
                break;
        }
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
    ImGui::Checkbox("\tVisualize VXGI", &rendererSettings.debugVxgi);

    f32 gamma = _renderer->getGamma();
    if (ImGui::SliderFloat("Gamma", &gamma, 0, 5))
        _renderer->setGamma(gamma);

    ImGui::End();
}