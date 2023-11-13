#include "Cala/backend/vulkan/ShaderModule.h"
#include <Cala/backend/vulkan/Device.h>

cala::backend::vulkan::ShaderModule::ShaderModule(cala::backend::vulkan::Device* device)
    : _device(device),
    _module(VK_NULL_HANDLE),
    _stage(ShaderStage::NONE),
    _interface({}, _stage)
{}