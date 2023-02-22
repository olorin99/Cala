#include "Cala/RenderGraph.h"
#include <Ende/log/log.h>
#include <Ende/profile/profile.h>

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
            width, height, 1, format, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::STORAGE | backend::ImageUsage::TRANSFER_SRC | backend::ImageUsage::TRANSFER_DST | (backend::isDepthFormat(format) ? backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT : backend::ImageUsage::COLOUR_ATTACHMENT)
        });
        engine->driver().immediate([&](backend::vulkan::CommandBuffer& cmd) {
            if (backend::isDepthFormat(format)) {
                auto b = handle->barrier(backend::Access::NONE, backend::Access::NONE, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TOP, { &b, 1 });
            } else {
                auto b = handle->barrier(backend::Access::NONE, backend::Access::NONE, backend::ImageLayout::COLOUR_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TOP, { &b, 1 });
            }
        });
    }
    if (matchSwapchain && (handle->width() != extent.width || handle->height() != extent.height)) {
        width = extent.width;
        height = extent.height;
        engine->destroyImage(handle);
        handle = engine->createImage({
            width, height, 1, format, 1, 1, backend::ImageUsage::SAMPLED | backend::ImageUsage::STORAGE | backend::ImageUsage::TRANSFER_SRC | backend::ImageUsage::TRANSFER_DST | (backend::isDepthFormat(format) ? backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT : backend::ImageUsage::COLOUR_ATTACHMENT)
        });
        engine->driver().immediate([&](backend::vulkan::CommandBuffer& cmd) {
            if (backend::isDepthFormat(format)) {
                auto b = handle->barrier(backend::Access::NONE, backend::Access::NONE, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TOP,{ &b, 1 });
            } else {
                auto b = handle->barrier(backend::Access::NONE, backend::Access::NONE, backend::ImageLayout::COLOUR_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TOP, { &b, 1 });
            }
        });
    }
}

