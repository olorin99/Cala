#ifndef CALA_VK_PRIMITIVES_H
#define CALA_VK_PRIMITIVES_H

#include <volk/volk.h>

#include <Ende/platform.h>
#include <type_traits>

namespace cala::vk {

    constexpr u32 MAX_SET_COUNT = 5;

    constexpr u32 MAX_BINDING_PER_SET = 8;

    constexpr u32 MAX_VERTEX_INPUT_BINDINGS = 10;

    constexpr u32 MAX_VERTEX_INPUT_ATTRIBUTES = 10;

    enum class Error {
        HOST_MEMORY = -1,
        DEVICE_MEMORY = -2,
        DEVICE_LOST = -4,

        INVALID_GPU = -8,
        INVALID_COMMAND_BUFFER = -9,
        INVALID_SURFACE = -10,
        INVALID_PLATFORM = -11,
        INVALID_SWAPCHAIN = -12
    };

    enum class PhysicalDeviceType {
        OTHER = 0,
        INTEGRATED_GPU = 1,
        DISCRETE_GPU = 2,
        VIRTUAL_GPU = 3,
        CPU = 4
    };

    enum class QueueType {
        NONE = 0,
        GRAPHICS = 0x00000001,
        COMPUTE = 0x00000002,
        TRANSFER = 0x00000004,
        SPARSE_BINDING = 0x00000008,
        PRESENT = 0x20
    };

    enum class PresentMode {
        IMMEDIATE = 0,
        MAILBOX = 1,
        FIFO = 2,
        FIFO_RELAXED = 3,
        SHARED_DEMAND_REFRESH = 1000111000,
        SHARED_CONTINUOUS_REFRESH = 1000111001
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

        RGBA16_SFLOAT = 97,
        R32_UINT = 98,
        R32_SINT = 99,
        R32_SFLOAT = 100,
        RG32_UINT = 101,
        RG32_SINT = 102,
        RG32_SFLOAT = 103,
        RGB32_SFLOAT = 106,
        RGBA32_UINT = 107,
        RGBA32_SINT = 108,
        RGBA32_SFLOAT = 109,
        R64_UINT = 110,
        R64_SINT = 111,
        R64_SFLOAT = 112,

        D16_UNORM = 124,

        D32_SFLOAT = 126,
        D24_UNORM_S8_UINT = 129
    };

    constexpr inline bool isDepthFormat(Format format) {
        return format == Format::D16_UNORM || format == Format::D32_SFLOAT || format == Format::D24_UNORM_S8_UINT;
    }

    enum class MemoryProperties {
        STAGING = 1,
        DEVICE = 2,
        READBACK = 4
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
        PREINITIALIZED = 8,
        DEPTH_READ_ONLY_STENCIL_ATTACHMENT = 1000117000,
        DEPTH_ATTACHMENT_STENCIL_READ_ONLY = 1000117001,
        DEPTH_ATTACHMENT = 1000241000,
        DEPTH_READ_ONLY = 1000241001,
        STENCIL_ATTACHMENT = 1000241002,
        STENCIL_READ_ONLY = 1000241003,
        READ_ONLY = 1000314000,
        ATTACHMENT = 1000314001,
        PRESENT = 1000001002,
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
        INDIRECT = 0x00000100,
        DEVICE_ADDRESS = 0x00020000
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

    enum class ImageViewType {
        AUTO = 7,
        VIEW1D = 0,
        VIEW2D = 1,
        VIEW3D = 2,
        CUBE = 3,
        VIEW1D_ARRAY = 4,
        VIEW2D_ARRAY = 5,
        CUBE_ARRAY = 6,
    };

    enum class LoadOp {
        LOAD = 0,
        CLEAR = 1,
        DONT_CARE = 2,
        NONE = 1000400000
    };

    enum class StoreOp {
        STORE = 0,
        DONT_CARE = 1,
        NONE = 1000301000
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
        ALL = 0x7FFFFFFF,
        RAYGEN = 0x00000100,
        ANYHIT = 0x00000200,
        CLOSESTHIT = 0x00000400,
        MISS = 0x00000800,
        INTERSECTION = 0x00001000,
        CALLABLE = 0x00002000,
        TASK = 0x00000040,
        MESH = 0x00000080
    };

