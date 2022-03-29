#include <cstring>
#include "Cala/backend/vulkan/Buffer.h"
#include <Cala/backend/vulkan/primitives.h>


cala::backend::vulkan::Buffer::Buffer(Context &context, u32 size, BufferUsage usage, MemoryProperties flags)
    : _context(context),
    _buffer(VK_NULL_HANDLE),
    _memory(VK_NULL_HANDLE),
    _size(size),
    _usage(usage),
    _flags(flags)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = getBufferUsage(usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(_context.device(), &bufferInfo, nullptr, &_buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_context.device(), _buffer, &memRequirements);
    _memory = _context.allocate(memRequirements.size, memRequirements.memoryTypeBits, flags);
    vkBindBufferMemory(_context.device(), _buffer, _memory, 0);
}

cala::backend::vulkan::Buffer::~Buffer() {
    vkFreeMemory(_context.device(), _memory, nullptr);
    vkDestroyBuffer(_context.device(), _buffer, nullptr);
}


cala::backend::vulkan::Buffer::Buffer(Buffer &&rhs)
    : _context(rhs._context),
    _buffer(VK_NULL_HANDLE),
    _memory(VK_NULL_HANDLE),
    _size(0),
    _flags(MemoryProperties::HOST_VISIBLE),
    _usage(BufferUsage::UNIFORM)
{
    std::swap(_buffer, rhs._buffer);
    std::swap(_memory, rhs._memory);
    std::swap(_size, rhs._size);
    std::swap(_flags, rhs._flags);
    std::swap(_usage, rhs._usage);
}


cala::backend::vulkan::Buffer::Mapped::~Mapped() {
    buffer->unmap();
}

cala::backend::vulkan::Buffer::Mapped cala::backend::vulkan::Buffer::map(u32 offset, u32 size) {
    void* address = nullptr;
    vkMapMemory(_context.device(), _memory, offset, size, 0, &address);
    return { address, this };
}

void cala::backend::vulkan::Buffer::unmap() {
    vkUnmapMemory(_context.device(), _memory);
}

void cala::backend::vulkan::Buffer::data(ende::Span<const void> data, u32 offset) {
    auto mapped = map(offset, data.size());
    memcpy(mapped.address, data.data(), data.size());
}