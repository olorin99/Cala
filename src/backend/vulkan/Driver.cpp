#include "Cala/backend/vulkan/Driver.h"


cala::backend::vulkan::Driver::Driver(ende::Span<const char *> extensions, void *window, void *display)
    : _context(extensions),
      _swapchain(_context, window, display)
{}


cala::backend::vulkan::CommandBuffer* cala::backend::vulkan::Driver::beginFrame() {
    return _context._commands->get();
}

bool cala::backend::vulkan::Driver::endFrame() {
    return _context._commands->flush();
}


void cala::backend::vulkan::Driver::draw(CommandBuffer::RasterState state, Primitive primitive) {

    auto commandBuffer = _context._commands->get();

//    _context._pipelines->bindRasterState(state);
//    _context._pipelines->bindPipeline(commandBuffer);

    commandBuffer->bindRasterState(state);
    commandBuffer->bindPipeline();

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer->buffer(), 0, 1, &primitive.vertex, offsets);

    vkCmdDraw(commandBuffer->buffer(), primitive.vertices, 1, 0, 0);

}