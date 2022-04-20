#include "Cala/backend/vulkan/SamplerArray.h"

cala::backend::vulkan::SamplerArray::SamplerArray(Driver &driver)
    : _driver(driver),
    _samplers{Sampler(driver, {}), Sampler(driver, {}), Sampler(driver, {}), Sampler(driver, {}), Sampler(driver, {})},
    _count(0)
{}

cala::backend::vulkan::SamplerArray::SamplerArray(SamplerArray &&rhs) noexcept
    : SamplerArray(rhs._driver)
{
    std::swap(_views, rhs._views);
    std::swap(_samplers, rhs._samplers);
    std::swap(_count, rhs._count);
}

cala::backend::vulkan::SamplerArray &cala::backend::vulkan::SamplerArray::operator=(SamplerArray &&rhs) noexcept {
    std::swap(_views, rhs._views);
    std::swap(_samplers, rhs._samplers);
    std::swap(_count, rhs._count);
    return *this;
}

void cala::backend::vulkan::SamplerArray::set(u32 index, Image::View &&view, Sampler &&sampler) {
    _views[index] = std::move(view);
    _samplers[index] = std::move(sampler);
    if (_count <= index)
        _count = index + 1;
}

std::pair<cala::backend::vulkan::Image::View &, cala::backend::vulkan::Sampler &> cala::backend::vulkan::SamplerArray::get(u32 i) {
    return { _views[i], _samplers[i] };
}

std::array<VkImageView, 5> cala::backend::vulkan::SamplerArray::views() const {
    std::array<VkImageView, 5> vs = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    u32 i = 0;
    for (auto& view : _views) {
        vs[i++] = view.view;
    }
    return vs;
}