#include "Cala/Engine.h"
#include <Ende/filesystem/File.h>
#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/shapes.h>
#include <Cala/Mesh.h>
#include <Ende/profile/profile.h>

template <>
cala::backend::vulkan::Buffer &cala::BufferHandle::operator*() noexcept {
    return *_engine->_buffers[_index];
}
template <>
cala::backend::vulkan::Buffer *cala::BufferHandle::operator->() noexcept {
    return _engine->_buffers[_index];
}

template <>
cala::backend::vulkan::Image &cala::ImageHandle::operator*() noexcept {
    return *_engine->_images[_index];
}
template <>
cala::backend::vulkan::Image *cala::ImageHandle::operator->() noexcept {
    return _engine->_images[_index];
}

template <>
cala::backend::vulkan::ShaderProgram &cala::ProgramHandle::operator*() noexcept {
    return _engine->_programs[_index];
}
template <>
cala::backend::vulkan::ShaderProgram *cala::ProgramHandle::operator->() noexcept {
    return &_engine->_programs[_index];
}

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


cala::Engine::Engine(backend::Platform &platform, bool clear)
    : _driver(platform, clear),
      _startTime(ende::time::SystemTime::now()),
      _shadowPass(_driver, { &shadowPassAttachment, 1 }),
      _defaultSampler(_driver, {}),
      _lodSampler(_driver, {
          .maxLod = 10
      }),
      _irradianceSampler(_driver, {
          .filter = VK_FILTER_LINEAR,
          .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
      }),
      _shadowSampler(_driver, {
          .filter = VK_FILTER_NEAREST,
          .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
          .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
      }),
      _materialDataDirty(true),
      _vertexOffset(0),
      _indexOffset(0),
      _stagingReady(false)
{
    ende::fs::File file;
    {
        file.open("../../res/shaders/shadow_point.vert.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> vertexData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(vertexData.data()), static_cast<u32>(vertexData.size() * sizeof(u32)) });

        file.open("../../res/shaders/shadow_point.frag.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> fragmentData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(fragmentData.data()), static_cast<u32>(fragmentData.size() * sizeof(u32)) });

        _pointShadowProgram = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(vertexData, backend::ShaderStage::VERTEX)
                .addStage(fragmentData, backend::ShaderStage::FRAGMENT)
                .compile(_driver)));
    }
    {
        file.open("../../res/shaders/shadow.vert.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> vertexData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(vertexData.data()), static_cast<u32>(vertexData.size() * sizeof(u32)) });

        _directShadowProgram = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(vertexData, backend::ShaderStage::VERTEX)
                .compile(_driver)));
    }
    {
        file.open("../../res/shaders/equirectangularToCubeMap.comp.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> computeData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(computeData.data()), static_cast<u32>(computeData.size() * sizeof(u32)) });

        _equirectangularToCubeMap = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(computeData, backend::ShaderStage::COMPUTE)
                .compile(_driver)));
    }
    {
        file.open("../../res/shaders/irradiance.comp.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> computeData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(computeData.data()), static_cast<u32>(computeData.size() * sizeof(u32)) });

        _irradianceProgram = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(computeData, backend::ShaderStage::COMPUTE)
                .compile(_driver)));
    }
    {
        file.open("../../res/shaders/prefilter.comp.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> computeData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(computeData.data()), static_cast<u32>(computeData.size() * sizeof(u32)) });

        _prefilterProgram = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(computeData, backend::ShaderStage::COMPUTE)
                .compile(_driver)));
    }
    {
        file.open("../../res/shaders/brdf.comp.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> computeData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(computeData.data()), static_cast<u32>(computeData.size() * sizeof(u32)) });

        _brdfProgram = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(computeData, backend::ShaderStage::COMPUTE)
                .compile(_driver)));
    }
    {
        file.open("../../res/shaders/skybox.vert.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> vertexData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(vertexData.data()), static_cast<u32>(vertexData.size() * sizeof(u32)) });

        file.open("../../res/shaders/skybox.frag.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> fragmentData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(fragmentData.data()), static_cast<u32>(fragmentData.size() * sizeof(u32)) });

        _skyboxProgram = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(vertexData, backend::ShaderStage::VERTEX)
                .addStage(fragmentData, backend::ShaderStage::FRAGMENT)
                .compile(_driver)));
    }
    {
        file.open("../../res/shaders/hdr.comp.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> computeData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(computeData.data()), static_cast<u32>(computeData.size() * sizeof(u32)) });

        _tonemapProgram = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(computeData, backend::ShaderStage::COMPUTE)
                .compile(_driver)));
    }
    {
        file.open("../../res/shaders/cull.comp.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> computeData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(computeData.data()), static_cast<u32>(computeData.size() * sizeof(u32)) });

        _cullProgram = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(computeData, backend::ShaderStage::COMPUTE)
                .compile(_driver)));
    }
    {
        file.open("../../res/shaders/create_clusters.comp.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> computeData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(computeData.data()), static_cast<u32>(computeData.size() * sizeof(u32)) });

        _createClustersProgram = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(computeData, backend::ShaderStage::COMPUTE)
                .compile(_driver)));
    }
    {
        file.open("../../res/shaders/cull_lights.comp.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> computeData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(computeData.data()), static_cast<u32>(computeData.size() * sizeof(u32)) });

        _cullLightsProgram = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(computeData, backend::ShaderStage::COMPUTE)
                .compile(_driver)));
    }


    _defaultPointShadow = createImage({
        1,
        1,
        1,
        backend::Format::D32_SFLOAT,
        1,
        6,
        backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST,
        backend::ImageType::IMAGE2D
    });

    f32 data = 1.f;
    for (u32 i = 0; i < 6; i++) {
        _defaultPointShadow->data(_driver, {
                0, 1, 1, 1, 4, { &data, sizeof(data) }, i
        });
    }

    _defaultDirectionalShadow = createImage({
        1,
        1,
        1,
        backend::Format::D32_SFLOAT,
        1,
        1,
        backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST,
        backend::ImageType::IMAGE2D
    });

    _defaultDirectionalShadow->data(_driver, {
            0, 1, 1, 1, 4, { &data, sizeof(data) }
    });

    f32 white[4] = { 1, 1, 1, 1 };
    (_defaultAlbedo = createImage({1, 1, 1, backend::Format::RGBA32_SFLOAT, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST, backend::ImageType::IMAGE2D}))
            ->data(_driver, {0, 1, 1, 1, 4 * 4, { white, sizeof(f32) * 4 }});
    f32 normalData[] = { 0.52, 0.52, 1, 1 };
    (_defaultNormal = createImage({1, 1, 1, backend::Format::RGBA32_SFLOAT, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST, backend::ImageType::IMAGE2D}))
            ->data(_driver, {0, 1, 1, 1, 4 * 4, { normalData, sizeof(f32) * 4 }});
    f32 metallicRoughnessData[] = { 0.f, 1.f, 0.f, 1.f };
    (_defaultMetallicRoughness = createImage({1, 1, 1, backend::Format::RGBA32_SFLOAT, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST, backend::ImageType::IMAGE2D}))
            ->data(_driver, {0, 1, 1, 1, 4 * 4, { metallicRoughnessData, sizeof(f32) * 4 }});


    _brdfImage = createImage({ 512, 512, 1, backend::Format::RG16_SFLOAT, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::STORAGE });

    _defaultIrradiance = createImage({
        1, 1, 1,
        backend::Format::RGBA32_SFLOAT,
        1, 6,
        backend::ImageUsage::STORAGE | backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST,
        backend::ImageType::IMAGE2D
        }, &_irradianceSampler);

    f32 irradianceData[4];
    std::memset(irradianceData, 0, sizeof(f32) * 4);
    for (u32 i = 0; i < 6; i++)
        _defaultIrradiance->data(_driver, { 0, 1, 1, 1, 4 * 4, { irradianceData, sizeof(f32) * 4 }, i });


    _defaultPrefilter = createImage({
        512, 512, 1,
        backend::Format::RGBA32_SFLOAT,
        5, 6,
        backend::ImageUsage::STORAGE | backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST | backend::ImageUsage::TRANSFER_SRC,
        backend::ImageType::IMAGE2D
        }, &_lodSampler);

    f32 prefilterData[4 * 512 * 512];
    std::memset(prefilterData, 0, sizeof(f32) * 4 * 512 * 512);
    for (u32 i = 0; i < 6; i++)
        _defaultPrefilter->data(_driver, { 0, 512, 512, 1, 4 * 4, { prefilterData, sizeof(f32) * 4 * 512 * 512 }, i });

    _driver.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto prefilterBarrier = _defaultPrefilter->barrier(backend::Access::NONE, backend::Access::NONE, backend::ImageLayout::TRANSFER_DST);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::BOTTOM, { &prefilterBarrier, 1 });
        _defaultPrefilter->generateMips(cmd);


        backend::vulkan::Image::Barrier barriers[5];
        barriers[0] = _defaultPointShadow->barrier(backend::Access::NONE, backend::Access::TRANSFER_WRITE, backend::ImageLayout::SHADER_READ_ONLY);
        barriers[1] = _defaultDirectionalShadow->barrier(backend::Access::NONE, backend::Access::TRANSFER_WRITE, backend::ImageLayout::SHADER_READ_ONLY);
        barriers[2] = _defaultAlbedo->barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY);
        barriers[3] = _defaultNormal->barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY);
        barriers[4] = _defaultMetallicRoughness->barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TRANSFER | backend::PipelineStage::FRAGMENT_SHADER, { barriers, 5 });

        auto brdfBarrier = _brdfImage->barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, { &brdfBarrier, 1 });

        cmd.bindProgram(*_brdfProgram);
        cmd.bindImage(1, 0, getImageView(_brdfImage), _defaultSampler, true);
        cmd.bindPipeline();
        cmd.bindDescriptors();
        cmd.dispatchCompute(512 / 32, 512 / 32, 1);

        brdfBarrier = _brdfImage->barrier(backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, { &brdfBarrier, 1 });

    });

    _defaultPointShadowView = _defaultPointShadow->newView();
    _defaultDirectionalShadowView = _defaultDirectionalShadow->newView();

    _materialBuffer = createBuffer(256, backend::BufferUsage::STORAGE | backend::BufferUsage::UNIFORM);

    _globalVertexBuffer = createBuffer(10000, backend::BufferUsage::VERTEX | backend::BufferUsage::TRANSFER_DST, backend::MemoryProperties::DEVICE_LOCAL);
    _vertexStagingBuffer = createBuffer(10000, backend::BufferUsage::TRANSFER_SRC);
    _globalIndexBuffer = createBuffer(10000, backend::BufferUsage::INDEX | backend::BufferUsage::TRANSFER_DST, backend::MemoryProperties::DEVICE_LOCAL);
    _indexStagingBuffer = createBuffer(10000, backend::BufferUsage::TRANSFER_SRC);

    _cube = new Mesh(shapes::cube().mesh(this));

}

