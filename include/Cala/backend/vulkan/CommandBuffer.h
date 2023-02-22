#ifndef CALA_COMMANDBUFFER_H
#define CALA_COMMANDBUFFER_H

#include <vulkan/vulkan.h>

#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/backend/vulkan/RenderPass.h>
#include <Cala/backend/vulkan/Framebuffer.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Image.h>
#include <Cala/backend/vulkan/Sampler.h>

#include <cstring>
#include <Ende/util/hash.h>
#include "../third_party/tsl/robin_map.h"

namespace cala::backend::vulkan {

    class CommandBuffer {
    public:

        CommandBuffer(Device& driver, VkQueue queue, VkCommandBuffer buffer);

        ~CommandBuffer();

        // non copyable
//        CommandBuffer(const CommandBuffer& buf) = delete;
        CommandBuffer& operator=(const CommandBuffer& buf) = delete;


        void setBuffer(VkCommandBuffer buffer) { _buffer = buffer; }

        VkCommandBuffer buffer() const { return _buffer; }


        bool begin();

        bool end();

        void begin(RenderPass& renderPass, VkFramebuffer framebuffer, std::pair<u32, u32> extent);
        void begin(Framebuffer& framebuffer);

        void end(RenderPass& renderPass);
        void end(Framebuffer& framebuffer);




        void bindProgram(const ShaderProgram& program);

        void bindAttributes(ende::Span<Attribute> attributes);

        void bindBindings(ende::Span<VkVertexInputBindingDescription> bindings);

        void bindAttributeDescriptions(ende::Span<VkVertexInputAttributeDescription> attributes);

        void bindVertexArray(ende::Span<VkVertexInputBindingDescription> bindings, ende::Span<VkVertexInputAttributeDescription> attributes);


        void bindRenderPass(RenderPass& renderPass);


        void bindViewPort(const ViewPort& viewport);

        struct RasterState {
            CullMode cullMode = CullMode::BACK;
            FrontFace frontFace = FrontFace::CW;
            PolygonMode polygonMode = PolygonMode::FILL;
            f32 lineWidth = 1.f;
            bool depthClamp = false;
            bool rasterDiscard = false;
            bool depthBias = false;
        };
        void bindRasterState(RasterState state);

        struct DepthState {
            bool test = false;
            bool write = false;
            CompareOp compareOp = CompareOp::LESS;
        };
        void bindDepthState(DepthState state);

        struct BlendState {
            bool blend = false;
            BlendFactor srcFactor = BlendFactor::ONE;
            BlendFactor dstFactor = BlendFactor::ONE;
        };
        void bindBlendState(BlendState state);


        void bindPipeline();


        void bindBuffer(u32 set, u32 binding, Buffer& buffer, u32 offset, u32 range, bool storage = false);
        void bindBuffer(u32 set, u32 binding, Buffer& buffer, bool storage = false);

        void bindImage(u32 set, u32 binding, Image::View& image, Sampler& sampler, bool storage = false);

        void pushConstants(ende::Span<const void> data, u32 offset = 0);

        void setBindlessIndex(u32 index);

        void bindDescriptors();
        void clearDescriptors();

        void bindVertexBuffer(u32 first, VkBuffer buffer, u32 offset = 0);
        void bindVertexBuffers(u32 first, ende::Span<VkBuffer> buffers, ende::Span<VkDeviceSize> offsets);

        void bindVertexBuffer(u32 first, Buffer& buffer, u32 offset);

        void bindIndexBuffer(Buffer& buffer, u32 offset = 0);


        void draw(u32 count, u32 instanceCount, u32 first, u32 firstInstance, bool indexed = true);

        void drawIndirect(Buffer& buffer, u32 offset, u32 drawCount, u32 stride = 0);

        void drawIndirectCount(Buffer& buffer, u32 bufferOffset, Buffer& countBuffer, u32 countOffset, u32 maxDrawCount, u32 stride = sizeof(VkDrawIndexedIndirectCommand));

