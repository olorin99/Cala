#include "Cala/Engine.h"
#include <Ende/filesystem/File.h>
#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/shapes.h>
#include <Cala/Mesh.h>
#include <Ende/profile/profile.h>
#include <Cala/Material.h>

#include <json.hpp>
#include <spdlog/sinks/basic_file_sink.h>

cala::backend::vulkan::RenderPass::Attachment shadowPassAttachment {
        cala::backend::Format::D32_SFLOAT,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
};


cala::Engine::Engine(backend::Platform &platform)
    : _logger("Cala", {
        std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>(),
        std::make_shared<spdlog::sinks::basic_file_sink_mt>("debug.log", true)
      }),
      _device(platform, _logger, { true }),
      _startTime(ende::time::SystemTime::now()),
      _shadowPass(_device, {&shadowPassAttachment, 1 }),
      _lodSampler(_device.getSampler({
          .maxLod = 10
      })),
      _irradianceSampler(_device.getSampler({
          .filter = VK_FILTER_LINEAR,
          .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
      })),
      _vertexOffset(0),
      _indexOffset(0),
      _stagingReady(false)
{
    spdlog::flush_every(std::chrono::seconds(5));
    _device.setBindlessSetIndex(0);
    {
        _pointShadowProgram = loadProgram({
            { "../../res/shaders/shadow_point.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/shadow_point.frag"_path, backend::ShaderStage::FRAGMENT }
        });
    }
    {
        _directShadowProgram = loadProgram({
            { "../../res/shaders/shadow.vert"_path, backend::ShaderStage::VERTEX }
        });
    }
    {
        _equirectangularToCubeMap = loadProgram({
            { "../../res/shaders/equirectangularToCubeMap.comp"_path, backend::ShaderStage::COMPUTE }
        });
    }
    {
        _irradianceProgram = loadProgram({
            { "../../res/shaders/irradiance.comp"_path, backend::ShaderStage::COMPUTE }
        });
    }
    {
        _prefilterProgram = loadProgram({
            { "../../res/shaders/prefilter.comp"_path, backend::ShaderStage::COMPUTE }
        });
    }
    {
        _brdfProgram = loadProgram({
            { "../../res/shaders/brdf.comp"_path, backend::ShaderStage::COMPUTE }
        });
    }
    {
        _skyboxProgram = loadProgram({
            { "../../res/shaders/skybox.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/skybox.frag"_path, backend::ShaderStage::FRAGMENT }
        });
    }
    {
        _tonemapProgram = loadProgram({
            { "../../res/shaders/hdr.comp"_path, backend::ShaderStage::COMPUTE }
        });
    }
    {
        _cullProgram = loadProgram({
            { "../../res/shaders/cull.comp"_path, backend::ShaderStage::COMPUTE }
        });
    }
    {
        _pointShadowCullProgram = loadProgram({
            { "../../res/shaders/point_shadow_cull.comp"_path, backend::ShaderStage::COMPUTE }
        });
    }
    {
        _directShadowCullProgram = loadProgram({
            { "../../res/shaders/direct_shadow_cull.comp"_path, backend::ShaderStage::COMPUTE }
        });
    }
    {
        _createClustersProgram = loadProgram({
            { "../../res/shaders/create_clusters.comp"_path, backend::ShaderStage::COMPUTE }
        });
    }
    {
        _cullLightsProgram = loadProgram({
            { "../../res/shaders/cull_lights.comp"_path, backend::ShaderStage::COMPUTE }
        });
    }
    {
        _clusterDebugProgram = loadProgram({
            { "../../res/shaders/fullscreen.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/debug/clusters_debug.frag"_path, backend::ShaderStage::FRAGMENT }
        });
    }
    {
        _worldPosDebugProgram = loadProgram({
            { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/debug/world_pos.frag"_path, backend::ShaderStage::FRAGMENT }
        });
    }
    {
        _solidColourProgram = loadProgram({
            { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/solid_colour.frag"_path, backend::ShaderStage::FRAGMENT }
        });
    }
    {
        _normalsDebugProgram = loadProgram({
            { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/normals.geom"_path, backend::ShaderStage::GEOMETRY },
            { "../../res/shaders/solid_colour.frag"_path, backend::ShaderStage::FRAGMENT }
        });
    }
    {
        _voxelVisualisationProgram = loadProgram({
//            { "../../res/shaders/fullscreen.vert"_path, backend::ShaderStage::VERTEX },
//            { "../../res/shaders/voxel/visualise.frag"_path, backend::ShaderStage::FRAGMENT }
            { "../../res/shaders/voxel/visualise.comp"_path, backend::ShaderStage::COMPUTE }
        });
    }

    _brdfImage = _device.createImage({ 512, 512, 1, backend::Format::RG16_SFLOAT, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::STORAGE });
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE, (u64)_brdfImage->image(), "brdf");
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (u64)_device.getImageView(_brdfImage).view, "brdf_View");

    _defaultIrradiance = _device.createImage({
        1, 1, 1,
        backend::Format::RGBA32_SFLOAT,
        1, 6,
        backend::ImageUsage::STORAGE | backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST,
        backend::ImageType::IMAGE2D
        });
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE, (u64)_defaultIrradiance->image(), "defaultIrradiance");
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (u64)_device.getImageView(_defaultIrradiance).view, "defaultIrradiance_View");

    f32 irradianceData[4];
    std::memset(irradianceData, 0, sizeof(f32) * 4);
    for (u32 i = 0; i < 6; i++)
        _defaultIrradiance->data(_device, {0, 1, 1, 1, 4 * 4, {irradianceData, sizeof(f32) * 4 }, i });


    _defaultPrefilter = _device.createImage({
        512, 512, 1,
        backend::Format::RGBA32_SFLOAT,
        5, 6,
        backend::ImageUsage::STORAGE | backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST | backend::ImageUsage::TRANSFER_SRC,
        backend::ImageType::IMAGE2D
        });
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE, (u64)_defaultPrefilter->image(), "defaultPrefilter");
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (u64)_device.getImageView(_defaultPrefilter).view, "defaultPrefilter_View");

    f32 prefilterData[4 * 512 * 512];
    std::memset(prefilterData, 0, sizeof(f32) * 4 * 512 * 512);
    for (u32 i = 0; i < 6; i++)
        _defaultPrefilter->data(_device, {0, 512, 512, 1, 4 * 4, {prefilterData, sizeof(f32) * 4 * 512 * 512 }, i });

    _device.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto prefilterBarrier = _defaultPrefilter->barrier(backend::PipelineStage::TOP, backend::PipelineStage::BOTTOM, backend::Access::NONE, backend::Access::NONE, backend::ImageLayout::TRANSFER_DST);
        cmd.pipelineBarrier({ &prefilterBarrier, 1 });
        _defaultPrefilter->generateMips(cmd);

        auto brdfBarrier = _brdfImage->barrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, backend::Access::NONE, backend::Access::SHADER_WRITE | backend::Access::SHADER_READ, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier({ &brdfBarrier, 1 });

        cmd.bindProgram(_brdfProgram);
        cmd.bindImage(1, 0, _device.getImageView(_brdfImage), _device.defaultSampler(), true);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.dispatchCompute(512 / 32, 512 / 32, 1);

        brdfBarrier = _brdfImage->barrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::FRAGMENT_SHADER, backend::Access::SHADER_READ | backend::Access::SHADER_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier({ &brdfBarrier, 1 });

    });

    _globalVertexBuffer = _device.createBuffer(10000, backend::BufferUsage::VERTEX | backend::BufferUsage::TRANSFER_DST, backend::MemoryProperties::DEVICE);
    _vertexStagingBuffer = _device.createBuffer(10000, backend::BufferUsage::TRANSFER_SRC, backend::MemoryProperties::STAGING, true);
    _globalIndexBuffer = _device.createBuffer(10000, backend::BufferUsage::INDEX | backend::BufferUsage::TRANSFER_DST, backend::MemoryProperties::DEVICE);
    _indexStagingBuffer = _device.createBuffer(10000, backend::BufferUsage::TRANSFER_SRC, backend::MemoryProperties::STAGING, true);

    _device.context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_globalVertexBuffer->buffer(), "globalVertexBuffer");
    _device.context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_vertexStagingBuffer->buffer(), "vertexStagingBuffer");
    _device.context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_globalIndexBuffer->buffer(), "globalIndexBuffer");
    _device.context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_indexStagingBuffer->buffer(), "indexStagingBuffer");

    _cube = new Mesh(shapes::cube().mesh(this));

}

