#ifndef CALA_ASSETMANAGER_H
#define CALA_ASSETMANAGER_H

#include <Ende/platform.h>
#include <filesystem>
#include "../third_party/tsl/robin_map.h"
#include <Cala/backend/vulkan/Handle.h>
#include <span>

namespace cala {

    class Engine;

    class AssetManager {
    public:

        AssetManager(Engine* engine);

        void setAssetPath(const std::filesystem::path& path);

        const std::filesystem::path& getAssetPath() const { return _rootAssetPath; }

        void addSearchPath(const std::filesystem::path& path);

        std::span<const std::filesystem::path> getSearchPaths() const { return _searchPaths; }


        i32 getAssetIndex(u32 hash);

        template<typename T>
        i32 registerAsset(const std::filesystem::path& path) {
            if constexpr (std::is_same<T, backend::vulkan::ShaderModuleHandle>::value) {
                return registerShaderModule(path);
            }
        }

        i32 registerShaderModule(const std::filesystem::path& path);

        bool isRegistered(u32 hash);

        bool isRegistered(const std::filesystem::path& path);

        template <typename T>
        class Asset {
        public:

            Asset(AssetManager* manager, i32 index)
                : _manager(manager),
                _index(index)
            {}

            T& operator*() noexcept;

            explicit operator bool() const noexcept {
                if (_index < 0)
                    return false;
                auto& metadata = _manager->_metadata[_index];
                return metadata.loaded;
            }

        private:

            AssetManager* _manager;
            i32 _index;

        };

        Asset<backend::vulkan::ShaderModuleHandle> loadShaderModule(const std::filesystem::path& path, std::span<std::pair<std::string_view, std::string_view>> macros = {});

        bool isLoaded(u32 hash);

        bool isLoaded(const std::filesystem::path& path);


    private:

        Engine* _engine;

        std::filesystem::path _rootAssetPath;
        std::vector<std::filesystem::path> _searchPaths;

        struct AssetMetadata {
            std::filesystem::path path;
            bool loaded;
            i32 index;
        };

        tsl::robin_map<u32, i32> _assetMap;
        std::vector<AssetMetadata> _metadata;

        struct ShaderModuleMetadata {
            std::vector<std::pair<std::string_view, std::string_view>> macros;
            std::vector<std::string_view> includes;
            backend::vulkan::ShaderModuleHandle moduleHandle;
        };
        std::vector<ShaderModuleMetadata> _shaderModules;

    };

}

#endif //CALA_ASSETMANAGER_H
