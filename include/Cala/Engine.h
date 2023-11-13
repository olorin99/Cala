#ifndef CALA_ENGINE_H
#define CALA_ENGINE_H

#include <Ende/Shared.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Image.h>

#include <Cala/backend/vulkan/Platform.h>
#include <Cala/backend/vulkan/Device.h>

#include <filesystem>
#include <spdlog/spdlog.h>

#include <Cala/AssetManager.h>

namespace cala {

    class Probe;
    class Scene;
    class Renderer;
    class Material;
    class MaterialInstance;
    class Mesh;

    class Engine {
    public:

        Engine(backend::Platform& platform);

        backend::vulkan::Device& device() { return _device; }

        ende::time::Duration getRunningTime() const { return _startTime.elapsed(); }

        bool gc();

        backend::vulkan::ImageHandle convertToCubeMap(backend::vulkan::ImageHandle equirectangular);

        backend::vulkan::ImageHandle generateIrradianceMap(backend::vulkan::ImageHandle cubeMap);

        backend::vulkan::ImageHandle generatePrefilteredIrradiance(backend::vulkan::ImageHandle cubeMap);

        u32 uploadVertexData(std::span<f32> data);

        u32 uploadIndexData(std::span<u32> data);

        template <typename T>
        void stageData(backend::vulkan::BufferHandle dstHandle, const T& data, u32 dstOffset = 0) {
            stageData(dstHandle, std::span<const u8>(reinterpret_cast<const u8*>(&data), sizeof(data)), dstOffset);
        }

        void stageData(backend::vulkan::BufferHandle dstHandle, std::span<const u8> data, u32 dstOffset = 0);

        template <typename T>
        void stageData(backend::vulkan::ImageHandle dstHandle, std::span<T> data, backend::vulkan::Image::DataInfo dataInfo) {
            stageData(dstHandle, std::span<const u8>(reinterpret_cast<const u8*>(data.data()), data.size() * sizeof(T)), dataInfo);
        }

        void stageData(backend::vulkan::ImageHandle dstHandle, std::span<const u8> data, backend::vulkan::Image::DataInfo dataInfo);

        void flushStagedData();

        struct ShaderInfo {
            std::filesystem::path path;
            backend::ShaderStage stage;
            std::vector<std::pair<std::string_view, std::string_view>> macros = {};
            std::vector<std::string> includes = {};
        };
        backend::vulkan::ProgramHandle loadProgram(const std::vector<ShaderInfo>& shaderInfo);

        Material* createMaterial(u32 size);

        template <typename T>
        Material* createMaterial() {
            return createMaterial(sizeof(T));
        }

        //TODO: Return handle cause pointers may not be stable
        Material* loadMaterial(const std::filesystem::path& path, u32 size);

        template <typename T>
        Material* loadMaterial(const std::filesystem::path& path) {
            return loadMaterial(path, sizeof(T));
        }

        Material* getMaterial(u32 index);

        u32 materialCount() const;

        spdlog::logger& logger() { return _logger; }

        backend::vulkan::BufferHandle vertexBuffer() const { return _globalVertexBuffer; }

        backend::vulkan::BufferHandle indexBuffer() const { return _globalIndexBuffer; }

        enum class ProgramType {
            SHADOW_POINT,
            SHADOW_DIRECT,
            TONEMAP,
            CULL,
            CULL_POINT,
            CULL_DIRECT,
            CULL_LIGHTS,
            CREATE_CLUSTERS,
            DEBUG_CLUSTER,
            DEBUG_NORMALS,
            DEBUG_WORLDPOS,
            SOLID_COLOUR,
            VOXEL_VISUALISE
        };


        backend::vulkan::ProgramHandle getProgram(ProgramType type);

    private:
        friend Renderer;
        friend Scene;
        friend Material;
        friend MaterialInstance;

        spdlog::logger _logger;

        backend::vulkan::Device _device;

        AssetManager _assetManager;

        ende::time::SystemTime _startTime;

        backend::vulkan::SamplerHandle _lodSampler;
        backend::vulkan::SamplerHandle _irradianceSampler;

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
        backend::vulkan::ProgramHandle _pointShadowCullProgram;
        backend::vulkan::ProgramHandle _directShadowCullProgram;
        backend::vulkan::ProgramHandle _createClustersProgram;
        backend::vulkan::ProgramHandle _cullLightsProgram;

        backend::vulkan::ProgramHandle _clusterDebugProgram;
        backend::vulkan::ProgramHandle _normalsDebugProgram;
        backend::vulkan::ProgramHandle _worldPosDebugProgram;
        backend::vulkan::ProgramHandle _solidColourProgram;
        backend::vulkan::ProgramHandle _voxelVisualisationProgram;

        backend::vulkan::ImageHandle _brdfImage;

        backend::vulkan::BufferHandle _globalVertexBuffer;
        backend::vulkan::BufferHandle _globalIndexBuffer;
        u32 _vertexOffset;
        u32 _indexOffset;

        struct StagedBufferInfo {
            backend::vulkan::BufferHandle dstBuffer;
            u32 dstOffset;
            u32 srcSize;
            u32 srcOffset;
        };
        std::vector<StagedBufferInfo> _pendingStagedBuffer;

        struct StagedImageInfo {
            backend::vulkan::ImageHandle dstImage;
            ende::math::Vec<3, u32> dstDimensions;
            ende::math::Vec<3, i32> dstOffset;
            u32 dstMipLevel;
            u32 dstLayer;
            u32 dstLayerCount;
            u32 srcSize;
            u32 srcOffset;
        };
        std::vector<StagedImageInfo> _pendingStagedImage;

        u32 _stagingSize;
        u32 _stagingOffset;
        backend::vulkan::BufferHandle _stagingBuffer;

        Mesh* _cube;

        std::vector<backend::vulkan::ImageHandle> _shadowMaps;

        std::vector<Material> _materials;

        backend::vulkan::ImageHandle getShadowMap(u32 index);

        void updateMaterialdata();

    };

}

#endif //CALA_ENGINE_H