    enum class Access : u64 {
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
        MEMORY_WRITE = 0x00010000,
        SAMPLED_READ = 0x100000000,
        STORAGE_READ = 0x200000000,
        STORAGE_WRITE = 0x400000000,
    };

    constexpr bool isReadAccess(Access access) {
        switch (access) {
            case Access::INDIRECT_READ:
            case Access::INDEX_READ:
            case Access::VERTEX_READ:
            case Access::UNIFORM_READ:
            case Access::INPUT_READ:
            case Access::SHADER_READ:
            case Access::COLOUR_ATTACHMENT_READ:
            case Access::DEPTH_STENCIL_READ:
            case Access::TRANSFER_READ:
            case Access::HOST_READ:
            case Access::MEMORY_READ:
            case Access::SAMPLED_READ:
            case Access::STORAGE_READ:
                return true;
        }
        return false;
    }

    constexpr bool isWriteAccess(Access access) {
        switch (access) {
            case Access::SHADER_WRITE:
            case Access::COLOUR_ATTACHMENT_WRITE:
            case Access::DEPTH_STENCIL_WRITE:
            case Access::TRANSFER_WRITE:
            case Access::HOST_WRITE:
            case Access::MEMORY_WRITE:
            case Access::STORAGE_WRITE:
                return true;
        }
        return false;
    }

    enum class PipelineStage : u64 {
        NONE = 0,
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
        ALL_COMMANDS = 0x00010000,
        COPY = 0x100000000,
        RESOLVE = 0x200000000,
        BLIT = 0x400000000,
        CLEAR = 0x800000000,
        INDEX_INPUT = 0x1000000000,
        ACCELERATION_STRUCTURE_BUILD = 0x02000000,
        RAY_TRACING_SHADER = 0x00200000,
        TASK_SHADER = 0x00080000,
        MESH_SHADER = 0x00100000
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

#define _tostr(a) #a
#define tostr(a) _tostr(a)

    constexpr const char* formatToString(Format format) {
        switch (format) {
            case Format::UNDEFINED:
                return tostr(Format::UNDEFINED);
            case Format::RGB8_UNORM:
                return tostr(Format::RGB8_UNORM);
            case Format::RGB8_SNORM:
                return tostr(Format::RGB8_SNORM);
            case Format::RGB8_UINT:
                return tostr(Format::RGB8_UINT);
            case Format::RGB8_SINT:
                return tostr(Format::RGB8_SINT);
            case Format::RGB8_SRGB:
                return tostr(Format::RGB8_SRGB);
            case Format::RGBA8_UNORM:
                return tostr(Format::RGBA8_UNORM);
            case Format::RGBA8_SNORM:
                return tostr(Format::RGBA8_SNORM);
            case Format::RGBA8_UINT:
                return tostr(Format::RGBA8_UINT);
            case Format::RGBA8_SINT:
                return tostr(Format::RGBA8_SINT);
            case Format::RGBA8_SRGB:
                return tostr(Format::RGBA8_SRGB);
            case Format::RG16_SFLOAT:
                return tostr(Format::RG16_SFLOAT);
            case Format::RGBA16_SFLOAT:
                return tostr(Format::RGBA16_SFLOAT);
            case Format::R32_SFLOAT:
                return tostr(Format::R32_SFLOAT);
            case Format::RG32_SFLOAT:
                return tostr(Format::RG32_SFLOAT);
            case Format::RGB32_SFLOAT:
                return tostr(Format::RGB32_SFLOAT);
            case Format::RGBA32_UINT:
                return tostr(Format::RGBA32_UINT);
            case Format::RGBA32_SINT:
                return tostr(Format::RGBA32_SINT);
            case Format::RGBA32_SFLOAT:
                return tostr(Format::RGBA32_SFLOAT);
            case Format::D16_UNORM:
                return tostr(Format::D16_UNORM);
            case Format::D32_SFLOAT:
                return tostr(Format::D32_SFLOAT);
            case Format::D24_UNORM_S8_UINT:
                return tostr(Format::D24_UNORM_S8_UINT);
        }

        return nullptr;
    }

