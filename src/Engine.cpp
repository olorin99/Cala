#include "Cala/Engine.h"


cala::Engine::Engine(backend::Platform &platform)
    : _driver(platform)
{}


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