#include <Cala/Engine.h>
#include <Ende/filesystem/File.h>
#include <Cala/vulkan/ShaderProgram.h>
#include <Cala/shapes.h>
#include <Cala/Mesh.h>
#include <Ende/profile/profile.h>
#include <Cala/Material.h>

#include <json.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <stb_image_write.h>
#include <PoissonGenerator.h>

cala::vk::RenderPass::Attachment shadowPassAttachment {
        cala::vk::Format::D32_SFLOAT,
        VK_SAMPLE_COUNT_1_BIT,
        cala::vk::LoadOp::CLEAR,
        cala::vk::StoreOp::STORE,
        cala::vk::LoadOp::CLEAR,
        cala::vk::StoreOp::DONT_CARE,
        cala::vk::ImageLayout::UNDEFINED,
        cala::vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT,
        cala::vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT
};


cala::Engine::Engine(vk::Platform &platform)
    : _logger("Cala", {
        std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>(),
        std::make_shared<spdlog::sinks::basic_file_sink_mt>("debug.log", true)
      }),
      _device(vk::Device::create({
          .useTimeline = true,
          .platform = &platform,
          .logger = &_logger
      }).value()),
      _assetManager(this),
      _startTime(ende::time::SystemTime::now()),
      _shadowPass(*_device, {&shadowPassAttachment, 1 }),
      _lodSampler(_device->getSampler({
          .maxLod = 10
      })),
      _irradianceSampler(_device->getSampler({
          .filter = VK_FILTER_LINEAR,
          .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
      })),
      _vertexOffset(0),
      _indexOffset(0),
      _meshletOffset(0),
      _meshletIndexOffset(0),
      _primitiveOffset(0),
      _stagingSize(1 << 24), // 16mb
      _stagingOffset(0),
      _stagingBuffer(_device->createBuffer({
          .size = _stagingSize,
          .usage = vk::BufferUsage::TRANSFER_SRC,
          .memoryType = vk::MemoryProperties::STAGING,
          .persistentlyMapped = true,
          .name = "StagingBuffer"
      })),
      _shadowMapSize(0)
{
    spdlog::flush_every(std::chrono::seconds(5));
    _device->setBindlessSetIndex(0);
    _assetManager.setAssetPath("../../res");
    _pointShadowProgram = loadProgram("pointShadowProgram", {
        { "shaders/shadow/point_shadow.task", vk::ShaderStage::TASK },
        { "shaders/shadow/point_shadow.mesh", vk::ShaderStage::MESH },
        { "shaders/shadow_point.frag", vk::ShaderStage::FRAGMENT }
    });
    _directShadowProgram = loadProgram("directShadowProgram", {
        { "shaders/shadow/shadow.task", vk::ShaderStage::TASK },
        { "shaders/shadow/direct_shadow.mesh", vk::ShaderStage::MESH }
    });
    {
        _equirectangularToCubeMap = loadProgram("equirectangularToCubeMap", {
            { "shaders/equirectangularToCubeMap.comp", vk::ShaderStage::COMPUTE }
        });
    }
    {
        _irradianceProgram = loadProgram("irradianceProgram", {
            { "shaders/irradiance.comp", vk::ShaderStage::COMPUTE }
        });
    }
    {
        _prefilterProgram = loadProgram("prefilterProgram", {
            { "shaders/prefilter.comp", vk::ShaderStage::COMPUTE }
        });
    }
    {
        _brdfProgram = loadProgram("brdfProgram", {
            { "shaders/brdf.comp", vk::ShaderStage::COMPUTE }
        });
    }
    {
        _skyboxProgram = loadProgram("skyboxProgram", {
            { "shaders/skybox.vert", vk::ShaderStage::VERTEX },
            { "shaders/skybox.frag", vk::ShaderStage::FRAGMENT }
        });
    }
    {
        _tonemapProgram = loadProgram("tonemapProgram", {
            { "shaders/hdr.comp", vk::ShaderStage::COMPUTE }
        });
    }
    {
        _cullProgram = loadProgram("cullProgram", {
            { "shaders/cull.comp", vk::ShaderStage::COMPUTE }
        });
    }
    _cullMeshShaderProgram = loadProgram("cullMeshShaderProgram", {
        { "shaders/cull_mesh_shader.comp", vk::ShaderStage::COMPUTE }
    });
    _pointShadowCullProgram = loadProgram("pointShadowCullProgram", {
        { "shaders/shadow/cull_point_shadow.comp", vk::ShaderStage::COMPUTE }
    });
    _directShadowCullProgram = loadProgram("directShadowCullProgram", {
        { "shaders/shadow/cull_direct_shadow.comp", vk::ShaderStage::COMPUTE }
    });
    {
        _createClustersProgram = loadProgram("createClustersProgram", {
            { "shaders/create_clusters.comp", vk::ShaderStage::COMPUTE }
        });
    }
    {
        _cullLightsProgram = loadProgram("cullLightsProgram", {
            { "shaders/cull_lights.comp", vk::ShaderStage::COMPUTE }
        });
    }
    _meshletDebugProgram = loadProgram("debugMeshletProgram", {
        { "shaders/default.task", vk::ShaderStage::TASK },
        { "shaders/default.mesh", vk::ShaderStage::MESH },
        { "shaders/debug/meshlet.frag", vk::ShaderStage::FRAGMENT }
    });
    {
        _clusterDebugProgram = loadProgram("clusterDebugProgram", {
            { "shaders/fullscreen.vert", vk::ShaderStage::VERTEX },
            { "shaders/debug/clusters_debug.frag", vk::ShaderStage::FRAGMENT }
        });
    }
    _worldPosDebugProgram = loadProgram("worldPosDebugProgram", {
            { "shaders/default.task", vk::ShaderStage::TASK },
            { "shaders/default.mesh", vk::ShaderStage::MESH },
            { "shaders/debug/world_pos.frag", vk::ShaderStage::FRAGMENT }
    });
    {
        _frustumDebugProgram = loadProgram("frustumDebugProgram", {
            { "shaders/debug/debug_frustum.vert", vk::ShaderStage::VERTEX },
            { "shaders/solid_colour.frag", vk::ShaderStage::FRAGMENT }
        });
    }
    {
        _solidColourProgram = loadProgram("solidColourProgram", {
            { "shaders/default.vert", vk::ShaderStage::VERTEX },
            { "shaders/solid_colour.frag", vk::ShaderStage::FRAGMENT }
        });
    }
    {
        _normalsDebugProgram = loadProgram("normalsDebugProgram", {
            { "shaders/default.vert", vk::ShaderStage::VERTEX },
            { "shaders/normals.geom", vk::ShaderStage::GEOMETRY },
            { "shaders/solid_colour.frag", vk::ShaderStage::FRAGMENT }
        });
    }
    {
        _depthDebugProgram = loadProgram("depthDebugProgram", {
                { "shaders/fullscreen.vert", vk::ShaderStage::VERTEX },
                { "shaders/debug/debug_depth.frag", vk::ShaderStage::FRAGMENT }
        });
    }
    _bloomDownsampleProgram = loadProgram("bloomDownsampleProgram", {
        { "shaders/bloom_downsample.comp", vk::ShaderStage::COMPUTE }
    });
    _bloomUpsampleProgram = loadProgram("bloomUpsampleProgram", {
        { "shaders/bloom_upsample.comp", vk::ShaderStage::COMPUTE }
    });
    _bloomCompositeProgram = loadProgram("bloomCompositeProgram", {
        { "shaders/bloom_composite.comp", vk::ShaderStage::COMPUTE }
    });
    {
//        _voxelVisualisationProgram = loadProgram({
////            { "shaders/fullscreen.vert", vk::ShaderModule::VERTEX },
////            { "shaders/voxel/visualise.frag", vk::ShaderModule::FRAGMENT }
//            { "shaders/voxel/visualise.comp", vk::ShaderModule::COMPUTE }
//        });
    }

    PoissonGenerator::DefaultPRNG defaultPrng;
    auto poissonPoints = PoissonGenerator::generatePoissonPoints(16, defaultPrng);
    std::vector<f32> poissonData;
    for (auto& point : poissonPoints) {
        poissonData.push_back(point.x);
        poissonData.push_back(point.y);
    }

    _poissonImage = _device->createImage({
        .width = static_cast<u32>(poissonPoints.size()),
        .format = vk::Format::RG32_SFLOAT,
        .usage = vk::ImageUsage::SAMPLED | vk::ImageUsage::TRANSFER_DST,
        .name = "poisson"
    });
    stageData(_poissonImage, poissonData, {
        .mipLevel = 0,
        .width = static_cast<u32>(poissonPoints.size()),
        .format = 2 * 4
    });

    _brdfImage = _device->createImage({
        .width = 512,
        .height = 512,
        .depth = 1,
        .format = vk::Format::RG16_SFLOAT,
        .mipLevels = 1,
        .arrayLayers = 1,
        .usage = vk::ImageUsage::SAMPLED | vk::ImageUsage::STORAGE,
        .name = "brdf"
    });

    _device->immediate([&](vk::CommandHandle cmd) {
        auto brdfBarrier = _brdfImage->barrier(vk::PipelineStage::TOP, vk::PipelineStage::COMPUTE_SHADER, vk::Access::NONE, vk::Access::SHADER_WRITE | vk::Access::SHADER_READ, vk::ImageLayout::GENERAL);
        cmd->pipelineBarrier({ &brdfBarrier, 1 });

        cmd->bindProgram(_brdfProgram);
        cmd->bindImage(1, 0, _brdfImage->defaultView());
        cmd->bindPipeline();
        cmd->bindDescriptors();
        cmd->dispatch(512, 512, 1);

        brdfBarrier = _brdfImage->barrier(vk::PipelineStage::COMPUTE_SHADER, vk::PipelineStage::FRAGMENT_SHADER, vk::Access::SHADER_READ | vk::Access::SHADER_WRITE, vk::Access::SHADER_READ, vk::ImageLayout::SHADER_READ_ONLY);
        cmd->pipelineBarrier({ &brdfBarrier, 1 });

    });

    _globalVertexBuffer = _device->createBuffer({
        .size = 1000000,
        .usage = vk::BufferUsage::VERTEX | vk::BufferUsage::TRANSFER_DST,
        .memoryType = vk::MemoryProperties::DEVICE,
        .name = "globalVertexBuffer"
    });
    _globalIndexBuffer = _device->createBuffer({
        .size = 1000000,
        .usage = vk::BufferUsage::INDEX | vk::BufferUsage::TRANSFER_DST,
        .memoryType = vk::MemoryProperties::DEVICE,
        .name = "globalIndexBuffer"
    });

    _globalMeshletBuffer = _device->createBuffer({
        .size = 1000000,
        .usage = vk::BufferUsage::STORAGE | vk::BufferUsage::TRANSFER_DST,
        .memoryType = vk::MemoryProperties::DEVICE,
        .name = "globalMeshletBuffer"
    });

    _globalMeshletIndexBuffer = _device->createBuffer({
        .size = 1000000,
        .usage = vk::BufferUsage::STORAGE | vk::BufferUsage::TRANSFER_DST,
        .memoryType = vk::MemoryProperties::DEVICE,
        .name = "globalMeshletIndexBuffer"
    });

    _globalPrimitiveBuffer = _device->createBuffer({
        .size = 1000000,
        .usage = vk::BufferUsage::STORAGE | vk::BufferUsage::TRANSFER_DST,
        .memoryType = vk::MemoryProperties::DEVICE,
        .name = "globalPrimitiveBuffer"
    });

    _cube = new Mesh(shapes::cube().mesh(this));

    _materials.reserve(10);

    setShadowMapSize(1024);

}