        void dispatchCompute(u32 x, u32 y, u32 z);


        void pipelineBarrier(PipelineStage srcStage, PipelineStage dstStage, VkDependencyFlags dependencyFlags, ende::Span<VkBufferMemoryBarrier> bufferBarriers, ende::Span<VkImageMemoryBarrier> imageBarriers);

        void pipelineBarrier(PipelineStage srcStage, PipelineStage dstStage, ende::Span<Image::Barrier> imageBarriers);

        void pushDebugLabel(std::string_view label, std::array<f32, 4> colour = {0, 1, 0, 1});

        void popDebugLabel();

        void startPipelineStatistics();

        void stopPipelineStatistics();


        bool submit(ende::Span<VkSemaphore> wait = nullptr, VkFence fence = VK_NULL_HANDLE);

        VkSemaphore signal() const { return _signal; }

        bool active() const { return _active; }


        u32 pipelineCount() const { return _pipelines.size(); }

        u32 descriptorCount() const { return _descriptorSets.size(); }

        u32 drawCalls() const { return _drawCallCount; }

    private:

        VkPipeline getPipeline();

        VkDescriptorSet getDescriptorSet(u32 set);

        Device& _driver;
        VkCommandBuffer _buffer;
        VkSemaphore _signal;
        VkQueue _queue;
        bool _active;

        RenderPass* _renderPass;
        Framebuffer* _framebuffer;
        Buffer* _indexBuffer;
        const ShaderInterface* _boundInterface;
        bool _computeBound;

        struct PipelineKey {
            u32 shaderCount = 0;
            VkPipelineShaderStageCreateInfo shaders[2] = {};
            u32 bindingCount = 0;
            u32 attributeCount = 0;
            VkVertexInputBindingDescription bindings[MAX_VERTEX_INPUT_BINDINGS]{};
            VkVertexInputAttributeDescription attributes[MAX_VERTEX_INPUT_ATTRIBUTES]{};
            VkRenderPass renderPass = VK_NULL_HANDLE;
            VkPipelineLayout layout = VK_NULL_HANDLE;
            ViewPort viewPort = {};
            RasterState raster = {};
            DepthState depth = {};
            BlendState blend = {};
            //TODO: add rest of pipeline state to key
        } _pipelineKey;

        struct PipelineEqual {
            bool operator()(const PipelineKey& lhs, const PipelineKey& rhs) const;
        };

        VkPipeline _currentPipeline;
        tsl::robin_map<PipelineKey, VkPipeline, ende::util::MurmurHash<PipelineKey>, PipelineEqual> _pipelines;

        struct DescriptorKey {
            VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
            struct {
                Buffer* buffer = nullptr;
                u32 offset = 0;
                u32 range = 0;
                bool storage = false;
            } buffers[MAX_BINDING_PER_SET] {};
            struct {
                Image* image = nullptr;
                VkImageView view = VK_NULL_HANDLE;
                VkSampler sampler = VK_NULL_HANDLE;
                bool storage = false;
            } images[MAX_BINDING_PER_SET] {};

            bool operator==(const DescriptorKey& rhs) const {
                return memcmp(this, &rhs, sizeof(DescriptorKey)) == 0;
            }
        } _descriptorKey[MAX_SET_COUNT] {};
        i32 _bindlessIndex;

        //TODO: cull descriptors every now and again
        VkDescriptorSet _currentSets[MAX_SET_COUNT];
        tsl::robin_map<DescriptorKey, VkDescriptorSet, ende::util::MurmurHash<DescriptorKey>> _descriptorSets;
//        robin_hood::unordered_map<DescriptorKey, VkDescriptorSet> _descriptorSets;

        VkDescriptorPool _descriptorPool;
        u32 _drawCallCount;

#ifndef NDEBUG
        ende::Vector<std::string_view> _debugLabels;
#endif

    };

}

#endif //CALA_COMMANDBUFFER_H
