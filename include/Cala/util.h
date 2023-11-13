#ifndef CALA_UTIL_H
#define CALA_UTIL_H

#include <Ende/platform.h>
#include <vector>
#include <Cala/backend/vulkan/Device.h>

namespace cala::util {

    std::expected<std::vector<u32>, u32> compileGLSLToSpirv(backend::vulkan::Device* device, std::string_view name, std::string_view glsl, backend::ShaderStage stage, std::span<const std::pair<std::string_view, std::string_view>> macros = {}, std::span<const std::filesystem::path> searchPaths = {});

}

#endif //CALA_UTIL_H
