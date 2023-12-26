#ifndef CALA_ENGINE_H
#define CALA_ENGINE_H

#include <Ende/Shared.h>
#include <Cala/vulkan/Buffer.h>
#include <Cala/vulkan/Image.h>

#include <Cala/vulkan/Platform.h>
#include <Cala/vulkan/Device.h>

#include <filesystem>
#include <spdlog/spdlog.h>

#include <Cala/AssetManager.h>

#include <Cala/shaderBridge.h>

namespace cala {

    class Probe;
    class Scene;
    class Renderer;
    class Material;
    class MaterialInstance;
    class Mesh;

    class Engine {
    public:

        Engine(vk::Platform& platform);

        ~Engine();

        vk::Device& device() { return *_device; }

        AssetManager* assetManager() { return &_assetManager; }

        ende::time::Duration getRunningTime() const { return _startTime.elapsed(); }

        bool gc();

        vk::ImageHandle convertToCubeMap(vk::ImageHandle equirectangular);

        vk::ImageHandle generateIrradianceMap(vk::ImageHandle cubeMap);

        vk::ImageHandle generatePrefilteredIrradiance(vk::ImageHandle cubeMap);

        u32 uploadVertexData(std::span<f32> data);

        u32 uploadIndexData(std::span<u32> data);

        u32 uploadMeshletData(std::span<Meshlet> data);

        u32 uploadMeshletIndicesData(std::span<u32> data);

        u32 uploadPrimitiveData(std::span<u8> data);

        template <typename T>
        void stageData(vk::BufferHandle dstHandle, const T& data, u32 dstOffset = 0) {
            stageData(dstHandle, std::span<const u8>(reinterpret_cast<const u8*>(&data), sizeof(data)), dstOffset);
        }

        template <std::ranges::range Range>
        void stageData(vk::BufferHandle dstHandle, const Range& range, u32 dstOffset = 0) {
            stageData(dstHandle, std::span<const u8>(reinterpret_cast<const u8*>(std::ranges::data(range)), std::ranges::size(range) * sizeof(std::ranges::range_value_t<Range>)), dstOffset);
        }

        void stageData(vk::BufferHandle dstHandle, std::span<const u8> data, u32 dstOffset = 0);

        template <typename T>
        void stageData(vk::ImageHandle dstHandle, std::span<T> data, vk::Image::DataInfo dataInfo) {
            stageData(dstHandle, std::span<const u8>(reinterpret_cast<const u8*>(data.data()), data.size() * sizeof(T)), dataInfo);
        }

        template <std::ranges::range Range>
        void stageData(vk::ImageHandle dstHandle, const Range& range, vk::Image::DataInfo dataInfo) {
            stageData(dstHandle, std::span<const u8>(reinterpret_cast<const u8*>(std::ranges::data(range)), std::ranges::size(range) * sizeof(std::ranges::range_value_t<Range>)), dataInfo);
        }

        void stageData(vk::ImageHandle dstHandle, std::span<const u8> data, vk::Image::DataInfo dataInfo);

        u32 flushStagedData();

        struct ShaderInfo {
            std::filesystem::path path;
            vk::ShaderStage stage;
            std::vector<util::Macro> macros = {};
            std::vector<std::string> includes = {};
        };
        vk::ShaderProgram loadProgram(const std::string& name, const std::vector<ShaderInfo>& shaderInfo);

        Material* createMaterial(u32 size);

        template <typename T>
        Material* createMaterial() {
            return createMaterial(sizeof(T));
        }

        //TODO: Return handle cause pointers may not be stable
        Material* loadMaterial(const std::filesystem::path& path, u32 size = 0);

        template <typename T>
        Material* loadMaterial(const std::filesystem::path& path) {
            return loadMaterial(path, sizeof(T));
        }

        Material* getMaterial(u32 index);

        u32 materialCount() const;

        spdlog::logger& logger() { return _logger; }

        vk::BufferHandle vertexBuffer() const { return _globalVertexBuffer; }

        vk::BufferHandle indexBuffer() const { return _globalIndexBuffer; }

        vk::BufferHandle meshletBuffer() const { return _globalMeshletBuffer; }

        vk::BufferHandle meshletIndexBuffer() const { return _globalMeshletIndexBuffer; }

        vk::BufferHandle primitiveBuffer() const { return _globalPrimitiveBuffer; }

        VkVertexInputBindingDescription globalBinding() const {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = 12 * sizeof(f32);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return binding;
        }

        std::array<vk::Attribute, 4> globalVertexAttributes() const {
            std::array<vk::Attribute, 4> attributes = {
                    vk::Attribute{0, 0, vk::AttribType::Vec3f},
                    vk::Attribute{1, 0, vk::AttribType::Vec3f},
                    vk::Attribute{2, 0, vk::AttribType::Vec2f},
                    vk::Attribute{3, 0, vk::AttribType::Vec4f}
            };
            return attributes;
        }


        Mesh* unitCube() const { return _cube; }

