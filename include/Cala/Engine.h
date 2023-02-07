#ifndef CALA_ENGINE_H
#define CALA_ENGINE_H

#include <Ende/Vector.h>
#include <vector>
#include <Ende/Shared.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Image.h>

#include <Cala/backend/vulkan/Platform.h>
#include <Cala/backend/vulkan/Driver.h>

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

        backend::vulkan::Driver& driver() { return _driver; }

        ende::time::Duration getRunningTime() const { return _startTime.elapsed(); }

        bool gc();


        BufferHandle createBuffer(u32 size, backend::BufferUsage usage, backend::MemoryProperties flags = backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT);

        ImageHandle createImage(backend::vulkan::Image::CreateInfo info);

        ProgramHandle createProgram(backend::vulkan::ShaderProgram&& program);

        ImageHandle convertToCubeMap(ImageHandle equirectangular);


        void destroyImage(ImageHandle handle);


        backend::vulkan::Image::View& getImageView(ImageHandle handle);

        ImageHandle defaultAlbedo() const { return _defaultAlbedo; }

        ImageHandle defaultNormal() const { return _defaultNormal; }

        ImageHandle defaultMetallic() const { return _defaultMetallic; }

        ImageHandle defaultRoughness() const { return _defaultRoughness; }

        ImageHandle defaultAO() const { return _defaultAO; }

    private:
        friend Renderer;
        friend Scene;
        friend Material;
        friend MaterialInstance;
        friend BufferHandle;
        friend ImageHandle;
        friend ProgramHandle;

        backend::vulkan::Driver _driver;

        ende::time::SystemTime _startTime;

        backend::vulkan::Sampler _defaultSampler;

        backend::vulkan::RenderPass _shadowPass;
        ProgramHandle _pointShadowProgram;
        ProgramHandle _directShadowProgram;

        ProgramHandle _equirectangularToCubeMap;
        ProgramHandle _skyboxProgram;
        ProgramHandle _tonemapProgram;
        ProgramHandle _cullProgram;

        ImageHandle _defaultPointShadow;
        backend::vulkan::Image::View _defaultPointShadowView;
        ImageHandle _defaultDirectionalShadow;
        backend::vulkan::Image::View _defaultDirectionalShadowView;
        backend::vulkan::Sampler _shadowSampler;

        ImageHandle _defaultAlbedo;
        ImageHandle _defaultNormal;
        ImageHandle _defaultMetallic;
        ImageHandle _defaultRoughness;
        ImageHandle _defaultAO;

        Mesh* _cube;

        ende::Vector<backend::vulkan::Buffer> _buffers;
        ende::Vector<backend::vulkan::Image*> _images;
        ende::Vector<backend::vulkan::Image::View> _imageViews;
        ende::Vector<backend::vulkan::ShaderProgram> _programs;
        std::vector<Probe> _shadowProbes; //TODO: fix vectors in Ende, Optional implementation sucks

        ende::Vector<ImageHandle> _shadowMaps;

        ende::Vector<std::pair<i32, ImageHandle>> _imagesToDestroy;
        ende::Vector<u32> _freeImages;

        ImageHandle getShadowMap(u32 index);

        Probe& getShadowProbe(u32 index);

        bool _materialDataDirty;
        ende::Vector<u8> _materialData;
        BufferHandle _materialBuffer;

        void updateMaterialdata();

    };

}

#endif //CALA_ENGINE_H