cala::Engine::~Engine() {
    _assetManager.clear();
    _pointShadowProgram = {};
    _directShadowProgram = {};

    _equirectangularToCubeMap = {};
    _irradianceProgram = {};
    _prefilterProgram = {};
    _brdfProgram = {};
    _skyboxProgram = {};
    _tonemapProgram = {};
    _cullProgram = {};
    _pointShadowCullProgram = {};
    _directShadowCullProgram = {};
    _createClustersProgram = {};
    _cullLightsProgram = {};

    _clusterDebugProgram = {};
    _normalsDebugProgram = {};
    _worldPosDebugProgram = {};
    _frustumDebugProgram = {};
    _solidColourProgram = {};
    _depthDebugProgram = {};
    _voxelVisualisationProgram = {};

    _lodSampler = {};
    _irradianceSampler = {};

    _globalIndexBuffer = {};
    _globalVertexBuffer = {};

    _brdfImage = {};

    _stagingBuffer = {};
    _shadowMaps.clear();
    delete _cube;
}

bool cala::Engine::gc() {
    PROFILE_NAMED("Engine::gc");
    flushStagedData();

    return _device->gc();
}

cala::vk::ImageHandle cala::Engine::convertToCubeMap(vk::ImageHandle equirectangular) {
    auto cubeMap = _device->createImage({
        .width = 512,
        .height = 512,
        .depth = 1,
        .format = vk::Format::RGBA32_SFLOAT,
        .mipLevels = 10,
        .arrayLayers = 6,
        .usage = vk::ImageUsage::STORAGE | vk::ImageUsage::SAMPLED | vk::ImageUsage::TRANSFER_SRC | vk::ImageUsage::TRANSFER_DST,
        .name = "cubeMap"
    });
    auto equirectangularView = equirectangular->newView();
    auto cubeView = cubeMap->newView(0, 10);
    _device->immediate([&](vk::CommandHandle cmd) {
        auto envBarrier = cubeMap->barrier(vk::PipelineStage::TOP, vk::PipelineStage::COMPUTE_SHADER, vk::Access::NONE, vk::Access::SHADER_WRITE, vk::ImageLayout::GENERAL);
        cmd->pipelineBarrier({ &envBarrier, 1 });
//        auto hdrBarrier = equirectangular->barrier(vk::Access::NONE, vk::Access::SHADER_READ, vk::ImageLayout::SHADER_READ_ONLY, vk::ImageLayout::SHADER_READ_ONLY);
//        cmd.pipelineBarrier(vk::PipelineStage::TOP, vk::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &hdrBarrier, 1 });


        cmd->bindProgram(_equirectangularToCubeMap);
        cmd->bindImage(1, 0, equirectangularView, _lodSampler);
        cmd->bindImage(1, 1, cubeView);

        cmd->bindPipeline();
        cmd->bindDescriptors();

        cmd->dispatch(512, 512, 6);


        envBarrier = cubeMap->barrier(vk::PipelineStage::COMPUTE_SHADER, vk::PipelineStage::BOTTOM, vk::Access::SHADER_WRITE, vk::Access::NONE, vk::ImageLayout::TRANSFER_DST);
        cmd->pipelineBarrier({ &envBarrier, 1 });
        cubeMap->generateMips(cmd);
//        envBarrier = cubeMap->barrier(vk::Access::NONE, vk::Access::SHADER_READ, vk::ImageLayout::TRANSFER_DST, vk::ImageLayout::SHADER_READ_ONLY);
//        cmd.pipelineBarrier(vk::PipelineStage::TOP, vk::PipelineStage::FRAGMENT_SHADER, 0, nullptr, { &envBarrier, 1 });
    });
    return cubeMap;
}

