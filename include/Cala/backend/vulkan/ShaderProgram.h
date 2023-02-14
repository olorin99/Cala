#ifndef CALA_SHADERPROGRAM_H
#define CALA_SHADERPROGRAM_H

#include <vulkan/vulkan.h>
#include <Ende/Vector.h>
#include <Cala/backend/ShaderInterface.h>
#include <Cala/backend/primitives.h>

namespace cala::backend::vulkan {

    class Device;
    class CommandBuffer;

    class ShaderProgram {
    public:

        class Builder {
        public:

            Builder& addStage(ende::Span<u32> code, ShaderStage stage);

            ShaderProgram compile(Device& driver);

        private:

            ende::Vector<std::pair<ende::Span<u32>, ShaderStage>> _stages;

        };

        static Builder create();


        ShaderProgram(VkDevice device);

        ~ShaderProgram();

        ShaderProgram(const ShaderProgram& rhs) = delete;

        ShaderProgram(ShaderProgram&& rhs);

        ShaderProgram& operator=(const ShaderProgram& rhs) = delete;

        ShaderProgram& operator=(ShaderProgram&& rhs) noexcept;


        VkPipelineLayout layout();

        bool stagePresent(ShaderStage stageFlags) const;

        const ShaderInterface& interface() const { return _interface; }

    private:
        friend Builder;
        friend CommandBuffer;

        VkDevice _device;
        ende::Vector<VkPipelineShaderStageCreateInfo> _stages;
        VkDescriptorSetLayout _setLayout[MAX_SET_COUNT];
        VkPipelineLayout _layout;

        ShaderInterface _interface;
        ShaderStage _stageFlags;

    };

}

#endif //CALA_SHADERPROGRAM_H
