#ifndef CALA_PRIMITIVES_H
#define CALA_PRIMITIVES_H

#include <Ende/platform.h>
#include <type_traits>

namespace cala::backend {

    constexpr u32 MAX_SET_COUNT = 4;

    constexpr u32 MAX_BINDING_PER_SET = 8;

    constexpr u32 MAX_VERTEX_INPUT_BINDINGS = 10;

    constexpr u32 MAX_VERTEX_INPUT_ATTRIBUTES = 10;

    enum class PhysicalDeviceType {
        OTHER = 0,
        INTEGRATED_GPU = 1,
        DISCRETE_GPU = 2,
        VIRTUAL_GPU = 3,
        CPU = 4
    };

    enum class QueueType {
        GRAPHICS = 0x00000001,
        COMPUTE = 0x00000002,
        TRANSFER = 0x00000004,
        SPARSE_BINDING = 0x00000008,
        PRESENT = 0x20
    };

    enum class Format {
        UNDEFINED = 0,
        //TODO: fill in extras


        RGB8_UNORM = 23,
        RGB8_SNORM = 24,

        RGB8_UINT = 27,
        RGB8_SINT = 28,
        RGB8_SRGB = 29,


        RGBA8_UNORM = 37,
        RGBA8_SNORM = 38,

        RGBA8_UINT = 41,
        RGBA8_SINT = 42,
        RGBA8_SRGB = 43,

        RG16_SFLOAT = 83,

        R32_SFLOAT = 100,
        RG32_SFLOAT = 103,
        RGB32_SFLOAT = 106,
        RGBA32_UINT = 107,
        RGBA32_SINT = 108,
        RGBA32_SFLOAT = 109,

        D16_UNORM = 124,

        D32_SFLOAT = 126,
        D24_UNORM_S8_UINT = 129
    };

    enum class MemoryProperties {
        DEVICE_LOCAL = 0x00000001,
        HOST_VISIBLE = 0x00000002,
        HOST_COHERENT = 0x00000004,
        HOST_CACHED = 0x00000008,
        LAZILY_ALLOCATED = 0x00000010
    };

    enum class ImageLayout {
        UNDEFINED = 0,
        GENERAL = 1,
        COLOUR_ATTACHMENT = 2,
        DEPTH_STENCIL_ATTACHMENT = 3,
        DEPTH_STENCIL_READ_ONLY = 4,
        SHADER_READ_ONLY = 5,
        TRANSFER_SRC = 6,
        TRANSFER_DST = 7,
        PREINITIALIZED = 8
    };

    enum class BufferUsage {
        TRANSFER_SRC = 0x00000001,
        TRANSFER_DST = 0x00000002,
        UNIFORM_TEXEL = 0x00000004,
        STORAGE_TEXEL = 0x00000008,
        UNIFORM = 0x00000010,
        STORAGE = 0x00000020,
        INDEX = 0x00000040,
        VERTEX = 0x00000080,
        INDIRECT = 0x00000100
    };

    enum class ImageUsage {
        TRANSFER_SRC = 0x00000001,
        TRANSFER_DST = 0x00000002,
        SAMPLED = 0x00000004,
        STORAGE = 0x00000008,
        COLOUR_ATTACHMENT = 0x00000010,
        DEPTH_STENCIL_ATTACHMENT = 0x00000020,
        TRANSIENT_ATTACHMENT = 0x00000040,
        INPUT_ATTACHMENT = 0x00000080
    };

    enum class ImageType {
        AUTO = 4,
        IMAGE1D = 0,
        IMAGE2D = 1,
        IMAGE3D = 2
    };

    enum class PolygonMode {
        FILL = 0,
        LINE = 1,
        POINT = 2
    };

    enum class CullMode {
        NONE = 0,
        FRONT = 0x00000001,
        BACK = 0x00000002,
        FRONT_BACK = 0x00000003
    };

    enum class FrontFace {
        CCW = 0,
        CW = 1
    };

