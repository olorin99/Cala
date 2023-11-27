#include "Cala/RenderGraph.h"
#include <Ende/profile/profile.h>
#include <Cala/backend/vulkan/primitives.h>


cala::RenderPass::RenderPass(cala::RenderGraph *graph, const char *label)
    : _graph(graph),
    _label(label),
    _depthResource(-1),
    _framebuffer(nullptr),
    _debugColour({ 23.f / 255.f, 87.f / 255.f, 41.f / 255.f, 1.f }),
    _width(0),
    _height(0),
    _type(Type::GRAPHICS)
{}

void cala::RenderPass::setExecuteFunction(std::function<void(backend::vulkan::CommandHandle, RenderGraph & )> func) {
    _function = std::move(func);
}

void cala::RenderPass::setDebugColour(const std::array<f32, 4> &colour) {
    _debugColour = colour;
}

void cala::RenderPass::setDimensions(u32 width, u32 height) {
    _width = width;
    _height = height;
}



void cala::RenderPass::addColourWrite(const char *label) {
    if (auto resource = writes(label, backend::Access::COLOUR_ATTACHMENT_READ | backend::Access::COLOUR_ATTACHMENT_WRITE,
                               backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT,
                               backend::ImageLayout::COLOUR_ATTACHMENT); resource) {
        auto imageResource = dynamic_cast<ImageResource*>(resource);
        imageResource->usage = imageResource->usage | backend::ImageUsage::COLOUR_ATTACHMENT;
        _colourAttachments.push_back(imageResource->index);
    }
}

void cala::RenderPass::addColourRead(const char *label) {
    if (auto resource = reads(label, backend::Access::COLOUR_ATTACHMENT_READ | backend::Access::COLOUR_ATTACHMENT_WRITE,
                               backend::PipelineStage::COLOUR_ATTACHMENT_OUTPUT,
                               backend::ImageLayout::COLOUR_ATTACHMENT); resource) {
        auto imageResource = dynamic_cast<ImageResource*>(resource);
        imageResource->usage = imageResource->usage | backend::ImageUsage::COLOUR_ATTACHMENT;
    }
}

void cala::RenderPass::addDepthWrite(const char *label) {
    if (auto resource = writes(label, backend::Access::DEPTH_STENCIL_READ | backend::Access::DEPTH_STENCIL_WRITE,
                               backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT,
                               backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT); resource) {
        auto imageResource = dynamic_cast<ImageResource*>(resource);
        imageResource->usage = imageResource->usage | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT;
        _depthResource = imageResource->index;
    }
}

void cala::RenderPass::addDepthRead(const char *label) {
    if (auto resource = reads(label, backend::Access::DEPTH_STENCIL_READ | backend::Access::DEPTH_STENCIL_WRITE,
                               backend::PipelineStage::EARLY_FRAGMENT | backend::PipelineStage::LATE_FRAGMENT,
                               backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT); resource) {
        auto imageResource = dynamic_cast<ImageResource*>(resource);
        imageResource->usage = imageResource->usage | backend::ImageUsage::DEPTH_STENCIL_ATTACHMENT;
        _depthResource = imageResource->index;
    }
}

void cala::RenderPass::addIndirectRead(const char *label) {
    if (auto resource = reads(label, backend::Access::INDIRECT_READ,
                              backend::PipelineStage::DRAW_INDIRECT | backend::PipelineStage::VERTEX_INPUT | backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::INDEX_INPUT,
                              backend::ImageLayout::UNDEFINED); resource) {
        auto bufferResource = dynamic_cast<BufferResource*>(resource);
        bufferResource->usage = bufferResource->usage | backend::BufferUsage::INDIRECT;
    }
}

void cala::RenderPass::addVertexRead(const char *label) {
    if (auto resource = reads(label, backend::Access::VERTEX_READ,
                              backend::PipelineStage::VERTEX_INPUT | backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::INDEX_INPUT,
                              backend::ImageLayout::UNDEFINED); resource) {
        auto bufferResource = dynamic_cast<BufferResource*>(resource);
        bufferResource->usage = bufferResource->usage | backend::BufferUsage::VERTEX;
    }
}

