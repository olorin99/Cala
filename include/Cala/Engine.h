#ifndef CALA_ENGINE_H
#define CALA_ENGINE_H

#include <Ende/Vector.h>
#include <Ende/Shared.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Image.h>

#include <Cala/backend/vulkan/Platform.h>
#include <Cala/backend/vulkan/Device.h>

namespace cala {

    class Probe;
    class Scene;
    class Renderer;
    class Material;
    class MaterialInstance;
    class Mesh;

    class Engine {
    public:

        Engine(backend::Platform& platform, bool clear = true);

        backend::vulkan::Device& device() { return _device; }

        ende::time::Duration getRunningTime() const { return _startTime.elapsed(); }

        bool gc();

        backend::vulkan::ImageHandle convertToCubeMap(backend::vulkan::ImageHandle equirectangular);

        backend::vulkan::ImageHandle generateIrradianceMap(backend::vulkan::ImageHandle cubeMap);

        backend::vulkan::ImageHandle generatePrefilteredIrradiance(backend::vulkan::ImageHandle cubeMap);

        u32 uploadVertexData(ende::Span<f32> data);

        u32 uploadIndexData(ende::Span<u32> data);

    private:
        friend Renderer;
        friend Scene;
        friend Material;
        friend MaterialInstance;

        backend::vulkan::Device _device;

        ende::time::SystemTime _startTime;

        backend::vulkan::Sampler _lodSampler;
        backend::vulkan::Sampler _irradianceSampler;

        backend::vulkan::RenderPass _shadowPass;

        backend::vulkan::ProgramHandle _pointShadowProgram;
        backend::vulkan::ProgramHandle _directShadowProgram;

        backend::vulkan::ProgramHandle _equirectangularToCubeMap;
        backend::vulkan::ProgramHandle _irradianceProgram;
        backend::vulkan::ProgramHandle _prefilterProgram;
        backend::vulkan::ProgramHandle _brdfProgram;
        backend::vulkan::ProgramHandle _skyboxProgram;
        backend::vulkan::ProgramHandle _tonemapProgram;
        backend::vulkan::ProgramHandle _cullProgram;
        backend::vulkan::ProgramHandle _createClustersProgram;
        backend::vulkan::ProgramHandle _cullLightsProgram;

        backend::vulkan::ProgramHandle _clusterDebugProgram;

        backend::vulkan::ImageHandle _brdfImage;
        backend::vulkan::ImageHandle _defaultIrradiance;
        backend::vulkan::ImageHandle _defaultPrefilter;

        backend::vulkan::BufferHandle _globalVertexBuffer;
        backend::vulkan::BufferHandle _vertexStagingBuffer;
        backend::vulkan::BufferHandle _globalIndexBuffer;
        backend::vulkan::BufferHandle _indexStagingBuffer;
        u32 _vertexOffset;
        u32 _indexOffset;
        bool _stagingReady;

        Mesh* _cube;

        ende::Vector<backend::vulkan::ImageHandle> _shadowMaps;

        backend::vulkan::ImageHandle getShadowMap(u32 index);

        bool _materialDataDirty;
        ende::Vector<u8> _materialData;
        backend::vulkan::BufferHandle _materialBuffer;

        void updateMaterialdata();

    };

}

#endif //CALA_ENGINE_H
