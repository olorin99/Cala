#include "Cala/backend/vulkan/Handle.h"
#include <Cala/backend/vulkan/Device.h>
#include <Cala/backend/vulkan/ShaderModule.h>

template <>
cala::backend::vulkan::Buffer &cala::backend::vulkan::BufferHandle::operator*() noexcept {
    return *_owner->_buffers[_index].first;
}

template <>
cala::backend::vulkan::Buffer *cala::backend::vulkan::BufferHandle::operator->() noexcept {
    return _owner->_buffers[_index].first.get();
}

template <>
cala::backend::vulkan::Buffer *cala::backend::vulkan::BufferHandle::operator->() const noexcept {
    return _owner->_buffers[_index].first.get();
}

template <>
bool cala::backend::vulkan::BufferHandle::isValid() const {
    return _owner->_buffers[_index].first->buffer() != VK_NULL_HANDLE;
}



template <>
cala::backend::vulkan::Image &cala::backend::vulkan::ImageHandle::operator*() noexcept {
    return *_owner->_images[_index].first;
}

template <>
cala::backend::vulkan::Image *cala::backend::vulkan::ImageHandle::operator->() noexcept {
    return _owner->_images[_index].first.get();
}

template <>
cala::backend::vulkan::Image *cala::backend::vulkan::ImageHandle::operator->() const noexcept {
    return _owner->_images[_index].first.get();
}

template <>
bool cala::backend::vulkan::ImageHandle::isValid() const {
    return _owner->_images[_index].first->image() != VK_NULL_HANDLE;
}



template <>
cala::backend::vulkan::ShaderProgram &cala::backend::vulkan::ProgramHandle::operator*() noexcept {
    return *_owner->_programs[_index].first;
}

template <>
cala::backend::vulkan::ShaderProgram *cala::backend::vulkan::ProgramHandle::operator->() noexcept {
    return _owner->_programs[_index].first.get();
}

template <>
cala::backend::vulkan::ShaderProgram *cala::backend::vulkan::ProgramHandle::operator->() const noexcept {
    return _owner->_programs[_index].first.get();
}

template <>
bool cala::backend::vulkan::ProgramHandle::isValid() const {
    return _owner->_programs[_index].first && _owner->_programs[_index].first->layout() != VK_NULL_HANDLE;
}



template <>
cala::backend::vulkan::ShaderModule &cala::backend::vulkan::ShaderModuleHandle::operator*() noexcept {
    return *_owner->_shaderModules[_index].first;
}

template <>
cala::backend::vulkan::ShaderModule *cala::backend::vulkan::ShaderModuleHandle::operator->() noexcept {
    return _owner->_shaderModules[_index].first.get();
}

template <>
cala::backend::vulkan::ShaderModule *cala::backend::vulkan::ShaderModuleHandle::operator->() const noexcept {
    return _owner->_shaderModules[_index].first.get();
}

template <>
bool cala::backend::vulkan::ShaderModuleHandle::isValid() const {
    return _owner->_shaderModules[_index].first && _owner->_shaderModules[_index].first->module() != VK_NULL_HANDLE;
}



template <>
cala::backend::vulkan::Sampler &cala::backend::vulkan::SamplerHandle::operator*() noexcept {
    return *_owner->_samplers[_index].second;
}

template <>
cala::backend::vulkan::Sampler *cala::backend::vulkan::SamplerHandle::operator->() noexcept {
    return _owner->_samplers[_index].second.get();
}

template <>
cala::backend::vulkan::Sampler *cala::backend::vulkan::SamplerHandle::operator->() const noexcept {
    return _owner->_samplers[_index].second.get();
}

template <>
bool cala::backend::vulkan::SamplerHandle::isValid() const {
    return _owner->_samplers[_index].second && _owner->_samplers[_index].second->sampler() != VK_NULL_HANDLE;
}



template <>
cala::backend::vulkan::CommandBuffer &cala::backend::vulkan::CommandHandle::operator*() noexcept {
    return _owner->_buffers[_index];
}

template <>
cala::backend::vulkan::CommandBuffer *cala::backend::vulkan::CommandHandle::operator->() noexcept {
    return &_owner->_buffers[_index];
}

template <>
cala::backend::vulkan::CommandBuffer *cala::backend::vulkan::CommandHandle::operator->() const noexcept {
    return &_owner->_buffers[_index];
}

template <>
bool cala::backend::vulkan::CommandHandle::isValid() const {
    return _owner->_buffers.size() > _index && _owner->_buffers[_index].buffer() != VK_NULL_HANDLE;
}



template <>
cala::backend::vulkan::PipelineLayout &cala::backend::vulkan::PipelineLayoutHandle::operator*() noexcept {
    return *_owner->_pipelineLayoutList.getResource(_index);
}

template <>
cala::backend::vulkan::PipelineLayout *cala::backend::vulkan::PipelineLayoutHandle::operator->() noexcept {
    return _owner->_pipelineLayoutList.getResource(_index);
}

template <>
cala::backend::vulkan::PipelineLayout *cala::backend::vulkan::PipelineLayoutHandle::operator->() const noexcept {
    return _owner->_pipelineLayoutList.getResource(_index);
}

template <>
bool cala::backend::vulkan::PipelineLayoutHandle::isValid() const {
    return _owner->_pipelineLayoutList._resources.size() > _index && _owner->_pipelineLayoutList.getResource(_index)->layout() != VK_NULL_HANDLE;
}