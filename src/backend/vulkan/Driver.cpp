#include "Cala/backend/vulkan/Driver.h"


cala::backend::vulkan::Driver::Driver(ende::Span<const char *> extensions, void *window, void *display)
    : _context(extensions),
      _swapchain(_context, window, display)
{}