        enum class ProgramType {
            SHADOW_POINT,
            SHADOW_DIRECT,
            TONEMAP,
            BLOOM_DOWNSAMPLE,
            BLOOM_UPSAMPLE,
            BLOOM_COMPOSITE,
            CULL,
            CULL_MESH_SHADER,
            CULL_POINT,
            CULL_DIRECT,
            CULL_LIGHTS,
            CREATE_CLUSTERS,
            VISIBILITY,
            VISIBILITY_COUNT,
            VISIBILITY_OFFSET,
            VISIBILITY_POSITION,
            DEBUG_MESHLETS,
            DEBUG_CLUSTER,
            DEBUG_NORMALS,
            DEBUG_WORLDPOS,
            DEBUG_FRUSTUM,
            DEBUG_DEPTH,
            SOLID_COLOUR,
            VOXEL_VISUALISE,
            SKYBOX
        };


        const vk::ShaderProgram& getProgram(ProgramType type);

        vk::ImageHandle getShadowMap(u32 index, bool point, vk::CommandHandle cmd = {});

        void setShadowMapSize(u32 size);

        u32 getShadowMapSize() const { return _shadowMapSize; }

        vk::ImageHandle getShadowTarget() const { return _shadowTarget; }

        vk::Framebuffer* getShadowFramebuffer();


        void saveImageToDisk(const std::filesystem::path& path, vk::ImageHandle handle);

    private:
        friend Renderer;
        friend Scene;
        friend Material;
        friend MaterialInstance;

        spdlog::logger _logger;

        std::unique_ptr<vk::Device> _device;

        AssetManager _assetManager;

        ende::time::SystemTime _startTime;

        vk::SamplerHandle _lodSampler;
        vk::SamplerHandle _irradianceSampler;

        vk::RenderPass _shadowPass;

        vk::ShaderProgram _pointShadowProgram;
        vk::ShaderProgram _directShadowProgram;

        vk::ShaderProgram _equirectangularToCubeMap;
        vk::ShaderProgram _irradianceProgram;
        vk::ShaderProgram _prefilterProgram;
        vk::ShaderProgram _brdfProgram;
        vk::ShaderProgram _skyboxProgram;
        vk::ShaderProgram _tonemapProgram;
        vk::ShaderProgram _cullProgram;
        vk::ShaderProgram _cullMeshShaderProgram;
        vk::ShaderProgram _pointShadowCullProgram;
        vk::ShaderProgram _directShadowCullProgram;
        vk::ShaderProgram _createClustersProgram;
        vk::ShaderProgram _cullLightsProgram;

        vk::ShaderProgram _bloomDownsampleProgram;
        vk::ShaderProgram _bloomUpsampleProgram;
        vk::ShaderProgram _bloomCompositeProgram;

        vk::ShaderProgram _visibilityBufferProgram;
        vk::ShaderProgram _visibilityCountProgram;
        vk::ShaderProgram _visibilityOffsetProgram;
        vk::ShaderProgram _visibilityPositionsProgram;

        vk::ShaderProgram _meshletDebugProgram;
        vk::ShaderProgram _clusterDebugProgram;
        vk::ShaderProgram _normalsDebugProgram;
        vk::ShaderProgram _worldPosDebugProgram;
        vk::ShaderProgram _solidColourProgram;
        vk::ShaderProgram _frustumDebugProgram;
        vk::ShaderProgram _depthDebugProgram;
        vk::ShaderProgram _voxelVisualisationProgram;

        vk::ImageHandle _poissonImage;
        vk::ImageHandle _brdfImage;

        vk::BufferHandle _globalVertexBuffer;
        vk::BufferHandle _globalIndexBuffer;
        u32 _vertexOffset;
        u32 _indexOffset;

        vk::BufferHandle _globalMeshletBuffer;
        vk::BufferHandle _globalMeshletIndexBuffer;
        vk::BufferHandle _globalPrimitiveBuffer;
        u32 _meshletOffset;
        u32 _meshletIndexOffset;
        u32 _primitiveOffset;

        struct StagedBufferInfo {
            vk::BufferHandle dstBuffer = {};
            u32 dstOffset = 0;
            u32 srcSize = 0;
            u32 srcOffset = 0;
        };
        std::vector<StagedBufferInfo> _pendingStagedBuffer;

        struct StagedImageInfo {
            vk::ImageHandle dstImage = {};
            ende::math::Vec<3, u32> dstDimensions = { 0, 0, 0 };
            ende::math::Vec<3, i32> dstOffset = { 0, 0, 0 };
            u32 dstMipLevel = 0;
            u32 dstLayer = 0;
            u32 dstLayerCount = 0;
            u32 srcSize = 0;
            u32 srcOffset = 0;
        };
        std::vector<StagedImageInfo> _pendingStagedImage;

        u32 _stagingSize;
        u32 _stagingOffset;
        vk::BufferHandle _stagingBuffer;

        Mesh* _cube;

        std::vector<vk::ImageHandle> _shadowMaps;
        u32 _shadowMapSize;
        vk::ImageHandle _shadowTarget;
        vk::Framebuffer* _shadowFramebuffer;

        std::vector<Material> _materials;

        void updateMaterialdata();

    };

}

#endif //CALA_ENGINE_H
