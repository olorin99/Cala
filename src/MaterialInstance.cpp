#include <Cala/MaterialInstance.h>
#include <Cala/Material.h>
#include <Cala/vulkan/CommandBuffer.h>
#include "Cala/shaderBridge.h"

cala::MaterialInstance::MaterialInstance(Material *material, u32 offset)
    : _material(material),
      _offset(offset)
{}

cala::MaterialInstance::MaterialInstance(MaterialInstance &&rhs) noexcept
    : _material(nullptr),
    _offset(0)
{
    std::swap(_material, rhs._material);
    std::swap(_offset, rhs._offset);
}

bool cala::MaterialInstance::setParameter(const std::string& name, u8* data, u32 size) {
    auto layout = _material->_programs[0]._pipelineLayout;
    auto member = layout->interface().getBindingMember(CALA_MATERIAL_SET, CALA_MATERIAL_BINDING, name);
    if (member.size == 0)
        return false;

    assert(member.size >= size);
    setData(data, size, member.offset);
    return true;
}

void *cala::MaterialInstance::getParameter(const std::string &name, u32 size) {
    auto layout = _material->_programs[0]._pipelineLayout;
    auto member = layout->interface().getBindingMember(CALA_MATERIAL_SET, CALA_MATERIAL_BINDING, name);
    if (member.size == 0)
        return nullptr;
    assert(_material->_data.size() >= member.offset + _offset + size);
    return &_material->_data[member.offset + _offset];
}

void cala::MaterialInstance::setData(u8 *data, u32 size, u32 offset) {
    assert(size <= _material->_setSize);
    assert(offset + size <= _material->_setSize);
    std::memcpy(_material->_data.data() + _offset + offset, data, size);
    _material->_dirty = true;
}

u32 cala::MaterialInstance::getIndex() const {
    return getOffset() / material()->size();
}

void cala::MaterialInstance::bind(vk::CommandBuffer &cmd, u32 set, u32 first) {
    if (_material->_setSize > 0)
        cmd.bindBuffer(2, 0, _material->_materialBuffer, _offset, _material->_setSize, false);
}