bool cala::Engine::gc() {
    PROFILE_NAMED("Engine::gc");

    if (_stagingReady) {

        if (_vertexStagingBuffer->size() > _globalVertexBuffer->size()) {
            _globalVertexBuffer = _device.resizeBuffer(_globalVertexBuffer, _vertexStagingBuffer->size(), true);
            _device.context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_globalVertexBuffer->buffer(), "globalVertexBuffer");
        }
        if (_indexStagingBuffer->size() > _globalIndexBuffer->size()) {
            _globalIndexBuffer = _device.resizeBuffer(_globalIndexBuffer, _indexStagingBuffer->size(), true);
            _device.context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_globalIndexBuffer->buffer(), "globalIndexBuffer");
        }

        _device.immediate([&](backend::vulkan::CommandBuffer& cmd) { //TODO: async transfer queue
            backend::vulkan::Buffer::Barrier barriers[4];

            barriers[0] = _globalVertexBuffer->barrier(backend::PipelineStage::TOP, backend::PipelineStage::TRANSFER, backend::Access::NONE, backend::Access::TRANSFER_WRITE);
            barriers[1] = _vertexStagingBuffer->barrier(backend::PipelineStage::TOP, backend::PipelineStage::TRANSFER, backend::Access::NONE, backend::Access::TRANSFER_READ);
            barriers[2] = _globalIndexBuffer->barrier(backend::PipelineStage::TOP, backend::PipelineStage::TRANSFER, backend::Access::NONE, backend::Access::TRANSFER_WRITE);
            barriers[3] = _indexStagingBuffer->barrier(backend::PipelineStage::TOP, backend::PipelineStage::TRANSFER, backend::Access::NONE, backend::Access::TRANSFER_READ);

            cmd.pipelineBarrier({ barriers, 4 });

            VkBufferCopy vertexCopy{};
            vertexCopy.dstOffset = 0;
            vertexCopy.srcOffset = 0;
            vertexCopy.size = _vertexStagingBuffer->size();
            vkCmdCopyBuffer(cmd.buffer(), _vertexStagingBuffer->buffer(), _globalVertexBuffer->buffer(), 1, &vertexCopy);

            VkBufferCopy indexCopy{};
            indexCopy.dstOffset = 0;
            indexCopy.srcOffset = 0;
            indexCopy.size = _indexStagingBuffer->size();
            vkCmdCopyBuffer(cmd.buffer(), _indexStagingBuffer->buffer(), _globalIndexBuffer->buffer(), 1, &indexCopy);

            barriers[0] = _globalVertexBuffer->barrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::BOTTOM, backend::Access::TRANSFER_WRITE, backend::Access::NONE);
            barriers[1] = _vertexStagingBuffer->barrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::BOTTOM, backend::Access::TRANSFER_READ, backend::Access::NONE);
            barriers[2] = _globalIndexBuffer->barrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::BOTTOM, backend::Access::TRANSFER_WRITE, backend::Access::NONE);
            barriers[3] = _indexStagingBuffer->barrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::BOTTOM, backend::Access::TRANSFER_READ, backend::Access::NONE);

            cmd.pipelineBarrier({ barriers, 4 });
        });

        _stagingReady = false;

    }

    return _device.gc();
}

