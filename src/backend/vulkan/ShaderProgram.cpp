#include "Cala/backend/vulkan/ShaderProgram.h"
#include <Cala/backend/vulkan/Device.h>
#include <Cala/backend/vulkan/primitives.h>
#include <SPIRV-Cross/spirv_cross.hpp>
#include <shaderc/shaderc.hpp>
#include <Ende/filesystem/File.h>

cala::backend::vulkan::ShaderProgram::ShaderProgram()
        : _device(nullptr),
          _pipelineLayout()
{}

cala::backend::vulkan::ShaderProgram::ShaderProgram(cala::backend::vulkan::Device* device, std::span<const cala::backend::vulkan::ShaderModuleHandle> modules)
    : _device(device),
      _pipelineLayout()
{
    std::vector<ShaderModuleInterface> interfaces;
    for (auto& module : modules) {
        _modules.push_back(module);
        interfaces.push_back(module->interface());
    }
    _pipelineLayout = _device->createPipelineLayout(ShaderInterface(interfaces));
}

cala::backend::vulkan::ShaderProgram::ShaderProgram(ShaderProgram &&rhs) noexcept
    : _device(nullptr),
    _pipelineLayout()
{
    if (this == &rhs)
        return;
    std::swap(_device, rhs._device);
    std::swap(_modules, rhs._modules);
    std::swap(_pipelineLayout, rhs._pipelineLayout);
}

cala::backend::vulkan::ShaderProgram &cala::backend::vulkan::ShaderProgram::operator=(ShaderProgram &&rhs) noexcept {
    if (this == &rhs)
        return *this;
    std::swap(_device, rhs._device);
    std::swap(_modules, rhs._modules);
    std::swap(_pipelineLayout, rhs._pipelineLayout);
    return *this;
}

VkPipelineLayout cala::backend::vulkan::ShaderProgram::layout() const {
    return _pipelineLayout->layout();
}

VkDescriptorSetLayout cala::backend::vulkan::ShaderProgram::setLayout(u32 set) const {
    return _pipelineLayout->setLayout(set);
}

const cala::backend::ShaderInterface &cala::backend::vulkan::ShaderProgram::interface() const {
    return _pipelineLayout->interface();
}