#ifndef CALA_SCENEGRAPHWINDOW_H
#define CALA_SCENEGRAPHWINDOW_H

#include "Window.h"
#include <Cala/Scene.h>

namespace cala::ui {

    class SceneGraphWindow : public Window {
    public:

        SceneGraphWindow(ImGuiContext* context, Scene* scene);

        void render() override;

    private:

        Scene* _scene;


    };

}

#endif //CALA_SCENEGRAPHWINDOW_H
