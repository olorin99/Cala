#ifndef CALA_SHADERPROGRAM_H
#define CALA_SHADERPROGRAM_H

#include <vulkan/vulkan.h>
#include <Ende/Vector.h>
#include <Cala/backend/ShaderInterface.h>

namespace cala::backend::vulkan {

    class Driver;

    class ShaderProgram {
    public:

        class Builder {
        public:

            Builder& addStage(ende::Span<u32> code, u32 flags);

            ShaderProgram compile(Driver& driver);

        private:

            ende::Vector<std::pair<ende::Span<u32>, u32>> _stages;

        };

        static Builder create();


        ShaderProgram(VkDevice device);

        ~ShaderProgram();

        ShaderProgram(const ShaderProgram& rhs) = delete;

        ShaderProgram(ShaderProgram&& rhs);

        ShaderProgram& operator=(const ShaderProgram& rhs) = delete;


        VkPipelineLayout layout();

        bool stagePresent(u32 stageFlags) const;

//    private:
        friend Builder;

        VkDevice _device;
        ende::Vector<VkPipelineShaderStageCreateInfo> _stages;
        VkDescriptorSetLayout _setLayout[MAX_SET_COUNT];
        VkPipelineLayout _layout;

        ShaderInterface _interface;
        u32 _stageFlags;

    };

}

#endif //CALA_SHADERPROGRAM_H
