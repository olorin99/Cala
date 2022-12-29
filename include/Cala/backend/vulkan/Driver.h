#ifndef CALA_DRIVER_H
#define CALA_DRIVER_H

#include <Cala/backend/vulkan/Context.h>
#include <Cala/backend/vulkan/Swapchain.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/backend/vulkan/CommandBufferList.h>
#include "Platform.h"

#include <Ende/time/StopWatch.h>

namespace cala::backend::vulkan {

    const u32 FRAMES_IN_FLIGHT = 2;

    class Driver {
    public:

        Driver(Platform& platform, bool clear = true); // change clear to options struct

        ~Driver();

        struct FrameInfo {
            u64 frame = 0;
            CommandBuffer* cmd = nullptr;
            VkFence fence = VK_NULL_HANDLE;
        };

        FrameInfo beginFrame();

        ende::time::Duration endFrame();

        bool waitFrame(u64 frame, u64 timeout = 1000000000);

        bool wait(u64 timeout = 1000000000); // waits for all frames

        struct Primitive {
            VkBuffer vertex;
            u32 vertices;
        };

        void draw(CommandBuffer::RasterState state, Primitive primitive);


        Buffer stagingBuffer(u32 size);


        CommandBuffer beginSingleTimeCommands();

        void endSingleTimeCommands(CommandBuffer& buffer);

        template <typename F>
        void immediate(F func) {
            auto cmd = beginSingleTimeCommands();
            func(cmd);
            endSingleTimeCommands(cmd);
        }

        VkDeviceMemory allocate(u32 size, u32 typeBits, MemoryProperties flags);

        VkDescriptorSetLayout getSetLayout(ende::Span<VkDescriptorSetLayoutBinding> bindings);


        const Context& context() const { return _context; }

        Swapchain& swapchain() { return _swapchain; }

        u32 setLayoutCount() const { return _setLayouts.size(); }

        f64 fps() const { return 1000.f / (static_cast<f64>(_lastFrameTime.microseconds()) / 1000.f); }

        f64 milliseconds() const { return static_cast<f64>(_lastFrameTime.microseconds()) / 1000.f; }

    private:

        Context _context;
        Swapchain _swapchain;
        VkCommandPool _commandPool;
        CommandBuffer _frameCommands[FRAMES_IN_FLIGHT];
        VkFence _frameFences[FRAMES_IN_FLIGHT];
        u64 _frameCount;
        ende::time::StopWatch _frameClock;
        ende::time::Duration _lastFrameTime;


        struct SetLayoutKey {
            VkDescriptorSetLayoutBinding bindings[8];
            bool operator==(const SetLayoutKey& rhs) const {
                return memcmp(this, &rhs, sizeof(SetLayoutKey)) == 0;
            }
        };
        std::unordered_map<SetLayoutKey, VkDescriptorSetLayout, ende::util::MurmurHash<SetLayoutKey>> _setLayouts;

    };

}

#endif //CALA_DRIVER_H
