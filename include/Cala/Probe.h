//
// Created by christian on 12/3/22.
//

#ifndef CALA_PROBE_H
#define CALA_PROBE_H

#include <Cala/backend/vulkan/Framebuffer.h>
#include <Cala/backend/vulkan/Image.h>

#include <functional>

namespace cala {

    class Probe {
    public:

        struct ProbeInfo {
            u32 width;
            u32 height;
            backend::Format targetFormat;
            backend::ImageUsage targetUsage;
            backend::vulkan::RenderPass* renderPass;
        };

        Probe(backend::vulkan::Driver& driver, ProbeInfo info);

        ~Probe();

        void draw(backend::vulkan::CommandBuffer& cmd, std::function<void(backend::vulkan::CommandBuffer& cmd, u32 face)> perFace);

        backend::vulkan::Image& map() const { return *_cubeMap; }

    private:

        backend::vulkan::Framebuffer* _drawBuffer;
        backend::vulkan::Image* _renderTarget;
        backend::vulkan::Image::View _view;
        backend::vulkan::Image* _cubeMap;


    };

}

#endif //CALA_PROBE_H
