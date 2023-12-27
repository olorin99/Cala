#include "Cala/ui/RendererSettingsWindow.h"
#include <imgui.h>

cala::ui::RendererSettingsWindow::RendererSettingsWindow(ImGuiContext* context, Engine* engine, Renderer *renderer, vk::Swapchain* swapchain)
    : Window(context),
    _engine(engine),
    _renderer(renderer),
    _swapchain(swapchain),
    _targetFPS(240)
{}

void cala::ui::RendererSettingsWindow::render() {
    auto& rendererSettings = _renderer->settings();
    if (ImGui::Begin("Render Settings")) {
        ImGui::Checkbox("Forward Pass", &rendererSettings.forward);
        ImGui::Checkbox("Depth Pre Pass", &rendererSettings.depthPre);
        ImGui::Checkbox("Skybox Pass", &rendererSettings.skybox);
        if (ImGui::TreeNode("Shadows")) {
            const char* sizeStrings[] = { "256", "512", "1024", "2048", "4096" };
            u32 sizes[] = { 256, 512, 1024, 2048, 4096 };
            static int sizeIndex = 0;
            if (ImGui::Combo("ShadowMap Size", &sizeIndex, sizeStrings, 5)) {
                _engine->setShadowMapSize(sizes[sizeIndex]);
            }
            const char* modeStrings[] = { "PCSS", "PCF", "HARD" };
            static int modeIndex = 0;
            if (ImGui::Combo("Shadow Mode", &modeIndex, modeStrings, 3))
                rendererSettings.shadowMode = modeIndex;
            ImGui::SliderInt("PCF Samples", &rendererSettings.pcfSamples, 1, 128);
            ImGui::SliderInt("PCSS Blocker Samples", &rendererSettings.blockerSamples, 1, 128);
            ImGui::SliderInt("Shadow LOD bias", &rendererSettings.shadowLodBias, 0, MAX_LODS - 1);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Tonemapping Settings")) {
            ImGui::Checkbox("Tonemap Pass", &rendererSettings.tonemap);

            const char* modes[] = { "AGX", "ACES", "REINHARD", "REINHARD2", "LOTTES", "UCHIMURA" };
            static int modeIndex = 0;
            if (ImGui::Combo("Tonemap Operator", &modeIndex, modes, 6)) {
                rendererSettings.tonemapType = modeIndex;
            }

            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Bloom Settings")) {
            ImGui::Checkbox("Bloom", &rendererSettings.bloom);
            if (rendererSettings.bloom) {
                ImGui::SliderFloat("Bloom Strength", &rendererSettings.bloomStrength, 0, 1);
            }
            ImGui::TreePop();
        }
        ImGui::Checkbox("Freeze Frustum,", &rendererSettings.freezeFrustum);
        ImGui::Checkbox("IBL,", &rendererSettings.ibl);
        ImGui::Checkbox("GPU Culling", &rendererSettings.gpuCulling);
        ImGui::SliderInt("LOD bias", &rendererSettings.lodBias, 0, MAX_LODS - 1);
        ImGui::SliderInt("Shadow LOD bias", &rendererSettings.shadowLodBias, 0, MAX_LODS - 1);
        ImGui::Checkbox("Bounded FrameTime", &rendererSettings.boundedFrameTime);
        ImGui::SliderInt("Target FPS", &_targetFPS, 5, 240);
        rendererSettings.millisecondTarget = 1000.f / _targetFPS;

        const char* modes[] = { "FIFO", "MAILBOX", "IMMEDIATE" };
        static int modeIndex = 0;
        if (ImGui::Combo("PresentMode", &modeIndex, modes, 3)) {
            _engine->device().wait();
            switch (modeIndex) {
                case 0:
                    _swapchain->setPresentMode(vk::PresentMode::FIFO);
                    break;
                case 1:
                    _swapchain->setPresentMode(vk::PresentMode::MAILBOX);
                    break;
                case 2:
                    _swapchain->setPresentMode(vk::PresentMode::IMMEDIATE);
                    break;
            }
        }

        if (ImGui::TreeNode("Debug Settings")) {
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
            ImGui::Checkbox("\tFrustum", &rendererSettings.debugFrustum);
            ImGui::Checkbox("\tDepth", &rendererSettings.debugDepth);
            ImGui::Checkbox("\tMeshlets", &rendererSettings.debugMeshlets);
            ImGui::Checkbox("\tPrimitives", &rendererSettings.debugPrimitives);

            f32 gamma = _renderer->getGamma();
            if (ImGui::SliderFloat("Gamma", &gamma, 0, 5))
                _renderer->setGamma(gamma);

            ImGui::TreePop();
        }

    }
    ImGui::End();
}