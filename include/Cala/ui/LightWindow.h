#ifndef CALA_LIGHTWINDOW_H
#define CALA_LIGHTWINDOW_H

#include "Window.h"
#include "Cala/Scene.h"

namespace cala::ui {

    class LightWindow : public Window {
    public:

        LightWindow(Scene* scene);

        void render() override;

    private:

        Scene* _scene;
        i32 _lightIndex;


    };

}

#endif //CALA_LIGHTWINDOW_H
