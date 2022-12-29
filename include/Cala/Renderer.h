#ifndef CALA_RENDERER_H
#define CALA_RENDERER_H

#include <Cala/Engine.h>
#include <Cala/Camera.h>
#include <Cala/backend/vulkan/Buffer.h>

namespace cala {

    class Scene;

    namespace backend::vulkan {
        class Driver;
    }

    class Renderer {
    public:

        Renderer(Engine* engine);

        bool beginFrame();

        f64 endFrame();

        void render(Scene& scene, Camera& camera);


    private:

        Engine* _engine;

        BufferHandle _cameraBuffer;


        backend::vulkan::Driver::FrameInfo _frameInfo;
        backend::vulkan::Swapchain::Frame _swapchainInfo;



    };

}

#endif //CALA_RENDERER_H
