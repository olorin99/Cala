#ifndef CALA_ASSETMANAGERWINDOW_H
#define CALA_ASSETMANAGERWINDOW_H

#include "Window.h"
#include <Cala/AssetManager.h>

namespace cala::ui {

    class AssetManagerWindow : public Window {
    public:

        AssetManagerWindow(AssetManager* assetManager);

        ~AssetManagerWindow();

        void render() override;

    private:

        AssetManager* _assetManager;

        AssetManager::AssetMetadata _currentAssetMetadata;

    };

}

#endif //CALA_ASSETMANAGERWINDOW_H
