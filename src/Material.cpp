#include "Cala/Material.h"


cala::Material::Material(backend::vulkan::Driver &driver, backend::vulkan::ShaderProgram &&program)
    : _program(std::move(program)),
      _uniformBuffer(driver._context, 256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
      _setSize(_program._interface.sets[1].byteSize)
{}

cala::MaterialInstance cala::Material::instance() {
    auto mat = MaterialInstance(*this, _uniformData.size());
    _uniformData.resize(_uniformData.size() + _setSize);
    return mat;
}