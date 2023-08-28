#include "Cala/Engine.h"
#include <Ende/filesystem/File.h>
#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/shapes.h>
#include <Cala/Mesh.h>
#include <Ende/profile/profile.h>
#include <Cala/Material.h>

#include <json.hpp>

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
    : _device(platform),
      _startTime(ende::time::SystemTime::now()),
      _shadowPass(_device, {&shadowPassAttachment, 1 }),
      _lodSampler(_device, {
          .maxLod = 10
      }),
      _irradianceSampler(_device, {
          .filter = VK_FILTER_LINEAR,
          .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
      }),
      _vertexOffset(0),
      _indexOffset(0),
      _stagingReady(false)
{
    ende::fs::File file;
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
            { "../../res/shaders/default/world_pos.frag"_path, backend::ShaderStage::FRAGMENT }
        });
    }
    _device.setBindlessSetIndex(0);
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

    _brdfImage = _device.createImage({ 512, 512, 1, backend::Format::RG16_SFLOAT, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::STORAGE });

    _defaultIrradiance = _device.createImage({
        1, 1, 1,
        backend::Format::RGBA32_SFLOAT,
        1, 6,
        backend::ImageUsage::STORAGE | backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST,
        backend::ImageType::IMAGE2D
        }, &_irradianceSampler);

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
        }, &_lodSampler);

    f32 prefilterData[4 * 512 * 512];
    std::memset(prefilterData, 0, sizeof(f32) * 4 * 512 * 512);
    for (u32 i = 0; i < 6; i++)
        _defaultPrefilter->data(_device, {0, 512, 512, 1, 4 * 4, {prefilterData, sizeof(f32) * 4 * 512 * 512 }, i });

    _device.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto prefilterBarrier = _defaultPrefilter->barrier(backend::Access::NONE, backend::Access::NONE, backend::ImageLayout::TRANSFER_DST);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::BOTTOM, { &prefilterBarrier, 1 });
        _defaultPrefilter->generateMips(cmd);

        auto brdfBarrier = _brdfImage->barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, { &brdfBarrier, 1 });

        cmd.bindProgram(_brdfProgram);
        cmd.bindImage(1, 0, _device.getImageView(_brdfImage), _device.defaultSampler(), true);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.dispatchCompute(512 / 32, 512 / 32, 1);

        brdfBarrier = _brdfImage->barrier(backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, { &brdfBarrier, 1 });

    });

    _globalVertexBuffer = _device.createBuffer(10000, backend::BufferUsage::VERTEX | backend::BufferUsage::TRANSFER_DST, backend::MemoryProperties::DEVICE);
    _vertexStagingBuffer = _device.createBuffer(10000, backend::BufferUsage::TRANSFER_SRC, backend::MemoryProperties::STAGING);
    _globalIndexBuffer = _device.createBuffer(10000, backend::BufferUsage::INDEX | backend::BufferUsage::TRANSFER_DST, backend::MemoryProperties::DEVICE);
    _indexStagingBuffer = _device.createBuffer(10000, backend::BufferUsage::TRANSFER_SRC, backend::MemoryProperties::STAGING);

    _cube = new Mesh(shapes::cube().mesh(this));

}

