#include "Cala/backend/vulkan/Timer.h"
#include <Cala/backend/vulkan/Device.h>

static u32 indexCount = 0;

cala::backend::vulkan::Timer::Timer(Device &driver, u32 index)
    : _driver(&driver),
    _index(indexCount++),
    _cmdBuffer({}),
    _result(0)
{}

void cala::backend::vulkan::Timer::start(CommandHandle cmd) {
    _cmdBuffer = cmd;
    _result = 0;
    vkCmdResetQueryPool(cmd->buffer(), _driver->context().timestampPool(), _index * 2, 2);
    vkCmdWriteTimestamp2(cmd->buffer(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, _driver->context().timestampPool(), _index * 2);
}

void cala::backend::vulkan::Timer::stop() {
    vkCmdWriteTimestamp2(_cmdBuffer->buffer(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, _driver->context().timestampPool(), _index * 2 + 1);
}

u64 cala::backend::vulkan::Timer::result() {
    if (_result) return _result;
    u64 buffer[2];
    VkResult res = vkGetQueryPoolResults(_driver->context().device(), _driver->context().timestampPool(), _index * 2, 2, sizeof(u64) * 2, buffer, sizeof(u64), VK_QUERY_RESULT_64_BIT);
    if (res == VK_NOT_READY)
        return 0;
    else if (res == VK_SUCCESS) {
        _result = (buffer[1] - buffer[0]) * (u32)_driver->context().timestampPeriod();
    } else {
        throw "query fail";
    }

    //vkResetQueryPool(_device.context().device(), _device.context().timestampPool(), _index * 2, 2);
    return _result;
}