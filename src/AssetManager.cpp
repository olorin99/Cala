#include "Cala/AssetManager.h"
#include <Ende/filesystem/File.h>
#include <Cala/backend/primitives.h>
#include <Cala/util.h>
#include <Cala/Engine.h>
#include "../../third_party/stb_image.h"
#include <Cala/backend/primitives.h>
#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <Cala/Material.h>
#include <Cala/MaterialInstance.h>
#include <Ende/math/Mat.h>
#include <Ende/math/Quaternion.h>

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

template <>
cala::Model& cala::AssetManager::Asset<cala::Model>::operator*() noexcept {
    auto& metadata = _manager->_metadata[_index];
    auto& modelMetadata = _manager->_models[metadata.index];
    return modelMetadata.model;
}

template <>
cala::Model& cala::AssetManager::Asset<cala::Model>::operator*() const noexcept {
    auto& metadata = _manager->_metadata[_index];
    auto& modelMetadata = _manager->_models[metadata.index];
    return modelMetadata.model;
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

i32 cala::AssetManager::registerModel(const std::string &name, const std::filesystem::path &path, u32 hash) {
    i32 index = getAssetIndex(hash);
    if (index >= 0)
        return index;

    index = _metadata.size();
    _assetMap.insert(std::make_pair(hash, index));
    _metadata.push_back({
        name,
        path,
        false,
        static_cast<i32>(_models.size())
    });
    _models.push_back({
        hash,
        name,
        path,
        Model()
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

cala::backend::vulkan::ImageHandle cala::AssetManager::loadImage(const std::string &name, const std::filesystem::path &path, backend::Format format) {
    u32 hash = std::hash<std::filesystem::path>()(path);

    i32 index = getAssetIndex(hash);
    if (index < 0)
        index = registerImage(name, path, hash);

    auto& metadata = _metadata[index];
    if (metadata.loaded) {
        auto& imageMetadata = _images[metadata.index];
        return imageMetadata.imageHandle;
    }

    i32 width, height, channels;
    u8* data = nullptr;
    u32 length = 0;
    if (backend::formatToSize(format) > 4) {
        stbi_set_flip_vertically_on_load(true);
        f32* hdrData = stbi_loadf(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        data = reinterpret_cast<u8*>(hdrData);
    } else {
        data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    }
    if (!data) {
        _engine->logger().warn("unable to load image: {}", path.string());
        return { nullptr, -1, nullptr };
    };

    length = width * height * backend::formatToSize(format);

    u32 mips = std::floor(std::log2(std::max(width, height))) + 1;
    auto handle = _engine->device().createImage({
        (u32)width,
        (u32)height,
        1,
        format,
        mips,
        1,
        backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST | backend::ImageUsage::TRANSFER_SRC});

    _engine->device().deferred([handle](backend::vulkan::CommandHandle cmd) {
        handle->generateMips(cmd);
    });

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
    imageMetadata.format = format;
    imageMetadata.imageHandle = handle;

    metadata.loaded = true;

    return imageMetadata.imageHandle;
}

cala::backend::vulkan::ImageHandle cala::AssetManager::reloadImage(u32 hash) {
    i32 index = getAssetIndex(hash);
    if (index < 0)
        return { nullptr, -1, nullptr };

    auto& metadata = _metadata[index];
    auto& imageMetadata = _images[metadata.index];

    metadata.loaded = false;

    auto image = loadImage(imageMetadata.name, imageMetadata.path, imageMetadata.format);

    metadata.loaded = true;

    return image;
}

template <>
struct fastgltf::ElementTraits<ende::math::Vec3f> : fastgltf::ElementTraitsBase<ende::math::Vec3f, AccessorType::Vec3, float> {};
template <>
struct fastgltf::ElementTraits<ende::math::Vec<2, f32>> : fastgltf::ElementTraitsBase<ende::math::Vec<2, f32>, AccessorType::Vec2, float> {};
template <>
struct fastgltf::ElementTraits<ende::math::Vec4f> : fastgltf::ElementTraitsBase<ende::math::Vec4f, AccessorType::Vec4, float> {};


cala::AssetManager::Asset<cala::Model> cala::AssetManager::loadModel(const std::string &name, const std::filesystem::path &path, Material* material) {
    u32 hash = std::hash<std::filesystem::path>()(path);

    i32 index = getAssetIndex(hash);
    if (index < 0)
        index = registerModel(name, path, hash);

    assert(index < _metadata.size());
    auto& metadata = _metadata[index];
    if (metadata.loaded)
        return { this, index };

    assert(metadata.index < _models.size());
    auto& modelMetadata = _models[metadata.index];

    fastgltf::Parser parser;
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(path);

    auto type = fastgltf::determineGltfFileType(&data);
    auto asset = type == fastgltf::GltfType::GLB ?
            parser.loadBinaryGLTF(&data, path.parent_path(), fastgltf::Options::None) :
            parser.loadGLTF(&data, path.parent_path(), fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers);

    if (auto error = asset.error(); error != fastgltf::Error::None) {
        _engine->logger().warn("unable to load model: {}", path.string());
        return { this, index };
    }

    // load images
    std::vector<backend::vulkan::ImageHandle> images;

    // load materials
    std::vector<MaterialInstance> materials;
    for (auto& modelMaterial : asset->materials) {
        MaterialInstance materialInstance = material->instance();

        if (modelMaterial.pbrData.baseColorTexture.has_value()) {
            i32 textureIndex = modelMaterial.pbrData.baseColorTexture->textureIndex;
            i32 imageIndex = asset->textures[textureIndex].imageIndex.value();
            auto& image = asset->images[imageIndex];
            if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                images.push_back(loadImage(image.name.c_str(), path.parent_path() / filePath->uri.path(), backend::Format::RGBA8_SRGB));
            }
            if (!materialInstance.setParameter("albedoIndex", images.back().index()))
                _engine->logger().warn("tried to load \"albedoIndex\" but supplied material does not have appropriate parameter");
        } else
            materialInstance.setParameter("albedoIndex", -1);


        if (modelMaterial.normalTexture.has_value()) {
            i32 textureIndex = modelMaterial.normalTexture->textureIndex;
            i32 imageIndex = asset->textures[textureIndex].imageIndex.value();
            auto& image = asset->images[imageIndex];
            if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                images.push_back(loadImage(image.name.c_str(), path.parent_path() / filePath->uri.path(), backend::Format::RGBA8_UNORM));
            }
            if (!materialInstance.setParameter("normalIndex", images.back().index()))
                _engine->logger().warn("tried to load \"normalIndex\" but supplied material does not have appropriate parameter");
        } else
            materialInstance.setParameter("normalIndex", -1);


        if (modelMaterial.pbrData.metallicRoughnessTexture.has_value()) {
            i32 textureIndex = modelMaterial.pbrData.metallicRoughnessTexture->textureIndex;
            i32 imageIndex = asset->textures[textureIndex].imageIndex.value();
            auto& image = asset->images[imageIndex];
            if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                images.push_back(loadImage(image.name.c_str(), path.parent_path() / filePath->uri.path(), backend::Format::RGBA8_UNORM));
            }
            if (!materialInstance.setParameter("metallicRoughnessIndex", images.back().index()))
                _engine->logger().warn("tried to load \"metallicRoughnessIndex\" but supplied material does not have appropriate parameter");
        } else
            materialInstance.setParameter("metallicRoughnessIndex", -1);

        materials.push_back(std::move(materialInstance));
    }

    // load meshes
    struct Vertex {
        ende::math::Vec3f position = { 0, 0, 0 };
        ende::math::Vec3f normal = { 1, 0, 0 };
        ende::math::Vec<2, f32> texCoords = { 0, 0 };
        ende::math::Vec4f tangent = { 0, 1, 0 };
    };
    std::vector<Vertex> vertices;
    std::vector<u32> indices;

    std::vector<Model::Primitive> meshes;

    for (auto& node : asset->nodes) {
        ende::math::Mat4f transform = ende::math::identity<4, f32>();
        if (auto* trs = std::get_if<fastgltf::Node::TRS>(&node.transform); trs) {
            ende::math::Quaternion rotation(trs->rotation[0], trs->rotation[1], trs->rotation[2], trs->rotation[3]);
            ende::math::Vec3f scale{ trs->scale[0], trs->scale[1], trs->scale[2] };
            ende::math::Vec3f translation{ trs->translation[0], trs->translation[1], trs->translation[2] };

//            transform = ende::math::scale<4>(scale);
            transform = ende::math::translation<4, f32>(translation) * rotation.toMat() * ende::math::scale<4, f32>(scale);
        } else if (auto* mat = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform); mat) {
            transform = ende::math::Mat4f(*mat);
        }

        u32 meshIndex = node.meshIndex.value();
        auto& mesh = asset->meshes[meshIndex];
        for (auto& primitive : mesh.primitives) {

            ende::math::Vec3f min = { 10000, 10000, 10000 };
            ende::math::Vec3f max = min * -1;

            u32 firstIndex = indices.size();
            u32 firstVertex = vertices.size();
            auto& indicesAccessor = asset->accessors[primitive.indicesAccessor.value()];
            u32 indexCount = indicesAccessor.count;
            indices.resize(indices.size() + indicesAccessor.count);
            fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset.get(), indicesAccessor, [&](std::uint32_t index, std::size_t idx) {
                assert(firstIndex + idx < indices.size());
                indices[firstIndex + idx] = index + firstVertex;
            });

            auto positionIT = primitive.findAttribute("POSITION");
            auto& positionAccessor = asset->accessors[positionIT->second];
            vertices.resize(vertices.size() + positionAccessor.count);
            fastgltf::iterateAccessorWithIndex<ende::math::Vec3f>(asset.get(), positionAccessor, [&](ende::math::Vec3f position, std::size_t idx) {
                assert(firstVertex + idx < vertices.size());
                position = transform.transform(position);
                vertices[firstVertex + idx].position = position;
                min = {
                        std::min(min.x(), position.x()),
                        std::min(min.y(), position.y()),
                        std::min(min.z(), position.z())
                };
                max = {
                        std::max(max.x(), position.x()),
                        std::max(max.y(), position.y()),
                        std::max(max.z(), position.z())
                };
            });

            auto normalIT = primitive.findAttribute("NORMAL");
            auto& normalAccessor = asset->accessors[normalIT->second];
            fastgltf::iterateAccessorWithIndex<ende::math::Vec3f>(asset.get(), normalAccessor, [&](ende::math::Vec3f normal, std::size_t idx) {
                assert(firstVertex + idx < vertices.size());
                vertices[firstVertex + idx].normal = normal;
            });

            auto texCoordIT = primitive.findAttribute("TEXCOORD_0");
            auto& texCoordAccessor = asset->accessors[texCoordIT->second];
            fastgltf::iterateAccessorWithIndex<ende::math::Vec<2, f32>>(asset.get(), texCoordAccessor, [&](ende::math::Vec<2, f32> texCoord, std::size_t idx) {
                assert(firstVertex + idx < vertices.size());
                vertices[firstVertex + idx].texCoords = texCoord;
            });

            auto tangentIT = primitive.findAttribute("TANGENT");
            auto& tangentAccessor = asset->accessors[tangentIT->second];
            fastgltf::iterateAccessorWithIndex<ende::math::Vec4f>(asset.get(), tangentAccessor, [&](ende::math::Vec4f tangent, std::size_t idx) {
                assert(firstVertex + idx < vertices.size());
                vertices[firstVertex + idx].tangent = tangent;
            });

            Model::Primitive model{};
            model.firstIndex = firstIndex;
            model.indexCount = indexCount;
            model.materialIndex = primitive.materialIndex.value();
            model.aabb.min = min;
            model.aabb.max = max;
            meshes.push_back(model);
        }
    }

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 12 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<backend::Attribute, 4> attributes = {
            backend::Attribute{0, 0, backend::AttribType::Vec3f},
            backend::Attribute{1, 0, backend::AttribType::Vec3f},
            backend::Attribute{2, 0, backend::AttribType::Vec2f},
            backend::Attribute{3, 0, backend::AttribType::Vec4f}
    };

    std::span<f32> vs(reinterpret_cast<f32*>(vertices.data()), vertices.size() * sizeof(Vertex) / sizeof(f32));
    u32 vertexOffset = _engine->uploadVertexData(vs);
    for (auto& idx : indices)
        idx += vertexOffset / sizeof(Vertex);
    u32 indexOffset = _engine->uploadIndexData(indices);
    for (auto& mesh : meshes)
        mesh.firstIndex += indexOffset / sizeof(u32);

    Model model;
    model.primitives = std::move(meshes);
    model.images = images;
    model.materials = std::move(materials);
    model._binding = binding;
    model._attributes = attributes;

    modelMetadata.hash = hash;
    modelMetadata.name = name;
    modelMetadata.path = path;
    modelMetadata.model = std::move(model);

    _metadata[index].loaded = true;

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

void cala::AssetManager::clear() {
    _shaderModules.clear();
    _images.clear();
    _models.clear();
}