#ifndef CALA_SCENEGRAPHWINDOW_H
#define CALA_SCENEGRAPHWINDOW_H

#include "Window.h"
#include <Cala/Scene.h>

namespace cala::ui {

    class SceneGraphWindow : public Window {
    public:

        SceneGraphWindow(ImGuiContext* context, Scene* scene);

        void render() override;

        void setSelectedMeshID(u32 id) { _selectedMeshID = id; }

    private:

        Scene* _scene;

        u32 _selectedMeshletID = 0;
        u32 _selectedMeshID = 0;

    };

}

#endif //CALA_SCENEGRAPHWINDOW_H