void cala::RenderPass::addIndexRead(const char *label) {
    if (auto resource = reads(label, backend::Access::INDEX_READ,
                              backend::PipelineStage::VERTEX_INPUT | backend::PipelineStage::VERTEX_SHADER | backend::PipelineStage::INDEX_INPUT,
                              backend::ImageLayout::UNDEFINED); resource) {
        auto bufferResource = dynamic_cast<BufferResource*>(resource);
        bufferResource->usage = bufferResource->usage | backend::BufferUsage::INDEX;
    }
}

void cala::RenderPass::addStorageImageWrite(const char *label, backend::PipelineStage stage) {
    if (auto resource = writes(label, backend::Access::STORAGE_READ | backend::Access::STORAGE_WRITE,
                               stage,
                               backend::ImageLayout::GENERAL); resource) {
        auto imageResource = dynamic_cast<ImageResource*>(resource);
        imageResource->usage = imageResource->usage | backend::ImageUsage::STORAGE;
    }
}

void cala::RenderPass::addStorageImageRead(const char *label, backend::PipelineStage stage) {
    if (auto resource = reads(label, backend::Access::STORAGE_READ | backend::Access::STORAGE_WRITE,
                               stage,
                               backend::ImageLayout::GENERAL); resource) {
        auto imageResource = dynamic_cast<ImageResource*>(resource);
        imageResource->usage = imageResource->usage | backend::ImageUsage::STORAGE;
    }
}

void cala::RenderPass::addStorageBufferWrite(const char *label, backend::PipelineStage stage) {
    if (auto resource = writes(label, backend::Access::STORAGE_READ | backend::Access::STORAGE_WRITE,
                              stage,
                              backend::ImageLayout::UNDEFINED); resource) {
        auto bufferResource = dynamic_cast<BufferResource*>(resource);
        bufferResource->usage = bufferResource->usage | backend::BufferUsage::STORAGE;
    }
}

void cala::RenderPass::addStorageBufferRead(const char *label, backend::PipelineStage stage) {
    if (auto resource = reads(label, backend::Access::STORAGE_READ,
                              stage,
                              backend::ImageLayout::UNDEFINED); resource) {
        auto bufferResource = dynamic_cast<BufferResource*>(resource);
        bufferResource->usage = bufferResource->usage | backend::BufferUsage::STORAGE;
    }
}

void cala::RenderPass::addUniformBufferRead(const char *label, backend::PipelineStage stage) {
    if (auto resource = reads(label, backend::Access::UNIFORM_READ,
                              stage,
                              backend::ImageLayout::UNDEFINED); resource) {
        auto bufferResource = dynamic_cast<BufferResource*>(resource);
        bufferResource->usage = bufferResource->usage | backend::BufferUsage::UNIFORM;
    }
}

void cala::RenderPass::addSampledImageRead(const char *label, backend::PipelineStage stage) {
    if (auto resource = reads(label, backend::Access::SAMPLED_READ,
                               stage,
                               backend::ImageLayout::SHADER_READ_ONLY); resource) {
        auto imageResource = dynamic_cast<ImageResource*>(resource);
        imageResource->usage = imageResource->usage | backend::ImageUsage::SAMPLED;
    }
}

void cala::RenderPass::addBlitWrite(const char *label) {
    if (auto resource = writes(label, backend::Access::TRANSFER_WRITE,
                               backend::PipelineStage::BLIT,
                               backend::ImageLayout::TRANSFER_DST); resource) {
        auto imageResource = dynamic_cast<ImageResource*>(resource);
        imageResource->usage = imageResource->usage | backend::ImageUsage::TRANSFER_DST;
    }
}

void cala::RenderPass::addBlitRead(const char *label) {
    if (auto resource = reads(label, backend::Access::TRANSFER_READ,
                             backend::PipelineStage::BLIT,
                             backend::ImageLayout::TRANSFER_SRC); resource) {
        auto imageResource = dynamic_cast<ImageResource*>(resource);
        imageResource->usage = imageResource->usage | backend::ImageUsage::TRANSFER_SRC;
    }
}

