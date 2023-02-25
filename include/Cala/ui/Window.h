#ifndef CALA_WINDOW_H
#define CALA_WINDOW_H

namespace cala::ui {

    class Window {
    public:

        virtual ~Window() = default;

        virtual void render() {};

    };

}

#endif //CALA_WINDOW_H
