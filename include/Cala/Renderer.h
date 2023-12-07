#ifndef CALA_RENDERER_H
#define CALA_RENDERER_H

#include <Cala/Engine.h>
#include <Cala/Camera.h>
#include <Cala/vulkan/Buffer.h>
#include <Cala/vulkan/Timer.h>
#include <Cala/RenderGraph.h>

#include "shaderBridge.h"

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

        void render(Scene& scene, Camera& camera, ImGuiContext* imGui = nullptr);

        std::span<std::pair<const char*, vk::Timer>> timers() { return _graph.getTimers(); }

        struct Stats {
            u32 drawCallCount = 0;
        };

        Stats stats() const { return _stats; }

        Settings& settings() { return _renderSettings; }

        void setGamma(f32 gamma) { _globalData.gamma = gamma; }

        f32 getGamma() const { return _globalData.gamma; }

    private:

        Engine* _engine;
        vk::Swapchain* _swapchain;

        vk::BufferHandle _cameraBuffer[vk::FRAMES_IN_FLIGHT];
        vk::BufferHandle _globalDataBuffer[vk::FRAMES_IN_FLIGHT];
        vk::BufferHandle _frustumBuffer[vk::FRAMES_IN_FLIGHT];

    public:
        RenderGraph _graph;

    private:
        vk::Device::FrameInfo _frameInfo;
        vk::Swapchain::Frame _swapchainFrame;

        Stats _stats;

        Settings _renderSettings;

        struct RendererGlobal {
            f32 gamma = 2.2;
            u32 time = 0;
            u32 gpuCulling = 1;
            u32 maxDrawCount = 0;
            ende::math::Vec<4, u32> tileSizes = { 0, 0, 0, 0 };
            ende::math::Vec<2, u32> swapchainSize = {0, 0};
            f32 bloomStrength = 0.5;
            i32 irradianceIndex = -1;
            i32 prefilterIndex = -1;
            i32 brdfIndex = -1;
            i32 nearestRepeatSampler = -1;
            i32 linearRepeatSampler = -1;
            i32 lodSampler = -1;
            i32 shadowSampler = -1;
            u64 meshBuffer;
            u64 transformsBuffer;
            u64 cameraBuffer;
            u64 lightBuffer;
            u64 lightCountBuffer;
        };

        GlobalData _globalData;

        std::vector<ende::math::Vec4f> _frustumCorners;
        ende::math::Frustum _cullingFrustum;



    };

}

#endif //CALA_RENDERER_H
