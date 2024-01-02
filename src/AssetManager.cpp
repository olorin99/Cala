#include "Cala/AssetManager.h"
#include <Ende/filesystem/File.h>
#include <Cala/vulkan/primitives.h>
#include <Cala/util.h>
#include <Cala/Engine.h>
#include <stb_image.h>
#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <Cala/Material.h>
#include <Cala/MaterialInstance.h>
#include <Ende/math/Mat.h>
#include <Ende/math/Quaternion.h>
#include <stack>
#include <meshoptimizer.h>
#include <Cala/shaderBridge.h>

template <>
cala::vk::Handle<cala::vk::ShaderModule, cala::vk::Device>& cala::AssetManager::Asset<cala::vk::ShaderModuleHandle>::operator*() noexcept {
    auto& metadata = _manager->_metadata[_index];
    auto& shaderModuleMetadata = _manager->_shaderModules[metadata.index];
    return shaderModuleMetadata.moduleHandle;
}

template <>
cala::vk::Handle<cala::vk::ShaderModule, cala::vk::Device>& cala::AssetManager::Asset<cala::vk::ShaderModuleHandle>::operator*() const noexcept {
    auto& metadata = _manager->_metadata[_index];
    auto& shaderModuleMetadata = _manager->_shaderModules[metadata.index];
    return shaderModuleMetadata.moduleHandle;
}

template <>
cala::vk::ImageHandle& cala::AssetManager::Asset<cala::vk::ImageHandle>::operator*() noexcept {
    auto& metadata = _manager->_metadata[_index];
    auto& imageMetadata = _manager->_images[metadata.index];
    return imageMetadata.imageHandle;
}