    enum class CompareOp {
        NEVER = 0,
        LESS = 1,
        EQUAL = 2,
        LESS_EQUAL = 3,
        GREATER = 4,
        NOT_EQUAL = 5,
        GREATER_EQUAL = 6,
        ALWAYS = 7
    };

    enum class BlendFactor {
        ZERO = 0,
        ONE = 1,
        SRC_COLOUR = 2,
        ONE_MINUS_SRC_COLOUR = 3,
        DST_COLOUR = 4,
        ONE_MINUS_DST_COLOUR = 5,
        SRC_ALPHA = 6,
        ONE_MINUS_SRC_ALPHA = 7,
        DST_ALPHA = 8,
        ONE_MINUS_DST_ALPHA = 9,
        CONSTANT_COLOUR = 10,
        ONE_MINUS_CONSTANT_COLOUR = 11,
        CONSTANT_ALPHA = 12,
        ONE_MINUS_CONSTANT_ALPHA = 13,
        SRC_ALPHA_SATURATE = 14,
        SRC1_COLOUR = 15,
        ONE_MINUS_SRC1_COLOUR = 16,
        SRC1_ALPHA = 17,
        ONE_MINUS_SRC1_ALPHA = 18
    };

    enum class ShaderStage {
        NONE = 0,
        VERTEX = 0x00000001,
        TESS_CONTROL = 0x00000002,
        TESS_EVAL = 0x00000004,
        GEOMETRY = 0x00000008,
        FRAGMENT = 0x00000010,
        COMPUTE = 0x00000020,
        ALL_GRAPHICS = 0x0000001F,
        ALL = 0x7FFFFFFF
    };

    enum class Access {
        NONE = 0,
        INDIRECT_READ = 0x00000001,
        INDEX_READ = 0x00000002,
        VERTEX_READ = 0x00000004,
        UNIFORM_READ = 0x00000008,
        INPUT_READ = 0x00000010,
        SHADER_READ = 0x00000020,
        SHADER_WRITE = 0x00000040,
        COLOUR_ATTACHMENT_READ = 0x00000080,
        COLOUR_ATTACHMENT_WRITE = 0x00000100,
        DEPTH_STENCIL_READ = 0x00000200,
        DEPTH_STENCIL_WRITE = 0x00000400,
        TRANSFER_READ = 0x00000800,
        TRANSFER_WRITE = 0x00001000,
        HOST_READ = 0x00002000,
        HOST_WRITE = 0x00004000,
        MEMORY_READ = 0x00008000,
        MEMORY_WRITE = 0x00010000
    };

    enum class PipelineStage {
        TOP = 0x00000001,
        DRAW_INDIRECT = 0x00000002,
        VERTEX_INPUT = 0x00000004,
        VERTEX_SHADER = 0x00000008,
        TESS_CONTROL = 0x00000010,
        TESS_EVAL = 0x00000020,
        GEOMETRY_SHADER = 0x00000040,
        FRAGMENT_SHADER = 0x00000080,
        EARLY_FRAGMENT = 0x00000100,
        LATE_FRAGMENT = 0x00000200,
        COLOUR_ATTACHMENT_OUTPUT = 0x00000400,
        COMPUTE_SHADER = 0x00000800,
        TRANSFER = 0x00001000,
        BOTTOM = 0x00002000,
        HOST = 0x00004000,
        ALL_GRAPHICS = 0x00008000,
        ALL_COMMANDS = 0x00010000
    };

    enum class AttribType {
        Vec2f = 2,
        Vec3f = 3,
        Vec4f = 4,
        Mat4f = 16
    };




    struct Attribute {
        u32 location = 0;
        u32 binding = 0;
        AttribType type = AttribType::Vec3f;
    };

    struct ViewPort {
        f32 x = 0;
        f32 y = 0;
        f32 width = 0;
        f32 height = 0;
        f32 minDepth = 0.f;
        f32 maxDepth = 1.f;
    };

}

