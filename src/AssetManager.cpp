#include "Cala/AssetManager.h"
#include <Ende/filesystem/File.h>
#include <Cala/backend/primitives.h>
#include <Cala/util.h>
#include <Cala/Engine.h>
#include "../../third_party/stb_image.h"
#include <Cala/backend/primitives.h>

template <>
cala::backend::vulkan::Handle<cala::backend::vulkan::ShaderModule, cala::backend::vulkan::Device>& cala::AssetManager::Asset<cala::backend::vulkan::ShaderModuleHandle>::operator*() noexcept {
    auto& metadata = _manager->_metadata[_index];
    auto& shaderModuleMetadata = _manager->_shaderModules[metadata.index];
    return shaderModuleMetadata.moduleHandle;
}

template <>
cala::backend::vulkan::Handle<cala::backend::vulkan::ShaderModule, cala::backend::vulkan::Device>& cala::AssetManager::Asset<cala::backend::vulkan::ShaderModuleHandle>::operator*() const noexcept {
    auto& metadata = _manager->_metadata[_index];
    auto& shaderModuleMetadata = _manager->_shaderModules[metadata.index];
    return shaderModuleMetadata.moduleHandle;
}

template <>
cala::backend::vulkan::ImageHandle& cala::AssetManager::Asset<cala::backend::vulkan::ImageHandle>::operator*() noexcept {
    auto& metadata = _manager->_metadata[_index];
    auto& imageMetadata = _manager->_images[metadata.index];
    return imageMetadata.imageHandle;
}