template <>
cala::vk::ImageHandle& cala::AssetManager::Asset<cala::vk::ImageHandle>::operator*() const noexcept {
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

cala::vk::ShaderModuleHandle cala::AssetManager::loadShaderModule(const std::string& name, const std::filesystem::path &path, vk::ShaderStage stage, const std::vector<util::Macro>& macros, std::span<const std::string> includes, const ende::math::Vec<3, u32>& localSize) {
    u32 hash = std::hash<std::filesystem::path>()(absolute(path));

    std::hash<std::string_view> hasher;
    for (auto& macro : macros) {
        u32 h = hasher(macro.value);
        hash = ende::util::combineHash(hash, h);
    }
    for (auto& include : includes) {
        u32 h = hasher(include);
        hash = ende::util::combineHash(hash, h);
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
    if (stage == vk::ShaderStage::NONE) {
        auto extension = path.extension();
        if (extension == ".vert")
            stage = vk::ShaderStage::VERTEX;
        else if (extension == ".frag")
            stage = vk::ShaderStage::FRAGMENT;
        else if (extension == ".geom")
            stage = vk::ShaderStage::GEOMETRY;
        else if (extension == ".comp")
            stage = vk::ShaderStage::COMPUTE;
    }

    auto file = ende::fs::File::open(_rootAssetPath / path);
    if (!file)
        return { nullptr, -1, nullptr };

    auto rawSource = file->read();

    std::string source = "#version 460\n"
        "\n"
        "#extension GL_EXT_nonuniform_qualifier : enable\n"
        "#extension GL_GOOGLE_include_directive : enable\n"
        "#extension GL_EXT_buffer_reference2 : enable\n"
        "#extension GL_EXT_scalar_block_layout : enable\n"
        "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n\n";

    if (stage == vk::ShaderStage::MESH || stage == vk::ShaderStage::TASK)
        source += "#extension GL_EXT_mesh_shader : enable\n"
                  "#extension GL_KHR_shader_subgroup_ballot : enable\n\n";

    source += rawSource;

    size_t it = source.find("INCLUDES_GO_HERE;");
    if (it != std::string::npos) {
        source.erase(it, 17);
        for (auto& include : includes) {
            source.insert(it, std::format("\n#include \"{}\"\n", include));
        }
    }

    std::filesystem::path searchPaths[] = {
            _rootAssetPath / "shaders",
            _rootAssetPath / "../include/Cala"
    };

    auto& moduleMetadata = _shaderModules[metadata.index];

    auto finalMacros = macros;
    finalMacros.push_back({
        "LOCAL_SIZE_X",
        std::to_string(localSize.x())
    });
    finalMacros.push_back({
        "LOCAL_SIZE_Y",
        std::to_string(localSize.y())
    });
    finalMacros.push_back({
        "LOCAL_SIZE_Z",
        std::to_string(localSize.z())
    });
    finalMacros.push_back({
        "MAX_IMAGE_DIMENSIONS_1D",
        std::to_string(_engine->device().context().getLimits().maxImageDimensions1D)
    });
    finalMacros.push_back({
        "MAX_IMAGE_DIMENSIONS_2D",
        std::to_string(_engine->device().context().getLimits().maxImageDimensions2D)
    });
    finalMacros.push_back({
        "MAX_IMAGE_DIMENSIONS_3D",
        std::to_string(_engine->device().context().getLimits().maxImageDimensions3D)
    });

    auto expectedSpirV = util::compileGLSLToSpirv(path.string(), source, stage, finalMacros, searchPaths);
    if (!expectedSpirV.has_value()) {
        _engine->logger().warn("unable to load shader: {}", path.string());
        _engine->logger().error(expectedSpirV.error());
        return moduleMetadata.moduleHandle;
    }


    auto module = _engine->device().recreateShaderModule(moduleMetadata.moduleHandle, expectedSpirV.value(), stage);

    moduleMetadata.moduleHandle = module;
    moduleMetadata.localSize = module->localSize();
    moduleMetadata.macros.clear();
    for (auto& macro : macros)
        moduleMetadata.macros.push_back(macro);
    moduleMetadata.includes.clear();
    for (auto& include : includes)
        moduleMetadata.includes.push_back(include);
    moduleMetadata.stage = stage;
    moduleMetadata.path = path;
    moduleMetadata.name = name;
    moduleMetadata.hash = hash;

    metadata.loaded = true;

    return module;
}

cala::vk::ShaderModuleHandle cala::AssetManager::reloadShaderModule(u32 hash) {
    i32 index = getAssetIndex(hash);
    if (index < 0)
        return { nullptr, -1, nullptr };

    auto& metadata = _metadata[index];
    auto& moduleMetadata = _shaderModules[metadata.index];

    metadata.loaded = false;

    std::vector<util::Macro> macros;
    for (auto& macro : moduleMetadata.macros)
        macros.push_back(macro);

    std::vector<std::string> includes;
    for (auto& include : moduleMetadata.includes)
        includes.push_back(include);

    auto module = loadShaderModule(moduleMetadata.name, moduleMetadata.path, moduleMetadata.stage, macros, includes, moduleMetadata.localSize);

    metadata.loaded = true;
    return module;
}

cala::vk::ImageHandle cala::AssetManager::loadImage(const std::string &name, const std::filesystem::path &path, vk::Format format) {
    u32 hash = std::hash<std::filesystem::path>()(absolute(path));

    i32 index = getAssetIndex(hash);
    if (index < 0)
        index = registerImage(name, path, hash);

    auto& metadata = _metadata[index];
    if (metadata.loaded) {
        auto& imageMetadata = _images[metadata.index];
        return imageMetadata.imageHandle;
    }

    auto filePath = _rootAssetPath / path;

    i32 width, height, channels;
    u8* data = nullptr;
    u32 length = 0;
    if (vk::formatToSize(format) > 4) {
        stbi_set_flip_vertically_on_load(true);
        f32* hdrData = stbi_loadf(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        data = reinterpret_cast<u8*>(hdrData);
    } else {
        stbi_set_flip_vertically_on_load(false);
        data = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    }
    if (!data) {
        _engine->logger().warn("unable to load image: {}", path.string());
        return { nullptr, -1, nullptr };
    };

    length = width * height * vk::formatToSize(format);

    u32 mips = std::floor(std::log2(std::max(width, height))) + 1;
    auto handle = _engine->device().createImage({
        (u32)width,
        (u32)height,
        1,
        format,
        mips,
        1,
        vk::ImageUsage::SAMPLED | vk::ImageUsage::TRANSFER_DST | vk::ImageUsage::TRANSFER_SRC});

    _engine->device().deferred([handle](vk::CommandHandle cmd) {
        handle->generateMips(cmd);
    });

    _engine->stageData(handle, std::span(data, length), {
        0,
        static_cast<u32>(width),
        static_cast<u32>(height),
        1,
        vk::formatToSize(format)
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

cala::vk::ImageHandle cala::AssetManager::reloadImage(u32 hash) {
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
    u32 hash = std::hash<std::filesystem::path>()(absolute(path));

    i32 index = getAssetIndex(hash);
    if (index < 0)
        index = registerModel(name, path, hash);

    assert(index < _metadata.size());
    auto& metadata = _metadata[index];
    if (metadata.loaded)
        return { this, index };

    assert(metadata.index < _models.size());
    auto& modelMetadata = _models[metadata.index];

    auto filePath = _rootAssetPath / path;

    fastgltf::Parser parser;
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);

    auto type = fastgltf::determineGltfFileType(&data);
    auto asset = type == fastgltf::GltfType::GLB ?
            parser.loadGltfBinary(&data, filePath.parent_path(), fastgltf::Options::None) :
            parser.loadGltf(&data, filePath.parent_path(), fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers);

    if (auto error = asset.error(); error != fastgltf::Error::None) {
        _engine->logger().warn("unable to load model: {}", path.string());
        return { this, index };
    }

    // load images
    std::vector<vk::ImageHandle> images;

    // load materials
    std::vector<MaterialInstance> materials;
    for (auto& modelMaterial : asset->materials) {
        MaterialInstance materialInstance = material->instance();

        if (modelMaterial.pbrData.baseColorTexture.has_value()) {
            i32 textureIndex = modelMaterial.pbrData.baseColorTexture->textureIndex;
            i32 imageIndex = asset->textures[textureIndex].imageIndex.value();
            auto& image = asset->images[imageIndex];
            if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                images.push_back(loadImage(image.name.c_str(), path.parent_path() / filePath->uri.path(), vk::Format::RGBA8_SRGB));
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
                images.push_back(loadImage(image.name.c_str(), path.parent_path() / filePath->uri.path(), vk::Format::RGBA8_UNORM));
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
                images.push_back(loadImage(image.name.c_str(), path.parent_path() / filePath->uri.path(), vk::Format::RGBA8_UNORM));
            }
            if (!materialInstance.setParameter("metallicRoughnessIndex", images.back().index()))
                _engine->logger().warn("tried to load \"metallicRoughnessIndex\" but supplied material does not have appropriate parameter");
        } else
            materialInstance.setParameter("metallicRoughnessIndex", -1);

        if (modelMaterial.emissiveTexture.has_value()) {
            i32 textureIndex = modelMaterial.emissiveTexture->textureIndex;
            i32 imageIndex = asset->textures[textureIndex].imageIndex.value();
            auto& image = asset->images[imageIndex];
            if (const auto* filePath = std::get_if<fastgltf::sources::URI>(&image.data); filePath) {
                images.push_back(loadImage(image.name.c_str(), path.parent_path() / filePath->uri.path(), vk::Format::RGBA8_UNORM));
            }
            if (!materialInstance.setParameter("emissiveIndex", images.back().index()))
                _engine->logger().warn("tried to load \"emissiveIndex\" but supplied material does not have appropriate parameter");
        } else
            materialInstance.setParameter("emissiveIndex", -1);

        if (modelMaterial.emissiveStrength.has_value()) {
            f32 emissiveStrength = modelMaterial.emissiveStrength.value();
            if (!materialInstance.setParameter("emissiveStrength", emissiveStrength))
                _engine->logger().warn("tried to load \"emissiveStrength\" but supplied material does not have appropriate parameter");
        } else
            materialInstance.setParameter("emissiveStrength", 0);

        materials.push_back(std::move(materialInstance));
    }

    if (materials.empty()) {
        materials.push_back(material->instance());
    }

    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    std::vector<Meshlet> meshlets;
    std::vector<u8> primitives;

    std::vector<Model::Primitive> meshes;

    Model result;

    struct NodeInfo {
        u32 assetIndex;
        u32 modelIndex;
        ende::math::Mat4f parentTransform;
    };
    std::stack<NodeInfo> nodeIndices;

    for (auto& index : asset->scenes.front().nodeIndices) {
        NodeInfo info{};
        info.assetIndex = index;
        info.modelIndex = result.nodes.size();
        info.parentTransform = ende::math::identity<4, f32>();
        nodeIndices.push(info);
        result.nodes.push_back({});
    }

    while (!nodeIndices.empty()) {
        auto [ assetIndex, modelIndex, parentTransform ] = nodeIndices.top();
        nodeIndices.pop();

        auto& assetNode = asset->nodes[assetIndex];

        Transform transform;
        if (auto* trs = std::get_if<fastgltf::Node::TRS>(&assetNode.transform); trs) {
            ende::math::Quaternion rotation(trs->rotation[0], trs->rotation[1], trs->rotation[2], trs->rotation[3]);
            ende::math::Vec3f scale{ trs->scale[0], trs->scale[1], trs->scale[2] };
            ende::math::Vec3f translation{ trs->translation[0], trs->translation[1], trs->translation[2] };

            transform.setPos(translation);
            transform.setRot(rotation);
            transform.setScale(scale);
//            transform = ende::math::translation<4, f32>(translation) * rotation.toMat() * ende::math::scale<4, f32>(scale);
        } else if (auto* mat = std::get_if<fastgltf::Node::TransformMatrix>(&assetNode.transform); mat) {
            transform = ende::math::Mat4f(*mat);
        }
        ende::math::Mat4f localMatrix = transform.local();
        ende::math::Mat4f worldMatrix = parentTransform * localMatrix;

        for (auto& child : assetNode.children) {
            NodeInfo info{};
            info.assetIndex = child;
            info.modelIndex = result.nodes.size();
            info.parentTransform = worldMatrix;
            result.nodes[modelIndex].children.push_back(info.modelIndex);
            nodeIndices.push(info);
            result.nodes.push_back({});
        }

        auto& modelNode = result.nodes[modelIndex];
        if (assetNode.name.empty())
            modelNode.name = name;
        else
            modelNode.name = assetNode.name;

        modelNode.transform = transform;

        if (!assetNode.meshIndex.has_value())
            continue;
        u32 meshIndex = assetNode.meshIndex.value();
        auto& assetMesh = asset->meshes[meshIndex];
        for (auto& primitive : assetMesh.primitives) {

            ende::math::Vec3f min = { 10000, 10000, 10000 };
            ende::math::Vec3f max = min * -1;

            u32 firstIndex = indices.size();
            u32 firstVertex = vertices.size();
            u32 firstMeshlet = meshlets.size();
            u32 firstPrimitive = primitives.size();

            auto& indicesAccessor = asset->accessors[primitive.indicesAccessor.value()];
            u32 indexCount = indicesAccessor.count;

            std::vector<Vertex> meshVertices;
            std::vector<u32> meshIndices(indexCount);
            fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset.get(), indicesAccessor, [&](std::uint32_t index, std::size_t idx) {
                meshIndices[idx] = index;
            });

            if (auto positionIT = primitive.findAttribute("POSITION"); positionIT != primitive.attributes.end()) {
                auto& positionAccessor = asset->accessors[positionIT->second];
                meshVertices.resize(positionAccessor.count);
                fastgltf::iterateAccessorWithIndex<ende::math::Vec3f>(asset.get(), positionAccessor, [&](ende::math::Vec3f position, std::size_t idx) {
                    position = worldMatrix.transform(position);
                    meshVertices[idx].position = position;
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
            }

            if (auto normalIT = primitive.findAttribute("NORMAL"); normalIT != primitive.attributes.end()) {
                auto& normalAccessor = asset->accessors[normalIT->second];
                fastgltf::iterateAccessorWithIndex<ende::math::Vec3f>(asset.get(), normalAccessor, [&](ende::math::Vec3f normal, std::size_t idx) {
                    meshVertices[idx].normal = normal;
                });
            }

            if (auto texCoordIT = primitive.findAttribute("TEXCOORD_0"); texCoordIT != primitive.attributes.end()) {
                auto& texCoordAccessor = asset->accessors[texCoordIT->second];
                fastgltf::iterateAccessorWithIndex<ende::math::Vec<2, f32>>(asset.get(), texCoordAccessor, [&](ende::math::Vec<2, f32> texCoord, std::size_t idx) {
                    meshVertices[idx].texCoords = texCoord;
                });
            }

            struct MeshletLOD {
                std::vector<u32> indices;
                std::vector<Meshlet> meshlets;
                std::vector<u32> meshletIndices;
                std::vector<u8> primitives;
            };

            const auto generateMeshletLOD = [](const std::vector<Vertex>& vertices, const std::vector<u32>& indices, u32 vertexOffset, u32 indexOffset, u32 primitiveOffset, f32 threshold, f32 error) -> std::optional<MeshletLOD> {
                const u32 maxVertices = 64;
                const u32 maxTriangles = 64;
                const f32 coneWeight = 0.f;

                std::vector<u32> lodIndices = indices;
                if (threshold < 1.f) {
                    if (indices.size() < 1024)
                        return {};
                    lodIndices.clear();
                    lodIndices.resize(indices.size());
                    const u32 targetIndexCount = indices.size() * threshold;
                    f32 lodError = 0.f;
                    u32 lodIndexCount = meshopt_simplify(&lodIndices[0], indices.data(), indices.size(), (f32*)vertices.data(), vertices.size(), sizeof(Vertex), targetIndexCount, error, 0, &lodError);
//                    u32 lodIndexCount = meshopt_simplifySloppy(&lodIndices[0], indices.data(), indices.size(), (f32*)vertices.data(), vertices.size(), sizeof(Vertex), targetIndexCount, error, &lodError);
                    if (indices.size() == lodIndexCount)
                        return {};
                    lodIndices.resize(lodIndexCount);
                    meshopt_optimizeVertexCache(lodIndices.data(), lodIndices.data(), lodIndices.size(), vertices.size());
                }

                u32 maxMeshlets = meshopt_buildMeshletsBound(lodIndices.size(), maxVertices, maxTriangles);
                std::vector<meshopt_Meshlet> meshMeshlets(maxMeshlets);
                std::vector<u32> meshletVertices(maxMeshlets * maxVertices);
                std::vector<u8> meshletTriangles(maxMeshlets * maxTriangles * 3);

                u32 meshletCount = meshopt_buildMeshlets(meshMeshlets.data(), meshletVertices.data(), meshletTriangles.data(), lodIndices.data(), lodIndices.size(), (f32*)vertices.data(), vertices.size(), sizeof(Vertex), maxVertices, maxTriangles, coneWeight);

                auto& lastMeshlet = meshMeshlets[meshletCount - 1];
                meshletVertices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
                meshletTriangles.resize(lastMeshlet.triangle_offset + ((lastMeshlet.triangle_count * 3 + 3) & ~3));
                meshMeshlets.resize(meshletCount);

                std::vector<Meshlet> meshletsMesh;
                meshletsMesh.reserve(meshletCount);

                for (u32 i = 0; i < meshMeshlets.size(); i++) {
                    auto& meshlet = meshMeshlets[i];
                    meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshletVertices[meshlet.vertex_offset], &meshletTriangles[meshlet.triangle_offset], meshlet.triangle_count, (f32*)vertices.data(), vertices.size(), sizeof(Vertex));

                    ende::math::Vec3f center{ bounds.center[0], bounds.center[1], bounds.center[2] };
                    f32 radius = bounds.radius;
                    ende::math::Vec3f coneApex{ bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[2] };
                    ende::math::Vec3f coneAxis{ bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2] };
                    f32 coneCutoff = bounds.cone_cutoff;

                    meshletsMesh.push_back({
                        vertexOffset,
                        meshlet.vertex_offset + indexOffset,
                        meshlet.vertex_count,
                        meshlet.triangle_offset + primitiveOffset,
                        meshlet.triangle_count,
                        center,
                        radius,
                        coneApex,
                        coneAxis,
                        coneCutoff
                    });
                }

                return MeshletLOD{
                    lodIndices,
                    meshletsMesh,
                    meshletVertices,
                    meshletTriangles
                };
            };

            std::vector<unsigned int> remap(indexCount);
            size_t vertexCount = meshopt_generateVertexRemap(&remap[0], &meshIndices[0], indexCount, &meshVertices[0], meshVertices.size(), sizeof(Vertex));

            std::vector<Vertex> optimisedVertices(vertexCount);
            std::vector<u32> optimisedIndices(indexCount);

            meshopt_remapIndexBuffer(&optimisedIndices[0], &meshIndices[0], indexCount, &remap[0]);
            meshopt_remapVertexBuffer(&optimisedVertices[0], &meshVertices[0], meshVertices.size(), sizeof(Vertex), &remap[0]);

            meshopt_optimizeVertexCache(&optimisedIndices[0], &optimisedIndices[0], indexCount, vertexCount);
            meshopt_optimizeOverdraw(&optimisedIndices[0], &optimisedIndices[0], indexCount, (f32*)&optimisedVertices[0], vertexCount, sizeof(Vertex), 1.05f);
            meshopt_optimizeVertexFetch(&optimisedVertices[0], &optimisedIndices[0], indexCount, &optimisedVertices[0], vertexCount, sizeof(Vertex));

            auto lod0 = generateMeshletLOD(optimisedVertices, optimisedIndices, firstVertex, firstIndex, firstPrimitive, 1, 1e-2f).value();

            meshlets.insert(meshlets.end(), lod0.meshlets.begin(), lod0.meshlets.end());
            indices.insert(indices.end(), lod0.meshletIndices.begin(), lod0.meshletIndices.end());
            primitives.insert(primitives.end(), lod0.primitives.begin(), lod0.primitives.end());

            std::vector<MeshletLOD> lods;
            lods.push_back(lod0);
            u32 vertexOffset = firstVertex;
            u32 indexOffset = firstIndex + lod0.meshletIndices.size();
            u32 primitiveOffset = firstPrimitive + lod0.primitives.size();

            u32 lodCount = 1;
            for (u32 level = 1 ; level < MAX_LODS; level++) {
                auto& previousLod = lods.back();
                auto lodOptional = generateMeshletLOD(optimisedVertices, previousLod.indices, vertexOffset, indexOffset, primitiveOffset, 0.2, 1e-2f);
                if (!lodOptional)
                    break;

                auto lod = lodOptional.value();

                indexOffset += lod.meshletIndices.size();
                primitiveOffset += lod.primitives.size();

                meshlets.insert(meshlets.end(), lod.meshlets.begin(), lod.meshlets.end());
                indices.insert(indices.end(), lod.meshletIndices.begin(), lod.meshletIndices.end());
                primitives.insert(primitives.end(), lod.primitives.begin(), lod.primitives.end());

                lods.push_back(lod);
                lodCount++;
            }

//            std::for_each(std::execution::par, optimisedIndices.begin(), optimisedIndices.end(), [firstVertex](auto& index) {
//                index += firstVertex;
//            });
            for (auto& index : optimisedIndices)
                index += firstVertex;

            vertices.insert(vertices.end(), optimisedVertices.begin(), optimisedVertices.end());

            Model::Primitive mesh{};
            mesh.firstIndex = firstIndex;
            mesh.indexCount = indexCount;
            mesh.meshletIndex = firstMeshlet;
            mesh.meshletCount = lod0.meshlets.size();
            mesh.materialIndex = primitive.materialIndex.has_value() ? primitive.materialIndex.value() : 0;
            mesh.aabb.min = min;
            mesh.aabb.max = max;
            mesh.lodCount = lodCount;
            u32 meshletOffset = firstMeshlet;
            for (u32 level = 0; level < lodCount && level < MAX_LODS; level++) {
                auto& lod = lods[level];
                mesh.lods[level].meshletOffset = meshletOffset;
                mesh.lods[level].meshletCount = lod.meshlets.size();
                meshletOffset += lod.meshlets.size();
            }
            modelNode.primitives.push_back(meshes.size());
            meshes.push_back(mesh);
        }
    }

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = 12 * sizeof(f32);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<vk::Attribute, 4> attributes = {
            vk::Attribute{0, 0, vk::AttribType::Vec3f},
            vk::Attribute{1, 0, vk::AttribType::Vec3f},
            vk::Attribute{2, 0, vk::AttribType::Vec2f},
            vk::Attribute{3, 0, vk::AttribType::Vec4f}
    };

    std::span<f32> vs(reinterpret_cast<f32*>(vertices.data()), vertices.size() * sizeof(Vertex) / sizeof(f32));
    u32 vertexOffset = _engine->uploadVertexData(vs);
    u32 indexOffset = _engine->uploadIndexData(indices);
    u32 primitiveOffset = _engine->uploadPrimitiveData(primitives);
//    std::for_each(std::execution::par, meshlets.begin(), meshlets.end(), [vertexOffset, meshletIndexOffset, primitiveOffset](auto& meshlet) {
//        meshlet.vertexOffset += vertexOffset / sizeof(Vertex);
//        meshlet.indexOffset += meshletIndexOffset / sizeof(u32);
//        meshlet.primitiveOffset += primitiveOffset / sizeof(u8);
//    });
    for (auto& meshlet : meshlets) {
        meshlet.vertexOffset += vertexOffset / sizeof(Vertex);
        meshlet.indexOffset += indexOffset / sizeof(u32);
        meshlet.primitiveOffset += primitiveOffset / sizeof(u8);
    }
    u32 meshletOffset = _engine->uploadMeshletData(meshlets);
//    std::for_each(std::execution::par, meshes.begin(), meshes.end(), [meshletOffset](auto& mesh) {
//        mesh.meshletIndex += meshletOffset / sizeof(Meshlet);
//        for (u32 level = 0; level < mesh.lodCount && level < MAX_LODS; level++)
//            mesh.lods[level].meshletOffset += meshletOffset / sizeof(Meshlet);
//    });
    for (auto& mesh : meshes) {
        mesh.meshletIndex += meshletOffset / sizeof(Meshlet);
        for (u32 level = 0; level < mesh.lodCount && level < MAX_LODS; level++)
            mesh.lods[level].meshletOffset += meshletOffset / sizeof(Meshlet);
    }

//    Model model;
    result.primitives = std::move(meshes);
    result.images = images;
    result.materials = std::move(materials);
    result._binding = binding;
    result._attributes = attributes;

    modelMetadata.hash = hash;
    modelMetadata.name = name;
    modelMetadata.path = path;
    modelMetadata.model = std::move(result);

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