    constexpr u32 formatToSize(Format format) {
        switch (format) {
            case Format::UNDEFINED:
                return 0;
            case Format::RGB8_UNORM:
            case Format::RGB8_SNORM:
            case Format::RGB8_UINT:
            case Format::RGB8_SINT:
            case Format::RGB8_SRGB:
                return 1 * 3;
            case Format::RGBA8_UNORM:
            case Format::RGBA8_SNORM:
            case Format::RGBA8_UINT:
            case Format::RGBA8_SINT:
            case Format::RGBA8_SRGB:
                return 1 * 4;
            case Format::RG16_SFLOAT:
                return 2 * 2;
            case Format::RGBA16_SFLOAT:
                return 2 * 4;
            case Format::R32_SFLOAT:
                return 4 * 1;
            case Format::RG32_SFLOAT:
                return 4 * 2;
            case Format::RGB32_SFLOAT:
                return 4 * 3;
            case Format::RGBA32_UINT:
            case Format::RGBA32_SINT:
            case Format::RGBA32_SFLOAT:
                return 4 * 4;
            case Format::D16_UNORM:
                return 2;
            case Format::D32_SFLOAT:
            case Format::D24_UNORM_S8_UINT:
                return 4;
        }

        return 0;
    }

    constexpr const char* imageLayoutToString(ImageLayout layout) {
        switch (layout) {
            case ImageLayout::UNDEFINED:
                return tostr(ImageLayout::UNDEFINED);
            case ImageLayout::GENERAL:
                return tostr(ImageLayout::GENERAL);
            case ImageLayout::COLOUR_ATTACHMENT:
                return tostr(ImageLayout::COLOUR_ATTACHMENT);
            case ImageLayout::DEPTH_STENCIL_ATTACHMENT:
                return tostr(ImageLayout::DEPTH_STENCIL_ATTACHMENT);
            case ImageLayout::DEPTH_STENCIL_READ_ONLY:
                return tostr(ImageLayout::DEPTH_STENCIL_READ_ONLY);
            case ImageLayout::SHADER_READ_ONLY:
                return tostr(ImageLayout::SHADER_READ_ONLY);
            case ImageLayout::TRANSFER_SRC:
                return tostr(ImageLayout::TRANSFER_SRC);
            case ImageLayout::TRANSFER_DST:
                return tostr(ImageLayout::TRANSFER_DST);
            case ImageLayout::PREINITIALIZED:
                return tostr(ImageLayout::PREINITIALIZED);
            case ImageLayout::DEPTH_READ_ONLY_STENCIL_ATTACHMENT:
                return tostr(ImageLayout::DEPTH_READ_ONLY_STENCIL_ATTACHMENT);
            case ImageLayout::DEPTH_ATTACHMENT_STENCIL_READ_ONLY:
                return tostr(ImageLayout::DEPTH_ATTACHMENT_STENCIL_READ_ONLY);
            case ImageLayout::DEPTH_ATTACHMENT:
                return tostr(ImageLayout::DEPTH_ATTACHMENT);
            case ImageLayout::DEPTH_READ_ONLY:
                return tostr(ImageLayout::DEPTH_READ_ONLY);
            case ImageLayout::STENCIL_ATTACHMENT:
                return tostr(ImageLayout::STENCIL_ATTACHMENT);
            case ImageLayout::STENCIL_READ_ONLY:
                return tostr(ImageLayout::STENCIL_READ_ONLY);
            case ImageLayout::READ_ONLY:
                return tostr(ImageLayout::READ_ONLY);
            case ImageLayout::ATTACHMENT:
                return tostr(ImageLayout::ATTACHMENT);
            case ImageLayout::PRESENT:
                return tostr(ImageLayout::PRESENT);
        }
        return tostr(ImageLayout::UNDEFINED);
    }

