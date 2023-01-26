#include "Cala/RenderGraph.h"
#include <Ende/log/log.h>

cala::RenderPass::RenderPass(RenderGraph *graph, const char *name)
    : _graph(graph),
    _passName(name),
    _depthAttachment(nullptr),
    _framebuffer(nullptr)
{}

cala::RenderPass::~RenderPass() {
//    delete _framebuffer;
//    delete _renderPass;
}

void cala::RenderPass::addColourOutput(const char *label, AttachmentInfo info) {
    auto it = _graph->_attachments.find(label);
    if (it == _graph->_attachments.end())
        _graph->_attachments.emplace(label, info);
    _outputs.emplace(label);
}

void cala::RenderPass::addColourOutput(const char *label) {
    auto it = _graph->_attachments.find(label);
    _outputs.emplace(label);
}

void cala::RenderPass::setDepthOutput(const char *label, AttachmentInfo info) {
    auto it = _graph->_attachments.find(label);
    if (it == _graph->_attachments.end())
        _graph->_attachments.emplace(label, info);
    _depthAttachment = label;
    _outputs.emplace(label);
}

void cala::RenderPass::addAttachmentInput(const char *label) {
    auto it = _graph->_attachments.find(label);
    if (it != _graph->_attachments.end())
        _inputs.emplace(label);
    else
        throw "couldn't find attachment"; //TODO: better error handling
}

void cala::RenderPass::setDepthInput(const char *label) {
    auto it = _graph->_attachments.find(label);
    if (it != _graph->_attachments.end()) {
        _inputs.emplace(label);
        _depthAttachment = label;
    }
    else
        throw "couldn't find depth attachment"; //TODO: better error handling
}

void cala::RenderPass::setExecuteFunction(std::function<void(backend::vulkan::CommandBuffer&)> func) {
    _executeFunc = std::move(func);
}


cala::RenderGraph::RenderGraph(Engine *engine)
    : _engine(engine)
{}

cala::RenderPass &cala::RenderGraph::addPass(const char *name) {
    _passes.push(RenderPass(this, name));
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


    for (auto& attachment : _attachments) {
        auto it = _attachments.find(attachment.first);
        it.value().clear = true;
        auto extent = _engine->driver().swapchain().extent();
        if (!attachment.second.handle) {
            u32 width = attachment.second.width;
            u32 height = attachment.second.height;
            if (attachment.second.matchSwapchain) {
                width = extent.width;
                height = extent.height;
            }


            it.value().handle = _engine->createImage({
                width,
                height,
                1,
                attachment.second.format,
                1, 1,
                backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_SRC | backend::ImageUsage::TRANSFER_DST | (backend::isDepthFormat(attachment.second.format) ? backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT : backend::ImageUsage::COLOUR_ATTACHMENT)
            });
        }
        if (attachment.second.matchSwapchain && (it.value().handle->width() != extent.width || it.value().handle->height() != extent.height)) {
            u32 width = attachment.second.width;
            u32 height = attachment.second.height;
            if (attachment.second.matchSwapchain) {
                width = extent.width;
                height = extent.height;
            }
            _engine->destroyImage(it.value().handle);
            it.value().handle = _engine->createImage({
                width,
                height,
                1,
                attachment.second.format,
                1, 1,
                backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_SRC | backend::ImageUsage::TRANSFER_DST | (backend::isDepthFormat(attachment.second.format) ? backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT : backend::ImageUsage::COLOUR_ATTACHMENT)
            });
        }
    }

    for (u32 passIndex = 0; passIndex < _orderedPasses.size(); passIndex++) {
        auto& pass = _orderedPasses[passIndex];
        backend::vulkan::RenderPass* renderPass = nullptr;
        ende::Vector<backend::vulkan::RenderPass::Attachment> attachments;
        bool depthWritten = false;
        for (auto &output: pass->_outputs) {

            auto it = _attachments.find(output);
            bool depth = backend::isDepthFormat(it->second.format);

            backend::vulkan::RenderPass::Attachment attachment{};
            attachment.format = it->second.format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = it->second.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = it->second.clear ? VK_IMAGE_LAYOUT_UNDEFINED : (depth
                                                                                       ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                                                                       : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            attachment.finalLayout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                           : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.internalLayout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                              : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            if (depth)
                depthWritten = true;
            it.value().clear = false;
            attachments.push(attachment);
        }
        if (!depthWritten && pass->_depthAttachment) {
            auto it = _attachments.find(pass->_depthAttachment);
            backend::vulkan::RenderPass::Attachment attachment{};
            attachment.format = it->second.format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = it->second.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = it->second.clear ? VK_IMAGE_LAYOUT_UNDEFINED
                                                        : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachment.internalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.push(attachment);
            it.value().clear = false;
        }
        renderPass = _engine->driver().getRenderPass(attachments);
        if (!pass->_framebuffer) {
            ende::Vector<VkImageView> attachmentImages;
            u32 width = 0;
            u32 height = 0;
            bool depthWritten = false;
            for (auto& output : pass->_outputs) {
                auto it = _attachments.find(output);
                if (it->second.matchSwapchain) {
                    auto extent = _engine->driver().swapchain().extent();
                    width = extent.width;
                    height = extent.height;
                } else {
                    width = it->second.width;
                    height = it->second.height;
                }
                if (backend::isDepthFormat(it->second.format))
                    depthWritten = true;
                attachmentImages.push(_engine->getImageView(it->second.handle).view);
            }
            if (!depthWritten && pass->_depthAttachment) {
                auto it = _attachments.find(pass->_depthAttachment);
                if (it->second.matchSwapchain) {
                    auto extent = _engine->driver().swapchain().extent();
                    width = extent.width;
                    height = extent.height;
                } else {
                    width = it->second.width;
                    height = it->second.height;
                }
                attachmentImages.push(_engine->getImageView(it->second.handle).view);
            }
            pass->_framebuffer = _engine->driver().getFramebuffer(renderPass, attachmentImages, width, height);
        }
    }


    return true;
}

bool cala::RenderGraph::execute(backend::vulkan::CommandBuffer& cmd, u32 index) {
    for (auto& pass : _orderedPasses) {
        cmd.pushDebugLabel(pass->_passName);
        cmd.begin(*pass->_framebuffer);
        pass->_executeFunc(cmd);
        cmd.end(*pass->_framebuffer);
        cmd.popDebugLabel();
    }
    auto backbuffer = _attachments.find(_backbuffer);
    if (backbuffer == _attachments.end())
        return false;

    auto barrier = backbuffer.value().handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::TRANSFER_READ, backend::ImageLayout::COLOUR_ATTACHMENT, backend::ImageLayout::TRANSFER_SRC);
    cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT, backend::PipelineStage::TRANSFER, 0, nullptr, { &barrier, 1 });

    _engine->driver().swapchain().blitImageToFrame(index, cmd, *backbuffer.value().handle);

    barrier = backbuffer.value().handle->barrier(backend::Access::TRANSFER_READ, backend::Access::COLOUR_ATTACHMENT_WRITE, backend::ImageLayout::TRANSFER_SRC, backend::ImageLayout::COLOUR_ATTACHMENT);
    cmd.pipelineBarrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT, 0, nullptr, { &barrier, 1 });
    return true;
}

void cala::RenderGraph::reset() {
    _passes.clear();
//    _attachments.clear();
}