cala::backend::vulkan::ImageHandle cala::Engine::convertToCubeMap(backend::vulkan::ImageHandle equirectangular) {
    auto cubeMap = _device.createImage({
        512, 512, 1,
        backend::Format::RGBA32_SFLOAT,
        10, 6,
        backend::ImageUsage::STORAGE | backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_SRC | backend::ImageUsage::TRANSFER_DST
    });
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE, (u64)cubeMap->image(), "cubeMap");
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (u64)_device.getImageView(cubeMap).view, "cubeMap_View");
    auto equirectangularView = equirectangular->newView();
    auto cubeView = cubeMap->newView(0, 10);
    _device.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto envBarrier = cubeMap->barrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier({ &envBarrier, 1 });
//        auto hdrBarrier = equirectangular->barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY, backend::ImageLayout::SHADER_READ_ONLY);
//        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &hdrBarrier, 1 });


        cmd.bindProgram(_equirectangularToCubeMap);
        cmd.bindImage(1, 0, equirectangularView, *_lodSampler);
        cmd.bindImage(1, 1, cubeView, _device.defaultSampler(), true);

        cmd.bindPipeline();
        cmd.bindDescriptors();

        cmd.dispatchCompute(512 / 32, 512 / 32, 6);


        envBarrier = cubeMap->barrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::TRANSFER_DST);
        cmd.pipelineBarrier({ &envBarrier, 1 });
        cubeMap->generateMips(cmd);
//        envBarrier = cubeMap->barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::TRANSFER_DST, backend::ImageLayout::SHADER_READ_ONLY);
//        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::FRAGMENT_SHADER, 0, nullptr, { &envBarrier, 1 });
    });
    return cubeMap;
}