bool cala::Engine::gc() {
    PROFILE_NAMED("Engine::gc");

    if (_stagingReady) {

        if (_vertexStagingBuffer->size() > _globalVertexBuffer->size())
            _globalVertexBuffer = _device.resizeBuffer(_globalVertexBuffer, _vertexStagingBuffer->size(), true);
        if (_indexStagingBuffer->size() > _globalIndexBuffer->size())
            _globalIndexBuffer = _device.resizeBuffer(_globalIndexBuffer, _indexStagingBuffer->size(), true);

        _device.immediate([&](backend::vulkan::CommandBuffer& cmd) { //TODO: async transfer queue
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
    auto equirectangularView = equirectangular->newView();
    auto cubeView = cubeMap->newView(0, 10);
    _device.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto envBarrier = cubeMap->barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, { &envBarrier, 1 });
//        auto hdrBarrier = equirectangular->barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY, backend::ImageLayout::SHADER_READ_ONLY);
//        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &hdrBarrier, 1 });


        cmd.bindProgram(_equirectangularToCubeMap);
        cmd.bindImage(1, 0, equirectangularView, _lodSampler);
        cmd.bindImage(1, 1, cubeView, _device.defaultSampler(), true);

        cmd.bindPipeline();
        cmd.bindDescriptors();

        cmd.dispatchCompute(512 / 32, 512 / 32, 6);


        envBarrier = cubeMap->barrier(backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::TRANSFER_DST);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, { &envBarrier, 1 });
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
    }, &_irradianceSampler);
    _device.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto irradianceBarrier = irradianceMap->barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, { &irradianceBarrier, 1 });
        auto cubeBarrier = cubeMap->barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, { &cubeBarrier, 1 });

        cmd.bindProgram(_irradianceProgram);
        cmd.bindImage(1, 0, _device.getImageView(cubeMap), _lodSampler, true);
        cmd.bindImage(1, 1, _device.getImageView(irradianceMap), _device.defaultSampler(), true);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.dispatchCompute(irradianceMap->width() / 32, irradianceMap->height() / 32, 6);

        irradianceBarrier = irradianceMap->barrier(backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, { &irradianceBarrier, 1 });
        cubeBarrier = cubeMap->barrier(backend::Access::SHADER_READ, backend::Access::NONE, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, { &cubeBarrier, 1 });
    });
    return irradianceMap;
}

cala::backend::vulkan::ImageHandle cala::Engine::generatePrefilteredIrradiance(backend::vulkan::ImageHandle cubeMap) {
    backend::vulkan::ImageHandle prefilteredMap = _device.createImage({
        512, 512, 1,
        backend::Format::RGBA32_SFLOAT,
        10, 6,
        backend::ImageUsage::STORAGE | backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST
    }, &_lodSampler);
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
        auto prefilterBarrier = prefilteredMap->barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, { &prefilterBarrier, 1 });

        for (u32 mip = 0; mip < prefilteredMap->mips(); mip++) {
            cmd.bindProgram(_prefilterProgram);
            cmd.bindImage(1, 0, _device.getImageView(cubeMap), _lodSampler);
            cmd.bindImage(1, 1, mipViews[mip], _device.defaultSampler(), true);
            f32 roughness = (f32)mip / (f32)prefilteredMap->mips();
            cmd.pushConstants({ &roughness, sizeof(f32) });
            cmd.bindPipeline();
            cmd.bindDescriptors();
            f32 computeDim = 512.f * std::pow(0.5, mip);
            cmd.dispatchCompute(std::ceil(computeDim / 32.f), std::ceil(computeDim / 32.f), 6);
        }

        prefilterBarrier = prefilteredMap->barrier(backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, { &prefilterBarrier, 1 });
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
    _shadowMaps.push(map);
    _device.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto cubeBarrier = map->barrier(backend::Access::NONE, backend::Access::TRANSFER_WRITE, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TRANSFER, { &cubeBarrier, 1 });
    });
    return map;
}

void cala::Engine::updateMaterialdata() {
    for (auto& material : _materials)
        material.upload();
}


u32 cala::Engine::uploadVertexData(ende::Span<f32> data) {
    u32 currentOffset = _vertexOffset;
    if (currentOffset + data.size() >= _vertexStagingBuffer->size())
        _vertexStagingBuffer = _device.resizeBuffer(_vertexStagingBuffer, currentOffset + data.size(), true);

    auto mapped = _vertexStagingBuffer->map(currentOffset, data.size());
    std::memcpy(mapped.address, data.data(), data.size());

    _vertexOffset += data.size();
    _stagingReady = true;
    return currentOffset;
}

u32 cala::Engine::uploadIndexData(ende::Span<u32> data) {
    u32 currentOffset = _indexOffset;
    if (currentOffset + data.size() >= _indexStagingBuffer->size())
        _indexStagingBuffer = _device.resizeBuffer(_indexStagingBuffer, currentOffset + data.size(), true);

    auto mapped = _indexStagingBuffer->map(currentOffset, data.size());
    std::memcpy(mapped.address, data.data(), data.size());
    _indexOffset += data.size();
    _stagingReady = true;
    return currentOffset;
}

cala::Material *cala::Engine::createMaterial(u32 size) {
    u32 id = _materials.size();
    _materials.emplace(this, id, size);
    return &_materials.back();
}

