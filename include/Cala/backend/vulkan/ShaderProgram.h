#ifndef CALA_SHADERPROGRAM_H
#define CALA_SHADERPROGRAM_H

#include <vulkan/vulkan.h>
#include <Ende/Vector.h>


namespace cala::backend::vulkan {

    class ShaderProgram {
    public:

        class Builder {
        public:

            Builder& addStage(ende::Span<u32> code, u32 flags);

            ShaderProgram compile(VkDevice device);

        private:

            ende::Vector<std::pair<ende::Span<u32>, u32>> _stages;

        };

        static Builder create();


        ShaderProgram(VkDevice device);

        ~ShaderProgram();

        bool addStage(ende::Span<u32> code, u32 flags);

        VkPipelineLayout layout();

//    private:
        friend Builder;

        VkDevice _device;
        ende::Vector<VkPipelineShaderStageCreateInfo> _stages;
        VkDescriptorSetLayout _setLayout[4];
        VkPipelineLayout _layout;


    };

}

#endif //CALA_SHADERPROGRAM_H
