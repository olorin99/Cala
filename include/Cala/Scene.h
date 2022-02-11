#ifndef CALA_SCENE_H
#define CALA_SCENE_H

#include <Ende/Vector.h>

#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/Transform.h>

namespace cala {

    class Scene {
    public:

        struct Renderable {
            backend::vulkan::Buffer vertex;
            backend::vulkan::Buffer index;
        };

//    private:

        ende::Vector<std::pair<Renderable, Transform>> _renderables;

    };

}

#endif //CALA_SCENE_H
