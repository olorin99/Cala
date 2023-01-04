#ifndef CALA_MATERIAL_H
#define CALA_MATERIAL_H

#include <Cala/backend/vulkan/Driver.h>
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

        enum class Variants {
            POINT,
            DIRECTIONAL
        };

        Material(Engine* engine);

        MaterialInstance instance();

        void setProgram(Variants variant, ProgramHandle program);

        ProgramHandle getProgram(Variants variant);
//    private:

        Engine* _engine;

        ende::Vector<ProgramHandle> _programs;
        backend::vulkan::CommandBuffer::RasterState _rasterState;
        backend::vulkan::CommandBuffer::DepthState _depthState;

        backend::vulkan::Buffer _uniformBuffer;
        ende::Vector<u8> _uniformData;
        u32 _setSize;

    };

}

#endif //CALA_MATERIAL_H
