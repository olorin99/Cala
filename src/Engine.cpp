#include "Cala/Engine.h"
#include <Ende/filesystem/File.h>

#include <Cala/Probe.h>

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
    : _driver(platform),
      _shadowPass(_driver, { &shadowPassAttachment, 1 }),
      _defaultSampler(_driver, {})
{
    ende::fs::File file;
    file.open("../../res/shaders/shadow_point.vert.spv"_path, ende::fs::in | ende::fs::binary);
    ende::Vector<u32> vertexData(file.size() / sizeof(u32));
    file.read({ reinterpret_cast<char*>(vertexData.data()), static_cast<u32>(vertexData.size() * sizeof(u32)) });

    file.open("../../res/shaders/shadow_point.frag.spv"_path, ende::fs::in | ende::fs::binary);
    ende::Vector<u32> fragmentData(file.size() / sizeof(u32));
    file.read({ reinterpret_cast<char*>(fragmentData.data()), static_cast<u32>(fragmentData.size() * sizeof(u32)) });

    _pointShadowProgram = new backend::vulkan::ShaderProgram(backend::vulkan::ShaderProgram::create()
            .addStage(vertexData, backend::ShaderStage::VERTEX)
            .addStage(fragmentData, backend::ShaderStage::FRAGMENT)
            .compile(_driver));

}

cala::Engine::~Engine() {
    delete _pointShadowProgram;
}


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
    return _engine->_images[_index];
}
template <>
cala::backend::vulkan::Image *cala::ImageHandle::operator->() noexcept {
    return &_engine->_images[_index];
}




cala::BufferHandle cala::Engine::createBuffer(u32 size, backend::BufferUsage usage, backend::MemoryProperties flags) {
    _buffers.emplace(_driver, size, usage, flags);
    u32 index = _buffers.size() - 1;
    return { this, index };
}


cala::ImageHandle cala::Engine::createImage(backend::vulkan::Image::CreateInfo info) {
    _images.emplace(_driver, info);
    u32 index = _images.size() - 1;
    return { this, index };
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