void cala::RenderPass::addTransferWrite(const char *label) {
    if (auto resource = writes(label, backend::Access::TRANSFER_WRITE,
                               backend::PipelineStage::TRANSFER,
                               backend::ImageLayout::TRANSFER_DST); resource) {
        auto imageResource = dynamic_cast<ImageResource*>(resource);
        imageResource->usage = imageResource->usage | backend::ImageUsage::TRANSFER_DST;
    }
}

void cala::RenderPass::addTransferRead(const char *label) {
    if (auto resource = reads(label, backend::Access::TRANSFER_READ,
                              backend::PipelineStage::TRANSFER,
                              backend::ImageLayout::TRANSFER_SRC); resource) {
        auto imageResource = dynamic_cast<ImageResource*>(resource);
        imageResource->usage = imageResource->usage | backend::ImageUsage::TRANSFER_SRC;
    }
}

cala::Resource *cala::RenderPass::reads(const char *label, backend::Access access, backend::PipelineStage stage, backend::ImageLayout layout) {
    auto it = _graph->_labelToIndex.find(label);
    if (it != _graph->_labelToIndex.end()) {
        _inputs.emplace_back(label, it->second, access, stage, layout);
        _graph->_resources[it->second]->index = it->second;
        return _graph->_resources[it->second].get();
    }
    return nullptr;
}

cala::Resource *cala::RenderPass::writes(const char *label, backend::Access access, backend::PipelineStage stage, backend::ImageLayout layout) {
    auto it = _graph->_labelToIndex.find(label);
    if (it != _graph->_labelToIndex.end()) {
        _outputs.emplace_back(label, it->second, access, stage, layout);
        _graph->_resources[it->second]->index = it->second;
        return _graph->_resources[it->second].get();
    }
    return nullptr;
}


cala::RenderGraph::RenderGraph(cala::Engine *engine)
    : _engine(engine)
{}

cala::RenderPass &cala::RenderGraph::addPass(const char *label, RenderPass::Type type) {
    u32 index = _passes.size();
    while (_timers[_engine->device().frameIndex()].size() <= index) {
        _timers[_engine->device().frameIndex()].emplace_back(std::make_pair("", backend::vulkan::Timer(_engine->device())));
    }
    _passes.emplace_back(this, label);
    _passes.back()._type = type;
    _timers[_engine->device().frameIndex()][index].first = label;
    assert(_passes.size() <= _timers[_engine->device().frameIndex()].size());
    return _passes.back();
}

void cala::RenderGraph::setBackbuffer(const char *label) {
    _backbuffer = label;
}

void cala::RenderGraph::setBackbufferDimensions(u32 width, u32 height) {
    _backbufferWidth = width;
    _backbufferHeight = height;
}


u32 cala::RenderGraph::addImageResource(const char *label, cala::ImageResource resource, cala::backend::vulkan::ImageHandle handle) {
    u32 index = 0;
    resource.label = label;
    auto it = _labelToIndex.find(label);
    if (it != _labelToIndex.end()) {
        index = it->second;
        if (_resources.size() <= index)
            _resources.resize(index + 1);
        _resources[index] = std::make_unique<ImageResource>(std::move(resource));
        if (_images.size() <= index + 1)
            _images.resize(index + 1);
        if (handle)
            _images[index] = handle;
    } else {
        _resources.push_back(std::make_unique<ImageResource>(std::move(resource)));
        if (_images.size() < _resources.size())
            _images.resize(_resources.size());
        index = _resources.size() - 1;
        if (handle)
            _images[index] = handle;
        assert(_resources.size() == _images.size());
        _labelToIndex[label] = index;
    }
    return index;
}

