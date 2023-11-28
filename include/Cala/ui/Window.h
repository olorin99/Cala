#ifndef CALA_WINDOW_H
#define CALA_WINDOW_H

#include <Cala/ImGuiContext.h>

namespace cala::ui {

    class Window {
    public:

        Window(ImGuiContext* context) : _context(context) {}

        virtual ~Window() = default;

        virtual void render() {};

        ImGuiContext* _context = nullptr;

    };

}

#endif //CALA_WINDOW_H
