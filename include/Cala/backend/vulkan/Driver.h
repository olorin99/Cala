#ifndef CALA_DRIVER_H
#define CALA_DRIVER_H

#include <Cala/backend/vulkan/Context.h>
#include <Cala/backend/vulkan/Swapchain.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/backend/vulkan/CommandBufferList.h>

namespace cala::backend::vulkan {

    class Driver {
    public:

        Driver(ende::Span<const char*> extensions, void* window, void* display);

        ~Driver();

        CommandBuffer* beginFrame();

        bool endFrame();

        struct Primitive {
            VkBuffer vertex;
            u32 vertices;
        };

        void draw(CommandBuffer::RasterState state, Primitive primitive);


        Buffer stagingBuffer(u32 size);


        VkCommandBuffer beginSingleTimeCommands();

        void endSingleTimeCommands(VkCommandBuffer buffer);

        template <typename F>
        void immediate(F func) {
            auto cmd = beginSingleTimeCommands();
            func(cmd);
            endSingleTimeCommands(cmd);
        }


//    private:

        Context _context;
        Swapchain _swapchain;
        CommandBufferList _commands;
        VkCommandPool _commandPool;


        struct SetLayoutKey {
            VkDescriptorSetLayoutBinding bindings[8];
            bool operator==(const SetLayoutKey& rhs) const {
                return memcmp(this, &rhs, sizeof(SetLayoutKey)) == 0;
            }
        };
        std::unordered_map<SetLayoutKey, VkDescriptorSetLayout, ende::util::MurmurHash<SetLayoutKey>> _setLayouts;


        VkDescriptorSetLayout getSetLayout(ende::Span<VkDescriptorSetLayoutBinding> bindings);

    };

}

#endif //CALA_DRIVER_H
