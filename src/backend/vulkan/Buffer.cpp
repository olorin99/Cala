#include <cstring>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/primitives.h>
#include <Cala/backend/vulkan/Device.h>

cala::backend::vulkan::Buffer::Buffer(cala::backend::vulkan::Device *device)
    : _device(device),
    _buffer(VK_NULL_HANDLE),
    _allocation(nullptr),
    _invalidated(false)
{}


cala::backend::vulkan::Buffer::Buffer(Buffer &&rhs) noexcept
    : _device(rhs._device),
      _buffer(VK_NULL_HANDLE),
      _allocation(nullptr),
      _size(0),
      _flags(MemoryProperties::STAGING),
      _usage(BufferUsage::UNIFORM),
      _invalidated(false)
{
    std::swap(_buffer, rhs._buffer);
    std::swap(_allocation, rhs._allocation);
    std::swap(_size, rhs._size);
    std::swap(_flags, rhs._flags);
    std::swap(_usage, rhs._usage);
    std::swap(_invalidated, rhs._invalidated);
}

cala::backend::vulkan::Buffer &cala::backend::vulkan::Buffer::operator=(cala::backend::vulkan::Buffer &&rhs) noexcept {
    std::swap(_buffer, rhs._buffer);
    std::swap(_allocation, rhs._allocation);
    std::swap(_size, rhs._size);
    std::swap(_flags, rhs._flags);
    std::swap(_usage, rhs._usage);
    std::swap(_invalidated, rhs._invalidated);
    return *this;
}


cala::backend::vulkan::Buffer::Mapped::~Mapped() {
    if (address && buffer)
        buffer->unmap();
}

cala::backend::vulkan::Buffer::Mapped::Mapped()
    : address(nullptr),
    buffer(nullptr)
{}

cala::backend::vulkan::Buffer::Mapped::Mapped(Mapped &&rhs) noexcept
    : address(nullptr),
    buffer(nullptr)
{
    std::swap(address, rhs.address);
    std::swap(buffer, rhs.buffer);
}

cala::backend::vulkan::Buffer::Mapped &cala::backend::vulkan::Buffer::Mapped::operator=(Mapped &&rhs) noexcept {
    std::swap(address, rhs.address);
    std::swap(buffer, rhs.buffer);
    return *this;
}

cala::backend::vulkan::Buffer::Mapped cala::backend::vulkan::Buffer::map(u32 offset, u32 size) {
    if (size == 0)
        size = _size - offset;

    assert(_size >= size + offset);
    void* address = nullptr;
    VK_TRY(vmaMapMemory(_device->context().allocator(), _allocation, &address));
    Mapped mapped;
    mapped.address = (void*)((char*)address + offset);
    mapped.buffer = this;
    return std::move(mapped);
}

void cala::backend::vulkan::Buffer::unmap() {
    vmaUnmapMemory(_device->context().allocator(), _allocation);
}

void cala::backend::vulkan::Buffer::data(ende::Span<const void> data, u32 offset) {
    if (data.size() == 0 || size() - offset == 0)
        return;
    if (_mapped.address)
        std::memcpy(static_cast<char*>(_mapped.address) + offset, data.data(), data.size());
    else {
        auto mapped = map(offset, data.size());
        std::memcpy(mapped.address, data.data(), data.size());
    }
    invalidate();
}

cala::backend::vulkan::Buffer::View::View()
    : _parent(nullptr),
    _size(0),
    _offset(0)
{}

cala::backend::vulkan::Buffer::View::View(cala::backend::vulkan::Buffer &buffer)
    : _parent(&buffer),
      _size(buffer.size()),
      _offset(0)
{}

cala::backend::vulkan::Buffer::View::View(cala::backend::vulkan::Buffer &buffer, u32 size, u32 offset)
    : _parent(&buffer),
      _size(size),
      _offset(offset)
{}

cala::backend::vulkan::Buffer::Mapped cala::backend::vulkan::Buffer::View::map(u32 offset, u32 size) {
    return _parent->map(_offset + offset, _size + size);
}

void cala::backend::vulkan::Buffer::View::data(ende::Span<const void> data, u32 offset) {
    _parent->data(data, _offset + offset);
}

cala::backend::vulkan::Buffer::Barrier cala::backend::vulkan::Buffer::barrier(PipelineStage srcStage, PipelineStage dstStage, Access dstAccess) {
    Barrier b{};
    b.srcStage = srcStage;
    b.dstStage = dstStage;
    b.srcAccess = invalidated() ? Access::MEMORY_WRITE : Access::MEMORY_READ;
    b.dstAccess = dstAccess;
    b.buffer = this;
    return b;
}

cala::backend::vulkan::Buffer::Barrier cala::backend::vulkan::Buffer::barrier(PipelineStage srcStage, PipelineStage dstStage, Access srcAccess, Access dstAccess) {
    Barrier b{};
    b.srcStage = srcStage;
    b.dstStage = dstStage;
    b.srcAccess = srcAccess;
    b.dstAccess = dstAccess;
    b.buffer = this;
    return b;
}