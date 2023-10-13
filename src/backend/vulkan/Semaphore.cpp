#include "Cala/backend/vulkan/Semaphore.h"
#include <Cala/backend/vulkan/Device.h>

cala::backend::vulkan::Semaphore::Semaphore(cala::backend::vulkan::Device *device, i64 initialValue)
    : _device(device),
    _semaphore(VK_NULL_HANDLE),
    _value(0),
    _isTimeline(false)
{
    if (!_device)
        return;

    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    createInfo.flags = 0;

    VkSemaphoreTypeCreateInfo typeCreateInfo{};
    if (initialValue >= 0) {
        typeCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeCreateInfo.initialValue = initialValue;
        createInfo.pNext = &typeCreateInfo;
        _value = initialValue;
        _isTimeline = true;
    }
    vkCreateSemaphore(_device->context().device(), &createInfo, nullptr, &_semaphore);
}

cala::backend::vulkan::Semaphore::~Semaphore() {
    if (_device && _semaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(_device->context().device(), _semaphore, nullptr);
}

cala::backend::vulkan::Semaphore::Semaphore(cala::backend::vulkan::Semaphore &&rhs) noexcept
    : _device(nullptr),
    _semaphore(VK_NULL_HANDLE),
    _value(-1),
    _isTimeline(false)
{
    std::swap(_device, rhs._device);
    std::swap(_semaphore, rhs._semaphore);
    std::swap(_value, rhs._value);
    std::swap(_isTimeline, rhs._isTimeline);
}

cala::backend::vulkan::Semaphore &cala::backend::vulkan::Semaphore::operator=(cala::backend::vulkan::Semaphore &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(_semaphore, rhs._semaphore);
    std::swap(_value, rhs._value);
    std::swap(_isTimeline, rhs._isTimeline);
    return *this;
}

bool cala::backend::vulkan::Semaphore::wait(u64 value, u64 timeout) {
    VkSemaphoreWaitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &_semaphore;
    waitInfo.pValues = &value;
    auto result = vkWaitSemaphores(_device->context().device(), &waitInfo, timeout);
    if (result == VK_ERROR_DEVICE_LOST) {
        _device->logger().error("Device Lost - Semaphore wait value: {}", _value);
        _device->printMarkers();
        throw std::runtime_error("Device Lost");
    }
    return VK_SUCCESS == result;
}

bool cala::backend::vulkan::Semaphore::signal(u64 value) {
    VkSemaphoreSignalInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = _semaphore;
    signalInfo.value = value;
    auto result = vkSignalSemaphore(_device->context().device(), &signalInfo);
    if (result == VK_ERROR_DEVICE_LOST) {
        _device->logger().error("Device Lost - Semaphore signal value: {}", _value);
        _device->printMarkers();
        throw std::runtime_error("Device Lost");
    }
    _value = value;
    return VK_SUCCESS == result;
}

u64 cala::backend::vulkan::Semaphore::queryGPUValue() {
    u64 value = 0;
    vkGetSemaphoreCounterValue(_device->context().device(), _semaphore, &value);
    return value;
}