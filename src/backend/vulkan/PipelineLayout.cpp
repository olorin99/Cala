#include "Cala/backend/vulkan/PipelineLayout.h"
#include <Cala/backend/vulkan/primitives.h>
#include <Cala/backend/vulkan/Device.h>

cala::backend::vulkan::PipelineLayout::PipelineLayout(cala::backend::vulkan::Device *device)
    : _device(device),
      _interface({}),
      _layout(VK_NULL_HANDLE)
{
    for (auto& setLayout : _setLayouts)
        setLayout = VK_NULL_HANDLE;
}

cala::backend::vulkan::PipelineLayout::PipelineLayout(cala::backend::vulkan::Device *device, const cala::backend::ShaderInterface &interface)
    : _device(device),
      _interface(interface)
{
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
        _setLayouts[i] = setLayouts[i];
}

cala::backend::vulkan::PipelineLayout::~PipelineLayout() {
    if (_device == VK_NULL_HANDLE || _layout == VK_NULL_HANDLE)
        return;

    vkDestroyPipelineLayout(_device->context().device(), _layout, nullptr);
}

cala::backend::vulkan::PipelineLayout::PipelineLayout(cala::backend::vulkan::PipelineLayout &&rhs) noexcept
    : _device(nullptr),
    _interface({}),
    _layout(VK_NULL_HANDLE)
{
    std::swap(_device, rhs._device);
    std::swap(_interface, rhs._interface);
    std::swap(_layout, rhs._layout);
    for (u32 i = 0; i < MAX_SET_COUNT; i++) {
        _setLayouts[i] = VK_NULL_HANDLE;
        std::swap(_setLayouts[i], rhs._setLayouts[i]);
    }
}

cala::backend::vulkan::PipelineLayout &cala::backend::vulkan::PipelineLayout::operator=(cala::backend::vulkan::PipelineLayout &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(_interface, rhs._interface);
    std::swap(_layout, rhs._layout);
    for (u32 i = 0; i < MAX_SET_COUNT; i++)
        std::swap(_setLayouts[i], rhs._setLayouts[i]);
    return *this;
}