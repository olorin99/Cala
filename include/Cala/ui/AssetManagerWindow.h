#ifndef CALA_ASSETMANAGERWINDOW_H
#define CALA_ASSETMANAGERWINDOW_H

#include "Window.h"
#include <Cala/AssetManager.h>
#include <Cala/ui/ImageView.h>

namespace cala::ui {

    class AssetManagerWindow : public Window {
    public:

        AssetManagerWindow(ImGuiContext* context, AssetManager* assetManager);

        ~AssetManagerWindow();

        void render() override;

    private:

        AssetManager* _assetManager;

        AssetManager::AssetMetadata _currentAssetMetadata;

        std::vector<ui::ImageView> _assetViews;

    };

}

#endif //CALA_ASSETMANAGERWINDOW_H