u32 cala::RenderGraph::addBufferResource(const char *label, cala::BufferResource resource, cala::backend::vulkan::BufferHandle handle) {
    u32 index = 0;
    resource.label = label;
    auto it = _labelToIndex.find(label);
    if (it != _labelToIndex.end()) {
        index = it->second;
        if (_resources.size() <= index)
            _resources.resize(index + 1);
        _resources[index] = std::make_unique<BufferResource>(std::move(resource));
        if (_buffers.size() <= index)
            _buffers.resize(index + 1);
        if (handle)
            _buffers[index] = handle;
    } else {
        _resources.push_back(std::make_unique<BufferResource>(std::move(resource)));
        if (_buffers.size() < _resources.size())
            _buffers.resize(_resources.size());
        index = _resources.size() - 1;
        if (handle)
            _buffers[index] = handle;
        assert(_resources.size() == _buffers.size());
        _labelToIndex[label] = index;
    }
    return index;
}

u32 cala::RenderGraph::addAlias(const char *label, const char *alias) {
    auto it = _labelToIndex.find(label);
    if (it != _labelToIndex.end()) {
        return addAlias(it->second, alias);
    }
    return 0;
}

u32 cala::RenderGraph::addAlias(u32 resourceIndex, const char *alias) {
    assert(resourceIndex <= _resources.size());
    _labelToIndex[alias] = resourceIndex;
    if (_aliases.size() <= resourceIndex)
        _aliases.resize(resourceIndex + 1);
    _aliases[resourceIndex].push_back(alias);
    return resourceIndex;
}

cala::ImageResource *cala::RenderGraph::getImageResource(const char *label) {
    auto it = _labelToIndex.find(label);
    if (it != _labelToIndex.end()) {
        return getImageResource(it->second);
    }
    return nullptr;
}

cala::ImageResource *cala::RenderGraph::getImageResource(u32 resourceIndex) {
    assert(resourceIndex <= _resources.size());
    return dynamic_cast<ImageResource*>(_resources[resourceIndex].get());
}

cala::BufferResource *cala::RenderGraph::getBufferResource(const char *label) {
    auto it = _labelToIndex.find(label);
    if (it != _labelToIndex.end()) {
        return getBufferResource(it->second);
    }
    return nullptr;
}

cala::BufferResource *cala::RenderGraph::getBufferResource(u32 resourceIndex) {
    assert(resourceIndex <= _resources.size());
    return dynamic_cast<BufferResource*>(_resources[resourceIndex].get());
}

cala::backend::vulkan::ImageHandle cala::RenderGraph::getImage(const char *label) {
    auto it = _labelToIndex.find(label);
    if (it != _labelToIndex.end()) {
        return getImage(it->second);
    }
    return {};
}

cala::backend::vulkan::ImageHandle cala::RenderGraph::getImage(u32 resourceIndex) {
    assert(resourceIndex <= _resources.size());
    return _images[resourceIndex];
}

cala::backend::vulkan::BufferHandle cala::RenderGraph::getBuffer(const char *label) {
    auto it = _labelToIndex.find(label);
    if (it != _labelToIndex.end()) {
        return getBuffer(it->second);
    }
    return {};
}

cala::backend::vulkan::BufferHandle cala::RenderGraph::getBuffer(u32 resourceIndex) {
    assert(resourceIndex <= _resources.size());
    return _buffers[resourceIndex];
}

bool cala::RenderGraph::compile() {
    PROFILE_NAMED("RenderGraph::compile");
    _orderedPasses.clear();

    tsl::robin_map<const char*, std::vector<u32>> outputs;
    for (u32 i = 0; i < _passes.size(); i++) {
        auto& pass = _passes[i];
        for (auto& output : pass._outputs)
            outputs[output.label].push_back(i);
    }

    std::vector<bool> visited(_passes.size(), false);
    std::vector<bool> onStack(_passes.size(), false);

    std::function<bool(u32)> dfs = [&](u32 index) -> bool {
        visited[index] = true;
        onStack[index] = true;
        auto& pass = _passes[index];
        for (auto& input : pass._inputs) {
            for (auto& output : outputs[input.label]) {
                if (visited[output] && onStack[output])
                    return false;
                if (!visited[output]) {
                    if (!dfs(output))
                        return false;
                }
            }
        }
        _orderedPasses.push_back(&_passes[index]);
        onStack[index] = false;
        return true;
    };

    for (auto& pass : outputs[_backbuffer]) {
        if (!dfs(pass))
            return false;
    }

    buildBarriers();

    buildResources();

    buildRenderPasses();

//    log();

    return true;
}

