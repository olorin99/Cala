#include "Cala/Engine.h"
#include <Ende/filesystem/File.h>
#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/Probe.h>
#include <Cala/shapes.h>

template <>
cala::backend::vulkan::Buffer &cala::BufferHandle::operator*() noexcept {
    return _engine->_buffers[_index];
}
template <>
cala::backend::vulkan::Buffer *cala::BufferHandle::operator->() noexcept {
    return &_engine->_buffers[_index];
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
      _shadowPass(_driver, { &shadowPassAttachment, 1 }),
      _defaultSampler(_driver, {}),
      _cube(shapes::cube().mesh(_driver)),
      _shadowSampler(_driver, {
          .filter = VK_FILTER_NEAREST,
          .addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
          .borderColour = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
      }),
      _materialDataDirty(true)
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
        file.open("../../res/shaders/fullscreen.vert.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> vertexData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(vertexData.data()), static_cast<u32>(vertexData.size() * sizeof(u32)) });

        file.open("../../res/shaders/hdr.frag.spv"_path, ende::fs::in | ende::fs::binary);
        ende::Vector<u32> fragmentData(file.size() / sizeof(u32));
        file.read({ reinterpret_cast<char*>(fragmentData.data()), static_cast<u32>(fragmentData.size() * sizeof(u32)) });

        _tonemapProgram = createProgram(backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
                .addStage(vertexData, backend::ShaderStage::VERTEX)
                .addStage(fragmentData, backend::ShaderStage::FRAGMENT)
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
        backend::ImageLayout::UNDEFINED,
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
        backend::ImageLayout::UNDEFINED,
        backend::ImageType::IMAGE2D
    });

    _defaultDirectionalShadow->data(_driver, {
            0, 1, 1, 1, 4, { &data, sizeof(data) }
    });



    _driver.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto cubeBarrier = _defaultPointShadow->barrier(backend::Access::NONE, backend::Access::TRANSFER_WRITE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::SHADER_READ_ONLY);
        auto flatBarrier = _defaultDirectionalShadow->barrier(backend::Access::NONE, backend::Access::TRANSFER_WRITE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TRANSFER, 0, nullptr, { &cubeBarrier, 1 });
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TRANSFER, 0, nullptr, { &flatBarrier, 1 });
    });

    _defaultPointShadowView = _defaultPointShadow->newView();
    _defaultDirectionalShadowView = _defaultDirectionalShadow->newView();

    _materialBuffer = createBuffer(256, backend::BufferUsage::UNIFORM);

}

cala::Engine::~Engine() {
    for (auto& image : _images)
        delete image;
}

bool cala::Engine::gc() {
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
    return true;
}

cala::BufferHandle cala::Engine::createBuffer(u32 size, backend::BufferUsage usage, backend::MemoryProperties flags) {
    _buffers.emplace(_driver, size, usage, flags);
    u32 index = _buffers.size() - 1;
    return { this, index };
}


cala::ImageHandle cala::Engine::createImage(backend::vulkan::Image::CreateInfo info) {
    u32 index = 0;
    if (!_freeImages.empty()) {
        index = _freeImages.pop().value();
        _images[index] = new backend::vulkan::Image(_driver, info);
        _imageViews[index] = std::move(_images[index]->newView());
    } else {
        index = _images.size();
        _images.emplace(new backend::vulkan::Image(_driver, info));
        _imageViews.emplace(_images.back()->newView());
    }
    assert(_images.size() == _imageViews.size());
    _driver.updateBindlessImage(index, _imageViews[index], backend::isDepthFormat(info.format) ? _shadowSampler : _defaultSampler);
    return { this, index };
}

cala::ProgramHandle cala::Engine::createProgram(backend::vulkan::ShaderProgram &&program) {
    u32 index = _programs.size();
    _programs.push(std::move(program));
    return { this, index };
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
        auto envBarrier = cubeMap->barrier(backend::Access::NONE, backend::Access::SHADER_WRITE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::GENERAL);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &envBarrier, 1 });
        auto hdrBarrier = equirectangular->barrier(backend::Access::NONE, backend::Access::SHADER_READ, backend::ImageLayout::UNDEFINED, backend::ImageLayout::SHADER_READ_ONLY);
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::COMPUTE_SHADER, 0, nullptr, { &hdrBarrier, 1 });


        cmd.bindProgram(*_equirectangularToCubeMap);
        cmd.bindImage(1, 0, equirectangularView, _defaultSampler);
        cmd.bindImage(1, 1, cubeView, _defaultSampler, true);

        cmd.bindPipeline();
        cmd.bindDescriptors();

        cmd.dispatchCompute(512 / 32, 512 / 32, 6);


        envBarrier = cubeMap->barrier(backend::Access::SHADER_WRITE, backend::Access::NONE, backend::ImageLayout::GENERAL, backend::ImageLayout::TRANSFER_DST);
        cmd.pipelineBarrier(backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::BOTTOM, 0, nullptr, { &envBarrier, 1 });
        cubeMap->generateMips(cmd);
    });
    return cubeMap;
}

void cala::Engine::destroyImage(ImageHandle handle) {
    _imagesToDestroy.push(std::make_pair(10, handle));
}

cala::backend::vulkan::Image::View &cala::Engine::getImageView(ImageHandle handle) {
    return _imageViews[handle.index()];
}

cala::Probe &cala::Engine::getShadowProbe(u32 index) {
    if (index < _shadowProbes.size())
        return _shadowProbes[index];

    _shadowProbes.emplace_back(this, Probe::ProbeInfo{
        1024, 1024,
        backend::Format::D32_SFLOAT,
        backend::ImageUsage::SAMPLED | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT,
        &_shadowPass
    });

    return _shadowProbes[std::min((u64)index, _shadowProbes.size() - 1)];
}

void cala::Engine::updateMaterialdata() {
    if (_materialDataDirty)
        _materialBuffer->data(_materialData);
    _materialDataDirty = false;
}