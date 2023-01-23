#include "Cala/RenderGraph.h"


cala::RenderPass::RenderPass(RenderGraph *graph, const char *name)
    : _graph(graph),
    _passName(name),
    _depthAttachment(nullptr),
    _renderPass(nullptr),
    _framebuffer(nullptr)
{}

cala::RenderPass::~RenderPass() {
    delete _framebuffer;
    delete _renderPass;
}

void cala::RenderPass::addColourOutput(const char *label, AttachmentInfo info) {
    _graph->_attachments.emplace(label, info);
    _outputs.emplace(label);
}

void cala::RenderPass::setDepthOutput(const char *label, AttachmentInfo info) {
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

    tsl::robin_map<const char*, ende::Vector<RenderPass*>> outputs;
    for (auto& pass : _passes) {
        for (auto& output : pass._outputs)
            outputs[output].push(&pass);
    }

    //TODO: add check for cyclical dependencies
    std::function<void(RenderPass*)> dfs = [&](RenderPass* pass) {
        for (auto& input : pass->_inputs) {
            for (auto& parent : outputs[input]) {
                dfs(parent);
            }
        }
        _orderedPasses.push(pass);
    };

    for (auto& pass : outputs[_backbuffer]) {
        dfs(pass);
    }


    for (auto& attachment : _attachments) {
        if (!attachment.second.handle) {
            u32 width = attachment.second.width;
            u32 height = attachment.second.height;
            if (attachment.second.matchSwapchain) {
                auto extent = _engine->driver().swapchain().extent();
                width = extent.width;
                height = extent.height;
            }

            auto it = _attachments.find(attachment.first);
            it.value().handle = _engine->createImage({
                width,
                height,
                1,
                attachment.second.format,
                1, 1,
                backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_DST | (backend::isDepthFormat(attachment.second.format) ? backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT : backend::ImageUsage::COLOUR_ATTACHMENT)
            });
        }
    }

    for (auto& pass : _orderedPasses) {
        if (!pass->_renderPass) {
            ende::Vector<backend::vulkan::RenderPass::Attachment> attachments;

            for (auto& output : pass->_outputs) {
                auto it = _attachments.find(output);
                backend::vulkan::RenderPass::Attachment attachment{};
                attachment.format = it->second.format;
                attachment.samples = VK_SAMPLE_COUNT_1_BIT;
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                attachment.internalLayout = backend::isDepthFormat(attachment.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                attachments.push(attachment);
            }

            pass->_renderPass = new cala::backend::vulkan::RenderPass(_engine->driver(), attachments);
        }
        if (!pass->_framebuffer) {
            ende::Vector<VkImageView> attachments;
            u32 width = 0;
            u32 height = 0;
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
                if (output == _backbuffer) {
                    attachments.push(_engine->driver().swapchain().view(0));
                    continue;
                }
                attachments.push(_engine->getImageView(it->second.handle).view);
            }
            pass->_framebuffer = new cala::backend::vulkan::Framebuffer(_engine->driver().context().device(), *pass->_renderPass, attachments, width, height);
        }
    }


    return true;
}

bool cala::RenderGraph::execute(backend::vulkan::CommandBuffer& cmd) {
    for (auto& pass : _orderedPasses) {
        cmd.pushDebugLabel(pass->_passName);
        cmd.begin(*pass->_framebuffer);
        pass->_executeFunc(cmd);
        cmd.end(*pass->_framebuffer);
        cmd.popDebugLabel();
    }
    return true;
}