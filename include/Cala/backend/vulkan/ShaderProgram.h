#ifndef CALA_SHADERPROGRAM_H
#define CALA_SHADERPROGRAM_H

#include <volk.h>
#include <Cala/backend/ShaderInterface.h>
#include <Cala/backend/vulkan/ShaderModule.h>
#include <Cala/backend/primitives.h>
#include <vector>
#include <Cala/backend/vulkan/Handle.h>
#include <filesystem>

namespace cala::backend::vulkan {

    class Device;
    class CommandBuffer;

    class ShaderProgram {
    public:

//        class Builder {
//        public:
//
//            Builder(Device* device);
//
//            Builder& addStage(ShaderModuleHandle module);
//
//            Builder& addStageSPV(const std::vector<u32>& code, ShaderStage stage);
//
//            Builder& addStageGLSL(const std::filesystem::path& path, ShaderStage stage, const std::vector<std::pair<const char*, std::string>>& macros = {}, const std::vector<std::string>& includes = {});
//
//            ShaderProgram compile();
//
//        private:
//            Device* _device;
//            std::vector<std::pair<std::vector<u32>, ShaderStage>> _stages;
//
//        };
//
//        static Builder create(Device* device);


        ShaderProgram(Device* device, std::span<const ShaderModuleHandle> modules);

        ~ShaderProgram();

        ShaderProgram(const ShaderProgram& rhs) = delete;

        ShaderProgram(ShaderProgram&& rhs) noexcept;

        ShaderProgram& operator=(const ShaderProgram& rhs) = delete;

        ShaderProgram& operator=(ShaderProgram&& rhs) noexcept;


        VkPipelineLayout layout();

        bool stagePresent(ShaderStage stageFlags) const;

        const ShaderInterface& interface() const { return _interface; }

    private:
//        friend Builder;
        friend CommandBuffer;

        Device* _device;
        std::vector<ShaderModuleHandle> _modules;
        VkDescriptorSetLayout _setLayout[MAX_SET_COUNT];
        VkPipelineLayout _layout;

        ShaderInterface _interface;
        ShaderStage _stageFlags;

    };

}

#endif //CALA_SHADERPROGRAM_H
