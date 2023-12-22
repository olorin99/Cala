#ifndef CALA_RENDERER_H
#define CALA_RENDERER_H

#include <Cala/Engine.h>
#include <Cala/Camera.h>
#include <Cala/vulkan/Buffer.h>
#include <Cala/vulkan/Timer.h>
#include <Cala/RenderGraph.h>

#include <Cala/shaderBridge.h>

#include <random>

class ImGuiContext;

namespace cala {

    class Scene;

    namespace vk {
        class Device;
    }

    class Renderer {
    public:

        struct Settings {
            bool forward = true;
            bool deferred = true;
            bool tonemap = true;
            i32 tonemapType = 0;
            bool bloom = true;
            f32 bloomStrength = 0.3;
            int shadowMode = 0;
            i32 pcfSamples = 20;
            i32 blockerSamples = 20;
            bool depthPre = false;
            bool skybox = true;
            bool freezeFrustum = false;
            bool ibl = false;
            bool gpuCulling = true;
            bool boundedFrameTime = false;
            f32 millisecondTarget = 1000.f / 60.f;

            bool debugUnlit = false;
            bool debugClusters = false;
            bool debugNormals = false;
            bool debugRoughness = false;
            bool debugMetallic = false;
            bool debugWorldPos = false;
            bool debugWireframe = false;
            bool debugNormalLines = false;
            bool debugFrustum = false;
            bool debugDepth = false;
            std::array<f32, 4> wireframeColour = { 1.f, 1.f, 1.f, 1.f };
            f32 wireframeThickness = 1.f;
            f32 normalLength = 0.1;
        };

        Renderer(Engine* engine, Settings settings);

        bool beginFrame(vk::Swapchain* swapchain);

        f64 endFrame();

        void render(Scene& scene, ImGuiContext* imGui = nullptr);

        std::span<std::pair<const char*, vk::Timer>> timers() { return _graph.getTimers(); }

        struct Stats {
            u32 drawCallCount = 0;
            u32 drawnMeshes = 0;
            u32 drawnMeshlets = 0;
            u32 culledMeshes = 0;
            u32 culledMeshlets = 0;
        };

        Stats stats() const { return _stats; }

        Settings& settings() { return _renderSettings; }

        void setGamma(f32 gamma) { _globalData.gamma = gamma; }

        f32 getGamma() const { return _globalData.gamma; }

    private:

        Engine* _engine;
        vk::Swapchain* _swapchain;

        vk::BufferHandle _globalDataBuffer[vk::FRAMES_IN_FLIGHT];
        vk::BufferHandle _feedbackBuffer[vk::FRAMES_IN_FLIGHT];

    public:
        RenderGraph _graph;

    private:
        vk::Device::FrameInfo _frameInfo;
        vk::Swapchain::Frame _swapchainFrame;

        std::mt19937 _randomGenerator;
        std::uniform_real_distribution<> _randomDistribution;

        Stats _stats;

        Settings _renderSettings;

        GlobalData _globalData;
        FeedbackInfo _feedbackInfo;

    };

}

#endif //CALA_RENDERER_H
