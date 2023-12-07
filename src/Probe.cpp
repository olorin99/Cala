#include <Cala/Probe.h>
#include <Cala/vulkan/CommandBuffer.h>
#include <Cala/vulkan/Device.h>

cala::Probe::Probe(cala::Engine* engine, ProbeInfo info) {
    //TODO: ownership belongs to engine object

//    _renderTarget = new backend::vulkan::Image(device, {
    _renderTarget = engine->device().createImage({
        info.width,
        info.height,
        1,
        info.targetFormat,
        info.mipLevels,
        1,
        info.targetUsage | vk::ImageUsage::TRANSFER_SRC
    });

    _cubeMap = engine->device().createImage({
        info.width,
        info.height,
        1,
        info.targetFormat,
        info.mipLevels,
        6,
        info.targetUsage | vk::ImageUsage::TRANSFER_DST
    });

    engine->device().immediate([&](vk::CommandHandle cmd) {
        auto targetBarrier = _renderTarget->barrier(vk::PipelineStage::TOP, vk::PipelineStage::EARLY_FRAGMENT, vk::Access::NONE, vk::Access::DEPTH_STENCIL_WRITE | vk::Access::DEPTH_STENCIL_READ, vk::ImageLayout::UNDEFINED, vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
        auto cubeBarrier = _cubeMap->barrier(vk::PipelineStage::TOP, vk::PipelineStage::TRANSFER, vk::Access::NONE, vk::Access::TRANSFER_WRITE, vk::ImageLayout::UNDEFINED, vk::ImageLayout::SHADER_READ_ONLY);

        cmd->pipelineBarrier({ &targetBarrier, 1 });
        cmd->pipelineBarrier({ &cubeBarrier, 1 });
    });

    _view = _renderTarget->newView();
    auto imageView = _view.view;
    _drawBuffer = new vk::Framebuffer(engine->device().context().device(), *info.renderPass, {&imageView, 1 }, info.width, info.height);
    _cubeView = _cubeMap->newView(0, info.mipLevels);
}

cala::Probe::~Probe() {
    delete _drawBuffer;
//    delete _cubeMap;
//    delete _renderTarget;
}

cala::Probe::Probe(Probe &&rhs) noexcept
    : _drawBuffer(nullptr)
{
    std::swap(_renderTarget, rhs._renderTarget);
    std::swap(_cubeMap, rhs._cubeMap);
    std::swap(_drawBuffer, rhs._drawBuffer);
    std::swap(_view, rhs._view);
}

void cala::Probe::draw(vk::CommandHandle cmd, std::function<void(vk::CommandHandle, u32)> perFace) {

    for (u32 face = 0; face < 6; face++) {
        cmd->begin(*_drawBuffer);

        // draw objects
        perFace(cmd, face);

        cmd->end(*_drawBuffer);

        auto srcBarrier = _renderTarget->barrier(vk::PipelineStage::LATE_FRAGMENT, vk::PipelineStage::TRANSFER, vk::Access::DEPTH_STENCIL_WRITE, vk::Access::TRANSFER_READ, vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT, vk::ImageLayout::TRANSFER_SRC);
        auto dstBarrier = _cubeMap->barrier(vk::PipelineStage::FRAGMENT_SHADER, vk::PipelineStage::TRANSFER, vk::Access::SHADER_READ, vk::Access::TRANSFER_WRITE, vk::ImageLayout::SHADER_READ_ONLY, vk::ImageLayout::TRANSFER_DST, face);

        cmd->pipelineBarrier({ &srcBarrier, 1 });
        cmd->pipelineBarrier({ &dstBarrier, 1 });

        _renderTarget->copy(cmd, *_cubeMap, 0, face);

        srcBarrier = _renderTarget->barrier(vk::PipelineStage::TRANSFER, vk::PipelineStage::EARLY_FRAGMENT, vk::Access::TRANSFER_READ, vk::Access::DEPTH_STENCIL_WRITE, vk::ImageLayout::TRANSFER_SRC, vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
        dstBarrier = _cubeMap->barrier(vk::PipelineStage::TRANSFER, vk::PipelineStage::FRAGMENT_SHADER, vk::Access::TRANSFER_WRITE, vk::Access::SHADER_READ, vk::ImageLayout::TRANSFER_DST, vk::ImageLayout::SHADER_READ_ONLY, face);

        cmd->pipelineBarrier({ &srcBarrier, 1 });
        cmd->pipelineBarrier({ &dstBarrier, 1 });

    }

}