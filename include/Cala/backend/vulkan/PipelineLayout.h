#ifndef CALA_PIPELINELAYOUT_H
#define CALA_PIPELINELAYOUT_H

#include <volk.h>
#include "Cala/backend/ShaderInterface.h"

namespace cala::backend::vulkan {

    class Device;

    class PipelineLayout {
    public:

        PipelineLayout(Device* device);

        PipelineLayout(Device* device, const ShaderInterface& interface);

        ~PipelineLayout();

        PipelineLayout(PipelineLayout&& rhs) noexcept;

        PipelineLayout& operator=(PipelineLayout&& rhs) noexcept;

        operator bool() const noexcept {
            return _device && _layout != VK_NULL_HANDLE;
        }

        VkPipelineLayout layout() const { return _layout; }

        VkDescriptorSetLayout setLayout(u32 set) const { return _setLayouts[set]; }

        const ShaderInterface& interface() const { return _interface; }

    private:

        Device* _device;

        ShaderInterface _interface;

        VkPipelineLayout _layout;
        VkDescriptorSetLayout _setLayouts[MAX_SET_COUNT];

    };

}

#endif //CALA_PIPELINELAYOUT_H
