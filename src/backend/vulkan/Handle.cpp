#include "Cala/backend/vulkan/Handle.h"
#include <Cala/backend/vulkan/Device.h>

template <>
cala::backend::vulkan::Buffer &cala::backend::vulkan::BufferHandle::operator*() noexcept {
    return *_device->_buffers[_index].first;
}

template <>
cala::backend::vulkan::Buffer *cala::backend::vulkan::BufferHandle::operator->() noexcept {
    return _device->_buffers[_index].first.get();
}

template <>
bool cala::backend::vulkan::BufferHandle::isValid() const {
    return _device->_buffers[_index].first->buffer() != VK_NULL_HANDLE;
}

template <>
cala::backend::vulkan::Image &cala::backend::vulkan::ImageHandle::operator*() noexcept {
    return *_device->_images[_index].first;
}

template <>
cala::backend::vulkan::Image *cala::backend::vulkan::ImageHandle ::operator->() noexcept {
    return _device->_images[_index].first.get();
}

template <>
bool cala::backend::vulkan::ImageHandle::isValid() const {
    return _device->_images[_index].first->image() != VK_NULL_HANDLE;
}

template <>
cala::backend::vulkan::ShaderProgram &cala::backend::vulkan::ProgramHandle::operator*() noexcept {
    return *_device->_programs[_index].first;
}

template <>
cala::backend::vulkan::ShaderProgram *cala::backend::vulkan::ProgramHandle ::operator->() noexcept {
    return _device->_programs[_index].first.get();
}

template <>
bool cala::backend::vulkan::ProgramHandle::isValid() const {
    return _device->_programs[_index].first && _device->_programs[_index].first->layout() != VK_NULL_HANDLE;
}

template <>
cala::backend::vulkan::Sampler &cala::backend::vulkan::SamplerHandle::operator*() noexcept {
    return *_device->_samplers[_index].second;
}

template <>
cala::backend::vulkan::Sampler *cala::backend::vulkan::SamplerHandle ::operator->() noexcept {
    return _device->_samplers[_index].second.get();
}

template <>
bool cala::backend::vulkan::SamplerHandle::isValid() const {
    return _device->_samplers[_index].second && _device->_samplers[_index].second->sampler() != VK_NULL_HANDLE;
}