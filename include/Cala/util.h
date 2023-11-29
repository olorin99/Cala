#ifndef CALA_UTIL_H
#define CALA_UTIL_H

#include <Ende/platform.h>
#include <vector>
#include <Cala/backend/vulkan/Device.h>

namespace cala::util {

    struct Macro {
        std::string name;
        std::string value;
    };

    std::expected<std::vector<u32>, u32> compileGLSLToSpirv(backend::vulkan::Device* device, std::string_view name, std::string_view glsl, backend::ShaderStage stage, const std::vector<Macro>& macros = {}, std::span<const std::filesystem::path> searchPaths = {});

}

#endif //CALA_UTIL_H
