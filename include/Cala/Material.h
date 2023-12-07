#ifndef CALA_MATERIAL_H
#define CALA_MATERIAL_H

#include <Cala/vulkan/Device.h>
#include <Cala/vulkan/ShaderProgram.h>
#include <Cala/vulkan/CommandBuffer.h>
#include <tsl/robin_map.h>
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

        //vk::ProgramHandle getProgram() const { return _program; };

        void setRasterState(vk::CommandBuffer::RasterState state) { _rasterState = state; }

        vk::CommandBuffer::RasterState getRasterState() const { return _rasterState; }

        void setDepthState(vk::CommandBuffer::DepthState state) { _depthState = state; }

        vk::CommandBuffer::DepthState getDepthState() const { return _depthState; }

        void setBlendState(vk::CommandBuffer::BlendState state) { _blendState = state; }

        vk::CommandBuffer::BlendState getBlendState() const { return _blendState; }

        u32 size() const { return _setSize; }

        u32 id() const { return _id; }

        vk::BufferHandle buffer() const { return _materialBuffer; }

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

        void setVariant(Variant variant, vk::ShaderProgram program);

        const vk::ShaderProgram& getVariant(Variant variant);

        bool variantPresent(Variant variant);

        bool build();

    private:
        friend MaterialInstance;

        Engine* _engine;

        std::array<vk::ShaderProgram, static_cast<u8>(Variant::MAX)> _programs;

        vk::CommandBuffer::RasterState _rasterState;
        vk::CommandBuffer::DepthState _depthState;
        vk::CommandBuffer::BlendState _blendState;

        bool _dirty;
        std::vector<u8> _data;
        vk::BufferHandle _materialBuffer;

        u32 _setSize;

        u32 _id;

    };

}

#endif //CALA_MATERIAL_H