void cala::BufferResource::devirtualize(Engine *engine) {
    if (!handle) {
        handle = engine->createBuffer(size, usage);
    }
    if (size > handle->size())
        handle = engine->resizeBuffer(handle, size * 2);
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

void cala::RenderPass::addImageInput(const char *label, bool storage) {
    auto it = _graph->_attachmentMap.find(label);
    if (it != _graph->_attachmentMap.end()) {
        _inputs.emplace(std::make_pair(label, storage));
    }
    else
        throw "couldn't find attachment"; //TODO: better error handling
}

void cala::RenderPass::addImageOutput(const char *label, bool storage) {
    auto it = _graph->_attachmentMap.find(label);
    if (it != _graph->_attachmentMap.end())
        _inputs.emplace(std::make_pair(label, storage));
    else
        throw "couldn't find attachment";
}

void cala::RenderPass::addImageOutput(const char *label, ImageResource info, bool storage) {
    auto it = _graph->_attachmentMap.find(label);
    if (it == _graph->_attachmentMap.end())
        _graph->_attachmentMap.emplace(label, new ImageResource(std::move(info)));
    _outputs.emplace(label);
}

void cala::RenderPass::setDepthInput(const char *label) {
    auto it = _graph->_attachmentMap.find(label);
    if (it != _graph->_attachmentMap.end()) {
        _inputs.emplace(std::make_pair(label, false));
        _attachments.push(label);
    }
    else
        throw "couldn't find depth attachment"; //TODO: better error handling
}

void cala::RenderPass::addBufferInput(const char *label, BufferResource info) {
    auto it = _graph->_attachmentMap.find(label);
    if (it == _graph->_attachmentMap.end())
        _graph->_attachmentMap.emplace(label, new BufferResource(std::move(info)));
    _inputs.emplace(std::make_pair(label, false));
}

void cala::RenderPass::addBufferOutput(const char *label, BufferResource info) {
    auto it = _graph->_attachmentMap.find(label);
    if (it == _graph->_attachmentMap.end())
        _graph->_attachmentMap.emplace(label, new BufferResource(std::move(info)));
    else if (auto bit = dynamic_cast<BufferResource*>(it->second); bit && bit->size != info.size)
        bit->size = info.size;
    _outputs.emplace(label);
}

void cala::RenderPass::addBufferInput(const char *label) {
    auto it = _graph->_attachmentMap.find(label);
    if (it != _graph->_attachmentMap.end())
        _inputs.emplace(std::make_pair(label, false));
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

void cala::RenderPass::setExecuteFunction(std::function<void(backend::vulkan::CommandBuffer&, cala::RenderGraph&)> func) {
    _executeFunc = std::move(func);
}

void cala::RenderPass::setDebugColour(std::array<f32, 4> colour) {
    _debugColour = colour;
}


cala::RenderGraph::RenderGraph(Engine *engine)
    : _engine(engine),
    _frameIndex(0)
{}

cala::RenderGraph::~RenderGraph() {
    for (auto& attachment : _attachmentMap)
        delete attachment.second;
}

cala::RenderPass &cala::RenderGraph::addPass(const char *name) {
    u32 index = _passes.size();
    while (_timers[_frameIndex].size() <= index)
        _timers[_frameIndex].push(std::make_pair("", backend::vulkan::Timer(_engine->driver())));
    _passes.push(RenderPass(this, name, index));
    _timers[_frameIndex][index].first = name;
    assert(_passes.size() <= _timers[_frameIndex].size());
    return _passes.back();
}

void cala::RenderGraph::setBackbuffer(const char *label) {
    _backbuffer = label;
}

bool cala::RenderGraph::compile() {
    PROFILE_NAMED("RenderGraph::Compile");
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
            for (u32 i = 0; i < outputs[input.first].size(); i++) {
                u32 j = outputs[input.first][i];
                if (visited[j] && onStack[j])
                    return false;
                if (!visited[j]) {
                    if (!dfs(j))
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
//        if (!pass->_framebuffer && !pass->_attachments.empty()) {
        if (!pass->_attachments.empty()) {
            ende::Vector<VkImageView> attachmentImages;
            ende::Vector<u32> attachmentHashes;
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
                attachmentHashes.push(resource->handle.index());
            }
            renderPass = _engine->driver().getRenderPass(attachments);
            pass->_framebuffer = _engine->driver().getFramebuffer(renderPass, attachmentImages, attachmentHashes, width, height);
        } else if (!pass->_framebuffer) {
            for (auto& output : pass->_outputs) {
                if (auto resource = getResource<ImageResource>(output); resource) {
                    resource->clear = false;
                }
            }
        }
    }
    return true;
}

bool cala::RenderGraph::execute(backend::vulkan::CommandBuffer& cmd, u32 index) {
    PROFILE_NAMED("RenderGraph::Execute");
    for (auto& pass : _orderedPasses) {
        for (auto& input : pass->_inputs) {
            bool attach = false;
            for (auto& attachment : pass->_attachments) {
                if (input.first == attachment) {
                    attach = true;
                    continue;
                }
            }
            if (attach)
                continue;
            if (auto resource = getResource<ImageResource>(input.first); resource) {
                if (input.second) {
                    if (resource->handle->layout() != backend::ImageLayout::GENERAL) {
                        auto b = resource->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::GENERAL);
                        cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::FRAGMENT_SHADER, { &b, 1 });
                    }
                } else {
                    if (resource->handle->layout() != backend::ImageLayout::SHADER_READ_ONLY) {
                        auto b = resource->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY);
                        cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::FRAGMENT_SHADER, { &b, 1 });
                    }
                }
            }
        }
        for (auto& output : pass->_outputs) {
            bool attach = false;
            for (auto& attachment : pass->_attachments) {
                if (output == attachment) {
                    attach = true;
                    continue;
                }
            }
            if (attach)
                continue;
            if (auto resource = getResource<ImageResource>(output); resource) {
                if (resource->handle->layout() != backend::ImageLayout::GENERAL) {
                    auto b = resource->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
                    cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::COMPUTE_SHADER | backend::PipelineStage::FRAGMENT_SHADER, { &b, 1 });
                }
            }
        }
        for (auto& attachment : pass->_attachments) {
            auto resource = getResource<ImageResource>(attachment);
            if (backend::isDepthFormat(resource->handle->format())) {
                if (resource->handle->layout() != backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT) {
                    auto b = resource->handle->barrier(backend::Access::SHADER_READ, backend::Access::DEPTH_STENCIL_WRITE | backend::Access::DEPTH_STENCIL_READ, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                    cmd.pipelineBarrier(backend::PipelineStage::FRAGMENT_SHADER, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, { &b, 1 });
                }
            } else {
                if (resource->handle->layout() != backend::ImageLayout::COLOUR_ATTACHMENT) {
                    auto b = resource->handle->barrier(backend::Access::SHADER_READ | backend::Access::SHADER_WRITE, backend::Access::COLOUR_ATTACHMENT_WRITE | backend::Access::COLOUR_ATTACHMENT_READ, backend::ImageLayout::COLOUR_ATTACHMENT);
                    cmd.pipelineBarrier(backend::PipelineStage::FRAGMENT_SHADER | backend::PipelineStage::COMPUTE_SHADER, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, { &b, 1 });
                }
            }
        }


        _timers[_frameIndex][pass->_passTimer].second.start(cmd);
        cmd.pushDebugLabel(pass->_passName, pass->_debugColour);
        if (pass->_framebuffer)
            cmd.begin(*pass->_framebuffer);
        if (pass->_executeFunc)
            pass->_executeFunc(cmd, *this);
        if (pass->_framebuffer)
            cmd.end(*pass->_framebuffer);
        cmd.popDebugLabel();
        _timers[_frameIndex][pass->_passTimer].second.stop();
        for (auto& attachment : pass->_attachments) {
            auto it = _attachmentMap.find(attachment);
            if (it == _attachmentMap.end())
                continue;
            auto resource = dynamic_cast<ImageResource*>(it.value());
            if (backend::isDepthFormat(resource->handle->format())) {
                auto b = resource->handle->barrier(backend::Access::DEPTH_STENCIL_WRITE, backend::Access::DEPTH_STENCIL_WRITE | backend::Access::DEPTH_STENCIL_READ, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, { &b, 1 });
            } else {
                auto b = resource->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::COLOUR_ATTACHMENT_WRITE | backend::Access::COLOUR_ATTACHMENT_READ, backend::ImageLayout::COLOUR_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, { &b, 1 });
            }
        }
    }
    auto it = _attachmentMap.find(_backbuffer);
    if (it == _attachmentMap.end())
        return false;

    auto backbuffer = dynamic_cast<ImageResource*>(it.value());

    auto barrier = backbuffer->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::TRANSFER_READ, backend::ImageLayout::TRANSFER_SRC);
    cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT, backend::PipelineStage::TRANSFER, { &barrier, 1 });

    _engine->driver().swapchain().blitImageToFrame(index, cmd, *backbuffer->handle);

    barrier = backbuffer->handle->barrier(backend::Access::TRANSFER_READ, backend::Access::COLOUR_ATTACHMENT_WRITE, backend::ImageLayout::COLOUR_ATTACHMENT);
    cmd.pipelineBarrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT, { &barrier, 1 });

    _frameIndex = (_frameIndex + 1) % 2;
    return true;
}

void cala::RenderGraph::reset() {
    _passes.clear();
    for (auto& attachment : _attachmentMap) {
        if (attachment.second->transient && dynamic_cast<BufferResource*>(attachment.second)) {
            delete attachment.second;
            _attachmentMap.erase(attachment.first);
        }
    }
//    _attachmentMap.clear();
}