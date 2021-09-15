#ifndef CALA_SHADERPROGRAM_H
#define CALA_SHADERPROGRAM_H

#include <vulkan/vulkan.h>
#include <Ende/Vector.h>


namespace cala::backend::vulkan {

    class ShaderProgram {
    public:

        ShaderProgram(VkDevice device);

        ~ShaderProgram();

        bool addStage(ende::Span<u32> code, u32 flags);

//    private:

        VkDevice _device;
        ende::Vector<VkPipelineShaderStageCreateInfo> _stages;


    };

}

#endif //CALA_SHADERPROGRAM_H