bool cala::RenderGraph::execute(backend::vulkan::CommandHandle cmd) {
    PROFILE_NAMED("RenderGraph::execute");

    for (u32 i = 0; i < _orderedPasses.size(); i++) {
        auto& pass = _orderedPasses[i];

        auto& timer = _timers[_engine->device().frameIndex()][i];
        timer.first = pass->_label;
        timer.second.start(cmd);
        cmd->pushDebugLabel(pass->_label, pass->_debugColour);

        u32 imageBarrierCount = 0;
        backend::vulkan::Image::Barrier imageBarriers[pass->_barriers.size()];
        u32 bufferBarrierCount = 0;
        backend::vulkan::Buffer::Barrier bufferBarriers[pass->_barriers.size()];

        for (u32 barrierIndex = 0; barrierIndex < pass->_barriers.size(); barrierIndex++) {
            auto& barrier = pass->_barriers[barrierIndex];
            if (barrier.dstLayout != backend::ImageLayout::UNDEFINED) {
                auto image = getImage(barrier.label);
                imageBarriers[imageBarrierCount++] = image->barrier(barrier.srcStage, barrier.dstStage, barrier.srcAccess, barrier.dstAccess, barrier.srcLayout, barrier.dstLayout);
            } else {
                auto buffer = getBuffer(barrier.label);
                bufferBarriers[bufferBarrierCount++] = buffer->barrier(barrier.srcStage, barrier.dstStage, barrier.srcAccess, barrier.dstAccess);
            }
        }
        cmd->pipelineBarrier({ imageBarriers, imageBarrierCount });
        cmd->pipelineBarrier({ bufferBarriers, bufferBarrierCount });

//        for (auto& barrier : pass->_barriers) {
//            if (barrier.dstLayout != backend::ImageLayout::UNDEFINED) {
//                auto image = getImage(barrier.label);
//                auto b = image->barrier(barrier.srcStage, barrier.dstStage, barrier.srcAccess, barrier.dstAccess, barrier.srcLayout, barrier.dstLayout);
//                cmd->pipelineBarrier({ &b, 1 });
//            } else {
//                auto buffer = getBuffer(barrier.label);
//                auto b = buffer->barrier(barrier.srcStage, barrier.dstStage, barrier.srcAccess, barrier.dstAccess);
//                cmd->pipelineBarrier({ &b, 1 });
//            }
//        }

        if (pass->_type == RenderPass::Type::GRAPHICS && pass->_framebuffer)
            cmd->begin(*pass->_framebuffer);
        if (pass->_function)
            pass->_function(cmd, *this);
        if (pass->_type == RenderPass::Type::GRAPHICS && pass->_framebuffer)
            cmd->end(*pass->_framebuffer);

        cmd->popDebugLabel();
        timer.second.stop();
    }
    return true;
}

void cala::RenderGraph::reset() {
//    _resources.clear();
    _passes.clear();
    _aliases.clear();
}


void cala::RenderGraph::buildResources() {
    if (_images.size() < _resources.size())
        _images.resize(_resources.size());
    if (_images.size() < _resources.size())
        _buffers.resize(_resources.size());

    for (u32 i = 0; i < _resources.size(); i++) {
        auto& resource = _resources[i];
        if (auto imageResource = dynamic_cast<ImageResource*>(resource.get()); imageResource) {
            if (imageResource->matchSwapchain) {
                imageResource->width = _backbufferWidth;
                imageResource->height = _backbufferHeight;
                imageResource->depth = 1;
            }
            backend::vulkan::ImageHandle image = _images[i];
            if (!image || image->usage() != imageResource->usage ||
                    image->width() != imageResource->width ||
                    image->height() != imageResource->height ||
                    image->depth() != imageResource->depth ||
                    image->mips() != imageResource->mipLevels ||
                    image->format() != imageResource->format) {

                _images[i] = _engine->device().createImage({
                    imageResource->width,
                    imageResource->height,
                    imageResource->depth,
                    imageResource->format,
                    imageResource->mipLevels,
                    1,
                    imageResource->usage
                });
                _engine->device().context().setDebugName(VK_OBJECT_TYPE_IMAGE, (u64)_images[i]->image(), imageResource->label);
            }
        } else if (auto bufferResource = dynamic_cast<BufferResource*>(resource.get()); bufferResource) {
            if (!_buffers[i]) {
                _buffers[i] = _engine->device().createBuffer(bufferResource->size, bufferResource->usage);
                _engine->device().context().setDebugName(VK_OBJECT_TYPE_BUFFER, (u64)_buffers[i]->buffer(), bufferResource->label);
            }
        }
    }
}

