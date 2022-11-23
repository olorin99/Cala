#ifndef CALA_RENDERER_H
#define CALA_RENDERER_H

#include <Cala/Camera.h>
#include <Cala/backend/vulkan/Buffer.h>

namespace cala {

    class Scene;

    namespace backend::vulkan {
        class Driver;
    }

    class Renderer {
    public:

        Renderer(backend::vulkan::Driver& driver);

        void render(Scene& scene, Camera& camera);


    private:

        backend::vulkan::Driver& _driver;

        backend::vulkan::Buffer _cameraBuffer;



    };

}

#endif //CALA_RENDERER_H