cala::Engine::~Engine() {
    for (auto& buffer : _buffers)
        delete buffer;
    for (auto& image : _images)
        delete image;
}

bool cala::Engine::gc() {
    PROFILE_NAMED("Engine::gc");
    for (auto it = _buffersToDestroy.begin(); it != _buffersToDestroy.end(); it++) {
        auto& frame = it->first;
        auto& handle = it->second;
        if (frame <= 0) {
            u32 index = handle.index();
            delete _buffers[index];
            _buffers[index] = nullptr;
            _freeBuffers.push(index);
            _buffersToDestroy.erase(it--);
        } else
            --frame;
    }

    for (auto it = _imagesToDestroy.begin(); it != _imagesToDestroy.end(); it++) {
        auto& frame = it->first;
        auto& handle = it->second;
        if (frame <= 0) {
            u32 index = handle.index();
            _imageViews[index] = backend::vulkan::Image::View();
            _driver.updateBindlessImage(index, _imageViews[0], _defaultSampler);
            delete _images[index];
            _images[index] = nullptr;
            _freeImages.push(index);
            _imagesToDestroy.erase(it--);
        } else
            --frame;
    }

    if (_stagingReady) {

        if (_vertexStagingBuffer->size() > _globalVertexBuffer->size())
            _globalVertexBuffer = resizeBuffer(_globalVertexBuffer, _vertexStagingBuffer->size(), true);
        if (_indexStagingBuffer->size() > _globalIndexBuffer->size())
            _globalIndexBuffer = resizeBuffer(_globalIndexBuffer, _indexStagingBuffer->size(), true);

        _driver.immediate([&](backend::vulkan::CommandBuffer& cmd) { //TODO: async transfer queue
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


    return true;
}

cala::ImageHandle cala::Engine::convertToCubeMap(ImageHandle equirectangular) {
    auto cubeMap = createImage({
        512, 512, 1,
        backend::Format::RGBA32_SFLOAT,
        10, 6,
        backend::ImageUsage::STORAGE | backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_SRC | backend::ImageUsage::TRANSFER_DST
    });
    auto equirectangularView = equirectangular->newView();
    auto cubeView = cubeMap->newView(0, 10);
    _driver.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto envBarrier = cubeMap->barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, { &envBarrier, 1 });
//        auto hdrBarrier = equirectangular->barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY, backend::ImageLayout::SHADER_READ_ONLY);
//        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &hdrBarrier, 1 });


        cmd.bindProgram(*_equirectangularToCubeMap);
        cmd.bindImage(1, 0, equirectangularView, _lodSampler);
        cmd.bindImage(1, 1, cubeView, _defaultSampler, true);

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

cala::ImageHandle cala::Engine::generateIrradianceMap(ImageHandle cubeMap) {
    auto irradianceMap = createImage({
        64, 64, 1,
        backend::Format::RGBA32_SFLOAT,
        1, 6,
        backend::ImageUsage::STORAGE | backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST
    }, &_irradianceSampler);
    _driver.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto irradianceBarrier = irradianceMap->barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, { &irradianceBarrier, 1 });
        auto cubeBarrier = cubeMap->barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, { &cubeBarrier, 1 });

        cmd.bindProgram(*_irradianceProgram);
        cmd.bindImage(1, 0, getImageView(cubeMap), _lodSampler, true);
        cmd.bindImage(1, 1, getImageView(irradianceMap), _defaultSampler, true);
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

cala::ImageHandle cala::Engine::generatePrefilteredIrradiance(ImageHandle cubeMap) {
    ImageHandle prefilteredMap = createImage({
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


    _driver.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto prefilterBarrier = prefilteredMap->barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, { &prefilterBarrier, 1 });

        for (u32 mip = 0; mip < prefilteredMap->mips(); mip++) {
            cmd.bindProgram(*_prefilterProgram);
            cmd.bindImage(1, 0, getImageView(cubeMap), _lodSampler);
            cmd.bindImage(1, 1, mipViews[mip], _defaultSampler, true);
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


cala::BufferHandle cala::Engine::createBuffer(u32 size, backend::BufferUsage usage, backend::MemoryProperties flags) {
    u32 index = 0;
    if (!_freeBuffers.empty()) {
        index = _freeBuffers.pop().value();
        _buffers[index] = new backend::vulkan::Buffer(_driver, size, usage, flags);
    } else {
        index = _buffers.size();
        _buffers.emplace(new backend::vulkan::Buffer(_driver, size, usage, flags));
    }
    return { this, index };
}

cala::ImageHandle cala::Engine::createImage(backend::vulkan::Image::CreateInfo info, backend::vulkan::Sampler* sampler) {
    u32 index = 0;
    if (!_freeImages.empty()) {
        index = _freeImages.pop().value();
        _images[index] = new backend::vulkan::Image(_driver, info);
        _imageViews[index] = std::move(_images[index]->newView(0, _images[index]->mips()));
    } else {
        index = _images.size();
        _images.emplace(new backend::vulkan::Image(_driver, info));
        _imageViews.emplace(_images.back()->newView(0, _images.back()->mips()));
    }
    assert(_images.size() == _imageViews.size());
    backend::vulkan::Sampler* chosenSampler = sampler;
    if (!chosenSampler)
        chosenSampler = backend::isDepthFormat(info.format) ? &_shadowSampler : &_defaultSampler;

    _driver.updateBindlessImage(index, _imageViews[index], *chosenSampler);
    return { this, index };
}

cala::ProgramHandle cala::Engine::createProgram(backend::vulkan::ShaderProgram &&program) {
    u32 index = _programs.size();
    _programs.push(std::move(program));
    return { this, index };
}

void cala::Engine::destroyBuffer(BufferHandle handle) {
    _buffersToDestroy.push(std::make_pair(10, handle));
}

void cala::Engine::destroyImage(ImageHandle handle) {
    _imagesToDestroy.push(std::make_pair(10, handle));
}

cala::BufferHandle cala::Engine::resizeBuffer(BufferHandle handle, u32 size, bool transfer) {
    BufferHandle newBuffer = createBuffer(size, handle->usage());
    if (transfer) {

        _driver.immediate([&](backend::vulkan::CommandBuffer& cmd) {
            VkBufferCopy bufferCopy{};
            bufferCopy.dstOffset = 0;
            bufferCopy.srcOffset = 0;
            bufferCopy.size = handle->size();
            vkCmdCopyBuffer(cmd.buffer(), handle->buffer(), newBuffer->buffer(), 1, &bufferCopy);
        });

    }
    destroyBuffer(handle);
    return newBuffer;
}

cala::backend::vulkan::Image::View &cala::Engine::getImageView(ImageHandle handle) {
    return _imageViews[handle.index()];
}

cala::ImageHandle cala::Engine::getShadowMap(u32 index) {
    if (index < _shadowMaps.size())
        return _shadowMaps[index];

    auto map = createImage({
        1024, 1024, 1,
        backend::Format::D32_SFLOAT,
        1, 6,
        backend::ImageUsage::SAMPLED | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT | backend::ImageUsage::TRANSFER_DST
    });
    _shadowMaps.push(map);
    _driver.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto cubeBarrier = map->barrier(backend::Access::NONE, backend::Access::TRANSFER_WRITE, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TRANSFER, { &cubeBarrier, 1 });
    });
    return map;
}

void cala::Engine::updateMaterialdata() {
    if (_materialDataDirty) {
        if (_materialData.size() > _materialBuffer->size()) {
            _materialBuffer = resizeBuffer(_materialBuffer, _materialData.size(), true);
        }
        _materialBuffer->data(_materialData);
    }
    _materialDataDirty = false;
}


u32 cala::Engine::uploadVertexData(ende::Span<f32> data) {
    u32 currentOffset = _vertexOffset;
    if (currentOffset + data.size() >= _vertexStagingBuffer->size())
        _vertexStagingBuffer = resizeBuffer(_vertexStagingBuffer, currentOffset + data.size(), true);

    auto mapped = _vertexStagingBuffer->map(currentOffset, data.size());
    std::memcpy(mapped.address, data.data(), data.size());

    _vertexOffset += data.size();
    _stagingReady = true;
    return currentOffset;
}

u32 cala::Engine::uploadIndexData(ende::Span<u32> data) {
    u32 currentOffset = _indexOffset;
    if (currentOffset + data.size() >= _indexStagingBuffer->size())
        _indexStagingBuffer = resizeBuffer(_indexStagingBuffer, currentOffset + data.size(), true);

    auto mapped = _indexStagingBuffer->map(currentOffset, data.size());
    std::memcpy(mapped.address, data.data(), data.size());
    _indexOffset += data.size();
    _stagingReady = true;
    return currentOffset;
}

cala::Engine::Stats cala::Engine::stats() const {
    u32 allocatedBuffers = _buffers.size();
    u32 buffersInUse = allocatedBuffers - _freeBuffers.size();
    u32 allocatedImages = _images.size();
    u32 imagesInUse = allocatedImages - _freeImages.size();
    return {
        buffersInUse,
        allocatedBuffers,
        imagesInUse,
        allocatedImages
    };
}