    constexpr const char* pipelineStageToString(PipelineStage stage) {
        switch (stage) {
            case PipelineStage::TOP:
                return tostr(PipelineStage::TOP);
            case PipelineStage::DRAW_INDIRECT:
                return tostr(PipelineStage::DRAW_INDIRECT);
            case PipelineStage::VERTEX_INPUT:
                return tostr(PipelineStage::VERTEX_INPUT);
            case PipelineStage::VERTEX_SHADER:
                return tostr(PipelineStage::VERTEX_SHADER);
            case PipelineStage::TESS_CONTROL:
                return tostr(PipelineStage::TESS_CONTROL);
            case PipelineStage::TESS_EVAL:
                return tostr(PipelineStage::TESS_EVAL);
            case PipelineStage::GEOMETRY_SHADER:
                return tostr(PipelineStage::GEOMETRY_SHADER);
            case PipelineStage::FRAGMENT_SHADER:
                return tostr(PipelineStage::FRAGMENT_SHADER);
            case PipelineStage::EARLY_FRAGMENT:
                return tostr(PipelineStage::EARLY_FRAGMENT);
            case PipelineStage::LATE_FRAGMENT:
                return tostr(PipelineStage::LATE_FRAGMENT);
            case PipelineStage::COLOUR_ATTACHMENT_OUTPUT:
                return tostr(PipelineStage::COLOUR_ATTACHMENT_OUTPUT);
            case PipelineStage::COMPUTE_SHADER:
                return tostr(PipelineStage::COMPUTE_SHADER);
            case PipelineStage::TRANSFER:
                return tostr(PipelineStage::TRANSFER);
            case PipelineStage::BOTTOM:
                return tostr(PipelineStage::BOTTOM);
            case PipelineStage::HOST:
                return tostr(PipelineStage::HOST);
            case PipelineStage::ALL_GRAPHICS:
                return tostr(PipelineStage::ALL_GRAPHICS);
            case PipelineStage::ALL_COMMANDS:
                return tostr(PipelineStage::ALL_COMMANDS);
            case PipelineStage::COPY:
                return tostr(PipelineStage::COPY);
            case PipelineStage::RESOLVE:
                return tostr(PipelineStage::RESOLVE);
            case PipelineStage::BLIT:
                return tostr(PipelineStage::BLIT);
            case PipelineStage::CLEAR:
                return tostr(PipelineStage::CLEAR);
            case PipelineStage::INDEX_INPUT:
                return tostr(PipelineStage::INDEX_INPUT);
        }
        return tostr(PipelineStage::TOP);
    }

    constexpr const char* accessToString(Access access) {
        switch (access) {
            case Access::NONE:
                return tostr(Access::NONE);
            case Access::INDIRECT_READ:
                return tostr(Access::INDIRECT_READ);
            case Access::INDEX_READ:
                return tostr(Access::INDEX_READ);
            case Access::VERTEX_READ:
                return tostr(Access::VERTEX_READ);
            case Access::UNIFORM_READ:
                return tostr(Access::UNIFORM_READ);
            case Access::INPUT_READ:
                return tostr(Access::INPUT_READ);
            case Access::SHADER_READ:
                return tostr(Access::SHADER_READ);
            case Access::SHADER_WRITE:
                return tostr(Access::SHADER_WRITE);
            case Access::COLOUR_ATTACHMENT_READ:
                return tostr(Access::COLOUR_ATTACHMENT_READ);
            case Access::COLOUR_ATTACHMENT_WRITE:
                return tostr(Access::COLOUR_ATTACHMENT_WRITE);
            case Access::DEPTH_STENCIL_READ:
                return tostr(Access::DEPTH_STENCIL_READ);
            case Access::DEPTH_STENCIL_WRITE:
                return tostr(Access::DEPTH_STENCIL_WRITE);
            case Access::TRANSFER_READ:
                return tostr(Access::TRANSFER_READ);
            case Access::TRANSFER_WRITE:
                return tostr(Access::TRANSFER_WRITE);
            case Access::HOST_READ:
                return tostr(Access::HOST_READ);
            case Access::HOST_WRITE:
                return tostr(Access::HOST_WRITE);
            case Access::MEMORY_READ:
                return tostr(Access::MEMORY_READ);
            case Access::MEMORY_WRITE:
                return tostr(Access::MEMORY_WRITE);
            case Access::SAMPLED_READ:
                return tostr(Access::SAMPLED_READ);
            case Access::STORAGE_READ:
                return tostr(Access::STORAGE_READ);
            case Access::STORAGE_WRITE:
                return tostr(Access::STORAGE_WRITE);
        }
        return tostr(Access::NONE);
    }

