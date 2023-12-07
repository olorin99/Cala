#ifndef CALA_PROBE_H
#define CALA_PROBE_H

#include <Cala/Engine.h>
#include <Cala/vulkan/Framebuffer.h>
#include <Cala/vulkan/Image.h>
#include <functional>

namespace cala {

    class Probe {
    public:

        struct ProbeInfo {
            u32 width;
            u32 height;
            vk::Format targetFormat;
            vk::ImageUsage targetUsage;
            vk::RenderPass* renderPass;
            u32 mipLevels = 1;
        };

        Probe(Engine* engine, ProbeInfo info);

        ~Probe();

        Probe(Probe&& rhs) noexcept;

        Probe(const Probe&) = delete;

        Probe& operator=(const Probe&) = delete;

        void draw(vk::CommandHandle cmd, std::function<void(vk::CommandHandle cmd, u32 face)> perFace);

        vk::ImageHandle map() const { return _cubeMap; }

        vk::Image::View& view() { return _cubeView; }

    private:

        vk::Framebuffer* _drawBuffer;
        vk::ImageHandle _renderTarget;
        vk::Image::View _view;
        vk::ImageHandle _cubeMap;
        vk::Image::View _cubeView;


    };

}

#endif //CALA_PROBE_H
