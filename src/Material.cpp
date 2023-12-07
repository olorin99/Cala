#include "Cala/Material.h"
#include "Cala/shaderBridge.h"

cala::Material::Material(cala::Engine* engine, u32 id, u32 size)
    : _engine(engine),
      _setSize(size),
      _id(id),
      _dirty(true),
      _materialBuffer(engine->device().createBuffer({
          .size = 256,
          .usage = backend::BufferUsage::STORAGE | backend::BufferUsage::UNIFORM,
          .memoryType = backend::MemoryProperties::DEVICE,
          .name = "Material: " + std::to_string(_id)
      }))
{}

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
    for (auto& parameter : _programs[0].interface().getBindingMemberList(CALA_MATERIAL_SET, CALA_MATERIAL_BINDING)) {
        switch (parameter.type) {
            case backend::vulkan::ShaderModuleInterface::MemberType::INT:
                mat.setParameter(parameter.name, -1);
                break;
            case backend::vulkan::ShaderModuleInterface::MemberType::FLOAT:
                mat.setParameter(parameter.name, 0);
                break;
            case backend::vulkan::ShaderModuleInterface::MemberType::VEC3F:
                mat.setParameter(parameter.name, ende::math::Vec3f{0, 0, 0});
                break;
            case backend::vulkan::ShaderModuleInterface::MemberType::VEC4F:
                mat.setParameter(parameter.name, ende::math::Vec4f{0, 0, 0});
                break;
        }
    }
    return std::move(mat);
}

void cala::Material::upload() {
    if (_dirty) {
        if (_data.size() > _materialBuffer->size()) {
            _engine->flushStagedData();
            _materialBuffer = _engine->device().resizeBuffer(_materialBuffer, _data.size(), true);
        }
        std::span<const u8> ms = _data;
        _engine->stageData(_materialBuffer, ms);
        _dirty = false;
    }
}

void cala::Material::setVariant(cala::Material::Variant variant, backend::vulkan::ShaderProgram program) {
    assert(static_cast<u8>(variant) < static_cast<u8>(Variant::MAX));
    _programs[static_cast<u8>(variant)] = std::move(program);
}

const cala::backend::vulkan::ShaderProgram& cala::Material::getVariant(cala::Material::Variant variant) {
    assert(static_cast<u8>(variant) < static_cast<u8>(Variant::MAX));
    return _programs[static_cast<u8>(variant)];
}

bool cala::Material::variantPresent(cala::Material::Variant variant) {
    assert(static_cast<u8>(variant) < static_cast<u8>(Variant::MAX));
    return _programs[static_cast<u8>(variant)];
}

bool cala::Material::build() {
    u32 size = 0;
    for (auto& program : _programs) {
        if (!program)
            continue;

        u32 programMaterialSize = program._pipelineLayout->interface().bindingSize(2, 0);
        if (size == 0)
            size = programMaterialSize;
        if (size != programMaterialSize) {
            _engine->logger().error("material variants have differing material sizes: {} - {}", size, programMaterialSize);
            return false;
        }
    }
    _setSize = size;
    return true;
}