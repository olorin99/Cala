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
            bool ibl = true;
        };

        Renderer(Engine* engine, Settings settings);

        bool beginFrame();

        f64 endFrame();

        void render(Scene& scene, Camera& camera, ImGuiContext* imGui = nullptr);

        ende::Span<std::pair<const char*, backend::vulkan::Timer>> timers() { return _graph.getTimers(); }

        u32 frameIndex() const { return _frameInfo.frame % backend::vulkan::FRAMES_IN_FLIGHT; }

        struct Stats {
            u32 pipelineCount = 0;
            u32 descriptorCount = 0;
            u32 drawCallCount = 0;
        };

        Stats stats() const { return _stats; }

        Settings& settings() { return _renderSettings; }

        void setGamma(f32 gamma) { _globalData.gamma = gamma; }

        f32 getGamma() const { return _globalData.gamma; }

    private:

        Engine* _engine;

        BufferHandle _cameraBuffer[2];
        BufferHandle _drawCountBuffer[2];
        BufferHandle _globalDataBuffer;

        BufferHandle _drawCommands[2];

        ImageHandle _shadowTarget;
        backend::vulkan::Framebuffer* _shadowFramebuffer;

        RenderGraph _graph;

        backend::vulkan::Device::FrameInfo _frameInfo;

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
