//
// Created by christian on 27/06/22.
//

#include "Cala/backend/vulkan/Timer.h"
#include <Cala/backend/vulkan/Driver.h>

cala::backend::vulkan::Timer::Timer(Driver &driver, u32 index)
    : _driver(&driver),
    _index(index),
    _cmdBuffer(nullptr),
    _result(0)
{}

void cala::backend::vulkan::Timer::start(CommandBuffer &cmd) {
    _cmdBuffer = &cmd;
    _result = 0;
    vkCmdResetQueryPool(cmd.buffer(), _driver->context().queryPool(), _index * 2, 2);
    vkCmdWriteTimestamp(cmd.buffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, _driver->context().queryPool(), _index * 2);
}

void cala::backend::vulkan::Timer::stop() {
    vkCmdWriteTimestamp(_cmdBuffer->buffer(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, _driver->context().queryPool(), _index * 2 + 1);
}

u64 cala::backend::vulkan::Timer::result() {
    if (_result) return _result;
    u64 buffer[2];
    VkResult res = vkGetQueryPoolResults(_driver->context().device(), _driver->context().queryPool(), _index * 2, 2, sizeof(u64) * 2, buffer, sizeof(u64), VK_QUERY_RESULT_64_BIT);
    if (res == VK_NOT_READY)
        return 0;
    else if (res == VK_SUCCESS) {
        _result = buffer[1] - buffer[0];
    } else {
        throw "query fail";
    }

    //vkResetQueryPool(_driver.context().device(), _driver.context().queryPool(), _index * 2, 2);
    return _result;
}