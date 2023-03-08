#include "Cala/Material.h"

cala::Material::Material(cala::Engine* engine, backend::vulkan::ProgramHandle program, u32 id, u32 size)
    : _engine(engine),
      _program(program),
      _setSize(size == 0 ? shaderDataSize() : size),
      _id(id),
      _dirty(true),
      _materialBuffer(engine->device().createBuffer(256, backend::BufferUsage::STORAGE | backend::BufferUsage::UNIFORM))
{}

cala::MaterialInstance cala::Material::instance() {
    u32 offset = _data.size();
    _data.resize(offset + _setSize);
    auto mat = MaterialInstance(*this, offset);
    return std::move(mat);
}

void cala::Material::upload() {
    if (_dirty) {
        if (_data.size() > _materialBuffer->size())
            _materialBuffer = _engine->device().resizeBuffer(_materialBuffer, _data.size(), true);
        _materialBuffer->data(_data);
        _dirty = false;
    }
}