#ifndef CALA_IMGUIWINDOW_H
#define CALA_IMGUIWINDOW_H

namespace cala::imgui {

    class Window {
    public:

        virtual ~Window() = default;

        virtual void render() {}

    };

}

#endif //CALA_IMGUIWINDOW_H