    constexpr const char* loadOpToString(LoadOp op) {
        switch (op) {
            case LoadOp::LOAD:
                return tostr(LoadOp::LOAD);
            case LoadOp::CLEAR:
                return tostr(LoadOp::CLEAR);
            case LoadOp::DONT_CARE:
                return tostr(LoadOp::DONT_CARE);
            case LoadOp::NONE:
                return tostr(LoadOp::NONE);
        }
        return tostr(LoadOp::NONE);
    }

    constexpr const char* storeOpToString(StoreOp op) {
        switch (op) {
            case StoreOp::STORE:
                return tostr(StoreOp::STORE);
            case StoreOp::DONT_CARE:
                return tostr(StoreOp::DONT_CARE);
            case StoreOp::NONE:
                return tostr(StoreOp::NONE);
        }
        return tostr(StoreOp::NONE);
    }

}

constexpr cala::vk::QueueType operator|(cala::vk::QueueType lhs, cala::vk::QueueType rhs) {
    return static_cast<cala::vk::QueueType>(static_cast<std::underlying_type<cala::vk::QueueType>::type>(lhs) | static_cast<std::underlying_type<cala::vk::QueueType>::type>(rhs));
}

constexpr cala::vk::QueueType operator&(cala::vk::QueueType lhs, cala::vk::QueueType rhs) {
    return static_cast<cala::vk::QueueType>(static_cast<std::underlying_type<cala::vk::QueueType>::type>(lhs) & static_cast<std::underlying_type<cala::vk::QueueType>::type>(rhs));
}

constexpr cala::vk::MemoryProperties operator|(cala::vk::MemoryProperties lhs, cala::vk::MemoryProperties rhs) {
    return static_cast<cala::vk::MemoryProperties>(static_cast<std::underlying_type<cala::vk::MemoryProperties>::type>(lhs) | static_cast<std::underlying_type<cala::vk::MemoryProperties>::type>(rhs));
}

constexpr cala::vk::MemoryProperties operator&(cala::vk::MemoryProperties lhs, cala::vk::MemoryProperties rhs) {
    return static_cast<cala::vk::MemoryProperties>(static_cast<std::underlying_type<cala::vk::MemoryProperties>::type>(lhs) & static_cast<std::underlying_type<cala::vk::MemoryProperties>::type>(rhs));
}

constexpr cala::vk::ImageUsage operator|(cala::vk::ImageUsage lhs, cala::vk::ImageUsage rhs) {
    return static_cast<cala::vk::ImageUsage>(static_cast<std::underlying_type<cala::vk::ImageUsage>::type>(lhs) | static_cast<std::underlying_type<cala::vk::ImageUsage>::type>(rhs));
}

constexpr cala::vk::ImageUsage operator&(cala::vk::ImageUsage lhs, cala::vk::ImageUsage rhs) {
    return static_cast<cala::vk::ImageUsage>(static_cast<std::underlying_type<cala::vk::ImageUsage>::type>(lhs) & static_cast<std::underlying_type<cala::vk::ImageUsage>::type>(rhs));
}

constexpr cala::vk::BufferUsage operator|(cala::vk::BufferUsage lhs, cala::vk::BufferUsage rhs) {
    return static_cast<cala::vk::BufferUsage>(static_cast<std::underlying_type<cala::vk::BufferUsage>::type>(lhs) | static_cast<std::underlying_type<cala::vk::BufferUsage>::type>(rhs));
}

constexpr cala::vk::BufferUsage operator&(cala::vk::BufferUsage lhs, cala::vk::BufferUsage rhs) {
    return static_cast<cala::vk::BufferUsage>(static_cast<std::underlying_type<cala::vk::BufferUsage>::type>(lhs) & static_cast<std::underlying_type<cala::vk::BufferUsage>::type>(rhs));
}

constexpr cala::vk::ShaderStage operator|(cala::vk::ShaderStage lhs, cala::vk::ShaderStage rhs) {
    return static_cast<cala::vk::ShaderStage>(static_cast<std::underlying_type<cala::vk::ShaderStage>::type>(lhs) | static_cast<std::underlying_type<cala::vk::ShaderStage>::type>(rhs));
}

constexpr cala::vk::ShaderStage operator&(cala::vk::ShaderStage lhs, cala::vk::ShaderStage rhs) {
    return static_cast<cala::vk::ShaderStage>(static_cast<std::underlying_type<cala::vk::ShaderStage>::type>(lhs) & static_cast<std::underlying_type<cala::vk::ShaderStage>::type>(rhs));
}

constexpr cala::vk::ShaderStage& operator|=(cala::vk::ShaderStage& lhs, cala::vk::ShaderStage rhs) {
    lhs =  static_cast<cala::vk::ShaderStage>(static_cast<std::underlying_type<cala::vk::ShaderStage>::type>(lhs) | static_cast<std::underlying_type<cala::vk::ShaderStage>::type>(rhs));
    return lhs;
}

constexpr cala::vk::Access operator|(cala::vk::Access lhs, cala::vk::Access rhs) {
    return static_cast<cala::vk::Access>(static_cast<std::underlying_type<cala::vk::Access>::type>(lhs) | static_cast<std::underlying_type<cala::vk::Access>::type>(rhs));
}

constexpr cala::vk::Access operator&(cala::vk::Access lhs, cala::vk::Access rhs) {
    return static_cast<cala::vk::Access>(static_cast<std::underlying_type<cala::vk::Access>::type>(lhs) & static_cast<std::underlying_type<cala::vk::Access>::type>(rhs));
}

constexpr cala::vk::PipelineStage operator|(cala::vk::PipelineStage lhs, cala::vk::PipelineStage rhs) {
    return static_cast<cala::vk::PipelineStage>(static_cast<std::underlying_type<cala::vk::PipelineStage>::type>(lhs) | static_cast<std::underlying_type<cala::vk::PipelineStage>::type>(rhs));
}

namespace cala::vk {

