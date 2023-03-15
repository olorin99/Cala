#include "../include/Editor/Editor.h"
#include <SDL2/SDL.h>
#include <Ende/profile/ProfileManager.h>

cala::Editor::Editor(cala::Engine* engine, cala::backend::Platform* platform)
    : _engine(engine),
      _swapchain(_engine->device(), *platform),
      _renderer(_engine, {}),
      _context(_engine->device(), &_swapchain, (SDL_Window*)platform->window()),
      _running(true)
{}

void cala::Editor::run() {
    SDL_Event event;
    while (_running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    _running = false;
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            _engine->device().wait();
                            _swapchain.resize(event.window.data1, event.window.data2);
                            break;
                    }
                    break;
            }
            _context.processEvent(&event);
        }

        _context.newFrame();
        for (auto& panel : _panels) {
            panel->onRender();
        }

        _engine->gc();

        _renderer.beginFrame(&_swapchain);

        _renderer.endFrame();

        ende::profile::ProfileManager::frame();

    }
}


void cala::Editor::addPanel(Panel *panel) {
    _panels.push(panel);
}