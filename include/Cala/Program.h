#ifndef CALA_PROGRAM_H
#define CALA_PROGRAM_H

#include <Cala/backend/primitives.h>
#include <Cala/AssetManager.h>
#include <span>
#include <Cala/backend/ShaderInterface.h>
#include <Cala/backend/vulkan/ShaderProgram.h>
#include <volk.h>

namespace cala {

    class Engine;

    class Program {
    public:

        Program();

        Program(Engine* engine, std::span<AssetManager::Asset<backend::vulkan::ShaderModuleHandle>> modules);

        Program(Program&& rhs) noexcept;

        Program& operator=(Program&& rhs) noexcept;

        operator bool() const noexcept {
            return _engine && _pipelineLayout;
        }


        bool stagePresent(backend::ShaderStage stage) { return _pipelineLayout->interface().stagePresent(stage); }

        backend::vulkan::PipelineLayoutHandle layout() const { return _pipelineLayout; }

        backend::vulkan::ShaderProgram getBackendProgram() const;

    private:

        Engine* _engine;

        AssetManager::Asset<backend::vulkan::ShaderModuleHandle> _modules[4];
        backend::vulkan::PipelineLayoutHandle _pipelineLayout;

    };

}

#endif //CALA_PROGRAM_H
