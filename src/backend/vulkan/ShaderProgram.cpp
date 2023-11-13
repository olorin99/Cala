#include "Cala/backend/vulkan/ShaderProgram.h"
#include <Cala/backend/vulkan/Device.h>
#include <Cala/backend/vulkan/primitives.h>
#include <SPIRV-Cross/spirv_cross.hpp>
#include <shaderc/shaderc.hpp>
#include <Ende/filesystem/File.h>

cala::backend::vulkan::ShaderProgram::ShaderProgram(cala::backend::vulkan::Device* device, std::span<const cala::backend::vulkan::ShaderModuleHandle> modules)
    : _device(device),
    _layout(VK_NULL_HANDLE),
    _setLayout{},
    _interface({}),
    _stageFlags(ShaderStage::NONE)
{
    std::vector<ShaderModuleInterface> interfaces;
    for (auto& module : modules) {
        _stageFlags |= module->stage();
        _modules.push_back(module);
        interfaces.push_back(module->interface());
    }
    _interface = ShaderInterface(interfaces);

    VkDescriptorSetLayout setLayouts[MAX_SET_COUNT] = {};

    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        if (_device->getBindlessIndex() == i) {
            setLayouts[i] = _device->bindlessLayout();
            continue;
        }
        VkDescriptorSetLayoutBinding layoutBinding[MAX_BINDING_PER_SET];
        u32 layoutBindingCount = 0;
        auto& set = _interface.sets[i];

        for (u32 j = 0; set.bindingCount > 0 && j < MAX_BINDING_PER_SET; j++) {
            auto& binding = set.bindings[j];

            if (binding.type == ShaderInterface::BindingType::NONE)
                break;

            layoutBinding[layoutBindingCount].binding = j;
            VkDescriptorType type;
            switch (binding.type) {
                case ShaderInterface::BindingType::UNIFORM_BUFFER:
                    type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    break;
                case ShaderInterface::BindingType::SAMPLED_IMAGE:
                    type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    break;
                case ShaderInterface::BindingType::STORAGE_IMAGE:
                    type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    break;
                case ShaderInterface::BindingType::STORAGE_BUFFER:
                    type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    break;
                default:
                    type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            }
            layoutBinding[layoutBindingCount].descriptorType = type;
            layoutBinding[layoutBindingCount].descriptorCount = 1;
            layoutBinding[layoutBindingCount].stageFlags = getShaderStage(binding.stage);
            layoutBinding[layoutBindingCount].pImmutableSamplers = nullptr;
            layoutBindingCount++;
        }

        setLayouts[i] = _device->getSetLayout({layoutBinding, layoutBindingCount});
    }
    VkPushConstantRange pushConstantRange[10]{};
    for (u32 i = 0; i < _interface.pushConstantRanges && i < 10; i++) {
        pushConstantRange[i].stageFlags |= getShaderStage(_interface.pushConstants[i].stage);
        pushConstantRange[i].size = _interface.pushConstants[i].byteSize;
        pushConstantRange[i].offset = _interface.pushConstants[i].offset;
    }


    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = MAX_SET_COUNT;
    pipelineLayoutInfo.pSetLayouts = setLayouts;

    pipelineLayoutInfo.pushConstantRangeCount = _interface.pushConstantRanges;
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRange;

    VkPipelineLayout  pipelineLayout;
    VK_TRY(vkCreatePipelineLayout(_device->context().device(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

    _layout = pipelineLayout;
    for (u32 i = 0; i < MAX_SET_COUNT; i++)
        _setLayout[i] = setLayouts[i];
}

cala::backend::vulkan::ShaderProgram::~ShaderProgram() {
    if (_device == VK_NULL_HANDLE) return;

    vkDestroyPipelineLayout(_device->context().device(), _layout, nullptr);
}

cala::backend::vulkan::ShaderProgram::ShaderProgram(ShaderProgram &&rhs) noexcept
    : _device(VK_NULL_HANDLE),
    _layout(VK_NULL_HANDLE),
    _setLayout{},
    _interface({}),
    _stageFlags(ShaderStage::NONE)
{
    if (this == &rhs)
        return;
    std::swap(_device, rhs._device);
    std::swap(_modules, rhs._modules);
    for (u32 i = 0; i < MAX_SET_COUNT; i++)
        _setLayout[i] = rhs._setLayout[i];
    std::swap(_layout, rhs._layout);
    std::swap(_interface, rhs._interface);
    std::swap(_stageFlags, rhs._stageFlags);
}

cala::backend::vulkan::ShaderProgram &cala::backend::vulkan::ShaderProgram::operator=(ShaderProgram &&rhs) noexcept {
    if (this == &rhs)
        return *this;
    std::swap(_device, rhs._device);
    std::swap(_device, rhs._device);
    std::swap(_modules, rhs._modules);
    for (u32 i = 0; i < MAX_SET_COUNT; i++)
        _setLayout[i] = rhs._setLayout[i];
    std::swap(_layout, rhs._layout);
    std::swap(_interface, rhs._interface);
    std::swap(_stageFlags, rhs._stageFlags);
    return *this;
}

VkPipelineLayout cala::backend::vulkan::ShaderProgram::layout() {
    if (_layout != VK_NULL_HANDLE)
        return _layout;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VK_TRY(vkCreatePipelineLayout(_device->context().device(), &pipelineLayoutInfo, nullptr, &_layout));
    return _layout;
}

bool cala::backend::vulkan::ShaderProgram::stagePresent(ShaderStage stageFlags) const {
    return (_stageFlags & stageFlags) == stageFlags;
}