void cala::RenderGraph::buildBarriers() {
    auto findNextAccess = [&](i32 startIndex, u32 resource) -> std::tuple<i32, i32, RenderPass::ResourceAccess> {
        for (i32 passIndex = startIndex + 1; passIndex < _orderedPasses.size(); passIndex++) {
            auto& pass = _orderedPasses[passIndex];
            for (i32 outputIndex = 0; outputIndex < pass->_outputs.size(); outputIndex++) {
                if (pass->_outputs[outputIndex].index == resource)
                    return { passIndex, outputIndex, pass->_outputs[outputIndex] };
            }
            for (i32 inputIndex = 0; inputIndex < pass->_inputs.size(); inputIndex++) {
                if (pass->_inputs[inputIndex].index == resource)
                    return { passIndex, inputIndex, pass->_inputs[inputIndex] };
            }
        }
        return { -1, -1, {} };
    };

    auto readsResource = [&](RenderPass& pass, u32 resource) -> bool {
        for (auto& access : pass._inputs) {
            if (access.index == resource)
                return true;
        }
        return false;
    };

    auto writesResource = [&](RenderPass& pass, u32 resource) -> bool {
        for (auto& access : pass._outputs) {
            if (access.index == resource)
                return true;
        }
        return false;
    };

    // for each pass for each input/output find next pass that access that resource and add barrier to next pass
    for (i32 passIndex = 0; passIndex < _orderedPasses.size(); passIndex++) {
        auto& pass = _orderedPasses[passIndex];

        for (i32 outputIndex = 0; outputIndex < pass->_outputs.size(); outputIndex++) {
            auto& currentAccess = pass->_outputs[outputIndex];
            auto [accessPassIndex, accessIndex, nextAccess] = findNextAccess(passIndex, currentAccess.index);
            if (accessPassIndex < 0)
                continue;
            auto& accessPass = _orderedPasses[accessPassIndex];
            accessPass->_barriers.push_back({
                currentAccess.label,
                nextAccess.index,
                currentAccess.stage,
                nextAccess.stage,
                currentAccess.access,
                nextAccess.access,
                currentAccess.layout,
                nextAccess.layout
            });
        }
        for (i32 inputIndex = 0; inputIndex < pass->_inputs.size(); inputIndex++) {
            auto& currentAccess = pass->_inputs[inputIndex];
            if (writesResource(*pass, currentAccess.index))
                continue;

            auto [accessPassIndex, accessIndex, nextAccess] = findNextAccess(passIndex, currentAccess.index);
            if (accessPassIndex < 0)
                continue;
            auto& accessPass = _orderedPasses[accessPassIndex];
            accessPass->_barriers.push_back({
                currentAccess.label,
                nextAccess.index,
                currentAccess.stage,
                nextAccess.stage,
                currentAccess.access,
                nextAccess.access,
                currentAccess.layout,
                nextAccess.layout
            });
        }
    }

    // find first access of resources
    for (u32 i = 0; i < _resources.size(); i++) {
        auto [accessPassIndex, accessIndex, nextAccess] = findNextAccess(-1, i);
        if (accessPassIndex < 0)
            continue;
        auto& resource = _resources[i];
        auto& accessPass = _orderedPasses[accessPassIndex];
        accessPass->_barriers.push_back({
            resource->label,
            nextAccess.index,
            backend::PipelineStage::TOP,
            nextAccess.stage,
            backend::Access::MEMORY_READ | backend::Access::MEMORY_WRITE,
            nextAccess.access,
            backend::ImageLayout::UNDEFINED,
            nextAccess.layout
        });
    }

}

