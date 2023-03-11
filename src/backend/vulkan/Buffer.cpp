#include <cstring>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/primitives.h>
#include <Cala/backend/vulkan/Device.h>


cala::backend::vulkan::Buffer::Buffer(Device &driver, u32 size, BufferUsage usage, MemoryProperties flags, bool persistentlyMapped)
    : _driver(driver),
    _buffer(VK_NULL_HANDLE),
    _allocation(nullptr),
    _size(size),
    _usage(usage | BufferUsage::TRANSFER_DST | BufferUsage::TRANSFER_SRC),
    _flags(flags)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = getBufferUsage(_usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if ((flags & MemoryProperties::HOST_VISIBLE) == MemoryProperties::HOST_VISIBLE) {
        if ((flags & MemoryProperties::HOST_CACHED) == MemoryProperties::HOST_CACHED)
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        else
            allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(_driver.context().allocator(), &bufferInfo, &allocInfo, &_buffer, &_allocation, nullptr);
    if (persistentlyMapped)
        _mapped = map();
}

cala::backend::vulkan::Buffer::~Buffer() {
    _mapped = Mapped();
    if (_allocation)
        vmaDestroyBuffer(_driver.context().allocator(), _buffer, _allocation);
}


cala::backend::vulkan::Buffer::Buffer(Buffer &&rhs) noexcept
    : _driver(rhs._driver),
    _buffer(VK_NULL_HANDLE),
    _allocation(nullptr),
    _size(0),
    _flags(MemoryProperties::HOST_VISIBLE),
    _usage(BufferUsage::UNIFORM)
{
    std::swap(_buffer, rhs._buffer);
    std::swap(_allocation, rhs._allocation);
    std::swap(_size, rhs._size);
    std::swap(_flags, rhs._flags);
    std::swap(_usage, rhs._usage);
}

cala::backend::vulkan::Buffer &cala::backend::vulkan::Buffer::operator=(cala::backend::vulkan::Buffer &&rhs) noexcept {
    std::swap(_buffer, rhs._buffer);
    std::swap(_allocation, rhs._allocation);
    std::swap(_size, rhs._size);
    std::swap(_flags, rhs._flags);
    std::swap(_usage, rhs._usage);
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
    vmaMapMemory(_driver.context().allocator(), _allocation, &address);
    Mapped mapped;
    mapped.address = (void*)((char*)address + offset);
    mapped.buffer = this;
    return std::move(mapped);
}

void cala::backend::vulkan::Buffer::unmap() {
    vmaUnmapMemory(_driver.context().allocator(), _allocation);
}

void cala::backend::vulkan::Buffer::data(ende::Span<const void> data, u32 offset) {
    if (data.size() == 0 || data.size() - offset == 0)
        return;
    if (_mapped.address)
        std::memcpy(_mapped.address, data.data(), data.size());
    else {
        auto mapped = map(offset, data.size());
        std::memcpy(mapped.address, data.data(), data.size());
    }
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