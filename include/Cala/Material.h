#ifndef CALA_MATERIAL_H
#define CALA_MATERIAL_H

#include <Cala/backend/vulkan/Driver.h>
#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Ende/Vector.h>

#include <Cala/MaterialInstance.h>

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

        Material(backend::vulkan::Driver& driver, backend::vulkan::ShaderProgram&& program);

        MaterialInstance instance();
//    private:

        backend::vulkan::ShaderProgram _program;
        backend::vulkan::CommandBuffer::RasterState _rasterState;
        backend::vulkan::CommandBuffer::DepthState _depthState;

        backend::vulkan::Buffer _uniformBuffer;
        ende::Vector<u8> _uniformData;
        u32 _setSize;

    };

}

#endif //CALA_MATERIAL_H
