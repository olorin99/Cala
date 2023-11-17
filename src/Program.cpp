#include "Cala/Program.h"
#include <Cala/backend/vulkan/ShaderModule.h>
#include <Cala/Engine.h>

cala::Program::Program()
    : _engine(nullptr)
{}

cala::Program::Program(cala::Engine* engine, std::span<AssetManager::Asset<backend::vulkan::ShaderModuleHandle>> modules)
    : _engine(engine),
    _pipelineLayout()
{
    std::vector<backend::vulkan::ShaderModuleInterface> interfaces;

    for (u32 i = 0; i < modules.size() && i < 4; i++) {
        _modules[i] = modules[i];
        interfaces.push_back((*modules[i])->interface());
    }
    _pipelineLayout = _engine->device().createPipelineLayout(backend::ShaderInterface(interfaces));
}

cala::Program::Program(cala::Program &&rhs) noexcept
    : _engine(nullptr),
    _pipelineLayout()
{
    std::swap(_engine, rhs._engine);
    std::swap(_pipelineLayout, rhs._pipelineLayout);
    for (u32 i = 0; i < 4; i++)
        std::swap(_modules[i], rhs._modules[i]);
}

cala::Program &cala::Program::operator=(cala::Program &&rhs) noexcept {
    std::swap(_engine, rhs._engine);
    std::swap(_pipelineLayout, rhs._pipelineLayout);
    for (u32 i = 0; i < 4; i++)
        std::swap(_modules[i], rhs._modules[i]);
    return *this;
}

cala::backend::vulkan::ShaderProgram cala::Program::getBackendProgram() const {
    backend::vulkan::ShaderProgram program(&_engine->device());

    for (auto& module : _modules) {
        if (module)
            program._modules.push_back(*module);
    }

    program._pipelineLayout = _pipelineLayout;


    return program;
//    return _engine->device().createProgram(std::move(program));
}