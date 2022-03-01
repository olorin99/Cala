#include "Cala/MaterialInstance.h"
#include <Cala/Material.h>

cala::MaterialInstance::MaterialInstance(Material &material, u32 offset)
    : _material(&material),
    _offset(offset)
{}

cala::MaterialInstance &cala::MaterialInstance::addImage(cala::backend::vulkan::Image::View &&view) {
    _samplers.push(std::forward<cala::backend::vulkan::Image::View>(view));
    return *this;
}

bool cala::MaterialInstance::setUniform(const char *name, u8 *data, u32 size) {
    i32 offset = _material->_program._interface.getUniformOffset(2, name);
    if (offset < 0)
        return false;
    if (material()->_uniformData.size() < offset + size)
        return false;
    std::memcpy(&material()->_uniformData[offset], data, size);
    material()->_uniformBuffer.data({material()->_uniformData.data(), static_cast<u32>(material()->_uniformData.size())});
    return true;
}

bool cala::MaterialInstance::setSampler(const char *name, cala::backend::vulkan::Image::View &&view) {
    i32 binding = material()->_program._interface.getSamplerBinding(2, name);
    if (binding < 0)
        return false;
    if (_samplers.size() < binding + 1)
        _samplers.resize(binding + 1);
    _samplers[binding] = std::forward<backend::vulkan::Image::View>(view);
    return true;
}