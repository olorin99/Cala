#include "Cala/backend/vulkan/Handle.h"
#include <Cala/backend/vulkan/Device.h>

template <>
cala::backend::vulkan::Buffer &cala::backend::vulkan::BufferHandle::operator*() noexcept {
    return *_device->_buffers[_index];
}

template <>
cala::backend::vulkan::Buffer *cala::backend::vulkan::BufferHandle::operator->() noexcept {
    return _device->_buffers[_index];
}

template <>
bool cala::backend::vulkan::BufferHandle::isValid() const {
    return _device->_buffers[_index] != nullptr;
}

template <>
cala::backend::vulkan::Image &cala::backend::vulkan::ImageHandle::operator*() noexcept {
    return *_device->_images[_index];
}

template <>
cala::backend::vulkan::Image *cala::backend::vulkan::ImageHandle ::operator->() noexcept {
    return _device->_images[_index];
}

template <>
bool cala::backend::vulkan::ImageHandle::isValid() const {
    return _device->_images[_index] != nullptr;
}

template <>
cala::backend::vulkan::ShaderProgram &cala::backend::vulkan::ProgramHandle::operator*() noexcept {
    return *_device->_programs[_index];
}

template <>
cala::backend::vulkan::ShaderProgram *cala::backend::vulkan::ProgramHandle ::operator->() noexcept {
    return _device->_programs[_index];
}

template <>
bool cala::backend::vulkan::ProgramHandle ::isValid() const {
    return _device->_programs[_index] != nullptr;
}