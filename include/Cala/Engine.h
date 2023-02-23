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

    class Engine;
    template <typename T>
    class Handle {
    public:

        Handle() = default;

        T& operator*() noexcept;

        T* operator->() noexcept;

        operator bool() const noexcept {
            return _engine && _index != -1;
        }

        i32 index() const { return _index; }

    private:
        friend Engine;

        Handle(Engine* engine, u32 index)
                : _engine(engine),
                  _index(index)
        {}

        Engine* _engine = nullptr;
        i32 _index = -1;

    };
    using BufferHandle = Handle<backend::vulkan::Buffer>;
    using ImageHandle = Handle<backend::vulkan::Image>;
    using ProgramHandle = Handle<backend::vulkan::ShaderProgram>;

    class Renderer;
    class Material;
    class MaterialInstance;
    class Mesh;

    class Engine {
    public:

        Engine(backend::Platform& platform, bool clear = true);

        ~Engine();

        backend::vulkan::Device& driver() { return _driver; }

        ende::time::Duration getRunningTime() const { return _startTime.elapsed(); }

        bool gc();

        ImageHandle convertToCubeMap(ImageHandle equirectangular);

        ImageHandle generateIrradianceMap(ImageHandle cubeMap);

        ImageHandle generatePrefilteredIrradiance(ImageHandle cubeMap);

        BufferHandle createBuffer(u32 size, backend::BufferUsage usage, backend::MemoryProperties flags = backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT);

        ImageHandle createImage(backend::vulkan::Image::CreateInfo info, backend::vulkan::Sampler* sampler = nullptr);

        ProgramHandle createProgram(backend::vulkan::ShaderProgram&& program);

        void destroyBuffer(BufferHandle handle);

        void destroyImage(ImageHandle handle);

        BufferHandle resizeBuffer(BufferHandle handle, u32 size, bool transfer = false);


        backend::vulkan::Image::View& getImageView(ImageHandle handle);

        ImageHandle defaultAlbedo() const { return _defaultAlbedo; }

        ImageHandle defaultNormal() const { return _defaultNormal; }

        ImageHandle defaultMetallicRoughness() const { return _defaultMetallicRoughness; }

        u32 uploadVertexData(ende::Span<f32> data);

        u32 uploadIndexData(ende::Span<u32> data);



        struct Stats {
            u32 buffersInUse = 0;
            u32 allocatedBuffers = 0;
            u32 imagesInUse = 0;
            u32 allocatedImages = 0;
        };

        Stats stats() const;

    private:
        friend Renderer;
        friend Scene;
        friend Material;
        friend MaterialInstance;
        friend BufferHandle;
        friend ImageHandle;
        friend ProgramHandle;

        backend::vulkan::Device _driver;

        ende::time::SystemTime _startTime;

        backend::vulkan::Sampler _defaultSampler;
        backend::vulkan::Sampler _lodSampler;
        backend::vulkan::Sampler _irradianceSampler;

        backend::vulkan::RenderPass _shadowPass;
        ProgramHandle _pointShadowProgram;
        ProgramHandle _directShadowProgram;

        ProgramHandle _equirectangularToCubeMap;
        ProgramHandle _irradianceProgram;
        ProgramHandle _prefilterProgram;
        ProgramHandle _brdfProgram;
        ProgramHandle _skyboxProgram;
        ProgramHandle _tonemapProgram;
        ProgramHandle _cullProgram;
        ProgramHandle _createClustersProgram;
        ProgramHandle _cullLightsProgram;

        ProgramHandle _clusterDebugProgram;

        ImageHandle _defaultPointShadow;
        backend::vulkan::Image::View _defaultPointShadowView;
        ImageHandle _defaultDirectionalShadow;
        backend::vulkan::Image::View _defaultDirectionalShadowView;
        backend::vulkan::Sampler _shadowSampler;

        ImageHandle _defaultAlbedo;
        ImageHandle _defaultNormal;
        ImageHandle _defaultMetallicRoughness;

        ImageHandle _brdfImage;
        ImageHandle _defaultIrradiance;
        ImageHandle _defaultPrefilter;

        BufferHandle _globalVertexBuffer;
        BufferHandle _vertexStagingBuffer;
        BufferHandle _globalIndexBuffer;
        BufferHandle _indexStagingBuffer;
        u32 _vertexOffset;
        u32 _indexOffset;
        bool _stagingReady;

        Mesh* _cube;

        ende::Vector<backend::vulkan::Buffer*> _buffers;
        ende::Vector<backend::vulkan::Image*> _images;
        ende::Vector<backend::vulkan::Image::View> _imageViews;
        std::vector<backend::vulkan::ShaderProgram> _programs;

        ende::Vector<ImageHandle> _shadowMaps;

        ende::Vector<std::pair<i32, BufferHandle>> _buffersToDestroy;
        ende::Vector<u32> _freeBuffers;

        ende::Vector<std::pair<i32, ImageHandle>> _imagesToDestroy;
        ende::Vector<u32> _freeImages;

        ImageHandle getShadowMap(u32 index);

        bool _materialDataDirty;
        ende::Vector<u8> _materialData;
        BufferHandle _materialBuffer;

        void updateMaterialdata();

    };

}

#endif //CALA_ENGINE_H
