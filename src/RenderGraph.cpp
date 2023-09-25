#include "Cala/RenderGraph.h"
#include <Ende/profile/profile.h>

void cala::ImageResource::devirtualize(cala::Engine* engine, backend::vulkan::Swapchain* swapchain) {
    auto extent = swapchain->extent();
    if (transient)
        clear = true;

    if (backend::isDepthFormat(format))
        usage = usage | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT;

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
}

void cala::ImageResource::destroyResource(cala::Engine *engine) {
//    engine->device().destroyImage(handle);
    handle = {};
}

bool cala::ImageResource::operator==(cala::Resource *rhs) {
    auto res = dynamic_cast<ImageResource*>(rhs);
    if (!res)
        return false;

    // If set to match swapchain disregard stored size as it will be updated later to swapchain
    bool match = res->matchSwapchain == matchSwapchain && matchSwapchain;
    if (!match) {
        return res->width == width &&
               res->height == height &&
               res->format == format &&
               res->matchSwapchain == matchSwapchain;
    } else {
        return res->format == format;
    }
}

bool cala::ImageResource::operator!=(cala::Resource *rhs) {
    return !(*this == rhs);
}

void cala::BufferResource::devirtualize(Engine *engine, backend::vulkan::Swapchain* swapchain) {
    if (!handle)
        handle = engine->device().createBuffer(size, usage);
}

void cala::BufferResource::destroyResource(cala::Engine *engine) {
//    engine->device().destroyBuffer(handle);
    handle = {};
}

bool cala::BufferResource::operator==(cala::Resource *rhs) {
    auto res = dynamic_cast<BufferResource*>(rhs);
    if (!res)
        return false;

    return res->size == size;
}

