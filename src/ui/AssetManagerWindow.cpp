#include "Cala/ui/AssetManagerWindow.h"
#include <imgui.h>

cala::ui::AssetManagerWindow::AssetManagerWindow(cala::AssetManager *assetManager)
    : _assetManager(assetManager)
{}

cala::ui::AssetManagerWindow::~AssetManagerWindow() {

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
            }

            ImGui::TreePop();
        }

        ImGui::End();
    }

}