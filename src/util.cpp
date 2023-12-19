#include "Cala/util.h"
#include <shaderc/shaderc.hpp>
#include <Ende/filesystem/File.h>
#include <unordered_set>

class FileFinder {
public:

    void addSearchPath(const std::string& path) {
        search_path.push_back(path);
    }

    std::string FindReadableFilepath(const std::string& filename) const {
        assert(!filename.empty());
        ende::fs::File file;

        for (const auto& prefix : search_path) {
            std::string prefixed_filename = prefix + ((prefix.empty() || prefix.back() == '/') ? "" : "/") + filename;
            std::string f = prefixed_filename;
            if (file.open(std::move(prefixed_filename), ende::fs::in))
                return f;
        }
        return "";
    }

    std::string FindRelativeReadableFilepath(const std::string& requesting_file, const std::string& filename) const {
        assert(!filename.empty());
        size_t last_slash = requesting_file.find_last_of("/\\");
        std::string dir_name = requesting_file;
        if (last_slash != std::string::npos) {
            dir_name = requesting_file.substr(0, last_slash);
        }
        if (dir_name.size() == requesting_file.size()) {
            dir_name = {};
        }

        ende::fs::File file;
        std::string relative_filename = dir_name + ((dir_name.empty() || dir_name.back() == '/') ? "" : "/") + filename;
        std::string f = relative_filename;
        if (file.open(std::move(relative_filename), ende::fs::in))
            return f;
        return FindReadableFilepath(filename);
    }

private:
    std::vector<std::string> search_path;

};

struct FileInfo {
    std::string path;
    std::string source;
};

class FileIncluder : public shaderc::CompileOptions::IncluderInterface {
public:

    explicit FileIncluder(const FileFinder* file_finder) : file_finder(*file_finder) {}

    ~FileIncluder() override = default;

    shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) override {
        std::string path = (type == shaderc_include_type_relative) ? file_finder.FindRelativeReadableFilepath(requesting_source, requested_source)
                                                                   : file_finder.FindReadableFilepath(requested_source);

        if (path.empty())
            return new shaderc_include_result{"", 0, "unable to open file", 15};

        ende::fs::File file;
        std::string f = path;
        if (!file.open(std::move(path), ende::fs::in))
            return new shaderc_include_result{"", 0, "unable to open file", 15};

        std::string source = file.read();
        if (source.empty())
            return new shaderc_include_result{"", 0, "unable to read file", 15};

        included_files.insert(f);
        FileInfo* info = new FileInfo{ f, source };

        return new shaderc_include_result{info->path.data(), info->path.size(),
                                          info->source.data(), info->source.size(),
                                          info};
    }

    void ReleaseInclude(shaderc_include_result* include_result) override {
        delete static_cast<FileInfo*>(include_result->user_data);
        delete include_result;
    }

    const std::unordered_set<std::string>& file_path_trace() const {
        return included_files;
    }

private:

    const FileFinder& file_finder;
    std::unordered_set<std::string> included_files;

};

std::string macroize(std::string_view str) {
    std::string result = str.data();
    size_t index = 0;
    size_t last = 0;

    while ((index = result.find('\n', index)) != std::string::npos) {
        result.insert(index++, "\\");
        last = ++index;
    }
    return result;
}

std::expected<std::vector<u32>, std::string> cala::util::compileGLSLToSpirv(std::string_view name, std::string_view glsl, vk::ShaderStage stage, const std::vector<Macro>& macros, std::span<const std::filesystem::path> searchPaths) {

    shaderc_shader_kind kind{};
    switch (stage) {
        case vk::ShaderStage::VERTEX:
            kind = shaderc_shader_kind::shaderc_vertex_shader;
            break;
        case vk::ShaderStage::TESS_CONTROL:
            kind = shaderc_shader_kind::shaderc_tess_control_shader;
            break;
        case vk::ShaderStage::TESS_EVAL:
            kind = shaderc_shader_kind::shaderc_tess_evaluation_shader;
            break;
        case vk::ShaderStage::GEOMETRY:
            kind = shaderc_shader_kind::shaderc_geometry_shader;
            break;
        case vk::ShaderStage::FRAGMENT:
            kind = shaderc_shader_kind::shaderc_fragment_shader;
            break;
        case vk::ShaderStage::COMPUTE:
            kind = shaderc_shader_kind::shaderc_compute_shader;
            break;
        case vk::ShaderStage::TASK:
            kind = shaderc_shader_kind::shaderc_task_shader;
            break;
        case vk::ShaderStage::MESH:
            kind = shaderc_shader_kind::shaderc_mesh_shader;
            break;
        default:
            return std::unexpected("invalid shader stage used for compilation");
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    FileFinder finder;
    for (auto& path : searchPaths)
        finder.addSearchPath(path);
    options.SetIncluder(std::make_unique<FileIncluder>(&finder));

    for (auto& macro : macros)
        options.AddMacroDefinition(macro.name, macroize(macro.value));

    shaderc::PreprocessedSourceCompilationResult  preprocessedResult = compiler.PreprocessGlsl(glsl.data(), kind, name.data(), options);
    if (preprocessedResult.GetCompilationStatus() != shaderc_compilation_status_success) {
        return std::unexpected(std::format("Failed to preprocess shader {}:\n\tErrors: {}\n\tWarnings: {}\n\tMessage: {}\n{}\n\nShader path: {}", name, preprocessedResult.GetNumErrors(), preprocessedResult.GetNumErrors(), preprocessedResult.GetErrorMessage(), glsl, name));
    }

    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(glsl.data(), kind, name.data(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::string preprocessed = { preprocessedResult.begin(), preprocessedResult.end() };
        return std::unexpected(std::format("Failed to compile shader {}:\n\tErrors: {}\n\tWarnings: {}\n\tMessage: {}\n{}\n\nShader path: {}", name, result.GetNumErrors(), result.GetNumWarnings(), result.GetErrorMessage(), preprocessed, name));
    }

    return std::vector<u32>(result.cbegin(), result.cend());
}