void cala::RenderGraph::buildRenderPasses() {
    auto findNextAccess = [&](i32 startIndex, u32 resource) -> std::tuple<i32, i32, RenderPass::ResourceAccess> {
        for (i32 passIndex = startIndex + 1; passIndex < _orderedPasses.size(); passIndex++) {
            auto& pass = _orderedPasses[passIndex];
            for (i32 outputIndex = 0; outputIndex < pass->_outputs.size(); outputIndex++) {
                if (pass->_outputs[outputIndex].index == resource)
                    return { passIndex, outputIndex, pass->_outputs[outputIndex] };
            }
            for (i32 inputIndex = 0; inputIndex < pass->_inputs.size(); inputIndex++) {
                if (pass->_inputs[inputIndex].index == resource)
                    return { passIndex, inputIndex, pass->_inputs[inputIndex] };
            }
        }
        return { -1, -1, {} };
    };

    auto findCurrAccess = [&](RenderPass& pass, u32 resource) -> std::tuple<bool, RenderPass::ResourceAccess> {
        for (auto& input : pass._inputs) {
            if (input.index == resource)
                return { true, input };
        }
        for (auto& output : pass._outputs) {
            if (output.index == resource)
                return { false, output };
        }
        return { false, {} };
    };

    auto findPrevAccess = [&](i32 startIndex, u32 resource) -> std::tuple<i32, i32, RenderPass::ResourceAccess> {
        for (i32 passIndex = startIndex; passIndex > -1; passIndex--) {
            auto& pass = _orderedPasses[passIndex];
            for (i32 outputIndex = 0; outputIndex < pass->_outputs.size(); outputIndex++) {
                if (pass->_outputs[outputIndex].index == resource)
                    return { passIndex, outputIndex, pass->_outputs[outputIndex] };
            }
            for (i32 inputIndex = 0; inputIndex < pass->_inputs.size(); inputIndex++) {
                if (pass->_inputs[inputIndex].index == resource)
                    return { passIndex, inputIndex, pass->_inputs[inputIndex] };
            }
        }
        return { -1, -1, {} };
    };



    for (i32 i = 0; i < _orderedPasses.size(); i++) {
        auto& pass = _orderedPasses[i];

        if (pass->_type != RenderPass::Type::GRAPHICS)
            continue;

        std::vector<backend::vulkan::RenderPass::Attachment> attachments;
        std::vector<VkImageView> attachmentImages;
        std::vector<u32> hashes;

        u32 width = pass->_width, height = pass->_height;

        for (auto& imageIndex : pass->_colourAttachments) {
            auto image = dynamic_cast<ImageResource*>(_resources[imageIndex].get());

            auto [prevAccessPassIndex, prevAccessIndex, prevAccess] = findPrevAccess(i - 1, image->index);
            auto [read, access] = findCurrAccess(*pass, image->index);
            auto [nextAccessPassIndex, nextAccessIndex, nextAccess] = findNextAccess(i, image->index);


            backend::vulkan::RenderPass::Attachment attachment{};
            attachment.format = image->format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            // if first access clear otherwise load
            attachment.loadOp = prevAccessPassIndex > -1 ? backend::LoadOp::LOAD : backend::LoadOp::CLEAR;
            // if last access none otherwise store
            attachment.storeOp = nextAccessPassIndex > -1 ? backend::StoreOp::STORE : backend::StoreOp::NONE;
            attachment.stencilLoadOp = backend::LoadOp::DONT_CARE;
            attachment.stencilStoreOp = backend::StoreOp::DONT_CARE;
            attachment.initialLayout = access.layout;
            attachment.finalLayout = access.layout;
            attachment.internalLayout = access.layout;

            width = std::max(width, image->width);
            height = std::max(height, image->height);


            attachments.push_back(attachment);
            attachmentImages.push_back(_engine->device().getImageView(_images[image->index].index()).view);
            hashes.push_back(_images[image->index].index());
        }

        if (pass->_depthResource > -1) {
            auto depthResource = dynamic_cast<ImageResource*>(_resources[pass->_depthResource].get());

            auto [prevAccessPassIndex, prevAccessIndex, prevAccess] = findPrevAccess(i - 1, depthResource->index);
            auto [read, access] = findCurrAccess(*pass, depthResource->index);
            auto [nextAccessPassIndex, nextAccessIndex, nextAccess] = findNextAccess(i, depthResource->index);

            backend::vulkan::RenderPass::Attachment attachment{};
            attachment.format = depthResource->format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = prevAccessPassIndex > -1 ? backend::LoadOp::LOAD : backend::LoadOp::CLEAR;
            attachment.storeOp = nextAccessPassIndex > -1 ? backend::StoreOp::STORE : backend::StoreOp::NONE;
            attachment.stencilLoadOp = backend::LoadOp::DONT_CARE;
            attachment.stencilStoreOp = backend::StoreOp::DONT_CARE;
            attachment.initialLayout = access.layout;
            attachment.finalLayout = access.layout;
            attachment.internalLayout = access.layout;

            width = std::max(width, depthResource->width);
            height = std::max(height, depthResource->height);

            attachments.push_back(attachment);
            attachmentImages.push_back(_engine->device().getImageView(_images[depthResource->index].index()).view);
            hashes.push_back(_images[depthResource->index].index());
        }

        auto renderPass = _engine->device().getRenderPass(attachments);
        auto framebuffer = _engine->device().getFramebuffer(renderPass, attachmentImages, hashes, width, height);

        pass->_framebuffer = framebuffer;
    }
}

