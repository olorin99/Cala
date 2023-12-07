#include <Cala/vulkan/Handle.h>
#include <Cala/vulkan/Device.h>
#include <Cala/vulkan/ShaderModule.h>

template <>
cala::vk::Buffer &cala::vk::BufferHandle::operator*() noexcept {
    return *_owner->_bufferList.getResource(_data->index);
}

template <>
cala::vk::Buffer &cala::vk::BufferHandle::operator*() const noexcept {
    return *_owner->_bufferList.getResource(_data->index);
}

template <>
cala::vk::Buffer *cala::vk::BufferHandle::operator->() noexcept {
    return _owner->_bufferList.getResource(_data->index);
}

template <>
cala::vk::Buffer *cala::vk::BufferHandle::operator->() const noexcept {
    return _owner->_bufferList.getResource(_data->index);
}

template <>
bool cala::vk::BufferHandle::isValid() const {
    return _owner->_bufferList.allocated() > _data->index && _owner->_bufferList.getResource(_data->index)->buffer() != VK_NULL_HANDLE;
}



template <>
cala::vk::Image &cala::vk::ImageHandle::operator*() noexcept {
    return *_owner->_imageList.getResource(_data->index);
}

template <>
cala::vk::Image *cala::vk::ImageHandle::operator->() noexcept {
    return _owner->_imageList.getResource(_data->index);
}

template <>
cala::vk::Image *cala::vk::ImageHandle::operator->() const noexcept {
    return _owner->_imageList.getResource(_data->index);
}

template <>
bool cala::vk::ImageHandle::isValid() const {
    return _owner->_imageList.allocated() > _data->index && _owner->_imageList.getResource(_data->index)->image() != VK_NULL_HANDLE;
}


template <>
cala::vk::ShaderModule &cala::vk::ShaderModuleHandle::operator*() noexcept {
    return *_owner->_shaderModulesList.getResource(_data->index);
}

template <>
cala::vk::ShaderModule *cala::vk::ShaderModuleHandle::operator->() noexcept {
    return _owner->_shaderModulesList.getResource(_data->index);
}

template <>
cala::vk::ShaderModule *cala::vk::ShaderModuleHandle::operator->() const noexcept {
    return _owner->_shaderModulesList.getResource(_data->index);
}

template <>
bool cala::vk::ShaderModuleHandle::isValid() const {
    return _owner->_shaderModulesList.allocated() > _data->index && _owner->_shaderModulesList.getResource(_data->index)->module() != VK_NULL_HANDLE;
}



template <>
cala::vk::Sampler &cala::vk::SamplerHandle::operator*() noexcept {
    return *_owner->_samplers[_index].second;
}

template <>
cala::vk::Sampler *cala::vk::SamplerHandle::operator->() noexcept {
    return _owner->_samplers[_index].second.get();
}

template <>
cala::vk::Sampler *cala::vk::SamplerHandle::operator->() const noexcept {
    return _owner->_samplers[_index].second.get();
}

template <>
bool cala::vk::SamplerHandle::isValid() const {
    return _owner->_samplers[_index].second && _owner->_samplers[_index].second->sampler() != VK_NULL_HANDLE;
}



template <>
cala::vk::CommandBuffer &cala::vk::CommandHandle::operator*() noexcept {
    return _owner->_buffers[_index];
}

template <>
cala::vk::CommandBuffer *cala::vk::CommandHandle::operator->() noexcept {
    return &_owner->_buffers[_index];
}

template <>
cala::vk::CommandBuffer *cala::vk::CommandHandle::operator->() const noexcept {
    return &_owner->_buffers[_index];
}

template <>
bool cala::vk::CommandHandle::isValid() const {
    return _owner->_buffers.size() > _index && _owner->_buffers[_index].buffer() != VK_NULL_HANDLE;
}



template <>
cala::vk::PipelineLayout &cala::vk::PipelineLayoutHandle::operator*() noexcept {
    return *_owner->_pipelineLayoutList.getResource(_data->index);
}

template <>
cala::vk::PipelineLayout *cala::vk::PipelineLayoutHandle::operator->() noexcept {
    return _owner->_pipelineLayoutList.getResource(_data->index);
}

template <>
cala::vk::PipelineLayout *cala::vk::PipelineLayoutHandle::operator->() const noexcept {
    return _owner->_pipelineLayoutList.getResource(_data->index);
}

template <>
bool cala::vk::PipelineLayoutHandle::isValid() const {
    return _owner->_pipelineLayoutList.allocated() > _data->index && _owner->_pipelineLayoutList.getResource(_data->index)->layout() != VK_NULL_HANDLE;
}