    constexpr inline VkPresentModeKHR getPresentMode(PresentMode mode) {
        return static_cast<VkPresentModeKHR>(mode);
    }

    constexpr inline VkFormat getFormat(Format format) {
        return static_cast<VkFormat>(format);
    }

    constexpr inline VkImageLayout getImageLayout(ImageLayout layout) {
        return static_cast<VkImageLayout>(layout);
    }

    constexpr inline VkMemoryPropertyFlagBits getMemoryProperties(MemoryProperties flags) {
        return static_cast<VkMemoryPropertyFlagBits>(flags);
    }

    constexpr inline VkBufferUsageFlagBits getBufferUsage(BufferUsage usage) {
        return static_cast<VkBufferUsageFlagBits>(usage);
    }

    constexpr inline VkImageUsageFlagBits getImageUsage(ImageUsage usage) {
        return static_cast<VkImageUsageFlagBits>(usage);
    }

    constexpr inline VkImageType getImageType(ImageType type) {
        return static_cast<VkImageType>(type);
    }

    constexpr inline VkImageViewType getImageViewType(ImageViewType type) {
        return static_cast<VkImageViewType>(type);
    }

    constexpr inline VkAttachmentLoadOp getLoadOp(LoadOp op) {
        return static_cast<VkAttachmentLoadOp>(op);
    }

    constexpr inline VkAttachmentStoreOp getStoreOp(StoreOp op) {
        return static_cast<VkAttachmentStoreOp>(op);
    }

    constexpr inline VkPolygonMode getPolygonMode(PolygonMode mode) {
        return static_cast<VkPolygonMode>(mode);
    }

    constexpr inline VkCullModeFlags getCullMode(CullMode mode) {
        return static_cast<VkCullModeFlags>(mode);
    }

    constexpr inline VkFrontFace getFrontFace(FrontFace face) {
        return static_cast<VkFrontFace>(face);
    }

    constexpr inline VkCompareOp getCompareOp(CompareOp op) {
        return static_cast<VkCompareOp>(op);
    }

    constexpr inline VkBlendFactor getBlendFactor(BlendFactor factor) {
        return static_cast<VkBlendFactor>(factor);
    }

    constexpr inline VkShaderStageFlagBits getShaderStage(ShaderStage stage) {
        return static_cast<VkShaderStageFlagBits>(stage);
    }

    constexpr inline VkAccessFlagBits2 getAccessFlags(Access flags) {
        return static_cast<VkAccessFlagBits2>(flags);
    }

    constexpr inline VkPipelineStageFlagBits2 getPipelineStage(PipelineStage stage) {
        return static_cast<VkPipelineStageFlagBits2>(stage);
    }

}

#endif //CALA_VK_PRIMITIVES_H