void cala::RenderGraph::log() {
    // log barriers
    _engine->logger().info("RenderGraph");

    _engine->logger().info("Barriers");
    for (auto& pass : _orderedPasses) {
        _engine->logger().info("Pass: {}", pass->_label);
        for (auto& barrier : pass->_barriers) {
            _engine->logger().info("\t{}: {} -> {}, {} -> {}, {} -> {}", barrier.label,
                                   backend::pipelineStageToString(barrier.srcStage), backend::pipelineStageToString(barrier.dstStage),
                                   backend::accessToString(barrier.srcAccess), backend::accessToString(barrier.dstAccess),
                                   backend::imageLayoutToString(barrier.srcLayout), backend::imageLayoutToString(barrier.dstLayout));

        }
    }
    for (auto& pass : _orderedPasses) {
        _engine->logger().info("Pass: {}", pass->_label);
        if (pass->_type == RenderPass::Type::GRAPHICS) {
            _engine->logger().info("\tCompute pass");
            continue;
        }
        for (auto& attachment : pass->_framebuffer->renderPass().attachments()) {
            _engine->logger().info("\n\tformat: {}\n\tloadOp: {}\n\tstoreOp: {}\n\tinitialLayout: {}\n\tfinalLayout: {}\n\tinternalLayout: {}",
                                   backend::formatToString(attachment.format),
                                   backend::loadOpToString(attachment.loadOp), backend::storeOpToString(attachment.storeOp),
                                   backend::imageLayoutToString(attachment.initialLayout),
                                   backend::imageLayoutToString(attachment.finalLayout),
                                   backend::imageLayoutToString(attachment.internalLayout));
        }
    }

    for (u32 i = 0; i < _images.size(); i++) {
        auto& image = _images[i];
        if (image) {
            _engine->logger().info("\n\tName: {}\n\tformat: {}\n\textent: ({}, {}, {})\n\tsize: {}", _resources[i]->label, backend::formatToString(image->format()), image->width(), image->height(), image->depth(), image->size());
        }
    }

    for (u32 i = 0; i < _buffers.size(); i++) {
        auto& buffer = _buffers[i];
        if (buffer) {
            _engine->logger().info("\n\tName: {}\n\tsize: {}", _resources[i]->label, buffer->size());
        }
    }
}