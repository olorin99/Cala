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
            bool vxgi = false;
            std::pair<ende::math::Vec<3, i32>, ende::math::Vec<3, i32>> voxelBounds = { { -10, -10, -10 }, { 10, 10, 10 } };

            bool debugUnlit = false;
            bool debugClusters = false;
            bool debugNormals = false;
            bool debugRoughness = false;
            bool debugMetallic = false;
            bool debugWorldPos = false;
            bool debugWireframe = false;
            bool debugNormalLines = false;
            bool debugVxgi = false;
            std::array<f32, 4> wireframeColour = { 1.f, 1.f, 1.f, 1.f };
            f32 wireframeThickness = 1.f;
            f32 normalLength = 0.1;
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
        backend::vulkan::BufferHandle _globalDataBuffer[2];

        backend::vulkan::ImageHandle _shadowTarget;
        backend::vulkan::Framebuffer* _shadowFramebuffer;

    public:
        RenderGraph _graph;

    private:
        backend::vulkan::Device::FrameInfo _frameInfo;
        backend::vulkan::Swapchain::Frame _swapchainFrame;

        Stats _stats;

        Settings _renderSettings;

        struct RendererGlobal {
            f32 gamma = 2.2;
            u32 time = 0;
            u32 maxDrawCount = 0;
            i32 tranformsBufferIndex = -1;
            i32 meshBufferIndex = -1;
            i32 materialBufferIndex = -1;
            i32 lightBufferIndex = -1;
            i32 cameraBufferIndex = -1;
            i32 irradianceIndex = -1;
            i32 prefilterIndex = -1;
            i32 brdfIndex = -1;
            i32 nearestRepeatSampler = -1;
            i32 linearRepeatSampler = -1;
            i32 lodSampler = -1;
            i32 shadowSampler = -1;
        };

        RendererGlobal _globalData;

        ende::math::Frustum _cullingFrustum;



    };

}

#endif //CALA_RENDERER_H
