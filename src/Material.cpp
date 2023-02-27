#include "Cala/Material.h"


cala::Material::Material(cala::Engine* engine, backend::vulkan::ProgramHandle program, u32 size)
    : _engine(engine),
      _program(program),
      _setSize(size == 0 ? shaderDataSize() : size)
{}

cala::MaterialInstance cala::Material::instance() {
    u32 offset = _engine->_materialData.size();
    _engine->_materialData.resize(offset + _setSize);
    auto mat = MaterialInstance(*this, offset);
    return std::move(mat);
}