bool cala::BufferResource::operator!=(cala::Resource *rhs) {
    return !(*this == rhs);
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


bool cala::RenderPass::reads(const char *label, bool storage, backend::PipelineStage stage, backend::Access access, backend::ImageLayout layout) {
    auto it = _graph->_attachmentMap.find(label);
    if (_graph->_attachmentMap.end() == it) {
        std::string err = "unable to find ";
        err += label;
        _graph->_engine->logger().error(err);
        return false;
    }
    else {
        _inputs.emplace(label, access, stage, layout);
        return true;
    }
}

bool cala::RenderPass::writes(const char *label, backend::PipelineStage stage, backend::Access access, backend::ImageLayout layout) {
    auto it = _graph->_attachmentMap.find(label);
    if (_graph->_attachmentMap.end() == it) {
        std::string err = "unable to find ";
        err += label;
        _graph->_engine->logger().error(err);
        return false;
    }
    else {
        _outputs.emplace(label, access, stage, layout);
        return true;
    }
}





void cala::RenderPass::addColourAttachment(const char *label) {
    //TODO: error handling
    if (writes(label, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT, backend::Access::COLOUR_ATTACHMENT_WRITE, backend::ImageLayout::COLOUR_ATTACHMENT)) {
        _attachments.emplace(label, backend::Access::COLOUR_ATTACHMENT_WRITE, backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT, backend::ImageLayout::COLOUR_ATTACHMENT);
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::COLOUR_ATTACHMENT;
    }
}

void cala::RenderPass::addDepthAttachment(const char *label) {
    if (writes(label, backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::Access::DEPTH_STENCIL_WRITE, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT)) {
        _attachments.emplace(label, backend::Access::DEPTH_STENCIL_WRITE, backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT;
    }
}

void cala::RenderPass::addDepthReadAttachment(const char *label) {
    if (reads(label, false, backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::Access::DEPTH_STENCIL_READ, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT)) {
        _attachments.emplace(label, backend::Access::DEPTH_STENCIL_READ, backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT, backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT;
    }
}

void cala::RenderPass::addIndirectBufferRead(const char *label) {
    if (reads(label, true, backend::PipelineStage::DRAW_INDIRECT, backend::Access::INDIRECT_READ)) {
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::STORAGE;
    }
}

void cala::RenderPass::addStorageImageRead(const char *label, backend::PipelineStage stage) {
    if (reads(label, true, stage, backend::Access::SHADER_READ, backend::ImageLayout::GENERAL)) {
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::STORAGE;
    }
}

void cala::RenderPass::addStorageImageWrite(const char *label, backend::PipelineStage stage) {
    if (writes(label, stage, backend::Access::SHADER_WRITE, backend::ImageLayout::GENERAL)) {
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::STORAGE;
    }
}

void cala::RenderPass::addStorageBufferRead(const char *label, backend::PipelineStage stage) {
    if (reads(label, true, stage, backend::Access::SHADER_READ)) {
        if (auto resource = _graph->getResource<BufferResource>(label); resource)
            resource->usage = resource->usage | backend::BufferUsage::STORAGE;
    }
}

void cala::RenderPass::addStorageBufferWrite(const char *label, backend::PipelineStage stage) {
    if (writes(label, stage, backend::Access::SHADER_WRITE)) {
        if (auto resource = _graph->getResource<BufferResource>(label); resource)
            resource->usage = resource->usage | backend::BufferUsage::STORAGE;
    }
}

void cala::RenderPass::addSampledImageRead(const char *label, backend::PipelineStage stage) {
    if (reads(label, false, stage, backend::Access::SHADER_READ, backend::ImageLayout::SHADER_READ_ONLY)) {
        if (auto resource = _graph->getResource<ImageResource>(label); resource)
            resource->usage = resource->usage | backend::ImageUsage::SAMPLED;
    }
}

void cala::RenderPass::addSampledImageWrite(const char *label, backend::PipelineStage stage) {
    if (writes(label, stage, backend::Access::SHADER_WRITE, backend::ImageLayout::SHADER_READ_ONLY)) {
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
            outputs[output.name].push(i);
    }

    ende::Vector<bool> visited(_passes.size(), false);
    ende::Vector<bool> onStack(_passes.size(), false);

    std::function<bool(u32)> dfs = [&](u32 index) -> bool {
        visited[index] = true;
        onStack[index] = true;
        auto& pass = _passes[index];
        for (auto& input : pass._inputs) {
            for (u32 i = 0; i < outputs[input.name].size(); i++) {
                u32 j = outputs[input.name][i];
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

    auto findNextAccess = [&](i32 startIndex, const char* resource) -> std::tuple<i32, i32, RenderPass::PassResource> {
        for (i32 passIndex = startIndex + 1; passIndex < _orderedPasses.size(); passIndex++) {
            auto& pass = _orderedPasses[passIndex];
            for (i32 outputIndex = 0; outputIndex < pass->_outputs.size(); outputIndex++) {
                if (pass->_outputs[outputIndex].name == resource)
                    return { passIndex, outputIndex, pass->_outputs[outputIndex] };
            }
            for (i32 inputIndex = 0; inputIndex < pass->_inputs.size(); inputIndex++) {
                if (pass->_inputs[inputIndex].name == resource)
                    return { passIndex, inputIndex, pass->_inputs[inputIndex] };
            }
        }
        return { -1, -1, {} };
    };

    for (i32 passIndex = 0; passIndex < _orderedPasses.size(); passIndex++) {
        auto& pass = _orderedPasses[passIndex];

        for (i32 outputIndex = 0; outputIndex < pass->_outputs.size(); outputIndex++) {
            auto& currentAccess = pass->_outputs[outputIndex];

            auto [accessPassIndex, accessIndex, nextAccess] = findNextAccess(passIndex, currentAccess.name);
            if (accessPassIndex < 0)
                continue;
            auto& accessPass = _orderedPasses[accessPassIndex];
            if (backend::isReadAccess(nextAccess.access)) {
                accessPass->_invalidate.push({
                    currentAccess.name,
                    accessIndex,
                    currentAccess.stage,
                    nextAccess.stage,
                    currentAccess.access,
                    nextAccess.access,
                    currentAccess.layout,
                    nextAccess.layout
                });
            } else {
                pass->_flush.push({
                    currentAccess.name,
                    accessIndex,
                    currentAccess.stage,
                    nextAccess.stage,
                    currentAccess.access,
                    nextAccess.access,
                    currentAccess.layout,
                    nextAccess.layout
                });
            }
        }
        for (i32 inputIndex = 0; inputIndex < pass->_inputs.size(); inputIndex++) {
            auto& currentAccess = pass->_inputs[inputIndex];

            auto [accessPassIndex, accessIndex, nextAccess] = findNextAccess(passIndex, currentAccess.name);
            if (accessPassIndex < 0)
                continue;
            auto& accessPass = _orderedPasses[accessPassIndex];
            if (backend::isReadAccess(nextAccess.access)) {
                accessPass->_invalidate.push({
                    currentAccess.name,
                    accessIndex,
                    currentAccess.stage,
                    nextAccess.stage,
                    currentAccess.access,
                    nextAccess.access,
                    currentAccess.layout,
                    nextAccess.layout
                });
            } else {
                pass->_flush.push({
                    currentAccess.name,
                    accessIndex,
                    currentAccess.stage,
                    nextAccess.stage,
                    currentAccess.access,
                    nextAccess.access,
                    currentAccess.layout,
                    nextAccess.layout
                });
            }
        }
    }

    for (auto& attachment : _attachmentMap) {
        if (attachment.second.internal) {
            if (auto resource = getResource<ImageResource>(attachment.first); resource) {
                if (resource->layout == backend::ImageLayout::UNDEFINED) {
                    // find first use
                    auto [accessPassIndex, accessIndex, nextAccess] = findNextAccess(-1, attachment.first);
                    auto& accessPass = _orderedPasses[accessPassIndex];
                    accessPass->_invalidate.push({
                        attachment.first,
                        accessIndex,
                        backend::PipelineStage::TOP,
                        nextAccess.stage,
                        backend::Access::NONE,
                        nextAccess.access,
                        backend::ImageLayout::UNDEFINED,
                        nextAccess.layout
                    });
                }
            }
        }
    }

#ifdef 0
    for (i32 passIndex = 0; passIndex < _orderedPasses.size(); passIndex++) {
        auto& pass = _orderedPasses[passIndex];
        _engine->logger().info("Pass: {}", pass->_passName);
        for (auto& barrier : pass->_invalidate) {
            _engine->logger().info("Invalidate: {}, src: {}, dst: {}, src: {}, dst: {}, {} -> {}", barrier.name, backend::pipelineStageToString(barrier.srcStage), backend::pipelineStageToString(barrier.dstStage), backend::accessToString(barrier.srcAccess), backend::accessToString(barrier.dstAccess), backend::imageLayoutToString(barrier.srcLayout), backend::imageLayoutToString(barrier.dstLayout));
        }
        for (auto& barrier : pass->_flush) {
            _engine->logger().info("Flush: {}, src: {}, dst: {}, src: {}, dst: {}, {} -> {}", barrier.name, backend::pipelineStageToString(barrier.srcStage), backend::pipelineStageToString(barrier.dstStage), backend::accessToString(barrier.srcAccess), backend::accessToString(barrier.dstAccess), backend::imageLayoutToString(barrier.srcLayout), backend::imageLayoutToString(barrier.dstLayout));
        }
    }
#endif

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

                auto resource = getResource<ImageResource>(attachment.name);

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
                if (auto resource = getResource<ImageResource>(output.name); resource) {
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

        auto& timer = _timers[_engine->device().frameIndex()][i];
        timer.first = pass->_passName;
        timer.second.start(cmd);

        for (auto& barrier : pass->_invalidate) {
            if (barrier.dstLayout != backend::ImageLayout::UNDEFINED) {
                if (auto resource = getResource<ImageResource>(barrier.name); resource) {
                    auto b = resource->handle->barrier(barrier.srcAccess, barrier.dstAccess, barrier.srcLayout, barrier.dstLayout);
                    cmd.pipelineBarrier(barrier.srcStage, barrier.dstStage, { &b, 1 });
                }
            } else {
                if (auto resource = getResource<BufferResource>(barrier.name); resource) {
                    auto b = resource->handle->barrier(barrier.dstAccess);
                    cmd.pipelineBarrier(barrier.srcStage, barrier.dstStage, { &b, 1 });
                }
            }
        }

        cmd.pushDebugLabel(pass->_passName, pass->_debugColour);
        if (pass->_framebuffer)
            cmd.begin(*pass->_framebuffer);
        if (pass->_executeFunc)
            pass->_executeFunc(cmd, *this);
        if (pass->_framebuffer)
            cmd.end(*pass->_framebuffer);
        cmd.popDebugLabel();

        for (auto& barrier : pass->_flush) {
            if (barrier.dstLayout != backend::ImageLayout::UNDEFINED) {
                if (auto resource = getResource<ImageResource>(barrier.name); resource) {
                    auto b = resource->handle->barrier(barrier.srcAccess, barrier.dstAccess, barrier.srcLayout, barrier.dstLayout);
                    cmd.pipelineBarrier(barrier.srcStage, barrier.dstStage, { &b, 1 });
                }
            } else {
                if (auto resource = getResource<BufferResource>(barrier.name); resource) {
                    auto b = resource->handle->barrier(barrier.dstAccess);
                    cmd.pipelineBarrier(barrier.srcStage, barrier.dstStage, { &b, 1 });
                }
            }
        }


        timer.second.stop();
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