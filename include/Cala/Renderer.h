#ifndef CALA_RENDERER_H
#define CALA_RENDERER_H

#include <Cala/Engine.h>
#include <Cala/Camera.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Timer.h>
#include <Cala/RenderGraph.h>

class ImGuiContext;

namespace cala {

    class Scene;

    namespace backend::vulkan {
        class Device;
    }

    class Renderer {
    public:

        struct Settings {
            bool forward = true;
            bool deferred = true;
            bool tonemap = true;
            bool depthPre = false;
            bool skybox = true;
            bool freezeFrustum = false;
            bool ibl = false;

            bool debugClusters = false;
            bool debugNormals = false;
            bool debugRoughness = false;
            bool debugMetallic = false;
        };

        Renderer(Engine* engine, Settings settings);

        bool beginFrame(backend::vulkan::Swapchain* swapchain);

        f64 endFrame();

        void render(Scene& scene, Camera& camera, ImGuiContext* imGui = nullptr);

        ende::Span<std::pair<const char*, backend::vulkan::Timer>> timers() { return _graph.getTimers(); }

        struct Stats {
            u32 drawCallCount = 0;
        };

        Stats stats() const { return _stats; }

        Settings& settings() { return _renderSettings; }

        void setGamma(f32 gamma) { _globalData.gamma = gamma; }

        f32 getGamma() const { return _globalData.gamma; }

    private:

        Engine* _engine;
        backend::vulkan::Swapchain* _swapchain;

        backend::vulkan::BufferHandle _cameraBuffer[2];
        backend::vulkan::BufferHandle _drawCountBuffer[2];
        backend::vulkan::BufferHandle _globalDataBuffer;

        backend::vulkan::BufferHandle _drawCommands[2];

        backend::vulkan::ImageHandle _shadowTarget;
        backend::vulkan::Framebuffer* _shadowFramebuffer;

        RenderGraph _graph;

        backend::vulkan::Device::FrameInfo _frameInfo;
        backend::vulkan::Swapchain::Frame _swapchainFrame;

        Stats _stats;

        Settings _renderSettings;

        struct RendererGlobal {
            f32 gamma = 2.2;
            u32 time = 0;
        };

        RendererGlobal _globalData;

        ende::math::Frustum _cullingFrustum;



    };

}

#endif //CALA_RENDERER_H