constexpr cala::backend::MemoryProperties operator|(cala::backend::MemoryProperties lhs, cala::backend::MemoryProperties rhs) {
    return static_cast<cala::backend::MemoryProperties>(static_cast<std::underlying_type<cala::backend::MemoryProperties>::type>(lhs) | static_cast<std::underlying_type<cala::backend::MemoryProperties>::type>(rhs));
}

constexpr cala::backend::MemoryProperties operator&(cala::backend::MemoryProperties lhs, cala::backend::MemoryProperties rhs) {
    return static_cast<cala::backend::MemoryProperties>(static_cast<std::underlying_type<cala::backend::MemoryProperties>::type>(lhs) & static_cast<std::underlying_type<cala::backend::MemoryProperties>::type>(rhs));
}

constexpr cala::backend::ImageUsage operator|(cala::backend::ImageUsage lhs, cala::backend::ImageUsage rhs) {
    return static_cast<cala::backend::ImageUsage>(static_cast<std::underlying_type<cala::backend::ImageUsage>::type>(lhs) | static_cast<std::underlying_type<cala::backend::ImageUsage>::type>(rhs));
}

constexpr cala::backend::ImageUsage operator&(cala::backend::ImageUsage lhs, cala::backend::ImageUsage rhs) {
    return static_cast<cala::backend::ImageUsage>(static_cast<std::underlying_type<cala::backend::ImageUsage>::type>(lhs) & static_cast<std::underlying_type<cala::backend::ImageUsage>::type>(rhs));
}

constexpr cala::backend::BufferUsage operator|(cala::backend::BufferUsage lhs, cala::backend::BufferUsage rhs) {
    return static_cast<cala::backend::BufferUsage>(static_cast<std::underlying_type<cala::backend::BufferUsage>::type>(lhs) | static_cast<std::underlying_type<cala::backend::BufferUsage>::type>(rhs));
}

constexpr cala::backend::BufferUsage operator&(cala::backend::BufferUsage lhs, cala::backend::BufferUsage rhs) {
    return static_cast<cala::backend::BufferUsage>(static_cast<std::underlying_type<cala::backend::BufferUsage>::type>(lhs) & static_cast<std::underlying_type<cala::backend::BufferUsage>::type>(rhs));
}

constexpr cala::backend::ShaderStage operator|(cala::backend::ShaderStage lhs, cala::backend::ShaderStage rhs) {
    return static_cast<cala::backend::ShaderStage>(static_cast<std::underlying_type<cala::backend::ShaderStage>::type>(lhs) | static_cast<std::underlying_type<cala::backend::ShaderStage>::type>(rhs));
}

constexpr cala::backend::ShaderStage operator&(cala::backend::ShaderStage lhs, cala::backend::ShaderStage rhs) {
    return static_cast<cala::backend::ShaderStage>(static_cast<std::underlying_type<cala::backend::ShaderStage>::type>(lhs) & static_cast<std::underlying_type<cala::backend::ShaderStage>::type>(rhs));
}

constexpr cala::backend::ShaderStage& operator|=(cala::backend::ShaderStage& lhs, cala::backend::ShaderStage rhs) {
    lhs =  static_cast<cala::backend::ShaderStage>(static_cast<std::underlying_type<cala::backend::ShaderStage>::type>(lhs) | static_cast<std::underlying_type<cala::backend::ShaderStage>::type>(rhs));
    return lhs;
}

constexpr cala::backend::Access operator|(cala::backend::Access lhs, cala::backend::Access rhs) {
    return static_cast<cala::backend::Access>(static_cast<std::underlying_type<cala::backend::Access>::type>(lhs) | static_cast<std::underlying_type<cala::backend::Access>::type>(rhs));
}

constexpr cala::backend::Access operator&(cala::backend::Access lhs, cala::backend::Access rhs) {
    return static_cast<cala::backend::Access>(static_cast<std::underlying_type<cala::backend::Access>::type>(lhs) & static_cast<std::underlying_type<cala::backend::Access>::type>(rhs));
}

#endif //CALA_PRIMITIVES_H
