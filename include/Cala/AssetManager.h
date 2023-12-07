#ifndef CALA_ASSETMANAGER_H
#define CALA_ASSETMANAGER_H

#include <Ende/platform.h>
#include <filesystem>
#include <tsl/robin_map.h>
#include <Cala/vulkan/Handle.h>
#include <Cala/vulkan/primitives.h>
#include <span>
#include <Cala/util.h>
#include <Cala/Model.h>

namespace cala {

    namespace ui {
        class AssetManagerWindow;
    }

    class Engine;
    class Material;

    class AssetManager {
    public:

        AssetManager(Engine* engine);

        void setAssetPath(const std::filesystem::path& path);

        const std::filesystem::path& getAssetPath() const { return _rootAssetPath; }

        void addSearchPath(const std::filesystem::path& path);

        std::span<const std::filesystem::path> getSearchPaths() const { return _searchPaths; }


        i32 getAssetIndex(u32 hash);

        i32 registerShaderModule(const std::string& name, const std::filesystem::path& path, u32 hash);

        i32 registerImage(const std::string& name, const std::filesystem::path& path, u32 hash);

        i32 registerModel(const std::string& name, const std::filesystem::path& path, u32 hash);

        bool isRegistered(u32 hash);

        bool isRegistered(const std::filesystem::path& path);

        template <typename T>
        class Asset {
        public:

            Asset()
                : _manager(nullptr),
                _index(-1)
            {}

            Asset(AssetManager* manager, i32 index)
                : _manager(manager),
                _index(index)
            {}

            T& operator*() noexcept;

            T& operator*() const noexcept;

            explicit operator bool() const noexcept {
                if (_index < 0 || !_manager)
                    return false;
                auto& metadata = _manager->_metadata[_index];
                return metadata.loaded;
            }

        private:

            AssetManager* _manager;
            i32 _index;

        };

        vk::ShaderModuleHandle loadShaderModule(const std::string& name, const std::filesystem::path& path, vk::ShaderStage stage = vk::ShaderStage::NONE, const std::vector<util::Macro>& macros = {}, std::span<const std::string> includes = {});

        vk::ShaderModuleHandle reloadShaderModule(u32 hash);


        vk::ImageHandle loadImage(const std::string& name, const std::filesystem::path& path, vk::Format format = vk::Format::RGBA8_UNORM);

        vk::ImageHandle reloadImage(u32 hash);


        Asset<Model> loadModel(const std::string& name, const std::filesystem::path& path, Material* material);


        bool isLoaded(u32 hash);

        bool isLoaded(const std::filesystem::path& path);

        void clear();


    private:

        friend ui::AssetManagerWindow;

        Engine* _engine;

        std::filesystem::path _rootAssetPath;
        std::vector<std::filesystem::path> _searchPaths;

        struct AssetMetadata {
            std::string name;
            std::filesystem::path path;
            bool loaded;
            i32 index;
        };

        tsl::robin_map<u32, i32> _assetMap;
        std::vector<AssetMetadata> _metadata;

        struct ShaderModuleMetadata {
            u32 hash;
            std::string name;
            std::filesystem::path path;
            vk::ShaderStage stage;
            std::vector<util::Macro> macros;
            std::vector<std::string> includes;
            vk::ShaderModuleHandle moduleHandle;
        };
        std::vector<ShaderModuleMetadata> _shaderModules;

        struct ImageMetadata {
            u32 hash;
            std::string name;
            std::filesystem::path path;
            vk::Format format;
            vk::ImageHandle imageHandle;
        };
        std::vector<ImageMetadata> _images;

        struct ModelMetadata {
            u32 hash;
            std::string name;
            std::filesystem::path path;
            Model model;
        };
        std::vector<ModelMetadata> _models;

    };

}

#endif //CALA_ASSETMANAGER_H
