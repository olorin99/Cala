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
                ImGui::Text("Name: %s", shaderModuleMetadata.name.c_str());
                ImGui::Text("Path: %s", shaderModuleMetadata.path.c_str());
                ImGui::Text("\tHandle: %i", shaderModuleMetadata.moduleHandle.index());
                if (!shaderModuleMetadata.includes.empty()) {
                    if (ImGui::TreeNode((void*)shaderID++, "Includes")) {
                        for (auto& include : shaderModuleMetadata.includes) {
                            ImGui::Text("%s: %s", include.data(), include.data());
                        }
                        ImGui::TreePop();
                    }
                }
                if (!shaderModuleMetadata.macros.empty()) {
                    if (ImGui::TreeNode((void*)shaderID++, "Macros")) {
                        for (auto& macro : shaderModuleMetadata.macros) {
                            ImGui::Text("%s: %s", macro.first.data(), macro.second.data());
                        }
                        ImGui::TreePop();
                    }
                }
                std::string label = std::format("Reload##{}", shaderID++);
                if (ImGui::Button(label.c_str())) {
                    _assetManager->_engine->logger().info("Reloading Shader: {}", shaderModuleMetadata.path.string());
                    _assetManager->reloadShaderModule(shaderModuleMetadata.hash);
                }
            }

            ImGui::TreePop();
        }

        ImGui::End();
    }

}