cala::vk::ImageHandle cala::Engine::generateIrradianceMap(vk::ImageHandle cubeMap) {
    auto irradianceMap = _device->createImage({
        .width = 64,
        .height = 64,
        .depth = 1,
        .format = vk::Format::RGBA32_SFLOAT,
        .mipLevels = 1,
        .arrayLayers = 6,
        .usage = vk::ImageUsage::STORAGE | vk::ImageUsage::SAMPLED | vk::ImageUsage::TRANSFER_DST,
        .name = "irradianceMap"
    });
    _device->immediate([&](vk::CommandHandle cmd) {
        auto irradianceBarrier = irradianceMap->barrier(vk::PipelineStage::TOP, vk::PipelineStage::COMPUTE_SHADER, vk::Access::NONE, vk::Access::SHADER_WRITE, vk::ImageLayout::GENERAL);
        cmd->pipelineBarrier({ &irradianceBarrier, 1 });
        auto cubeBarrier = cubeMap->barrier(vk::PipelineStage::TOP, vk::PipelineStage::COMPUTE_SHADER, vk::Access::NONE, vk::Access::SHADER_READ, vk::ImageLayout::GENERAL);
        cmd->pipelineBarrier({ &cubeBarrier, 1 });

        cmd->bindProgram(_irradianceProgram);
        cmd->bindImage(1, 0, cubeMap);
        cmd->bindImage(1, 1, irradianceMap);
        cmd->bindPipeline();
        cmd->bindDescriptors();
        cmd->dispatch(irradianceMap->width(), irradianceMap->height(), 6);

        irradianceBarrier = irradianceMap->barrier(vk::PipelineStage::COMPUTE_SHADER, vk::PipelineStage::FRAGMENT_SHADER, vk::Access::SHADER_WRITE, vk::Access::SHADER_READ, vk::ImageLayout::SHADER_READ_ONLY);
        cmd->pipelineBarrier({ &irradianceBarrier, 1 });
        cubeBarrier = cubeMap->barrier(vk::PipelineStage::COMPUTE_SHADER, vk::PipelineStage::FRAGMENT_SHADER, vk::Access::SHADER_READ, vk::Access::SHADER_READ, vk::ImageLayout::SHADER_READ_ONLY);
        cmd->pipelineBarrier({ &cubeBarrier, 1 });
    });
    return irradianceMap;
}

cala::vk::ImageHandle cala::Engine::generatePrefilteredIrradiance(vk::ImageHandle cubeMap) {
    vk::ImageHandle prefilteredMap = _device->createImage({
        .width = 512,
        .height = 512,
        .depth = 1,
        .format = vk::Format::RGBA32_SFLOAT,
        .mipLevels = 10,
        .arrayLayers = 6,
        .usage = vk::ImageUsage::STORAGE | vk::ImageUsage::SAMPLED | vk::ImageUsage::TRANSFER_DST,
        .name = "prefilterMap"
    });
    vk::Image::View mipViews[10] = {
            prefilteredMap->newView(0),
            prefilteredMap->newView(1),
            prefilteredMap->newView(2),
            prefilteredMap->newView(3),
            prefilteredMap->newView(4),
            prefilteredMap->newView(5),
            prefilteredMap->newView(6),
            prefilteredMap->newView(7),
            prefilteredMap->newView(8),
            prefilteredMap->newView(9)
    };


    _device->immediate([&](vk::CommandHandle cmd) {
        auto prefilterBarrier = prefilteredMap->barrier(vk::PipelineStage::TOP, vk::PipelineStage::COMPUTE_SHADER, vk::Access::NONE, vk::Access::SHADER_WRITE, vk::ImageLayout::GENERAL);
        cmd->pipelineBarrier({ &prefilterBarrier, 1 });

        for (u32 mip = 0; mip < prefilteredMap->mips(); mip++) {
            cmd->bindProgram(_prefilterProgram);
            cmd->bindImage(1, 0, cubeMap, _lodSampler);
            cmd->bindImage(1, 1, mipViews[mip]);
            f32 roughness = (f32)mip / (f32)prefilteredMap->mips();
            cmd->pushConstants(vk::ShaderStage::COMPUTE, roughness);
            cmd->bindPipeline();
            cmd->bindDescriptors();
            f32 computeDim = 512.f * std::pow(0.5, mip);
            cmd->dispatch(computeDim, computeDim, 6);
        }

        prefilterBarrier = prefilteredMap->barrier(vk::PipelineStage::COMPUTE_SHADER, vk::PipelineStage::BOTTOM, vk::Access::SHADER_WRITE, vk::Access::NONE, vk::ImageLayout::SHADER_READ_ONLY);
        cmd->pipelineBarrier({ &prefilterBarrier, 1 });
    });
    return prefilteredMap;
}


