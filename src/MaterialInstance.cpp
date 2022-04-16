#include "Cala/MaterialInstance.h"
#include <Cala/Material.h>
#include <Cala/backend/vulkan/CommandBuffer.h>

cala::MaterialInstance::MaterialInstance(Material &material, u32 offset)
    : _material(&material),
      _offset(offset),
      _samplers(material._driver)
{}

cala::MaterialInstance::MaterialInstance(MaterialInstance &&rhs) noexcept
    : _samplers(rhs._material->_driver)
{
    std::swap(_material, rhs._material);
    std::swap(_samplers, rhs._samplers);
}

bool cala::MaterialInstance::setUniform(u32 set, const char *name, u8 *data, u32 size) {
    i32 offset = _material->_program.interface().getUniformOffset(set, name);
    if (offset < 0)
        return false;
    if (material()->_uniformData.size() < offset + size)
        return false;
    std::memcpy(&material()->_uniformData[offset], data, size);
    material()->_uniformBuffer.data({material()->_uniformData.data(), static_cast<u32>(material()->_uniformData.size())});
    return true;
}

bool cala::MaterialInstance::setUniform(const char *name, u8 *data, u32 size) {
    return setUniform(2, name, data, size);
}

bool cala::MaterialInstance::setSampler(u32 set, const char *name, backend::vulkan::Image::View &&view, backend::vulkan::Sampler&& sampler) {
    i32 binding = material()->_program.interface().getSamplerBinding(set, name);
    if (binding < 0)
        return false;
    _samplers.set(binding, std::forward<backend::vulkan::Image::View>(view), std::forward<backend::vulkan::Sampler>(sampler));
    return true;
}

bool cala::MaterialInstance::setSampler(const char *name, backend::vulkan::Image::View &&view, backend::vulkan::Sampler &&sampler) {
    return setSampler(2, name, std::forward<backend::vulkan::Image::View>(view), std::forward<backend::vulkan::Sampler>(sampler));
}

void cala::MaterialInstance::bind(backend::vulkan::CommandBuffer &cmd, u32 set, u32 first) {
    for (u32 i = 0; i < _samplers.size(); i++) {
        auto viewPair = _samplers.get(i);
        cmd.bindImage(set, first + i, viewPair.first, viewPair.second);
    }
}