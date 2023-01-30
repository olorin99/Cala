#include "Cala/RenderGraph.h"
#include <Ende/log/log.h>

void cala::ImageResource::devirtualize(cala::Engine* engine) {
    auto extent = engine->driver().swapchain().extent();
    if (transient)
        clear = true;
    if (!handle) {
        if (matchSwapchain) {
            width = extent.width;
            height = extent.height;
        }
        handle = engine->createImage({
            width, height, 1, format, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_SRC | backend::ImageUsage::TRANSFER_DST | (backend::isDepthFormat(format) ? backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT : backend::ImageUsage::COLOUR_ATTACHMENT)
        });
        engine->driver().immediate([&](backend::vulkan::CommandBuffer& cmd) {
            if (backend::isDepthFormat(format)) {
                auto b = handle->barrier(backend::Access::NONE, backend::Access::NONE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TOP, 0, nullptr, { &b, 1 });
            } else {
                auto b = handle->barrier(backend::Access::NONE, backend::Access::NONE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::COLOUR_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TOP, 0, nullptr, { &b, 1 });
            }
        });
    }
    if (matchSwapchain && (handle->width() != extent.width || handle->height() != extent.height)) {
        width = extent.width;
        height = extent.height;
        engine->destroyImage(handle);
        handle = engine->createImage({
            width, height, 1, format, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_SRC | backend::ImageUsage::TRANSFER_DST | (backend::isDepthFormat(format) ? backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT : backend::ImageUsage::COLOUR_ATTACHMENT)
        });
        engine->driver().immediate([&](backend::vulkan::CommandBuffer& cmd) {
            if (backend::isDepthFormat(format)) {
                auto b = handle->barrier(backend::Access::NONE, backend::Access::NONE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TOP, 0, nullptr, { &b, 1 });
            } else {
                auto b = handle->barrier(backend::Access::NONE, backend::Access::NONE, backend::ImageLayout::UNDEFINED, backend::ImageLayout::COLOUR_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TOP, 0, nullptr, { &b, 1 });
            }
        });
    }
}

void cala::BufferResource::devirtualize(Engine *engine) {
    if (!handle) {
        handle = engine->createBuffer(size, usage);
    }
}

cala::RenderPass::RenderPass(RenderGraph *graph, const char *name, u32 index)
    : _graph(graph),
    _passName(name),
    _debugColour({0, 1, 0, 1}),
    _passTimer(index),
    _framebuffer(nullptr)
{}

cala::RenderPass::~RenderPass() {
//    delete _framebuffer;
//    delete _renderPass;
}

void cala::RenderPass::addColourOutput(const char *label, ImageResource info) {
    auto it = _graph->_attachmentMap.find(label);
    if (it == _graph->_attachmentMap.end())
        _graph->_attachmentMap.emplace(label, new ImageResource(std::move(info)));
    _outputs.emplace(label);
    _attachments.push(label);
}

void cala::RenderPass::addColourOutput(const char *label) {
    auto it = _graph->_attachmentMap.find(label);
    _outputs.emplace(label);
    _attachments.push(label);
}

void cala::RenderPass::setDepthOutput(const char *label, ImageResource info) {
    auto it = _graph->_attachmentMap.find(label);
    if (it == _graph->_attachmentMap.end()) {
        _graph->_attachmentMap.emplace(label, new ImageResource(std::move(info)));
    }
    _outputs.emplace(label);
    _attachments.push(label);
}

void cala::RenderPass::addImageInput(const char *label) {
    auto it = _graph->_attachmentMap.find(label);
    if (it != _graph->_attachmentMap.end()) {
        _inputs.emplace(label);
        _attachments.push(label);
    }
    else
        throw "couldn't find attachment"; //TODO: better error handling
}

void cala::RenderPass::setDepthInput(const char *label) {
    auto it = _graph->_attachmentMap.find(label);
    if (it != _graph->_attachmentMap.end()) {
        _inputs.emplace(label);
        _attachments.push(label);
    }
    else
        throw "couldn't find depth attachment"; //TODO: better error handling
}

void cala::RenderPass::addBufferInput(const char *label, BufferResource info) {
    auto it = _graph->_attachmentMap.find(label);
    if (it == _graph->_attachmentMap.end())
        _graph->_attachmentMap.emplace(label, new BufferResource(std::move(info)));
    _inputs.emplace(label);
}

void cala::RenderPass::addBufferOutput(const char *label, BufferResource info) {
    auto it = _graph->_attachmentMap.find(label);
    if (it == _graph->_attachmentMap.end())
        _graph->_attachmentMap.emplace(label, new BufferResource(std::move(info)));
    _outputs.emplace(label);
}

void cala::RenderPass::addBufferInput(const char *label) {
    auto it = _graph->_attachmentMap.find(label);
    if (it != _graph->_attachmentMap.end())
        _inputs.emplace(label);
    else
        throw "couldn't find buffer resource";
}

void cala::RenderPass::addBufferOutput(const char *label) {
    auto it = _graph->_attachmentMap.find(label);
    if (it != _graph->_attachmentMap.end())
        _outputs.emplace(label);
    else
        throw "couldn't find buffer resource";
}

void cala::RenderPass::setExecuteFunction(std::function<void(backend::vulkan::CommandBuffer&)> func) {
    _executeFunc = std::move(func);
}

void cala::RenderPass::setDebugColour(std::array<f32, 4> colour) {
    _debugColour = colour;
}


cala::RenderGraph::RenderGraph(Engine *engine)
    : _engine(engine)
{}

cala::RenderGraph::~RenderGraph() {
    for (auto& attachment : _attachmentMap)
        delete attachment.second;
}

