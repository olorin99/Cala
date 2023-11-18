#include "Cala/MaterialInstance.h"
#include <Cala/Material.h>
#include <Cala/backend/vulkan/CommandBuffer.h>

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
    auto layout = _material->_programs[0].layout();
    auto interface = layout->interface();
    auto member = layout->interface().getBindingMember(2, 0, name);
    if (member.size == 0)
        return false;

    assert(member.size >= size);
    setData(data, size, member.offset);
    return true;
}

void cala::MaterialInstance::setData(u8 *data, u32 size, u32 offset) {
    assert(size <= _material->_setSize);
    assert(offset + size <= _material->_setSize);
    std::memcpy(_material->_data.data() + _offset + offset, data, size);
    _material->_dirty = true;
}

void cala::MaterialInstance::bind(backend::vulkan::CommandBuffer &cmd, u32 set, u32 first) {
    if (_material->_setSize > 0)
        cmd.bindBuffer(2, 0, _material->_materialBuffer, _offset, _material->_setSize, false);
}