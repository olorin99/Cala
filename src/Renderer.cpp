#include "Cala/Renderer.h"
#include <Cala/Scene.h>
#include <Cala/backend/vulkan/Driver.h>

cala::Renderer::Renderer(backend::vulkan::Driver &driver)
    : _driver(driver),
      _cameraBuffer(driver, sizeof(Camera::Data), backend::BufferUsage::UNIFORM, backend::MemoryProperties::HOST_VISIBLE | backend::MemoryProperties::HOST_COHERENT)
{}

void cala::Renderer::render(cala::Scene &scene, cala::Camera &camera) {
    auto cameraData = camera.data();
    _cameraBuffer.data({ &cameraData, sizeof(cameraData) });

    auto frame = _driver.swapchain().nextImage();
//    backend::vulkan::CommandBuffer* cmd = _driver.beginFrame();
//
//    cmd->begin(frame.framebuffer);
//
//    cmd->bindBuffer(0, 0, _cameraBuffer);
//
//    scene.render(*cmd);
//
//    cmd->end(frame.framebuffer);
//
//    cmd->submit({ &frame.imageAquired, 1 }, frame.fence);
    _driver.endFrame();

}