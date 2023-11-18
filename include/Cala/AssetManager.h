#ifndef CALA_ASSETMANAGER_H
#define CALA_ASSETMANAGER_H

#include <Ende/platform.h>
#include <filesystem>
#include "../third_party/tsl/robin_map.h"
#include <Cala/backend/vulkan/Handle.h>
#include <Cala/backend/primitives.h>
#include <span>

namespace cala {

    namespace ui {
        class AssetManagerWindow;
    }

    class Engine;

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

        Asset<backend::vulkan::ShaderModuleHandle> loadShaderModule(const std::string& name, const std::filesystem::path& path, backend::ShaderStage stage = backend::ShaderStage::NONE, std::span<const std::pair<std::string_view, std::string_view>> macros = {}, std::span<const std::string> includes = {});

        Asset<backend::vulkan::ShaderModuleHandle> reloadShaderModule(u32 hash);


        Asset<backend::vulkan::ImageHandle> loadImage(const std::string& name, const std::filesystem::path& path, bool hdr = false);

        Asset<backend::vulkan::ImageHandle> reloadImage(u32 hash);


        bool isLoaded(u32 hash);

        bool isLoaded(const std::filesystem::path& path);


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
            backend::ShaderStage stage;
            std::vector<std::pair<std::string, std::string>> macros;
            std::vector<std::string> includes;
            backend::vulkan::ShaderModuleHandle moduleHandle;
        };
        std::vector<ShaderModuleMetadata> _shaderModules;

        struct ImageMetadata {
            u32 hash;
            std::string name;
            std::filesystem::path path;
            bool hdr;
            backend::vulkan::ImageHandle imageHandle;
        };
        std::vector<ImageMetadata> _images;

    };

}

#endif //CALA_ASSETMANAGER_H
