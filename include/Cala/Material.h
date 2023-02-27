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

        Material(Engine* engine, backend::vulkan::ProgramHandle program, u32 size = 0);

        MaterialInstance instance();

        backend::vulkan::ProgramHandle getProgram() const { return _program; };

        u32 shaderDataSize() { return _program->interface().setSize(2); }

//    private:

        Engine* _engine;

        backend::vulkan::ProgramHandle _program;
        backend::vulkan::CommandBuffer::RasterState _rasterState;
        backend::vulkan::CommandBuffer::DepthState _depthState;

        u32 _setSize;

    };

}

#endif //CALA_MATERIAL_H