cala::backend::vulkan::ImageHandle cala::Engine::generateIrradianceMap(backend::vulkan::ImageHandle cubeMap) {
    auto irradianceMap = _device.createImage({
        64, 64, 1,
        backend::Format::RGBA32_SFLOAT,
        1, 6,
        backend::ImageUsage::STORAGE | backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST
    });
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE, (u64)irradianceMap->image(), "irradianceMap");
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (u64)_device.getImageView(irradianceMap).view, "irradianceMap_View");
    _device.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto irradianceBarrier = irradianceMap->barrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier({ &irradianceBarrier, 1 });
        auto cubeBarrier = cubeMap->barrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier({ &cubeBarrier, 1 });

        cmd.bindProgram(_irradianceProgram);
        cmd.bindImage(1, 0, _device.getImageView(cubeMap), *_lodSampler, true);
        cmd.bindImage(1, 1, _device.getImageView(irradianceMap), _device.defaultSampler(), true);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.dispatchCompute(irradianceMap->width() / 32, irradianceMap->height() / 32, 6);

        irradianceBarrier = irradianceMap->barrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::FRAGMENT_SHADER, backend::Access::SHADER_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier({ &irradianceBarrier, 1 });
        cubeBarrier = cubeMap->barrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::FRAGMENT_SHADER, backend::Access::SHADER_READ, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier({ &cubeBarrier, 1 });
    });
    return irradianceMap;
}

cala::backend::vulkan::ImageHandle cala::Engine::generatePrefilteredIrradiance(backend::vulkan::ImageHandle cubeMap) {
    backend::vulkan::ImageHandle prefilteredMap = _device.createImage({
        512, 512, 1,
        backend::Format::RGBA32_SFLOAT,
        10, 6,
        backend::ImageUsage::STORAGE | backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST
    });
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE, (u64)prefilteredMap->image(), "prefilterMap");
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (u64)_device.getImageView(prefilteredMap).view, "prefilterMap_View");
    backend::vulkan::Image::View mipViews[10] = {
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


    _device.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto prefilterBarrier = prefilteredMap->barrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier({ &prefilterBarrier, 1 });

        for (u32 mip = 0; mip < prefilteredMap->mips(); mip++) {
            cmd.bindProgram(_prefilterProgram);
            cmd.bindImage(1, 0, _device.getImageView(cubeMap), *_lodSampler);
            cmd.bindImage(1, 1, mipViews[mip], _device.defaultSampler(), true);
            f32 roughness = (f32)mip / (f32)prefilteredMap->mips();
            cmd.pushConstants(backend::ShaderStage::COMPUTE, { &roughness, sizeof(f32) });
            cmd.bindPipeline();
            cmd.bindDescriptors();
            f32 computeDim = 512.f * std::pow(0.5, mip);
            cmd.dispatchCompute(std::ceil(computeDim / 32.f), std::ceil(computeDim / 32.f), 6);
        }

        prefilterBarrier = prefilteredMap->barrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier({ &prefilterBarrier, 1 });
    });
    return prefilteredMap;
}


cala::backend::vulkan::ImageHandle cala::Engine::getShadowMap(u32 index) {
    if (index < _shadowMaps.size())
        return _shadowMaps[index];

    auto map = _device.createImage({
        1024, 1024, 1,
        backend::Format::D32_SFLOAT,
        1, 6,
        backend::ImageUsage::SAMPLED | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT | backend::ImageUsage::TRANSFER_DST
    });
    std::string debugLabel = "ShadowMap: " + std::to_string(index);
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE, (u64)map->image(), debugLabel);
    _device.context().setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (u64)_device.getImageView(map).view, "shadowMap_View");
    _shadowMaps.push(map);
    _device.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto cubeBarrier = map->barrier(backend::PipelineStage::TOP, backend::PipelineStage::TRANSFER, backend::Access::NONE, backend::Access::TRANSFER_WRITE, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier({ &cubeBarrier, 1 });
    });
    return map;
}

void cala::Engine::updateMaterialdata() {
    for (auto& material : _materials)
        material.upload();
}


u32 cala::Engine::uploadVertexData(ende::Span<f32> data) {
    u32 currentOffset = _vertexOffset;
    if (currentOffset + data.size() >= _vertexStagingBuffer->size()) {
        _vertexStagingBuffer = _device.resizeBuffer(_vertexStagingBuffer, currentOffset + data.size(), true);
        _device.context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_vertexStagingBuffer->buffer(), "vertexStagingBuffer");
    }

