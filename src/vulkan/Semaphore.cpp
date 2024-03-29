#include <Cala/vulkan/Semaphore.h>
#include <Cala/vulkan/Device.h>

cala::vk::Semaphore::Semaphore(cala::vk::Device *device, i64 initialValue)
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

cala::vk::Semaphore::~Semaphore() {
    if (_device && _semaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(_device->context().device(), _semaphore, nullptr);
}

cala::vk::Semaphore::Semaphore(cala::vk::Semaphore &&rhs) noexcept
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

cala::vk::Semaphore &cala::vk::Semaphore::operator=(cala::vk::Semaphore &&rhs) noexcept {
    std::swap(_device, rhs._device);
    std::swap(_semaphore, rhs._semaphore);
    std::swap(_value, rhs._value);
    std::swap(_isTimeline, rhs._isTimeline);
    return *this;
}

std::expected<void, cala::vk::Error> cala::vk::Semaphore::wait(u64 value, u64 timeout) {
    VkSemaphoreWaitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &_semaphore;
    waitInfo.pValues = &value;
    auto result = vkWaitSemaphores(_device->context().device(), &waitInfo, timeout);
    if (result != VK_SUCCESS && result != VK_TIMEOUT) {
        return std::unexpected(static_cast<Error>(result));
    }
    return {};
}

std::expected<void, cala::vk::Error> cala::vk::Semaphore::signal(u64 value) {
    VkSemaphoreSignalInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = _semaphore;
    signalInfo.value = value;
    auto result = vkSignalSemaphore(_device->context().device(), &signalInfo);
    if (result != VK_SUCCESS && result != VK_TIMEOUT) {
        return std::unexpected(static_cast<Error>(result));
    }
    _value = value;
    return {};
}

u64 cala::vk::Semaphore::queryGPUValue() {
    u64 value = 0;
    vkGetSemaphoreCounterValue(_device->context().device(), _semaphore, &value);
    return value;
}