#include "Cala/Material.h"

cala::Material::Material(cala::Engine* engine, u32 id, u32 size)
    : _engine(engine),
      _setSize(size),
      _id(id),
      _dirty(true),
      _materialBuffer(engine->device().createBuffer(256, backend::BufferUsage::STORAGE | backend::BufferUsage::UNIFORM, backend::MemoryProperties::DEVICE))
{
    std::string debugLabel = "Material: " + std::to_string(_id);
    _engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_materialBuffer->buffer(), debugLabel);
}

cala::Material::Material(cala::Material &&rhs) noexcept
    : _engine(nullptr),
      _programs(),
      _rasterState(),
      _depthState(),
      _blendState(),
      _dirty(false),
      _data(),
      _materialBuffer(),
      _setSize(0),
      _id(0)
{
    std::swap(_engine, rhs._engine);
    std::swap(_programs, rhs._programs);
    std::swap(_rasterState, rhs._rasterState);
    std::swap(_depthState, rhs._depthState);
    std::swap(_blendState, rhs._blendState);
    std::swap(_dirty, rhs._dirty);
    std::swap(_data, rhs._data);
    std::swap(_materialBuffer, rhs._materialBuffer);
    std::swap(_setSize, rhs._setSize);
    std::swap(_id, rhs._id);
}

cala::Material &cala::Material::operator=(cala::Material &&rhs) noexcept {
    std::swap(_engine, rhs._engine);
    std::swap(_programs, rhs._programs);
    std::swap(_rasterState, rhs._rasterState);
    std::swap(_depthState, rhs._depthState);
    std::swap(_blendState, rhs._blendState);
    std::swap(_dirty, rhs._dirty);
    std::swap(_data, rhs._data);
    std::swap(_materialBuffer, rhs._materialBuffer);
    std::swap(_setSize, rhs._setSize);
    std::swap(_id, rhs._id);
    return *this;
}

cala::MaterialInstance cala::Material::instance() {
    u32 offset = _data.size();
    _data.resize(offset + _setSize);
    auto mat = MaterialInstance(this, offset);
    return std::move(mat);
}

void cala::Material::upload() {
    if (_dirty) {
        if (_data.size() > _materialBuffer->size()) {
            _engine->flushStagedData();
            _materialBuffer = _engine->device().resizeBuffer(_materialBuffer, _data.size(), true);
            std::string debugLabel = "Material: " + std::to_string(_id);
            _engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_materialBuffer->buffer(), debugLabel);
        }
        std::span<const u8> ms = _data;
        _engine->stageData(_materialBuffer, ms);
        _dirty = false;
    }
}

void cala::Material::setVariant(cala::Material::Variant variant, Program program) {
    assert(static_cast<u8>(variant) < static_cast<u8>(Variant::MAX));
    _programs[static_cast<u8>(variant)] = std::move(program);
}

const cala::Program& cala::Material::getVariant(cala::Material::Variant variant) {
    assert(static_cast<u8>(variant) < static_cast<u8>(Variant::MAX));
    return _programs[static_cast<u8>(variant)];
}

bool cala::Material::variantPresent(cala::Material::Variant variant) {
    assert(static_cast<u8>(variant) < static_cast<u8>(Variant::MAX));
    return _programs[static_cast<u8>(variant)];
}