#ifndef CALA_PROBE_H
#define CALA_PROBE_H

#include <Cala/Engine.h>
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
            u32 mipLevels = 1;
        };

        Probe(Engine* engine, ProbeInfo info);

        ~Probe();

        Probe(Probe&& rhs) noexcept;

        Probe(const Probe&) = delete;

        Probe& operator=(const Probe&) = delete;

        void draw(backend::vulkan::CommandBuffer& cmd, std::function<void(backend::vulkan::CommandBuffer& cmd, u32 face)> perFace);

        backend::vulkan::ImageHandle map() const { return _cubeMap; }

        backend::vulkan::Image::View& view() { return _cubeView; }

    private:

        backend::vulkan::Framebuffer* _drawBuffer;
        backend::vulkan::ImageHandle _renderTarget;
        backend::vulkan::Image::View _view;
        backend::vulkan::ImageHandle _cubeMap;
        backend::vulkan::Image::View _cubeView;


    };

}

#endif //CALA_PROBE_H