//    auto mapped = _vertexStagingBuffer->map(currentOffset, data.size());
//    std::memcpy(mapped.address, data.data(), data.size());
//    std::memcpy(_vertexStagingBuffer->persistentMapping(), data.data(), data.size());
    _vertexStagingBuffer->data(data, currentOffset);

    _vertexOffset += data.size();
    _stagingReady = true;
    return currentOffset;
}

u32 cala::Engine::uploadIndexData(ende::Span<u32> data) {
    u32 currentOffset = _indexOffset;
    if (currentOffset + data.size() >= _indexStagingBuffer->size()) {
        _indexStagingBuffer = _device.resizeBuffer(_indexStagingBuffer, currentOffset + data.size(), true);
        _device.context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_indexStagingBuffer->buffer(), "indexStagingBuffer");
    }

//    auto mapped = _indexStagingBuffer->map(currentOffset, data.size());
//    std::memcpy(mapped.address, data.data(), data.size());
//    std::memcpy(_indexStagingBuffer->persistentMapping(), data.data(), data.size());
    _indexStagingBuffer->data(data, currentOffset);
    _indexOffset += data.size();
    _stagingReady = true;
    return currentOffset;
}

cala::Material *cala::Engine::createMaterial(u32 size) {
    u32 id = _materials.size();
    _materials.emplace(this, id, size);
    return &_materials.back();
}

std::string macroize(const std::string& str) {
    std::string result = str;
    size_t index = 0;
    size_t last = 0;

    while ((index = result.find('\n', index)) != std::string::npos) {
        result.insert(index++, "\\");
        last = ++index;
    }
    return result;
}

std::optional<std::string> loadMaterialString(nlohmann::json& json, std::string_view label) {
    auto it = json.find(label);
    if (it == json.end())
        return {};
    if (it->is_object()) {
        auto path = it->find("path");
        if (path == it.value().end())
            return {};
        ende::fs::File file;
        if (!file.open("../../res/shaders/" + path->get<std::string>()))
            return {};
        return macroize(file.read());
    }
    return macroize(it.value());
}