cala::vk::ImageHandle cala::Engine::getShadowMap(u32 index, bool point, cala::vk::CommandHandle cmd) {
    if (index < _shadowMaps.size() && _shadowMaps[index]) {
        if (!point && _shadowMaps[index]->layers() != 6)
            return _shadowMaps[index];
        if (point && _shadowMaps[index]->layers() == 6)
            return _shadowMaps[index];
    }

    auto map = _device->createImage({
        .width = _shadowMapSize,
        .height = _shadowMapSize,
        .depth = 1,
        .format = vk::Format::D32_SFLOAT,
        .mipLevels = 1,
        .arrayLayers = point ? (u32)6 : (u32)1,
        .usage = vk::ImageUsage::SAMPLED | vk::ImageUsage::DEPTH_STENCIL_ATTACHMENT | vk::ImageUsage::TRANSFER_DST,
        .name = "ShadowMap: " + std::to_string(index)
    });
    if (index < _shadowMaps.size())
        _shadowMaps[index] = map;
    else
        _shadowMaps.push_back(map);
    if (cmd) {
        auto cubeBarrier = map->barrier(vk::PipelineStage::TOP, vk::PipelineStage::TRANSFER, vk::Access::NONE, vk::Access::TRANSFER_WRITE, vk::ImageLayout::SHADER_READ_ONLY);
        cmd->pipelineBarrier({ &cubeBarrier, 1 });
    } else {
        _device->immediate([&](vk::CommandHandle cmd) {
            auto cubeBarrier = map->barrier(vk::PipelineStage::TOP, vk::PipelineStage::TRANSFER, vk::Access::NONE, vk::Access::TRANSFER_WRITE, vk::ImageLayout::SHADER_READ_ONLY);
            cmd->pipelineBarrier({ &cubeBarrier, 1 });
        });
    }
    return map;
}

