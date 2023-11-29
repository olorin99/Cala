#include "Cala/ui/AssetManagerWindow.h"
#include <imgui.h>
#include <Cala/Engine.h>

cala::ui::AssetManagerWindow::AssetManagerWindow(ImGuiContext* context, cala::AssetManager *assetManager)
    : Window(context),
    _assetManager(assetManager)
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
                            ImGui::Text("%s: %s", macro.name.c_str(), macro.value.c_str());
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

        if (ImGui::TreeNode("Images")) {
            ImGui::Separator();
            u32 imageID = 0;
            _assetViews.resize(_assetManager->_images.size(), ImageView(_context, &_assetManager->_engine->device()));
            for (u32 i = 0; i < _assetManager->_images.size(); i++) {
                auto& imageMetadata = _assetManager->_images[i];
                ImGui::Text("Name: %s", imageMetadata.name.c_str());
                ImGui::Text("Path: %s", imageMetadata.path.c_str());
                ImGui::Text("\tHandle: %i, Count: %i", imageMetadata.imageHandle.index(), imageMetadata.imageHandle.count());
                std::string label = std::format("Image##{}", i);
                if (ImGui::TreeNode(label.c_str())) {
                    auto availSize = ImGui::GetContentRegionAvail();
                    _assetViews[i].setSize(availSize.x, availSize.x);
                    _assetViews[i].setMode(1);
                    _assetViews[i].setImageHandle(imageMetadata.imageHandle);
                    std::string name = !imageMetadata.name.empty() ? imageMetadata.name : std::format("AssetImage: {}", i);
                    _assetViews[i].setName(name);
                    _assetViews[i].render();
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

    }
    ImGui::End();

}