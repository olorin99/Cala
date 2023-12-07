#include <Cala/vulkan/ShaderModule.h>
#include <Cala/vulkan/Device.h>

cala::vk::ShaderModule::ShaderModule(cala::vk::Device* device)
    : _device(device),
    _module(VK_NULL_HANDLE),
    _stage(ShaderStage::NONE),
    _interface({}, _stage)
{}