void cala::Engine::setShadowMapSize(u32 size) {
    if (_shadowMapSize == size)
        return;

    _shadowMapSize = size;
    _shadowMaps.clear();
    _shadowTarget = device().createImage({
        _shadowMapSize, _shadowMapSize, 1,
        vk::Format::D32_SFLOAT,
        1, 1,
        vk::ImageUsage::SAMPLED | vk::ImageUsage::DEPTH_STENCIL_ATTACHMENT | vk::ImageUsage::TRANSFER_SRC
    });
    device().immediate([&](vk::CommandHandle cmd) {
        auto targetBarrier = _shadowTarget->barrier(vk::PipelineStage::TOP, vk::PipelineStage::EARLY_FRAGMENT, vk::Access::NONE, vk::Access::DEPTH_STENCIL_WRITE | vk::Access::DEPTH_STENCIL_READ, vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
        cmd->pipelineBarrier({ &targetBarrier, 1 });
    });
    vk::RenderPass::Attachment shadowAttachment = {
            cala::vk::Format::D32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            vk::LoadOp::CLEAR,
            vk::StoreOp::STORE,
            vk::LoadOp::DONT_CARE,
            vk::StoreOp::DONT_CARE,
            vk::ImageLayout::UNDEFINED,
            vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT,
            vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT
    };
    auto shadowRenderPass = device().getRenderPass({&shadowAttachment, 1 });
    u32 h = _shadowTarget.index() * _shadowMapSize;
    _shadowFramebuffer = device().getFramebuffer(shadowRenderPass, { &_shadowTarget->defaultView().view, 1 }, { &h, 1 }, _shadowMapSize, _shadowMapSize);
}

cala::vk::Framebuffer *cala::Engine::getShadowFramebuffer() {
    vk::RenderPass::Attachment shadowAttachment = {
            cala::vk::Format::D32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            vk::LoadOp::CLEAR,
            vk::StoreOp::STORE,
            vk::LoadOp::DONT_CARE,
            vk::StoreOp::DONT_CARE,
            vk::ImageLayout::UNDEFINED,
            vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT,
            vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT
    };
    auto shadowRenderPass = device().getRenderPass({&shadowAttachment, 1 });
    u32 h = _shadowTarget.index() * _shadowMapSize;
    _shadowFramebuffer = device().getFramebuffer(shadowRenderPass, { &_shadowTarget->defaultView().view, 1 }, { &h, 1 }, _shadowMapSize, _shadowMapSize);
    return _shadowFramebuffer;
}

void cala::Engine::updateMaterialdata() {
    for (auto& material : _materials)
        material.upload();
}


u32 cala::Engine::uploadVertexData(std::span<f32> data) {
    u32 currentOffset = _vertexOffset;
    if (currentOffset + data.size() * sizeof(f32) >= _globalVertexBuffer->size()) {
        flushStagedData();
        _globalVertexBuffer = _device->resizeBuffer(_globalVertexBuffer, currentOffset + data.size() * sizeof(f32), true);
    }

    stageData(_globalVertexBuffer, std::span<const u8>(reinterpret_cast<const u8*>(data.data()), data.size() * sizeof(f32)), currentOffset);

    _vertexOffset += data.size() * sizeof(f32);
    return currentOffset;
}

u32 cala::Engine::uploadIndexData(std::span<u32> data) {
    u32 currentOffset = _indexOffset;
    if (currentOffset + data.size() * sizeof(u32) >= _globalIndexBuffer->size()) {
        flushStagedData();
        _globalIndexBuffer = _device->resizeBuffer(_globalIndexBuffer, currentOffset + data.size() * sizeof(u32), true);
    }
    stageData(_globalIndexBuffer, std::span<const u8>(reinterpret_cast<const u8*>(data.data()), data.size() * sizeof(f32)), currentOffset);

    _indexOffset += data.size() * sizeof(u32);
    return currentOffset;
}

u32 cala::Engine::uploadMeshletData(std::span<Meshlet> data) {
    u32 currentOffset = _meshletOffset;
    if (currentOffset + data.size() * sizeof(Meshlet) >= _globalMeshletBuffer->size()) {
        flushStagedData();
        _globalMeshletBuffer = _device->resizeBuffer(_globalMeshletBuffer, currentOffset + data.size() * sizeof(Meshlet), true);
    }
    stageData(_globalMeshletBuffer, data, currentOffset);
    _meshletOffset += data.size() * sizeof(Meshlet);
    return currentOffset;
}

u32 cala::Engine::uploadMeshletIndicesData(std::span<u32> data) {
    u32 currentOffset = _meshletIndexOffset;
    if (currentOffset + data.size() * sizeof(u32) >= _globalMeshletIndexBuffer->size()) {
        flushStagedData();
        _globalMeshletIndexBuffer = _device->resizeBuffer(_globalMeshletIndexBuffer, currentOffset + data.size() * sizeof(u32), true);
    }
    stageData(_globalMeshletIndexBuffer, data, currentOffset);
    _meshletIndexOffset += data.size() * sizeof(u32);
    return currentOffset;
}

u32 cala::Engine::uploadPrimitiveData(std::span<u8> data) {
    u32 currentOffset = _primitiveOffset;
    if (currentOffset + data.size() * sizeof(u8) >= _globalPrimitiveBuffer->size()) {
        flushStagedData();
        _globalPrimitiveBuffer = _device->resizeBuffer(_globalPrimitiveBuffer, currentOffset + data.size() * sizeof(u8), true);
    }
    stageData(_globalPrimitiveBuffer, data, currentOffset);
    _primitiveOffset += data.size() * sizeof(u8);
    return currentOffset;
}

void cala::Engine::stageData(vk::BufferHandle dstHandle, std::span<const u8> data, u32 dstOffset) {
    u32 uploadOffset = 0;
    u32 uploadSizeRemaining = data.size() - uploadOffset;

    while (uploadSizeRemaining > 0) {
        u32 availableSize = _stagingSize - _stagingOffset;
        u32 allocationSize = std::min(availableSize, uploadSizeRemaining);

        if (allocationSize > 0) {
            std::memcpy(static_cast<u8*>(_stagingBuffer->persistentMapping()) + _stagingOffset, data.data() + uploadOffset, allocationSize);

            _pendingStagedBuffer.push_back({
                dstHandle,
                dstOffset,
                allocationSize,
                _stagingOffset
            });

            _stagingOffset += allocationSize;
            uploadOffset += allocationSize;
            uploadSizeRemaining = data.size() - uploadOffset;

            dstOffset += allocationSize;
        } else {
            flushStagedData();
        }
    }
}

void cala::Engine::stageData(vk::ImageHandle dstHandle, std::span<const u8> data, vk::Image::DataInfo dataInfo) {
    u32 uploadOffset = 0;
    u32 uploadSizeRemaining = data.size() - uploadOffset;
    ende::math::Vec<3, i32> dstOffset = { 0, 0, 0 };

    //TODO: support loading 3d images

    while (uploadSizeRemaining > 0) {
        u32 availableSize = _stagingSize - _stagingOffset;
        u32 allocationSize = std::min(availableSize, uploadSizeRemaining);

        if (allocationSize > 0 && allocationSize >= dataInfo.format) {
            u32 allocIndex = allocationSize / dataInfo.format;
            u32 regionZ = 0;
            if (dataInfo.depth > 1) {
                regionZ = allocIndex / (dataInfo.width * dataInfo.height);
                allocIndex -= (regionZ * (dataInfo.width * dataInfo.height));
            }
            u32 regionY = allocIndex / dataInfo.width;

            // find square copy region
            // get space remaining on current line. if this doesnt cover the whole line then only fill line
            u32 rWidth = std::min(dataInfo.width - dstOffset.x(), allocIndex);
            u32 rHeight = regionY;
            if (rWidth < dataInfo.width)
                rHeight = 1;


            u32 allocSize = rWidth * rHeight * dataInfo.format;
            allocationSize = allocSize;

            u32 remainder = _stagingOffset % dataInfo.format;

            u32 offset = _stagingOffset;
            if (remainder != 0)
                offset += (dataInfo.format - remainder);

            std::memcpy(static_cast<u8*>(_stagingBuffer->persistentMapping()) + offset, data.data() + uploadOffset, allocationSize);

            _pendingStagedImage.push_back({
                dstHandle,
                { rWidth, rHeight, dataInfo.depth },
                dstOffset,
                dataInfo.mipLevel,
                dataInfo.layer,
                1,
                allocationSize,
                offset
            });

            _stagingOffset = offset + allocationSize;
            uploadOffset += allocationSize;
            uploadSizeRemaining = data.size() - uploadOffset;

            i32 index = (data.size() - uploadSizeRemaining) / dataInfo.format;
            i32 z = 0;
            if (dataInfo.depth > 1) {
                z = index / (dataInfo.width * dataInfo.height);
                index -= (z * (dataInfo.width * dataInfo.height));
            }
            i32 y = 0;
            if (dataInfo.height > 1)
                y = index / dataInfo.width;
            i32 x = index % dataInfo.width;

            dstOffset = { x, y, z };

        } else {
            flushStagedData();
        }
    }
}

u32 cala::Engine::flushStagedData() {
    PROFILE_NAMED("Engine::flushStagedData");
    u32 bytesUploaded = 0;
    if (!_pendingStagedBuffer.empty() || !_pendingStagedImage.empty()) {
        _device->immediate([&](vk::CommandHandle cmd) {
            for (auto& staged : _pendingStagedBuffer) {
                VkBufferCopy  bufferCopy{};
                bufferCopy.dstOffset = staged.dstOffset;
                bufferCopy.srcOffset = staged.srcOffset;
                bufferCopy.size = staged.srcSize;
                assert(staged.dstBuffer->size() >= bufferCopy.dstOffset + bufferCopy.size);
                vkCmdCopyBuffer(cmd->buffer(), _stagingBuffer->buffer(), staged.dstBuffer->buffer(), 1, &bufferCopy);
                bytesUploaded += staged.srcSize;
            }
            for (auto& staged : _pendingStagedImage) {
                auto barrier = staged.dstImage->barrier(vk::PipelineStage::TOP, vk::PipelineStage::TRANSFER, vk::Access::NONE, vk::Access::TRANSFER_WRITE, vk::ImageLayout::TRANSFER_DST);
                cmd->pipelineBarrier({ &barrier, 1 });

                VkBufferImageCopy imageCopy{};
                imageCopy.bufferOffset = staged.srcOffset;
                imageCopy.bufferRowLength = 0;
                imageCopy.bufferImageHeight = 0;

                imageCopy.imageSubresource.aspectMask = vk::isDepthFormat(staged.dstImage->format()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                imageCopy.imageSubresource.mipLevel = staged.dstMipLevel;
                imageCopy.imageSubresource.baseArrayLayer = staged.dstLayer;
                imageCopy.imageSubresource.layerCount = staged.dstLayerCount;
                imageCopy.imageExtent.width = staged.dstDimensions.x();
                imageCopy.imageExtent.height = staged.dstDimensions.y();
                imageCopy.imageExtent.depth = staged.dstDimensions.z();
                imageCopy.imageOffset.x = staged.dstOffset.x();
                imageCopy.imageOffset.y = staged.dstOffset.y();
                imageCopy.imageOffset.z = staged.dstOffset.z();

                vkCmdCopyBufferToImage(cmd->buffer(), _stagingBuffer->buffer(), staged.dstImage->image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

                barrier = staged.dstImage->barrier(vk::PipelineStage::TRANSFER, vk::PipelineStage::FRAGMENT_SHADER | vk::PipelineStage::COMPUTE_SHADER, vk::Access::TRANSFER_WRITE, vk::Access::SHADER_READ, vk::ImageLayout::SHADER_READ_ONLY);
                cmd->pipelineBarrier({ &barrier, 1 });
                bytesUploaded += staged.srcSize;
            }
        });
        _pendingStagedBuffer.clear();
        _pendingStagedImage.clear();
        _stagingOffset = 0;
    }
    _device->increaseDataUploadCount(bytesUploaded);
    return bytesUploaded;
}



cala::Material *cala::Engine::createMaterial(u32 size) {
    u32 id = _materials.size();
    _materials.emplace_back(this, id, size);
    return &_materials.back();
}

// string or path
std::optional<std::pair<std::string, bool>> loadMaterialString(nlohmann::json& json, std::string_view label, const std::filesystem::path& root = {}) {
    auto it = json.find(label);
    if (it == json.end())
        return {};
    if (it->is_object()) {
        auto path = it->find("path");
        if (path == it.value().end())
            return {};
        return std::make_pair(path->get<std::string>(), true);
//        ende::fs::File file;
//        if (!file.open(root / "shaders" / path->get<std::string>()))
//            return {};
//        return std::make_pair(file.read(), true);
    }
    return std::make_pair(it.value(), false);
}

std::optional<cala::vk::CommandBuffer::RasterState> loadMaterialRasterState(nlohmann::json& json) {
    auto it = json.find("raster_state");
    if (it == json.end() || !it->is_object())
        return {};
    auto cullModeIt = it->find("cullMode");
    auto frontFaceIt = it->find("frontFace");
    auto polygonModeIt = it->find("polygonMode");
    auto lineWidthIt = it->find("lineWidth");
    auto depthClampIt = it->find("depthClamp");
    auto rasterDiscardIt = it->find("rasterDiscard");
    auto depthBiasIt = it->find("depthBias");

    cala::vk::CommandBuffer::RasterState state{};

    if (cullModeIt != it->end() && cullModeIt->is_string()) {
        std::string mode = cullModeIt->get<std::string>();
        if (mode == "NONE")
            state.cullMode = cala::vk::CullMode::NONE;
        else if (mode == "FRONT")
            state.cullMode = cala::vk::CullMode::FRONT;
        else if (mode == "BACK")
            state.cullMode = cala::vk::CullMode::BACK;
        else if (mode == "FRONT_BACK")
            state.cullMode = cala::vk::CullMode::FRONT_BACK;
    }
    if (frontFaceIt != it->end() && frontFaceIt->is_string()) {
        std::string face = frontFaceIt->get<std::string>();
        if (face == "CCW")
            state.frontFace = cala::vk::FrontFace::CCW;
        else if (face == "CW")
            state.frontFace = cala::vk::FrontFace::CW;
    }
    if (polygonModeIt != it->end() && polygonModeIt->is_string()) {
        std::string mode = polygonModeIt->get<std::string>();
        if (mode == "FILL")
            state.polygonMode = cala::vk::PolygonMode::FILL;
        else if (mode == "LINE")
            state.polygonMode = cala::vk::PolygonMode::LINE;
        else if (mode == "POINT")
            state.polygonMode = cala::vk::PolygonMode::POINT;
    }
    if (lineWidthIt != it->end() && lineWidthIt->is_number_float())
        state.lineWidth = lineWidthIt->get<f32>();
    if (depthClampIt != it->end() && depthClampIt->is_boolean())
        state.depthClamp = depthClampIt->get<bool>();
    if (rasterDiscardIt != it->end() && rasterDiscardIt->is_boolean())
        state.rasterDiscard = rasterDiscardIt->get<bool>();
    if (depthBiasIt != it->end() && depthBiasIt->is_boolean())
        state.depthBias = depthBiasIt->get<bool>();

    return state;
}

std::optional<cala::vk::CommandBuffer::DepthState> loadMaterialDepthState(nlohmann::json& json) {
    auto it = json.find("depth_state");
    if (it == json.end() || !it->is_object())
        return {};
    auto testIt = it->find("test");
    auto writeIt = it->find("write");
    auto compareOpIt = it->find("compareOp");

    cala::vk::CommandBuffer::DepthState state{};

    if (testIt != it->end() && testIt->is_boolean())
        state.test = testIt->get<bool>();
    if (writeIt != it->end() && writeIt->is_boolean())
        state.write = writeIt->get<bool>();
    if (compareOpIt != it->end() && compareOpIt->is_string()) {
        std::string comp = compareOpIt->get<std::string>();
        if (comp == "NEVER")
            state.compareOp = cala::vk::CompareOp::NEVER;
        else if (comp == "LESS")
            state.compareOp = cala::vk::CompareOp::LESS;
        else if (comp == "EQUAL")
            state.compareOp = cala::vk::CompareOp::EQUAL;
        else if (comp == "LESS_EQUAL")
            state.compareOp = cala::vk::CompareOp::LESS_EQUAL;
        else if (comp == "GREATER")
            state.compareOp = cala::vk::CompareOp::GREATER;
        else if (comp == "NOT_EQUAL")
            state.compareOp = cala::vk::CompareOp::NOT_EQUAL;
        else if (comp == "GREATER_EQUAL")
            state.compareOp = cala::vk::CompareOp::GREATER_EQUAL;
        else if (comp == "ALWAYS")
            state.compareOp = cala::vk::CompareOp::ALWAYS;
    }
    return state;
}

std::optional<cala::vk::CommandBuffer::BlendState> loadMaterialBlendState(nlohmann::json& json) {
    auto it = json.find("blend_state");
    if (it == json.end() || !it->is_object())
        return {};
    auto blendIt = it->find("blend");
    auto srcFactorIt = it->find("srcFactor");
    auto dstFactorIt = it->find("dstFactor");

    cala::vk::CommandBuffer::BlendState state;

    if (blendIt != it->end() && blendIt->is_boolean())
        state.blend = blendIt->get<bool>();
    if (srcFactorIt != it->end() && srcFactorIt->is_string()) {
        std::string factor = srcFactorIt->get<std::string>();
        if (factor == "ZERO")
            state.srcFactor = cala::vk::BlendFactor::ZERO;
        else if (factor == "ONE")
            state.srcFactor = cala::vk::BlendFactor::ONE;
        else if (factor == "SRC_COLOUR")
            state.srcFactor = cala::vk::BlendFactor::SRC_COLOUR;
        else if (factor == "ONE_MINUS_SRC_COLOUR")
            state.srcFactor = cala::vk::BlendFactor::ONE_MINUS_SRC_COLOUR;
        else if (factor == "DST_COLOUR")
            state.srcFactor = cala::vk::BlendFactor::DST_COLOUR;
        else if (factor == "ONE_MINUS_DST_COLOUR")
            state.srcFactor = cala::vk::BlendFactor::ONE_MINUS_DST_COLOUR;
        else if (factor == "SRC_ALPHA")
            state.srcFactor = cala::vk::BlendFactor::SRC_ALPHA;
        else if (factor == "ONE_MINUS_SRC_ALPHA")
            state.srcFactor = cala::vk::BlendFactor::ONE_MINUS_SRC_ALPHA;
        else if (factor == "DST_ALPHA")
            state.srcFactor = cala::vk::BlendFactor::DST_ALPHA;
        else if (factor == "ONE_MINUS_DST_ALPHA")
            state.srcFactor = cala::vk::BlendFactor::ONE_MINUS_DST_ALPHA;
        else if (factor == "CONSTANT_COLOUR")
            state.srcFactor = cala::vk::BlendFactor::CONSTANT_COLOUR;
        else if (factor == "ONE_MINUS_CONSTANT_COLOUR")
            state.srcFactor = cala::vk::BlendFactor::ONE_MINUS_CONSTANT_COLOUR;
        else if (factor == "CONSTANT_ALPHA")
            state.srcFactor = cala::vk::BlendFactor::CONSTANT_ALPHA;
        else if (factor == "ONE_MINUS_CONSTANT_ALPHA")
            state.srcFactor = cala::vk::BlendFactor::ONE_MINUS_CONSTANT_ALPHA;
        else if (factor == "SRC_ALPHA_SATURATE")
            state.srcFactor = cala::vk::BlendFactor::SRC_ALPHA_SATURATE;
        else if (factor == "SRC1_COLOUR")
            state.srcFactor = cala::vk::BlendFactor::SRC1_COLOUR;
        else if (factor == "ONE_MINUS_SRC1_COLOUR")
            state.srcFactor = cala::vk::BlendFactor::ONE_MINUS_SRC1_COLOUR;
        else if (factor == "SRC1_ALPHA")
            state.srcFactor = cala::vk::BlendFactor::SRC1_ALPHA;
        else if (factor == "ONE_MINUS_SRC1_ALPHA")
            state.srcFactor = cala::vk::BlendFactor::ONE_MINUS_SRC1_ALPHA;
    }
    if (dstFactorIt != it->end() && dstFactorIt->is_string()) {
        std::string factor = dstFactorIt->get<std::string>();
        if (factor == "ZERO")
            state.dstFactor = cala::vk::BlendFactor::ZERO;
        else if (factor == "ONE")
            state.dstFactor = cala::vk::BlendFactor::ONE;
        else if (factor == "SRC_COLOUR")
            state.dstFactor = cala::vk::BlendFactor::SRC_COLOUR;
        else if (factor == "ONE_MINUS_SRC_COLOUR")
            state.dstFactor = cala::vk::BlendFactor::ONE_MINUS_SRC_COLOUR;
        else if (factor == "DST_COLOUR")
            state.dstFactor = cala::vk::BlendFactor::DST_COLOUR;
        else if (factor == "ONE_MINUS_DST_COLOUR")
            state.dstFactor = cala::vk::BlendFactor::ONE_MINUS_DST_COLOUR;
        else if (factor == "SRC_ALPHA")
            state.dstFactor = cala::vk::BlendFactor::SRC_ALPHA;
        else if (factor == "ONE_MINUS_SRC_ALPHA")
            state.dstFactor = cala::vk::BlendFactor::ONE_MINUS_SRC_ALPHA;
        else if (factor == "DST_ALPHA")
            state.dstFactor = cala::vk::BlendFactor::DST_ALPHA;
        else if (factor == "ONE_MINUS_DST_ALPHA")
            state.dstFactor = cala::vk::BlendFactor::ONE_MINUS_DST_ALPHA;
        else if (factor == "CONSTANT_COLOUR")
            state.dstFactor = cala::vk::BlendFactor::CONSTANT_COLOUR;
        else if (factor == "ONE_MINUS_CONSTANT_COLOUR")
            state.dstFactor = cala::vk::BlendFactor::ONE_MINUS_CONSTANT_COLOUR;
        else if (factor == "CONSTANT_ALPHA")
            state.dstFactor = cala::vk::BlendFactor::CONSTANT_ALPHA;
        else if (factor == "ONE_MINUS_CONSTANT_ALPHA")
            state.dstFactor = cala::vk::BlendFactor::ONE_MINUS_CONSTANT_ALPHA;
        else if (factor == "SRC_ALPHA_SATURATE")
            state.dstFactor = cala::vk::BlendFactor::SRC_ALPHA_SATURATE;
        else if (factor == "SRC1_COLOUR")
            state.dstFactor = cala::vk::BlendFactor::SRC1_COLOUR;
        else if (factor == "ONE_MINUS_SRC1_COLOUR")
            state.dstFactor = cala::vk::BlendFactor::ONE_MINUS_SRC1_COLOUR;
        else if (factor == "SRC1_ALPHA")
            state.dstFactor = cala::vk::BlendFactor::SRC1_ALPHA;
        else if (factor == "ONE_MINUS_SRC1_ALPHA")
            state.dstFactor = cala::vk::BlendFactor::ONE_MINUS_SRC1_ALPHA;
    }
    return state;
}

cala::Material *cala::Engine::loadMaterial(const std::filesystem::path &path, u32 size) {
    try {

        ende::fs::File file;
        if (!file.open(path, ende::fs::in | ende::fs::binary))
            return nullptr;

        std::string source = file.read();

        nlohmann::json materialSource = nlohmann::json::parse(source);

        Material* material = createMaterial(size);

        auto materialData = loadMaterialString(materialSource, "materialData", _assetManager.getAssetPath()).value();
        auto materialDefinition = loadMaterialString(materialSource, "materialDefinition", _assetManager.getAssetPath()).value();
        auto materialLoad = loadMaterialString(materialSource, "materialLoad", _assetManager.getAssetPath()).value();
        auto litEval = loadMaterialString(materialSource, "lit", _assetManager.getAssetPath()).value();
        auto unlitEval = loadMaterialString(materialSource, "unlit", _assetManager.getAssetPath()).value();
        auto normalEval = loadMaterialString(materialSource, "normal", _assetManager.getAssetPath()).value();
        auto roughnessEval = loadMaterialString(materialSource, "roughness", _assetManager.getAssetPath()).value();
        auto metallicEval = loadMaterialString(materialSource, "metallic", _assetManager.getAssetPath()).value();
        auto voxelizeEval = loadMaterialString(materialSource, "voxelize", _assetManager.getAssetPath()).value();

        std::vector<std::string> includes;
        {
            auto it = materialSource.find("includes");
            if (it !=  materialSource.end() && it->is_array()) {
                for (auto i = it->begin(); i != it->end(); i++) {
                    includes.push_back(*i);
                }
            }
        }

        auto rasterState = loadMaterialRasterState(materialSource);
        if (rasterState)
            material->setRasterState(rasterState.value());
        auto depthState = loadMaterialDepthState(materialSource);
        if (depthState)
            material->setDepthState(depthState.value());
        auto blendState = loadMaterialBlendState(materialSource);
        if (blendState)
            material->setBlendState(blendState.value());

        auto addVariant = [&](const std::string& name, auto eval) {

            auto includedFiles = includes;
            if (materialData.second)
                includedFiles.push_back(materialData.first);
            if (materialDefinition.second)
                includedFiles.push_back(materialDefinition.first);
            if (materialLoad.second)
                includedFiles.push_back(materialLoad.first);
            if (eval.second)
                includedFiles.push_back(eval.first);



            if (_device->context().getEnabledFeatures().meshShading) {
                return loadProgram(name, {
                        { "shaders/default.task", vk::ShaderStage::TASK },
                        { "shaders/default.mesh", vk::ShaderStage::MESH },
                        { "shaders/default/default.frag", vk::ShaderStage::FRAGMENT, {
                            { "MATERIAL_DATA", materialData.second ? "" : materialData.first },
                            { "MATERIAL_DEFINITION", materialDefinition.second ? "" : materialDefinition.first },
                            { "MATERIAL_LOAD", materialLoad.second ? "" : materialLoad.first },
                            { "MATERIAL_EVAL", eval.second ? "" : eval.first },
                        }, includedFiles }
                });
            } else {
                return loadProgram(name, {
                        { "shaders/default.vert", vk::ShaderStage::VERTEX },
                        { "shaders/default/default.frag", vk::ShaderStage::FRAGMENT, {
                            { "MATERIAL_DATA", materialData.second ? "" : materialData.first },
                            { "MATERIAL_DEFINITION", materialDefinition.second ? "" : materialDefinition.first },
                            { "MATERIAL_LOAD", materialLoad.second ? "" : materialLoad.first },
                            { "MATERIAL_EVAL", eval.second ? "" : eval.first },
                        }, includedFiles }
                });
            }
        };

        vk::ShaderProgram litHandle = addVariant(std::format("{}##lit", path.filename().string()), litEval);
        material->setVariant(Material::Variant::LIT, std::move(litHandle));

        vk::ShaderProgram unlitHandle = addVariant(std::format("{}##unlit", path.filename().string()), unlitEval);
        material->setVariant(Material::Variant::UNLIT, std::move(unlitHandle));

        vk::ShaderProgram normalsHandle = addVariant(std::format("{}##normal", path.filename().string()), normalEval);
        material->setVariant(Material::Variant::NORMAL, std::move(normalsHandle));

        vk::ShaderProgram metallicHandle = addVariant(std::format("{}##metallic", path.filename().string()), metallicEval);
        material->setVariant(Material::Variant::METALLIC, std::move(metallicHandle));

        vk::ShaderProgram roughnessHandle = addVariant(std::format("{}##roughness", path.filename().string()), roughnessEval);
        material->setVariant(Material::Variant::ROUGHNESS, std::move(roughnessHandle));

        material->build();

        return material;
    } catch (std::exception& e) {
        _device->logger().error("Error with material: {}, {}", path.string(), e.what());
        return nullptr;
    }
}

cala::vk::ShaderProgram cala::Engine::loadProgram(const std::string& name, const std::vector<ShaderInfo>& shaderInfo) {
    std::vector<vk::ShaderModuleHandle> modules;

    for (auto& info : shaderInfo) {
        modules.push_back(_assetManager.loadShaderModule(name, info.path, info.stage, info.macros, info.includes, { 32, 32, 32 }));
    }

    return { _device.get(), modules };
}

cala::Material *cala::Engine::getMaterial(u32 index) {
    return &_materials[index];
}

const cala::vk::ShaderProgram& cala::Engine::getProgram(cala::Engine::ProgramType type) {
    switch (type) {
        case ProgramType::SHADOW_POINT:
            return _pointShadowProgram;
        case ProgramType::SHADOW_DIRECT:
            return _directShadowProgram;
        case ProgramType::TONEMAP:
            return _tonemapProgram;
        case ProgramType::BLOOM_DOWNSAMPLE:
            return _bloomDownsampleProgram;
        case ProgramType::BLOOM_UPSAMPLE:
            return _bloomUpsampleProgram;
        case ProgramType::BLOOM_COMPOSITE:
            return _bloomCompositeProgram;
        case ProgramType::CULL:
            return _cullProgram;
        case ProgramType::CULL_MESH_SHADER:
            return _cullMeshShaderProgram;
        case ProgramType::CULL_POINT:
            return _pointShadowCullProgram;
        case ProgramType::CULL_DIRECT:
            return _directShadowCullProgram;
        case ProgramType::CULL_LIGHTS:
            return _cullLightsProgram;
        case ProgramType::CREATE_CLUSTERS:
            return _createClustersProgram;
        case ProgramType::DEBUG_MESHLETS:
            return _meshletDebugProgram;
        case ProgramType::DEBUG_CLUSTER:
            return _clusterDebugProgram;
        case ProgramType::DEBUG_NORMALS:
            return _normalsDebugProgram;
        case ProgramType::DEBUG_WORLDPOS:
            return _worldPosDebugProgram;
        case ProgramType::DEBUG_FRUSTUM:
            return _frustumDebugProgram;
        case ProgramType::DEBUG_DEPTH:
            return _depthDebugProgram;
        case ProgramType::SOLID_COLOUR:
            return _solidColourProgram;
        case ProgramType::VOXEL_VISUALISE:
            return _voxelVisualisationProgram;
        case ProgramType::SKYBOX:
            return _skyboxProgram;
    }
    return _solidColourProgram;
}

u32 cala::Engine::materialCount() const {
    return _materials.size();
}

void cala::Engine::saveImageToDisk(const std::filesystem::path& path, vk::ImageHandle handle) {
    auto buffer = _device->createBuffer({
        .size = handle->size(),
        .usage = vk::BufferUsage::TRANSFER_DST,
        .memoryType = vk::MemoryProperties::READBACK,
        .persistentlyMapped = true
    });
    _device->immediate([path, handle, buffer](vk::CommandHandle cmd) {
        auto b = handle->barrier(vk::PipelineStage::TOP, vk::PipelineStage::TRANSFER, vk::Access::NONE, vk::Access::TRANSFER_READ, vk::ImageLayout::TRANSFER_SRC);
        cmd->pipelineBarrier({ &b, 1 });

        handle->copy(cmd, *buffer, 0);
    });
    stbi_write_jpg(path.c_str(), handle->width(), handle->height(), 4, buffer->persistentMapping(), 100);
}