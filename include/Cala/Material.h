#ifndef CALA_MATERIAL_H
#define CALA_MATERIAL_H

#include <Cala/backend/vulkan/Device.h>
#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Ende/Vector.h>
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

        MaterialInstance instance();

        //backend::vulkan::ProgramHandle getProgram() const { return _program; };

        void setRasterState(backend::vulkan::CommandBuffer::RasterState state) { _rasterState = state; }

        backend::vulkan::CommandBuffer::RasterState getRasterState() const { return _rasterState; }

        void setDepthState(backend::vulkan::CommandBuffer::DepthState state) { _depthState = state; }

        backend::vulkan::CommandBuffer::DepthState getDepthState() const { return _depthState; }

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
            MAX
        };

        void setVariant(Variant variant, backend::vulkan::ProgramHandle program);

        backend::vulkan::ProgramHandle getVariant(Variant variant);

    private:
        friend MaterialInstance;

        Engine* _engine;

        std::array<backend::vulkan::ProgramHandle, static_cast<u8>(Variant::MAX)> _programs;

        backend::vulkan::CommandBuffer::RasterState _rasterState;
        backend::vulkan::CommandBuffer::DepthState _depthState;

        bool _dirty;
        ende::Vector<u8> _data;
        backend::vulkan::BufferHandle _materialBuffer;

        u32 _setSize;

        u32 _id;

    };

}

#endif //CALA_MATERIAL_H
