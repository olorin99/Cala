#include <cstring>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/primitives.h>
#include <Cala/backend/vulkan/Driver.h>


cala::backend::vulkan::Buffer::Buffer(Driver &driver, u32 size, BufferUsage usage, MemoryProperties flags)
    : _driver(driver),
    _buffer(VK_NULL_HANDLE),
    _allocation(nullptr),
    _size(size),
    _usage(usage),
    _flags(flags)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = getBufferUsage(usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if ((flags & MemoryProperties::HOST_VISIBLE) == MemoryProperties::HOST_VISIBLE)
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    vmaCreateBuffer(_driver.context().allocator(), &bufferInfo, &allocInfo, &_buffer, &_allocation, nullptr);
}

cala::backend::vulkan::Buffer::~Buffer() {
    if (_allocation)
        vmaDestroyBuffer(_driver.context().allocator(), _buffer, _allocation);
}


cala::backend::vulkan::Buffer::Buffer(Buffer &&rhs)
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

cala::backend::vulkan::Buffer &cala::backend::vulkan::Buffer::operator=(cala::backend::vulkan::Buffer &&rhs) {
    std::swap(_buffer, rhs._buffer);
    std::swap(_allocation, rhs._allocation);
    std::swap(_size, rhs._size);
    std::swap(_flags, rhs._flags);
    std::swap(_usage, rhs._usage);
    return *this;
}


cala::backend::vulkan::Buffer::Mapped::~Mapped() {
    buffer->unmap();
}

cala::backend::vulkan::Buffer::Mapped cala::backend::vulkan::Buffer::map(u32 offset, u32 size) {
    void* address = nullptr;
    vmaMapMemory(_driver.context().allocator(), _allocation, &address);
    return { address, this };
}

void cala::backend::vulkan::Buffer::unmap() {
    vmaUnmapMemory(_driver.context().allocator(), _allocation);
}

void cala::backend::vulkan::Buffer::data(ende::Span<const void> data, u32 offset) {
    if (data.size() == 0 || data.size() - offset == 0)
        return;
    auto mapped = map(offset, data.size());
    memcpy(mapped.address, data.data(), data.size());
}