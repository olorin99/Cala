#include <cstring>
#include <Cala/vulkan/Buffer.h>
#include <Cala/vulkan/primitives.h>
#include <Cala/vulkan/Device.h>

cala::vk::Buffer::Buffer(cala::vk::Device *device)
    : _device(device),
    _buffer(VK_NULL_HANDLE),
    _allocation(nullptr),
    _invalidated(false),
    _mapped()
{}


cala::vk::Buffer::Buffer(Buffer &&rhs) noexcept
    : _device(nullptr),
      _buffer(VK_NULL_HANDLE),
      _allocation(nullptr),
      _size(0),
      _flags(MemoryProperties::STAGING),
      _usage(BufferUsage::UNIFORM),
      _invalidated(false),
      _mapped()
{
    std::swap(_device, rhs._device);
    std::swap(_buffer, rhs._buffer);
    std::swap(_allocation, rhs._allocation);
    std::swap(_size, rhs._size);
    std::swap(_usage, rhs._usage);
    std::swap(_flags, rhs._flags);
    std::swap(_mapped, rhs._mapped);
    std::swap(_invalidated, rhs._invalidated);
}

cala::vk::Buffer &cala::vk::Buffer::operator=(cala::vk::Buffer &&rhs) noexcept {
    std::swap(_buffer, rhs._buffer);
    std::swap(_allocation, rhs._allocation);
    std::swap(_size, rhs._size);
    std::swap(_usage, rhs._usage);
    std::swap(_flags, rhs._flags);
    std::swap(_mapped, rhs._mapped);
    std::swap(_invalidated, rhs._invalidated);
    return *this;
}


cala::vk::Buffer::Mapped::~Mapped() {
    if (address && buffer)
        buffer->unmap();
}

cala::vk::Buffer::Mapped::Mapped()
    : address(nullptr),
    buffer(nullptr)
{}

cala::vk::Buffer::Mapped::Mapped(Mapped &&rhs) noexcept
    : address(nullptr),
    buffer(nullptr)
{
    std::swap(address, rhs.address);
    std::swap(buffer, rhs.buffer);
}

cala::vk::Buffer::Mapped &cala::vk::Buffer::Mapped::operator=(Mapped &&rhs) noexcept {
    std::swap(address, rhs.address);
    std::swap(buffer, rhs.buffer);
    return *this;
}

cala::vk::Buffer::Mapped cala::vk::Buffer::map(u32 offset, u32 size) {
    if (size == 0)
        size = _size - offset;

    assert(_size >= size + offset);
    void* address = nullptr;
    VK_TRY(vmaMapMemory(_device->context().allocator(), _allocation, &address));
    assert(address);
    Mapped mapped;
    mapped.address = (void*)((char*)address + offset);
    mapped.buffer = this;
    return std::move(mapped);
}

void cala::vk::Buffer::unmap() {
    vmaUnmapMemory(_device->context().allocator(), _allocation);
}

void cala::vk::Buffer::_data(u8, std::span<u8> data, u32 offset) {
    u32 dataSize = data.size();
    assert(data.size() + offset <= _size);
    if (_mapped.address)
        std::memcpy(static_cast<char*>(_mapped.address) + offset, data.data(), data.size());
    else {
        auto mapped = map(offset, data.size());
        std::memcpy(mapped.address, data.data(), data.size());
    }
    invalidate();
    _device->increaseDataUploadCount(data.size());
}

void cala::vk::Buffer::_data(u8, std::span<const u8> data, u32 offset) {
    assert(data.size() + offset <= _size);
    if (_mapped.address)
        std::memcpy(static_cast<char*>(_mapped.address) + offset, data.data(), data.size());
    else {
        auto mapped = map(offset, data.size());
        std::memcpy(mapped.address, data.data(), data.size());
    }
    invalidate();
    _device->increaseDataUploadCount(data.size());
}

cala::vk::Buffer::View::View()
    : _parent(nullptr),
    _size(0),
    _offset(0)
{}

cala::vk::Buffer::View::View(cala::vk::Buffer &buffer)
    : _parent(&buffer),
      _size(buffer.size()),
      _offset(0)
{}

cala::vk::Buffer::View::View(cala::vk::Buffer &buffer, u32 size, u32 offset)
    : _parent(&buffer),
      _size(size),
      _offset(offset)
{}

cala::vk::Buffer::Mapped cala::vk::Buffer::View::map(u32 offset, u32 size) {
    return _parent->map(_offset + offset, _size + size);
}

cala::vk::Buffer::Barrier cala::vk::Buffer::barrier(PipelineStage srcStage, PipelineStage dstStage, Access dstAccess) {
    Barrier b{};
    b.srcStage = srcStage;
    b.dstStage = dstStage;
    b.srcAccess = invalidated() ? Access::MEMORY_WRITE : Access::MEMORY_READ;
    b.dstAccess = dstAccess;
    b.buffer = this;
    return b;
}

cala::vk::Buffer::Barrier cala::vk::Buffer::barrier(PipelineStage srcStage, PipelineStage dstStage, Access srcAccess, Access dstAccess) {
    Barrier b{};
    b.srcStage = srcStage;
    b.dstStage = dstStage;
    b.srcAccess = srcAccess;
    b.dstAccess = dstAccess;
    b.buffer = this;
    return b;
}