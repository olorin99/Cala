//
// Created by christian on 12/3/22.
//

#include "Cala/Probe.h"
#include <Cala/backend/vulkan/CommandBuffer.h>
#include <Cala/backend/vulkan/Driver.h>

cala::Probe::Probe(cala::backend::vulkan::Driver& driver, ProbeInfo info) {
    //TODO: ownership belongs to engine object

    _renderTarget = new backend::vulkan::Image(driver, {
        info.width,
        info.height,
        1,
        info.targetFormat,
        info.mipLevels,
        1,
        info.targetUsage | backend::ImageUsage::TRANSFER_SRC
    });

    _cubeMap = new backend::vulkan::Image(driver, {
        info.width,
        info.height,
        1,
        info.targetFormat,
        info.mipLevels,
        6,
        info.targetUsage | backend::ImageUsage::TRANSFER_DST
    });

    driver.immediate([&](backend::vulkan::CommandBuffer& cmd) {
        auto targetBarrier = _renderTarget->barrier(backend::Access::NONE, backend::Access::DEPTH_STENCIL_WRITE | backend::Access::DEPTH_STENCIL_READ, backend::ImageLayout::UNDEFINED, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
        auto cubeBarrier = _cubeMap->barrier(backend::Access::NONE, backend::Access::TRANSFER_WRITE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::SHADER_READ_ONLY);

        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::EARLY_FRAGMENT, 0, nullptr, { &targetBarrier, 1 });
        cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TRANSFER, 0, nullptr, { &cubeBarrier, 1 });
    });

    _view = _renderTarget->getView();
    auto imageView = _view.view;
    _drawBuffer = new backend::vulkan::Framebuffer(driver.context().device(), *info.renderPass, { &imageView, 1 }, info.width, info.height);
}

cala::Probe::~Probe() {
    delete _drawBuffer;
    delete _cubeMap;
    delete _renderTarget;
}

void cala::Probe::draw(backend::vulkan::CommandBuffer &cmd, std::function<void(backend::vulkan::CommandBuffer&, u32)> perFace) {

    for (u32 face = 0; face < 6; face++) {
        cmd.begin(*_drawBuffer);

        // draw objects
        perFace(cmd, face);

        cmd.end(*_drawBuffer);

        auto srcBarrier = _renderTarget->barrier(backend::Access::DEPTH_STENCIL_WRITE, backend::Access::TRANSFER_READ, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT, backend::ImageLayout::TRANSFER_SRC);
        auto dstBarrier = _cubeMap->barrier(backend::Access::SHADER_READ, backend::Access::TRANSFER_WRITE, backend::ImageLayout::SHADER_READ_ONLY, backend::ImageLayout::TRANSFER_DST, face);

        cmd.pipelineBarrier(backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::TRANSFER, 0, nullptr, { &srcBarrier, 1 });
        cmd.pipelineBarrier(backend::PipelineStage::FRAGMENT_SHADER, backend::PipelineStage::TRANSFER, 0, nullptr, { &dstBarrier, 1 });

        _renderTarget->copy(cmd, *_cubeMap, 0, face);

        srcBarrier = _renderTarget->barrier(backend::Access::TRANSFER_READ, backend::Access::DEPTH_STENCIL_WRITE, backend::ImageLayout::TRANSFER_SRC, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
        dstBarrier = _cubeMap->barrier(backend::Access::TRANSFER_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::TRANSFER_DST, backend::ImageLayout::SHADER_READ_ONLY, face);

        cmd.pipelineBarrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::EARLY_FRAGMENT, 0, nullptr, { &srcBarrier, 1 });
        cmd.pipelineBarrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::FRAGMENT_SHADER, 0, nullptr, { &dstBarrier, 1 });

    }

}