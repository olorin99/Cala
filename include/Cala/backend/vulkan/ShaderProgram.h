#ifndef CALA_SHADERPROGRAM_H
#define CALA_SHADERPROGRAM_H

#include <vulkan/vulkan.h>
#include <Ende/Vector.h>

#include <Cala/backend/vulkan/Context.h>

namespace cala::backend::vulkan {

    class ShaderProgram {
    public:

        ShaderProgram(Context& context);

        ~ShaderProgram();

        bool addStage(ende::Span<u32> code, u32 flags);

//    private:

        Context& _context;
        ende::Vector<VkPipelineShaderStageCreateInfo> _stages;


    };

}

#endif //CALA_SHADERPROGRAM_H