template <>
cala::backend::vulkan::ImageHandle& cala::AssetManager::Asset<cala::backend::vulkan::ImageHandle>::operator*() const noexcept {
    auto& metadata = _manager->_metadata[_index];
    auto& imageMetadata = _manager->_images[metadata.index];
    return imageMetadata.imageHandle;
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

i32 cala::AssetManager::registerShaderModule(const std::string& name, const std::filesystem::path &path, u32 hash) {
    i32 index = getAssetIndex(hash);
    if (index >= 0)
        return index;

    index = _metadata.size();
    _assetMap.insert(std::make_pair(hash, index));
    _metadata.push_back({
        name,
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

i32 cala::AssetManager::registerImage(const std::string &name, const std::filesystem::path &path, u32 hash) {
    i32 index = getAssetIndex(hash);
    if (index >= 0)
        return index;

    index = _metadata.size();
    _assetMap.insert(std::make_pair(hash, index));
    _metadata.push_back({
        name,
        path,
        false,
        static_cast<i32>(_images.size())
    });
    _images.push_back({
        hash,
        name,
        path,
        {}
    });
    return index;
}


inline u32 combineHash(u32 first, u32 second) {
    return first ^= second + 0x9e3779b9 + (first<<6) + (first>>2);
}

cala::backend::vulkan::ShaderModuleHandle cala::AssetManager::loadShaderModule(const std::string& name, const std::filesystem::path &path, backend::ShaderStage stage, std::span<const std::pair<std::string_view, std::string_view>> macros, std::span<const std::string> includes) {
    u32 hash = std::hash<std::filesystem::path>()(path);

    std::hash<std::string_view> hasher;
    for (auto& macro : macros) {
        u32 h = hasher(macro.second);
        hash = combineHash(hash, h);
    }


    i32 index = getAssetIndex(hash);
    if (index < 0)
        index = registerShaderModule(name, path, hash);

    auto& metadata = _metadata[index];
    if (metadata.loaded) {
        auto& moduleMetadata = _shaderModules[metadata.index];
        return moduleMetadata.moduleHandle;
    }

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
        return { nullptr, -1, nullptr };

    auto rawSource = file.read();

    std::string source = "#version 460\n"
        "\n"
        "#extension GL_EXT_nonuniform_qualifier : enable\n"
        "#extension GL_GOOGLE_include_directive : enable\n"
        "#extension GL_EXT_buffer_reference : enable\n"
        "#extension GL_EXT_scalar_block_layout : enable\n\n";

    source += rawSource;

    size_t it = source.find("INCLUDES_GO_HERE;");
    if (it != std::string::npos) {
        source.erase(it, 17);
        for (auto& include : includes) {
            source.insert(it, std::format("\n#include \"{}\"\n", include));
        }
    }

    std::filesystem::path searchPaths[] = {
            "/home/christian/Documents/Projects/Cala/res/shaders"
    };

    auto& moduleMetadata = _shaderModules[metadata.index];

    auto expectedSpirV = util::compileGLSLToSpirv(&_engine->device(), path.string(), source, stage, macros, searchPaths);
    if (!expectedSpirV.has_value()) {
        _engine->logger().warn("unable to load shader: {}", path.string());
        return moduleMetadata.moduleHandle;
    }


    auto module = _engine->device().recreateShaderModule(moduleMetadata.moduleHandle, expectedSpirV.value(), stage);

    moduleMetadata.moduleHandle = module;
    moduleMetadata.macros.clear();
    moduleMetadata.macros.insert(moduleMetadata.macros.begin(), macros.begin(), macros.end());
    moduleMetadata.includes.clear();
    moduleMetadata.includes.insert(moduleMetadata.includes.begin(), includes.begin(), includes.end());
    moduleMetadata.stage = stage;
    moduleMetadata.path = path;
    moduleMetadata.name = name;
    moduleMetadata.hash = hash;

    metadata.loaded = true;

    return module;
}

cala::backend::vulkan::ShaderModuleHandle cala::AssetManager::reloadShaderModule(u32 hash) {
    i32 index = getAssetIndex(hash);
    if (index < 0)
        return { nullptr, -1, nullptr };

    auto& metadata = _metadata[index];
    auto& moduleMetadata = _shaderModules[metadata.index];

    metadata.loaded = false;

    std::vector<std::pair<std::string_view, std::string_view>> macros;
    for (auto& macro : moduleMetadata.macros)
        macros.push_back({macro.first, macro.second});

    auto module = loadShaderModule(moduleMetadata.name, moduleMetadata.path, moduleMetadata.stage, macros, moduleMetadata.includes);

    metadata.loaded = true;
    return module;
}

cala::AssetManager::Asset <cala::backend::vulkan::ImageHandle> cala::AssetManager::loadImage(const std::string &name, const std::filesystem::path &path, bool hdr) {
    u32 hash = std::hash<std::filesystem::path>()(path);

    i32 index = getAssetIndex(hash);
    if (index < 0)
        index = registerImage(name, path, hash);

    auto& metadata = _metadata[index];
    if (metadata.loaded)
        return { this, index };

    i32 width, height, channels;
    u8* data = nullptr;
    backend::Format format = backend::Format::RGBA8_UNORM;
    u32 length = 0;
    if (hdr) {
        stbi_set_flip_vertically_on_load(true);
        f32* hdrData = stbi_loadf(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        data = reinterpret_cast<u8*>(hdrData);
        format = backend::Format::RGBA32_SFLOAT;
    } else {
        data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    }
    if (!data) {
        _engine->logger().warn("unable to load image: {}", path.string());
        return { this, index };
    };

    length = width * height * backend::formatToSize(format);
    auto handle = _engine->device().createImage({(u32)width, (u32)height, 1, format});

    _engine->stageData(handle, std::span(data, length), {
        0,
        static_cast<u32>(width),
        static_cast<u32>(height),
        1,
        backend::formatToSize(format)
    });

    stbi_image_free(data);

    auto& imageMetadata = _images[metadata.index];
    imageMetadata.hash = hash;
    imageMetadata.name = name;
    imageMetadata.path = path;
    imageMetadata.hdr = hdr;
    imageMetadata.imageHandle = handle;

    metadata.loaded = true;

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