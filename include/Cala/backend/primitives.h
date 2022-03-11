#ifndef CALA_PRIMITIVES_H
#define CALA_PRIMITIVES_H

#include <Ende/platform.h>

namespace cala::backend {

    constexpr u32 MAX_SET_COUNT = 4;

    constexpr u32 MAX_BINDING_PER_SET = 8;

    constexpr u32 MAX_VERTEX_INPUT_BINDINGS = 10;

    constexpr u32 MAX_VERTEX_INPUT_ATTRIBUTES = 10;


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
        RGBA8_SRGB = 43
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


}

constexpr cala::backend::ImageUsage operator|(cala::backend::ImageUsage lhs, cala::backend::ImageUsage rhs) {
    return static_cast<cala::backend::ImageUsage>(static_cast<std::underlying_type<cala::backend::ImageUsage>::type>(lhs) | static_cast<std::underlying_type<cala::backend::ImageUsage>::type>(rhs));
}

constexpr cala::backend::ImageUsage operator&(cala::backend::ImageUsage lhs, cala::backend::ImageUsage rhs) {
    return static_cast<cala::backend::ImageUsage>(static_cast<std::underlying_type<cala::backend::ImageUsage>::type>(lhs) & static_cast<std::underlying_type<cala::backend::ImageUsage>::type>(rhs));
}

#endif //CALA_PRIMITIVES_H
