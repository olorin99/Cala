#ifndef CALA_UTIL_H
#define CALA_UTIL_H

#include <Ende/platform.h>
#include <vector>
#include <Cala/vulkan/Device.h>

namespace cala::util {

    struct Macro {
        std::string name;
        std::string value;
    };

    std::expected<std::vector<u32>, std::string> compileGLSLToSpirv(std::string_view name, std::string_view glsl, vk::ShaderStage stage, const std::vector<Macro>& macros = {}, std::span<const std::filesystem::path> searchPaths = {});

}

#endif //CALA_UTIL_H