cala::RenderPass &cala::RenderGraph::addPass(const char *name) {
    u32 index = _passes.size();
    while (_timers.size() <= index)
        _timers.push(std::make_pair("", backend::vulkan::Timer(_engine->driver())));
    _passes.push(RenderPass(this, name, index));
    _timers[index].first = name;
    assert(_passes.size() <= _timers.size());
    return _passes.back();
}

void cala::RenderGraph::setBackbuffer(const char *label) {
    _backbuffer = label;
}

bool cala::RenderGraph::compile() {
    _orderedPasses.clear();

    tsl::robin_map<const char*, ende::Vector<u32>> outputs;
    for (u32 i = 0; i < _passes.size(); i++) {
        auto& pass = _passes[i];
        for (auto& output : pass._outputs)
            outputs[output].push(i);
    }

    ende::Vector<bool> visited(_passes.size(), false);
    ende::Vector<bool> onStack(_passes.size(), false);

    std::function<bool(u32)> dfs = [&](u32 index) -> bool {
        visited[index] = true;
        onStack[index] = true;
        auto& pass = _passes[index];
        for (auto& input : pass._inputs) {
            for (u32 i = 0; i < outputs[input].size(); i++) {
                if (visited[i] && onStack[i])
                    return false;
                if (!visited[i]) {
                    if (!dfs(i))
                        return false;
                }
            }
        }
        _orderedPasses.push(&_passes[index]);
        onStack[index] = false;
        return true;
    };

    for (auto& pass : outputs[_backbuffer]) {
        if (!dfs(pass))
            return false;
    }


    for (auto& attachment : _attachmentMap) {
        attachment.second->devirtualize(_engine);
    }

    for (u32 passIndex = 0; passIndex < _orderedPasses.size(); passIndex++) {
        auto& pass = _orderedPasses[passIndex];
        if (!pass->_framebuffer) {
            ende::Vector<VkImageView> attachmentImages;
            u32 width = 0;
            u32 height = 0;
            backend::vulkan::RenderPass* renderPass = nullptr;
            ende::Vector<backend::vulkan::RenderPass::Attachment> attachments;
            for (auto &attachment: pass->_attachments) {

                auto it = _attachmentMap.find(attachment);
                auto resource = dynamic_cast<ImageResource*>(it.value());

                bool depth = backend::isDepthFormat(resource->format);

                backend::vulkan::RenderPass::Attachment attachmentRenderPass{};
                attachmentRenderPass.format = resource->format;
                attachmentRenderPass.samples = VK_SAMPLE_COUNT_1_BIT;
                attachmentRenderPass.loadOp = resource->clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
                attachmentRenderPass.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachmentRenderPass.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachmentRenderPass.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachmentRenderPass.initialLayout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachmentRenderPass.finalLayout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachmentRenderPass.internalLayout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                resource->clear = false;
                attachments.push(attachmentRenderPass);

                if (resource->matchSwapchain) {
                    auto extent = _engine->driver().swapchain().extent();
                    width = extent.width;
                    height = extent.height;
                } else {
                    width = resource->width;
                    height = resource->height;
                }
                attachmentImages.push(_engine->getImageView(resource->handle).view);
            }
            renderPass = _engine->driver().getRenderPass(attachments);
            pass->_framebuffer = _engine->driver().getFramebuffer(renderPass, attachmentImages, width, height);
        }
    }
    return true;
}

bool cala::RenderGraph::execute(backend::vulkan::CommandBuffer& cmd, u32 index) {
    for (auto& pass : _orderedPasses) {
        _timers[pass->_passTimer].second.start(cmd);
        cmd.pushDebugLabel(pass->_passName, pass->_debugColour);
        cmd.begin(*pass->_framebuffer);
        if (pass->_executeFunc)
            pass->_executeFunc(cmd);
        cmd.end(*pass->_framebuffer);
        cmd.popDebugLabel();
        _timers[pass->_passTimer].second.stop();
        for (auto& attachment : pass->_attachments) {
            auto it = _attachmentMap.find(attachment);
            if (it == _attachmentMap.end())
                continue;
            auto resource = dynamic_cast<ImageResource*>(it.value());
            if (backend::isDepthFormat(resource->handle->format())) {
                auto b = resource->handle->barrier(backend::Access::DEPTH_STENCIL_WRITE, backend::Access::DEPTH_STENCIL_WRITE | backend::Access::DEPTH_STENCIL_READ, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, 0, nullptr, { &b, 1 });
            } else {
                auto b = resource->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::COLOUR_ATTACHMENT_WRITE | backend::Access::COLOUR_ATTACHMENT_READ, backend::ImageLayout::COLOUR_ATTACHMENT, backend::ImageLayout::COLOUR_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, 0, nullptr, { &b, 1 });
            }
        }
    }
    auto it = _attachmentMap.find(_backbuffer);
    if (it == _attachmentMap.end())
        return false;

    auto backbuffer = dynamic_cast<ImageResource*>(it.value());

    auto barrier = backbuffer->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::TRANSFER_READ, backend::ImageLayout::COLOUR_ATTACHMENT, backend::ImageLayout::TRANSFER_SRC);
    cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT, backend::PipelineStage::TRANSFER, 0, nullptr, { &barrier, 1 });

    _engine->driver().swapchain().blitImageToFrame(index, cmd, *backbuffer->handle);

    barrier = backbuffer->handle->barrier(backend::Access::TRANSFER_READ, backend::Access::COLOUR_ATTACHMENT_WRITE, backend::ImageLayout::TRANSFER_SRC, backend::ImageLayout::COLOUR_ATTACHMENT);
    cmd.pipelineBarrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT, 0, nullptr, { &barrier, 1 });
    return true;
}

void cala::RenderGraph::reset() {
    _passes.clear();
//    _attachmentMap.clear();
}