std::optional<cala::backend::vulkan::CommandBuffer::RasterState> loadMaterialRasterState(nlohmann::json& json) {
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

    cala::backend::vulkan::CommandBuffer::RasterState state{};

    if (cullModeIt != it->end() && cullModeIt->is_string()) {
        std::string mode = cullModeIt->get<std::string>();
        if (mode == "NONE")
            state.cullMode = cala::backend::CullMode::NONE;
        else if (mode == "FRONT")
            state.cullMode = cala::backend::CullMode::FRONT;
        else if (mode == "BACK")
            state.cullMode = cala::backend::CullMode::BACK;
        else if (mode == "FRONT_BACK")
            state.cullMode = cala::backend::CullMode::FRONT_BACK;
    }
    if (frontFaceIt != it->end() && frontFaceIt->is_string()) {
        std::string face = frontFaceIt->get<std::string>();
        if (face == "CCW")
            state.frontFace = cala::backend::FrontFace::CCW;
        else if (face == "CW")
            state.frontFace = cala::backend::FrontFace::CW;
    }
    if (polygonModeIt != it->end() && polygonModeIt->is_string()) {
        std::string mode = polygonModeIt->get<std::string>();
        if (mode == "FILL")
            state.polygonMode = cala::backend::PolygonMode::FILL;
        else if (mode == "LINE")
            state.polygonMode = cala::backend::PolygonMode::LINE;
        else if (mode == "POINT")
            state.polygonMode = cala::backend::PolygonMode::POINT;
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

std::optional<cala::backend::vulkan::CommandBuffer::DepthState> loadMaterialDepthState(nlohmann::json& json) {
    auto it = json.find("depth_state");
    if (it == json.end() || !it->is_object())
        return {};
    auto testIt = it->find("test");
    auto writeIt = it->find("write");
    auto compareOpIt = it->find("compareOp");

    cala::backend::vulkan::CommandBuffer::DepthState state{};

    if (testIt != it->end() && testIt->is_boolean())
        state.test = testIt->get<bool>();
    if (writeIt != it->end() && writeIt->is_boolean())
        state.write = writeIt->get<bool>();
    if (compareOpIt != it->end() && compareOpIt->is_string()) {
        std::string comp = compareOpIt->get<std::string>();
        if (comp == "NEVER")
            state.compareOp = cala::backend::CompareOp::NEVER;
        else if (comp == "LESS")
            state.compareOp = cala::backend::CompareOp::LESS;
        else if (comp == "EQUAL")
            state.compareOp = cala::backend::CompareOp::EQUAL;
        else if (comp == "LESS_EQUAL")
            state.compareOp = cala::backend::CompareOp::LESS_EQUAL;
        else if (comp == "GREATER")
            state.compareOp = cala::backend::CompareOp::GREATER;
        else if (comp == "NOT_EQUAL")
            state.compareOp = cala::backend::CompareOp::NOT_EQUAL;
        else if (comp == "GREATER_EQUAL")
            state.compareOp = cala::backend::CompareOp::GREATER_EQUAL;
        else if (comp == "ALWAYS")
            state.compareOp = cala::backend::CompareOp::ALWAYS;
    }
    return state;
}

std::optional<cala::backend::vulkan::CommandBuffer::BlendState> loadMaterialBlendState(nlohmann::json& json) {
    auto it = json.find("blend_state");
    if (it == json.end() || !it->is_object())
        return {};
    auto blendIt = it->find("blend");
    auto srcFactorIt = it->find("srcFactor");
    auto dstFactorIt = it->find("dstFactor");

    cala::backend::vulkan::CommandBuffer::BlendState state;

    if (blendIt != it->end() && blendIt->is_boolean())
        state.blend = blendIt->get<bool>();
    if (srcFactorIt != it->end() && srcFactorIt->is_string()) {
        std::string factor = srcFactorIt->get<std::string>();
        if (factor == "ZERO")
            state.srcFactor = cala::backend::BlendFactor::ZERO;
        else if (factor == "ONE")
            state.srcFactor = cala::backend::BlendFactor::ONE;
        else if (factor == "SRC_COLOUR")
            state.srcFactor = cala::backend::BlendFactor::SRC_COLOUR;
        else if (factor == "ONE_MINUS_SRC_COLOUR")
            state.srcFactor = cala::backend::BlendFactor::ONE_MINUS_SRC_COLOUR;
        else if (factor == "DST_COLOUR")
            state.srcFactor = cala::backend::BlendFactor::DST_COLOUR;
        else if (factor == "ONE_MINUS_DST_COLOUR")
            state.srcFactor = cala::backend::BlendFactor::ONE_MINUS_DST_COLOUR;
        else if (factor == "SRC_ALPHA")
            state.srcFactor = cala::backend::BlendFactor::SRC_ALPHA;
        else if (factor == "ONE_MINUS_SRC_ALPHA")
            state.srcFactor = cala::backend::BlendFactor::ONE_MINUS_SRC_ALPHA;
        else if (factor == "DST_ALPHA")
            state.srcFactor = cala::backend::BlendFactor::DST_ALPHA;
        else if (factor == "ONE_MINUS_DST_ALPHA")
            state.srcFactor = cala::backend::BlendFactor::ONE_MINUS_DST_ALPHA;
        else if (factor == "CONSTANT_COLOUR")
            state.srcFactor = cala::backend::BlendFactor::CONSTANT_COLOUR;
        else if (factor == "ONE_MINUS_CONSTANT_COLOUR")
            state.srcFactor = cala::backend::BlendFactor::ONE_MINUS_CONSTANT_COLOUR;
        else if (factor == "CONSTANT_ALPHA")
            state.srcFactor = cala::backend::BlendFactor::CONSTANT_ALPHA;
        else if (factor == "ONE_MINUS_CONSTANT_ALPHA")
            state.srcFactor = cala::backend::BlendFactor::ONE_MINUS_CONSTANT_ALPHA;
        else if (factor == "SRC_ALPHA_SATURATE")
            state.srcFactor = cala::backend::BlendFactor::SRC_ALPHA_SATURATE;
        else if (factor == "SRC1_COLOUR")
            state.srcFactor = cala::backend::BlendFactor::SRC1_COLOUR;
        else if (factor == "ONE_MINUS_SRC1_COLOUR")
            state.srcFactor = cala::backend::BlendFactor::ONE_MINUS_SRC1_COLOUR;
        else if (factor == "SRC1_ALPHA")
            state.srcFactor = cala::backend::BlendFactor::SRC1_ALPHA;
        else if (factor == "ONE_MINUS_SRC1_ALPHA")
            state.srcFactor = cala::backend::BlendFactor::ONE_MINUS_SRC1_ALPHA;
    }
    if (dstFactorIt != it->end() && dstFactorIt->is_string()) {
        std::string factor = dstFactorIt->get<std::string>();
        if (factor == "ZERO")
            state.dstFactor = cala::backend::BlendFactor::ZERO;
        else if (factor == "ONE")
            state.dstFactor = cala::backend::BlendFactor::ONE;
        else if (factor == "SRC_COLOUR")
            state.dstFactor = cala::backend::BlendFactor::SRC_COLOUR;
        else if (factor == "ONE_MINUS_SRC_COLOUR")
            state.dstFactor = cala::backend::BlendFactor::ONE_MINUS_SRC_COLOUR;
        else if (factor == "DST_COLOUR")
            state.dstFactor = cala::backend::BlendFactor::DST_COLOUR;
        else if (factor == "ONE_MINUS_DST_COLOUR")
            state.dstFactor = cala::backend::BlendFactor::ONE_MINUS_DST_COLOUR;
        else if (factor == "SRC_ALPHA")
            state.dstFactor = cala::backend::BlendFactor::SRC_ALPHA;
        else if (factor == "ONE_MINUS_SRC_ALPHA")
            state.dstFactor = cala::backend::BlendFactor::ONE_MINUS_SRC_ALPHA;
        else if (factor == "DST_ALPHA")
            state.dstFactor = cala::backend::BlendFactor::DST_ALPHA;
        else if (factor == "ONE_MINUS_DST_ALPHA")
            state.dstFactor = cala::backend::BlendFactor::ONE_MINUS_DST_ALPHA;
        else if (factor == "CONSTANT_COLOUR")
            state.dstFactor = cala::backend::BlendFactor::CONSTANT_COLOUR;
        else if (factor == "ONE_MINUS_CONSTANT_COLOUR")
            state.dstFactor = cala::backend::BlendFactor::ONE_MINUS_CONSTANT_COLOUR;
        else if (factor == "CONSTANT_ALPHA")
            state.dstFactor = cala::backend::BlendFactor::CONSTANT_ALPHA;
        else if (factor == "ONE_MINUS_CONSTANT_ALPHA")
            state.dstFactor = cala::backend::BlendFactor::ONE_MINUS_CONSTANT_ALPHA;
        else if (factor == "SRC_ALPHA_SATURATE")
            state.dstFactor = cala::backend::BlendFactor::SRC_ALPHA_SATURATE;
        else if (factor == "SRC1_COLOUR")
            state.dstFactor = cala::backend::BlendFactor::SRC1_COLOUR;
        else if (factor == "ONE_MINUS_SRC1_COLOUR")
            state.dstFactor = cala::backend::BlendFactor::ONE_MINUS_SRC1_COLOUR;
        else if (factor == "SRC1_ALPHA")
            state.dstFactor = cala::backend::BlendFactor::SRC1_ALPHA;
        else if (factor == "ONE_MINUS_SRC1_ALPHA")
            state.dstFactor = cala::backend::BlendFactor::ONE_MINUS_SRC1_ALPHA;
    }
    return state;
}

cala::Material *cala::Engine::loadMaterial(const ende::fs::Path &path, u32 size) {
    try {

        ende::fs::File file;
        if (!file.open(path, ende::fs::in | ende::fs::binary))
            return nullptr;

        std::string source = file.read();

        nlohmann::json materialSource = nlohmann::json::parse(source);

        Material* material = createMaterial(size);

        std::string materialData = loadMaterialString(materialSource, "materialData").value();
        std::string materialDefinition = loadMaterialString(materialSource, "materialDefinition").value();
        std::string materialLoad = loadMaterialString(materialSource, "materialLoad").value();
        std::string litEval = loadMaterialString(materialSource, "lit").value();
        std::string unlitEval = loadMaterialString(materialSource, "unlit").value();
        std::string normalEval = loadMaterialString(materialSource, "normal").value();
        std::string roughnessEval = loadMaterialString(materialSource, "roughness").value();
        std::string metallicEval = loadMaterialString(materialSource, "metallic").value();
        std::string voxelizeEval = loadMaterialString(materialSource, "voxelize").value();

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

        backend::vulkan::ProgramHandle litHandle = loadProgram({
            { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/default/default.frag"_path, backend::ShaderStage::FRAGMENT, {
                { "MATERIAL_DATA", materialData },
                { "MATERIAL_DEFINITION", materialDefinition },
                { "MATERIAL_LOAD", materialLoad },
                { "MATERIAL_EVAL", litEval },
            }, includes }
        });
        material->setVariant(Material::Variant::LIT, litHandle);

        backend::vulkan::ProgramHandle unlitHandle = loadProgram({
            { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/default/default.frag"_path, backend::ShaderStage::FRAGMENT, {
                { "MATERIAL_DATA", materialData },
                { "MATERIAL_DEFINITION", materialDefinition },
                { "MATERIAL_LOAD", materialLoad },
                { "MATERIAL_EVAL", unlitEval },
            }, includes }
        });
        material->setVariant(Material::Variant::UNLIT, unlitHandle);

        backend::vulkan::ProgramHandle normalsHandle = loadProgram({
            { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/default/default.frag"_path, backend::ShaderStage::FRAGMENT, {
                { "MATERIAL_DATA", materialData },
                { "MATERIAL_DEFINITION", materialDefinition },
                { "MATERIAL_LOAD", materialLoad },
                { "MATERIAL_EVAL", normalEval },
            }, includes }
        });
        material->setVariant(Material::Variant::NORMAL, normalsHandle);

        backend::vulkan::ProgramHandle metallicHandle = loadProgram({
            { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/default/default.frag"_path, backend::ShaderStage::FRAGMENT, {
                { "MATERIAL_DATA", materialData },
                { "MATERIAL_DEFINITION", materialDefinition },
                { "MATERIAL_LOAD", materialLoad },
                { "MATERIAL_EVAL", metallicEval },
            }, includes }
        });
        material->setVariant(Material::Variant::METALLIC, metallicHandle);

        backend::vulkan::ProgramHandle roughnessHandle = loadProgram({
            { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/default/default.frag"_path, backend::ShaderStage::FRAGMENT, {
                { "MATERIAL_DATA", materialData },
                { "MATERIAL_DEFINITION", materialDefinition },
                { "MATERIAL_LOAD", materialLoad },
                { "MATERIAL_EVAL", roughnessEval },
            }, includes }
        });
        material->setVariant(Material::Variant::ROUGHNESS, roughnessHandle);

        backend::vulkan::ProgramHandle voxelGIHandle = loadProgram({
            { "../../res/shaders/voxel/voxelize.vert"_path, backend::ShaderStage::VERTEX },
            { "../../res/shaders/voxel/voxelize.geom"_path, backend::ShaderStage::GEOMETRY },
            { "../../res/shaders/voxel/voxelize.frag"_path, backend::ShaderStage::FRAGMENT, {
                { "MATERIAL_DATA", materialData },
                { "MATERIAL_DEFINITION", materialDefinition },
                { "MATERIAL_LOAD", materialLoad },
                { "MATERIAL_EVAL", voxelizeEval },
            }, includes }
        });
        material->setVariant(Material::Variant::VOXELGI, voxelGIHandle);

        return material;
    } catch (std::exception& e) {
        _device.logger().error("Error with material: {}", *path);
        return nullptr;
    }
}

