#include "Cala/RenderGraph.h"
#include <Ende/log/log.h>
#include <Ende/profile/profile.h>

void cala::ImageResource::devirtualize(cala::Engine* engine, backend::vulkan::Swapchain* swapchain) {
    auto extent = swapchain->extent();
    if (transient)
        clear = true;

    if (backend::isDepthFormat(format))
        usage = usage | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT;

    auto use = backend::ImageUsage::SAMPLED | backend::ImageUsage::TRANSFER_SRC | backend::ImageUsage::TRANSFER_DST |
               backend::ImageUsage::STORAGE | (backend::isDepthFormat(format) ? backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT : backend::ImageUsage::COLOUR_ATTACHMENT);
    if (!handle) {
        if (matchSwapchain) {
            width = extent.width;
            height = extent.height;
        }

        handle = engine->device().createImage({
            width, height, 1, format, 1, 1, usage
        });
        engine->device().immediate([&](backend::vulkan::CommandBuffer& cmd) {
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
        engine->device().destroyImage(handle);
        handle = engine->device().createImage({
            width, height, 1, format, 1, 1, usage
        });
        engine->device().immediate([&](backend::vulkan::CommandBuffer& cmd) {
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

void cala::BufferResource::devirtualize(Engine *engine, backend::vulkan::Swapchain* swapchain) {
    if (!handle) {
        handle = engine->device().createBuffer(size, usage);
    }
    if (size > handle->size())
        handle = engine->device().resizeBuffer(handle, size * 2);
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


bool cala::RenderPass::reads(const char *label, bool storage) {
    auto it = _graph->_attachmentMap.find(label);
    if (_graph->_attachmentMap.end() == it) {
        std::string err = "unable to find ";
        err += label;
        ende::log::error(err);
        return false;
    }
    else {
        _inputs.emplace(std::make_pair(label, storage));
        return true;
    }
}

bool cala::RenderPass::writes(const char *label) {
    auto it = _graph->_attachmentMap.find(label);
    if (_graph->_attachmentMap.end() == it) {
        std::string err = "unable to find ";
        err += label;
        ende::log::error(err);
        return false;
    }
    else {
        _outputs.emplace(label);
        return true;
    }
}





void cala::RenderPass::addColourAttachment(const char *label) {
    //TODO: error handling
    if (writes(label)) {
        _attachments.emplace(label);
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::COLOUR_ATTACHMENT;
    }
}

void cala::RenderPass::addDepthAttachment(const char *label) {
    if (writes(label)) {
        _attachments.emplace(label);
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT;
    }
}

void cala::RenderPass::addDepthReadAttachment(const char *label) {
    if (reads(label)) {
        _attachments.emplace(label);
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT;
    }
}

void cala::RenderPass::addStorageImageRead(const char *label) {
    if (reads(label, true)) {
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::STORAGE;
    }
}

void cala::RenderPass::addStorageImageWrite(const char *label) {
    if (writes(label)) {
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::STORAGE;
    }
}

void cala::RenderPass::addStorageBufferRead(const char *label) {
    if (reads(label, true)) {
        if (auto resource = _graph->getResource<BufferResource>(label); resource)
            resource->usage = resource->usage | backend::BufferUsage::STORAGE;
    }
}

void cala::RenderPass::addStorageBufferWrite(const char *label) {
    if (writes(label)) {
        if (auto resource = _graph->getResource<BufferResource>(label); resource)
            resource->usage = resource->usage | backend::BufferUsage::STORAGE;
    }
}

void cala::RenderPass::addSampledImageRead(const char *label) {
    if (reads(label)) {
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::SAMPLED;
    }
}

void cala::RenderPass::addSampledImageWrite(const char *label) {
    if (writes(label)) {
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::SAMPLED;
    }
}




void cala::RenderPass::setExecuteFunction(std::function<void(backend::vulkan::CommandBuffer&, cala::RenderGraph&)> func) {
    _executeFunc = std::move(func);
}

void cala::RenderPass::setDebugColour(std::array<f32, 4> colour) {
    _debugColour = colour;
}


cala::RenderGraph::RenderGraph(Engine *engine)
    : _engine(engine),
    _swapchain(nullptr)
{}

cala::RenderGraph::~RenderGraph() {
//    for (auto& attachment : _attachmentMap)
//
//        delete attachment.second;
}

cala::RenderPass &cala::RenderGraph::addPass(const char *name) {
    u32 index = _passes.size();
    while (_timers[_engine->device().frameIndex()].size() <= index)
        _timers[_engine->device().frameIndex()].push(std::make_pair("", backend::vulkan::Timer(_engine->device())));
    _passes.push(RenderPass(this, name, index));
    _timers[_engine->device().frameIndex()][index].first = name;
    assert(_passes.size() <= _timers[_engine->device().frameIndex()].size());
    return _passes.back();
}

void cala::RenderGraph::setBackbuffer(const char *label) {
    _backbuffer = label;
}

bool cala::RenderGraph::compile(cala::backend::vulkan::Swapchain* swapchain) {
    PROFILE_NAMED("RenderGraph::Compile");
    _swapchain = swapchain;
    assert(_swapchain);
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
        u32 index = attachment.second.index;
        if (attachment.second.internal) {
            assert(index < _internalResources.size() && _internalResources[index]);
            _internalResources[index]->devirtualize(_engine, _swapchain);
        } else {
            assert(index < _externalResources.size() && _externalResources[index]);
            _externalResources[index]->devirtualize(_engine, _swapchain);
        }
//        attachment.second->devirtualize(_engine, _swapchain);
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

                auto resource = getResource<ImageResource>(attachment);

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
                    auto extent = _swapchain->extent();
                    width = extent.width;
                    height = extent.height;
                } else {
                    width = resource->width;
                    height = resource->height;
                }
                attachmentImages.push(_engine->device().getImageView(resource->handle).view);
                attachmentHashes.push(resource->handle.index());
            }
            renderPass = _engine->device().getRenderPass(attachments);
            pass->_framebuffer = _engine->device().getFramebuffer(renderPass, attachmentImages, attachmentHashes, width, height);
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
    assert(_swapchain);
    for (u32 i = 0; i < _orderedPasses.size(); i++) {
        auto& pass = _orderedPasses[i];
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
            if (auto imageResource = getResource<ImageResource>(input.first); imageResource) {
                if (input.second) {
                    if (imageResource->handle->layout() != backend::ImageLayout::GENERAL) {
                        auto b = imageResource->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::GENERAL);
                        cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::FRAGMENT_SHADER, { &b, 1 });
                    }
                } else {
                    if (imageResource->handle->layout() != backend::ImageLayout::SHADER_READ_ONLY) {
                        auto b = imageResource->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY);
                        cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::FRAGMENT_SHADER, { &b, 1 });
                    }
                }
            } else if (auto bufferResource = getResource<BufferResource>(input.first); bufferResource) {
                auto b = bufferResource->handle->barrier(backend::Access::MEMORY_READ);
                cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TOP, { &b, 1 });
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
            if (auto imageResource = getResource<ImageResource>(output); imageResource) {
                if (imageResource->handle->layout() != backend::ImageLayout::GENERAL) {
                    auto b = imageResource->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL);
                    cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::COMPUTE_SHADER | backend::PipelineStage::FRAGMENT_SHADER, { &b, 1 });
                }
            } else if (auto bufferResource = getResource<BufferResource>(output); bufferResource) {
                auto b = bufferResource->handle->barrier(backend::Access::MEMORY_WRITE);
                cmd.pipelineBarrier(backend::PipelineStage::TOP, backend::PipelineStage::TOP, { &b, 1 });
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


        auto& timer = _timers[_engine->device().frameIndex()][i];
        timer.first = pass->_passName;
        timer.second.start(cmd);

        cmd.pushDebugLabel(pass->_passName, pass->_debugColour);
        if (pass->_framebuffer)
            cmd.begin(*pass->_framebuffer);
        if (pass->_executeFunc)
            pass->_executeFunc(cmd, *this);
        if (pass->_framebuffer)
            cmd.end(*pass->_framebuffer);
        cmd.popDebugLabel();
        timer.second.stop();
        for (auto& attachment : pass->_attachments) {
            auto resource = getResource<ImageResource>(attachment);
            if (!resource)
                continue;
            if (backend::isDepthFormat(resource->handle->format())) {
                auto b = resource->handle->barrier(backend::Access::DEPTH_STENCIL_WRITE, backend::Access::DEPTH_STENCIL_WRITE | backend::Access::DEPTH_STENCIL_READ, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, { &b, 1 });
            } else {
                auto b = resource->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::COLOUR_ATTACHMENT_WRITE | backend::Access::COLOUR_ATTACHMENT_READ, backend::ImageLayout::COLOUR_ATTACHMENT);
                cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT | backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, { &b, 1 });
            }
        }
    }

    auto backbuffer = getResource<ImageResource>(_backbuffer);
    if (!backbuffer)
        return false;

    auto barrier = backbuffer->handle->barrier(backend::Access::COLOUR_ATTACHMENT_WRITE, backend::Access::TRANSFER_READ, backend::ImageLayout::TRANSFER_SRC);
    cmd.pipelineBarrier(backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT, backend::PipelineStage::TRANSFER, { &barrier, 1 });

    _swapchain->blitImageToFrame(index, cmd, *backbuffer->handle);

    barrier = backbuffer->handle->barrier(backend::Access::TRANSFER_READ, backend::Access::COLOUR_ATTACHMENT_WRITE, backend::ImageLayout::COLOUR_ATTACHMENT);
    cmd.pipelineBarrier(backend::PipelineStage::TRANSFER, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT, { &barrier, 1 });
    return true;
}

void cala::RenderGraph::reset() {
    _passes.clear();
//    for (auto& attachment : _attachmentMap) {
//        u32 index = attachment.second.index;
//
//
//        if (attachment.second->transient && dynamic_cast<BufferResource*>(attachment.second)) {
////            delete attachment.second;
//            _attachmentMap.erase(attachment.first);
//        }
//    }
//    _attachmentMap.clear();
}