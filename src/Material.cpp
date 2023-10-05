#include "Cala/Material.h"

cala::Material::Material(cala::Engine* engine, u32 id, u32 size)
    : _engine(engine),
      _setSize(size),
      _id(id),
      _dirty(true),
      _materialBuffer(engine->device().createBuffer(256, backend::BufferUsage::STORAGE | backend::BufferUsage::UNIFORM, backend::MemoryProperties::STAGING))
{
    std::string debugLabel = "Material: " + std::to_string(_id);
    _engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_materialBuffer->buffer(), debugLabel);
}

cala::MaterialInstance cala::Material::instance() {
    u32 offset = _data.size();
    _data.resize(offset + _setSize);
    auto mat = MaterialInstance(*this, offset);
    return std::move(mat);
}

void cala::Material::upload() {
    if (_dirty) {
        if (_data.size() > _materialBuffer->size()) {
            _materialBuffer = _engine->device().resizeBuffer(_materialBuffer, _data.size(), true);
            std::string debugLabel = "Material: " + std::to_string(_id);
            _engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_materialBuffer->buffer(), debugLabel);
        }
        _materialBuffer->data(_data);
        _dirty = false;
    }
}

void cala::Material::setVariant(cala::Material::Variant variant, backend::vulkan::ProgramHandle program) {
    assert(static_cast<u8>(variant) < static_cast<u8>(Variant::MAX));
//    if (_programs[static_cast<u8>(variant)])
//        _programs[static_cast<u8>(variant)].release();
//        _engine->device().destroyProgram(_programs[static_cast<u8>(variant)]);
    _programs[static_cast<u8>(variant)] = program;
}

cala::backend::vulkan::ProgramHandle cala::Material::getVariant(cala::Material::Variant variant) {
    assert(static_cast<u8>(variant) < static_cast<u8>(Variant::MAX));
    return _programs[static_cast<u8>(variant)];
}