#include "Cala/Material.h"


cala::Material::Material(backend::vulkan::Driver &driver, backend::vulkan::ShaderProgram &&program)
    : _program(std::move(program)),
      _uniformBuffer(driver, 256, backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT),
      _setSize(_program.interface().setSize(2))
{}

cala::MaterialInstance cala::Material::instance() {
    auto mat = MaterialInstance(*this, _uniformData.size());
    _uniformData.resize(_uniformData.size() + _setSize);
    return mat;
}