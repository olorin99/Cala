#include "Cala/ui/AssetManagerWindow.h"
#include <imgui.h>
#include <Cala/Engine.h>

cala::ui::AssetManagerWindow::AssetManagerWindow(cala::AssetManager *assetManager)
    : _assetManager(assetManager)
{}

cala::ui::AssetManagerWindow::~AssetManagerWindow() {
    _currentAssetMetadata = {};
}

void cala::ui::AssetManagerWindow::render() {

    if (ImGui::Begin("AssetManager")) {

        if (ImGui::TreeNode("Shaders")) {
            ImGui::Separator();

            u64 shaderID = 0;

            for (auto& shaderModuleMetadata : _assetManager->_shaderModules) {
                ImGui::Text("Path: %s", shaderModuleMetadata.path.c_str());
                ImGui::Text("\tHandle: %i", shaderModuleMetadata.moduleHandle.index());
                if (ImGui::TreeNode((void*)shaderID++, "Macros")) {
                    for (auto& macro : shaderModuleMetadata.macros) {
                        ImGui::Text("%s: %s", macro.first.data(), macro.second.data());
                    }
                    ImGui::TreePop();
                }
                std::string label = std::format("Reload##{}", shaderID++);
                if (ImGui::Button(label.c_str())) {
                    _assetManager->reloadShaderModule(shaderModuleMetadata.hash);
                    _assetManager->_engine->logger().info("Reloading Shader: {}", shaderModuleMetadata.path.string());

                }
            }

            ImGui::TreePop();
        }

        ImGui::End();
    }

}