cala::Material *cala::Engine::loadMaterial(const ende::fs::Path &path, u32 size) {
    ende::fs::File file;
    if (!file.open(path, ende::fs::in | ende::fs::binary))
        return nullptr;

    std::string source = file.read();

    nlohmann::json materialSource = nlohmann::json::parse(source);

    auto macroize = [](const std::string& str) -> std::string {
        std::string result = str;
        size_t index = 0;
        size_t last = 0;

        while ((index = result.find('\n', index)) != std::string::npos) {
            result.insert(index++, "\\");
            last = ++index;
        }
        return result;
    };

    Material* material = createMaterial(size);

    std::string materialData = macroize(materialSource["materialData"]);
    std::string materialDefinition = macroize(materialSource["materialDefinition"]);
    std::string materialLoad = macroize(materialSource["materialLoad"]);
    std::string litEval = macroize(materialSource["lit"]);
    std::string unlitEval = macroize(materialSource["unlit"]);
    std::string normalEval = macroize(materialSource["normal"]);
    std::string roughnessEval = macroize(materialSource["roughness"]);
    std::string metallicEval = macroize(materialSource["metallic"]);

    backend::vulkan::ProgramHandle litHandle = loadProgram({
        { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
        { "../../res/shaders/default/lit.frag"_path, backend::ShaderStage::FRAGMENT, {
            { "MATERIAL_DATA", materialData },
            { "MATERIAL_DEFINITION", materialDefinition },
            { "MATERIAL_LOAD", materialLoad },
            { "MATERIAL_EVAL", litEval },
        } }
    });
    material->setVariant(Material::Variant::LIT, litHandle);

    backend::vulkan::ProgramHandle unlitHandle = loadProgram({
        { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
        { "../../res/shaders/default/lit.frag"_path, backend::ShaderStage::FRAGMENT, {
            { "MATERIAL_DATA", materialData },
            { "MATERIAL_DEFINITION", materialDefinition },
            { "MATERIAL_LOAD", materialLoad },
            { "MATERIAL_EVAL", unlitEval },
            } }
    });
    material->setVariant(Material::Variant::UNLIT, unlitHandle);

    backend::vulkan::ProgramHandle normalsHandle = loadProgram({
        { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
        { "../../res/shaders/default/lit.frag"_path, backend::ShaderStage::FRAGMENT, {
            { "MATERIAL_DATA", materialData },
            { "MATERIAL_DEFINITION", materialDefinition },
            { "MATERIAL_LOAD", materialLoad },
            { "MATERIAL_EVAL", normalEval },
        } }
    });
    material->setVariant(Material::Variant::NORMAL, normalsHandle);

    backend::vulkan::ProgramHandle metallicHandle = loadProgram({
        { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
        { "../../res/shaders/default/lit.frag"_path, backend::ShaderStage::FRAGMENT, {
            { "MATERIAL_DATA", materialData },
            { "MATERIAL_DEFINITION", materialDefinition },
            { "MATERIAL_LOAD", materialLoad },
            { "MATERIAL_EVAL", metallicEval },
        } }
    });
    material->setVariant(Material::Variant::METALLIC, metallicHandle);

    backend::vulkan::ProgramHandle roughnessHandle = loadProgram({
        { "../../res/shaders/default.vert"_path, backend::ShaderStage::VERTEX },
        { "../../res/shaders/default/lit.frag"_path, backend::ShaderStage::FRAGMENT, {
            { "MATERIAL_DATA", materialData },
            { "MATERIAL_DEFINITION", materialDefinition },
            { "MATERIAL_LOAD", materialLoad },
            { "MATERIAL_EVAL", roughnessEval },
        } }
    });
    material->setVariant(Material::Variant::ROUGHNESS, roughnessHandle);

    return material;
}

cala::backend::vulkan::ProgramHandle cala::Engine::loadProgram(const ende::Vector<ShaderInfo>& shaderInfo) {
    auto programBuilder = backend::vulkan::ShaderProgram::create();

    for (auto& info : shaderInfo) {
        programBuilder.addStageGLSL(info.path, info.stage, info.macros);
    }
    auto program = programBuilder.compile(_device);
    return _device.createProgram(std::move(program));
}