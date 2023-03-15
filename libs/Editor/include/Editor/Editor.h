#ifndef CALA_EDITOR_H
#define CALA_EDITOR_H

#include <Editor/Panel.h>
#include <Ende/Vector.h>
#include <Cala/Engine.h>
#include <Cala/Renderer.h>
#include <Cala/ImGuiContext.h>

namespace cala {

    class Editor {
    public:

        Editor(Engine* engine, backend::Platform* platform);

        void run();


        template <typename T>
        void addPanel() {
            auto panel = new T();
            addPanel(panel);
        }

        void addPanel(Panel* panel);

    private:

        ende::Vector<Panel*> _panels;

        Engine* _engine;
        backend::vulkan::Swapchain _swapchain;
        Renderer _renderer;
        ImGuiContext _context;
        bool _running;

    };

}


#endif //CALA_EDITOR_H
