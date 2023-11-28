#include "Cala/backend/vulkan/Handle.h"
#include <Cala/backend/vulkan/Device.h>
#include <Cala/backend/vulkan/ShaderModule.h>

template <>
cala::backend::vulkan::Buffer &cala::backend::vulkan::BufferHandle::operator*() noexcept {
    return *_owner->_bufferList.getResource(_data->index);
}

template <>
cala::backend::vulkan::Buffer &cala::backend::vulkan::BufferHandle::operator*() const noexcept {
    return *_owner->_bufferList.getResource(_data->index);
}

template <>
cala::backend::vulkan::Buffer *cala::backend::vulkan::BufferHandle::operator->() noexcept {
    return _owner->_bufferList.getResource(_data->index);
}

template <>
cala::backend::vulkan::Buffer *cala::backend::vulkan::BufferHandle::operator->() const noexcept {
    return _owner->_bufferList.getResource(_data->index);
}

template <>
bool cala::backend::vulkan::BufferHandle::isValid() const {
    return _owner->_bufferList.allocated() > _data->index && _owner->_bufferList.getResource(_data->index)->buffer() != VK_NULL_HANDLE;
}



template <>
cala::backend::vulkan::Image &cala::backend::vulkan::ImageHandle::operator*() noexcept {
    return *_owner->_imageList.getResource(_data->index);
}

template <>
cala::backend::vulkan::Image *cala::backend::vulkan::ImageHandle::operator->() noexcept {
    return _owner->_imageList.getResource(_data->index);
}

template <>
cala::backend::vulkan::Image *cala::backend::vulkan::ImageHandle::operator->() const noexcept {
    return _owner->_imageList.getResource(_data->index);
}

template <>
bool cala::backend::vulkan::ImageHandle::isValid() const {
    return _owner->_imageList.allocated() > _data->index && _owner->_imageList.getResource(_data->index)->image() != VK_NULL_HANDLE;
}


template <>
cala::backend::vulkan::ShaderModule &cala::backend::vulkan::ShaderModuleHandle::operator*() noexcept {
    return *_owner->_shaderModulesList.getResource(_data->index);
}

template <>
cala::backend::vulkan::ShaderModule *cala::backend::vulkan::ShaderModuleHandle::operator->() noexcept {
    return _owner->_shaderModulesList.getResource(_data->index);
}

template <>
cala::backend::vulkan::ShaderModule *cala::backend::vulkan::ShaderModuleHandle::operator->() const noexcept {
    return _owner->_shaderModulesList.getResource(_data->index);
}

template <>
bool cala::backend::vulkan::ShaderModuleHandle::isValid() const {
    return _owner->_shaderModulesList.allocated() > _data->index && _owner->_shaderModulesList.getResource(_data->index)->module() != VK_NULL_HANDLE;
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
    return *_owner->_pipelineLayoutList.getResource(_data->index);
}

template <>
cala::backend::vulkan::PipelineLayout *cala::backend::vulkan::PipelineLayoutHandle::operator->() noexcept {
    return _owner->_pipelineLayoutList.getResource(_data->index);
}

template <>
cala::backend::vulkan::PipelineLayout *cala::backend::vulkan::PipelineLayoutHandle::operator->() const noexcept {
    return _owner->_pipelineLayoutList.getResource(_data->index);
}

template <>
bool cala::backend::vulkan::PipelineLayoutHandle::isValid() const {
    return _owner->_pipelineLayoutList.allocated() > _data->index && _owner->_pipelineLayoutList.getResource(_data->index)->layout() != VK_NULL_HANDLE;
}