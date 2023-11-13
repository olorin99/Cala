#include "Cala/AssetManager.h"
#include <Ende/filesystem/File.h>
#include <Cala/backend/primitives.h>
#include <Cala/util.h>
#include <Cala/Engine.h>

template <>
cala::backend::vulkan::Handle<cala::backend::vulkan::ShaderModule, cala::backend::vulkan::Device>& cala::AssetManager::Asset<cala::backend::vulkan::ShaderModuleHandle>::operator*() noexcept {
    auto& metadata = _manager->_metadata[_index];
    auto& shaderModuleMetadata = _manager->_shaderModules[metadata.index];
    return shaderModuleMetadata.moduleHandle;
}



cala::AssetManager::AssetManager(cala::Engine *engine)
    : _engine(engine)
{}

void cala::AssetManager::setAssetPath(const std::filesystem::path &path) {
    _rootAssetPath = path;
    addSearchPath(path);
}

void cala::AssetManager::addSearchPath(const std::filesystem::path &path) {
    _searchPaths.push_back(path);
}


i32 cala::AssetManager::getAssetIndex(u32 hash) {
    auto it = _assetMap.find(hash);
    if (it == _assetMap.end())
        return -1;
    return it->second;
}

bool cala::AssetManager::isRegistered(u32 hash) {
    return getAssetIndex(hash) >= 0;
}

bool cala::AssetManager::isRegistered(const std::filesystem::path &path) {
    return isRegistered(std::hash<std::filesystem::path>()(path));
}

i32 cala::AssetManager::registerShaderModule(const std::filesystem::path &path, u32 hash) {
    i32 index = getAssetIndex(hash);
    if (index >= 0)
        return index;

    index = _metadata.size();
    _assetMap.insert(std::make_pair(hash, index));
    _metadata.push_back({
        path,
        false,
        static_cast<i32>(_shaderModules.size())
    });
    _shaderModules.push_back({
        {},
        {},
        {}
    });
    return index;
}


inline u32 combineHash(u32 first, u32 second) {
    return first ^= second + 0x9e3779b9 + (first<<6) + (first>>2);
}

cala::AssetManager::Asset<cala::backend::vulkan::ShaderModuleHandle> cala::AssetManager::loadShaderModule(const std::filesystem::path &path, backend::ShaderStage stage, std::span<const std::pair<std::string_view, std::string_view>> macros) {
    u32 hash = std::hash<std::filesystem::path>()(path);

    std::hash<std::string_view> hasher;
    for (auto& macro : macros) {
        u32 h = hasher(macro.second);
        hash = combineHash(hash, h);
    }


    i32 index = getAssetIndex(hash);
    if (index < 0)
        index = registerShaderModule(path, hash);

    auto& metadata = _metadata[index];
    if (metadata.loaded)
        return { this, index };

    // if not defined use file extension to find stage
    if (stage == backend::ShaderStage::NONE) {
        auto extension = path.extension();
        if (extension == ".vert")
            stage = backend::ShaderStage::VERTEX;
        else if (extension == ".frag")
            stage = backend::ShaderStage::FRAGMENT;
        else if (extension == ".geom")
            stage = backend::ShaderStage::GEOMETRY;
        else if (extension == ".comp")
            stage = backend::ShaderStage::COMPUTE;
    }

    ende::fs::File file;
    if (!file.open(_rootAssetPath / path))
        return { this, -1 };

    auto rawSource = file.read();

    std::string source = "#version 460\n"
        "\n"
        "#extension GL_EXT_nonuniform_qualifier : enable\n"
        "#extension GL_GOOGLE_include_directive : enable\n"
        "#extension GL_EXT_buffer_reference : enable\n"
        "#extension GL_EXT_scalar_block_layout : enable\n";

    source += rawSource;

    std::filesystem::path searchPaths[] = {
            "/home/christian/Documents/Projects/Cala/res/shaders"
    };

    auto expectedSpirV = util::compileGLSLToSpirv(&_engine->device(), path.string(), source, stage, macros, searchPaths);

    auto module = _engine->device().createShaderModule(expectedSpirV.value(), stage);

    metadata.loaded = true;

    auto& moduleMetadata = _shaderModules[metadata.index];
    moduleMetadata.moduleHandle = module;
    moduleMetadata.macros.clear();
    moduleMetadata.macros.insert(moduleMetadata.macros.begin(), macros.begin(), macros.end());

    return { this, index };
}

bool cala::AssetManager::isLoaded(u32 hash) {
    i32 index = getAssetIndex(hash);
    if (index < 0)
        return false;
    auto& metadata = _metadata[index];
    return metadata.loaded;
}

bool cala::AssetManager::isLoaded(const std::filesystem::path &path) {
    u32 hash = std::hash<std::filesystem::path>()(path);
    return isLoaded(hash);
}