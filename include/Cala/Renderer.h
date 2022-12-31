#ifndef CALA_RENDERER_H
#define CALA_RENDERER_H

#include <Cala/Engine.h>
#include <Cala/Camera.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Timer.h>

class ImGuiContext;

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

        void render(Scene& scene, Camera& camera, ImGuiContext* imGui = nullptr);

        ende::Span<std::pair<const char*, backend::vulkan::Timer>> timers() { return _passTimers; }


    private:

        Engine* _engine;

        BufferHandle _cameraBuffer;
        BufferHandle _lightCameraBuffer;


        backend::vulkan::Driver::FrameInfo _frameInfo;
        backend::vulkan::Swapchain::Frame _swapchainInfo;

        ende::Vector<std::pair<const char*, backend::vulkan::Timer>> _passTimers;



    };

}

#endif //CALA_RENDERER_H