cala::backend::vulkan::ProgramHandle cala::Engine::loadProgram(const ende::Vector<ShaderInfo>& shaderInfo) {
    auto programBuilder = backend::vulkan::ShaderProgram::create(&_device);

    for (auto& info : shaderInfo) {
        programBuilder.addStageGLSL(info.path, info.stage, info.macros, info.includes);
    }
    auto program = programBuilder.compile();
    return _device.createProgram(std::move(program));
}

cala::Material *cala::Engine::getMaterial(u32 index) {
    return &_materials[index];
}

cala::backend::vulkan::ProgramHandle cala::Engine::getProgram(cala::Engine::ProgramType type) {
    switch (type) {
        case ProgramType::SHADOW_POINT:
            return _pointShadowProgram;
        case ProgramType::SHADOW_DIRECT:
            return _directShadowProgram;
        case ProgramType::TONEMAP:
            return _tonemapProgram;
        case ProgramType::CULL:
            return _cullProgram;
        case ProgramType::CULL_POINT:
            return _pointShadowCullProgram;
        case ProgramType::CULL_DIRECT:
            return _directShadowCullProgram;
        case ProgramType::CULL_LIGHTS:
            return _cullLightsProgram;
        case ProgramType::CREATE_CLUSTERS:
            return _createClustersProgram;
        case ProgramType::DEBUG_CLUSTER:
            return _clusterDebugProgram;
        case ProgramType::DEBUG_NORMALS:
            return _normalsDebugProgram;
        case ProgramType::DEBUG_WORLDPOS:
            return _worldPosDebugProgram;
        case ProgramType::SOLID_COLOUR:
            return _solidColourProgram;
        case ProgramType::VOXEL_VISUALISE:
            return _voxelVisualisationProgram;
    }
    return _solidColourProgram;
}