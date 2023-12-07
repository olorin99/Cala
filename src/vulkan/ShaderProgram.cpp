#include <Cala/vulkan/ShaderProgram.h>
#include <Cala/vulkan/Device.h>
#include <Cala/vulkan/primitives.h>
#include "SPIRV-Cross/spirv_cross.hpp"
#include <shaderc/shaderc.hpp>
#include "Ende/include/Ende/filesystem/File.h"

cala::vk::ShaderProgram::ShaderProgram()
        : _device(nullptr),
          _pipelineLayout()
{}

cala::vk::ShaderProgram::ShaderProgram(cala::vk::Device* device, std::span<const cala::vk::ShaderModuleHandle> modules)
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

cala::vk::ShaderProgram::ShaderProgram(ShaderProgram &&rhs) noexcept
    : _device(nullptr),
    _pipelineLayout()
{
    if (this == &rhs)
        return;
    std::swap(_device, rhs._device);
    std::swap(_modules, rhs._modules);
    std::swap(_pipelineLayout, rhs._pipelineLayout);
}

cala::vk::ShaderProgram &cala::vk::ShaderProgram::operator=(ShaderProgram &&rhs) noexcept {
    if (this == &rhs)
        return *this;
    std::swap(_device, rhs._device);
    std::swap(_modules, rhs._modules);
    std::swap(_pipelineLayout, rhs._pipelineLayout);
    return *this;
}

VkPipelineLayout cala::vk::ShaderProgram::layout() const {
    return _pipelineLayout->layout();
}

VkDescriptorSetLayout cala::vk::ShaderProgram::setLayout(u32 set) const {
    return _pipelineLayout->setLayout(set);
}

const cala::vk::ShaderInterface &cala::vk::ShaderProgram::interface() const {
    return _pipelineLayout->interface();
}