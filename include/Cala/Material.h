#ifndef CALA_MATERIAL_H
#define CALA_MATERIAL_H

#include <Cala/backend/vulkan/Device.h>
#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include "../third_party/tsl/robin_map.h"
#include <Cala/MaterialInstance.h>
#include <Cala/Engine.h>

/*
 * Material
 *
 * holds program, and pipeline state
 * holds uniform buffer for material data
 * material instance holds offset for individual instances
 *
 */

namespace cala {

    class Material {
    public:

        Material(Engine* engine, u32 id, u32 size = 0);

        Material(Material&& rhs) noexcept;

        Material& operator=(Material&& rhs) noexcept;

        MaterialInstance instance();

        //backend::vulkan::ProgramHandle getProgram() const { return _program; };

        void setRasterState(backend::vulkan::CommandBuffer::RasterState state) { _rasterState = state; }

        backend::vulkan::CommandBuffer::RasterState getRasterState() const { return _rasterState; }

        void setDepthState(backend::vulkan::CommandBuffer::DepthState state) { _depthState = state; }

        backend::vulkan::CommandBuffer::DepthState getDepthState() const { return _depthState; }

        void setBlendState(backend::vulkan::CommandBuffer::BlendState state) { _blendState = state; }

        backend::vulkan::CommandBuffer::BlendState getBlendState() const { return _blendState; }

        u32 size() const { return _setSize; }

        u32 id() const { return _id; }

        backend::vulkan::BufferHandle buffer() const { return _materialBuffer; }

        void upload();

        enum class Variant {
            LIT,
            UNLIT,
            NORMAL,
            ROUGHNESS,
            METALLIC,
            VOXELGI,
            MAX
        };

        void setVariant(Variant variant, Program program);

        const Program& getVariant(Variant variant);

        bool variantPresent(Variant variant);

    private:
        friend MaterialInstance;

        Engine* _engine;

        std::array<Program, static_cast<u8>(Variant::MAX)> _programs;

        backend::vulkan::CommandBuffer::RasterState _rasterState;
        backend::vulkan::CommandBuffer::DepthState _depthState;
        backend::vulkan::CommandBuffer::BlendState _blendState;

        bool _dirty;
        std::vector<u8> _data;
        backend::vulkan::BufferHandle _materialBuffer;

        u32 _setSize;

        u32 _id;

    };

}

#endif //CALA_MATERIAL_H
