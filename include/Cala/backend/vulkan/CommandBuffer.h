#ifndef CALA_COMMANDBUFFER_H
#define CALA_COMMANDBUFFER_H

#include <volk.h>
#include <Cala/backend/vulkan/Handle.h>
#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/backend/vulkan/RenderPass.h>
#include <Cala/backend/vulkan/Framebuffer.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Image.h>
#include <Cala/backend/vulkan/Sampler.h>
#include <Cala/backend/vulkan/Semaphore.h>
#include <cstring>
#include <Ende/util/hash.h>
#include "../third_party/tsl/robin_map.h"
#include "Ende/math/Vec.h"

namespace cala::backend::vulkan {

    class CommandBuffer {
    public:

        CommandBuffer(Device& driver, VkQueue queue, VkCommandBuffer buffer);

        ~CommandBuffer();

        // non copyable
        CommandBuffer(const CommandBuffer& buf) = delete;
        CommandBuffer& operator=(const CommandBuffer& buf) = delete;

        CommandBuffer(CommandBuffer&& rhs) noexcept;

        CommandBuffer& operator=(CommandBuffer&& rhs) noexcept;


        void setBuffer(VkCommandBuffer buffer) { _buffer = buffer; }

        VkCommandBuffer buffer() const { return _buffer; }


        bool begin();

        bool end();

        void begin(RenderPass& renderPass, VkFramebuffer framebuffer, std::pair<u32, u32> extent);
        void begin(Framebuffer& framebuffer);

        void end(RenderPass& renderPass);
        void end(Framebuffer& framebuffer);




        void bindProgram(const ShaderProgram& program);

        void bindAttributes(std::span<Attribute> attributes);

        void bindBindings(std::span<VkVertexInputBindingDescription> bindings);

        void bindAttributeDescriptions(std::span<VkVertexInputAttributeDescription> attributes);

        void bindVertexArray(std::span<VkVertexInputBindingDescription> bindings, std::span<VkVertexInputAttributeDescription> attributes);


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


        void bindBuffer(u32 set, u32 binding, BufferHandle buffer, u32 offset, u32 range, bool storage = false);
        void bindBuffer(u32 set, u32 binding, BufferHandle buffer, bool storage = false);

        void bindImage(u32 set, u32 binding, Image::View& image, Sampler& sampler, bool storage = false);

        void pushConstants(ShaderStage stage, std::span<const u8> data, u32 offset = 0);

        template <typename T>
        void pushConstants(ShaderStage stage, const T& data, u32 offset = 0) {
            pushConstants(stage, std::span<const u8>(reinterpret_cast<const u8*>(&data), sizeof(data)), offset);
        }

        void bindDescriptors();
        void clearDescriptors();

        void bindVertexBuffer(u32 first, BufferHandle buffer, u32 offset = 0);
        void bindVertexBuffers(u32 first, std::span<VkBuffer> buffers, std::span<VkDeviceSize> offsets);

//        void bindVertexBuffer(u32 first, BufferHandle buffer, u32 offset);

        void bindIndexBuffer(BufferHandle buffer, u32 offset = 0);


        void draw(u32 count, u32 instanceCount, u32 first, u32 firstInstance, bool indexed = true);

        void drawIndirect(BufferHandle buffer, u32 offset, u32 drawCount, u32 stride = 0);

        void drawIndirectCount(BufferHandle buffer, u32 bufferOffset, BufferHandle countBuffer, u32 countOffset, u32 stride = 0);

        void dispatchCompute(u32 x, u32 y, u32 z);


        void clearImage(ImageHandle image, const ende::math::Vec4f& clearValue = { 0.f, 0.f, 0.f, 0.f });

        void clearBuffer(BufferHandle buffer, u32 clearValue = 0);


        void pipelineBarrier(std::span<VkBufferMemoryBarrier2> bufferBarriers, std::span<VkImageMemoryBarrier2> imageBarriers);

        void pipelineBarrier(std::span<Image::Barrier> imageBarriers);

        void pipelineBarrier(std::span<Buffer::Barrier> bufferBarriers);

        struct MemoryBarrier {
            PipelineStage srcStage;
            PipelineStage dstStage;
            Access srcAccess;
            Access dstAccess;
        };

        void pipelineBarrier(std::span<MemoryBarrier> memoryBarriers);

        void pushDebugLabel(std::string_view label, std::array<f32, 4> colour = {0, 1, 0, 1});

        void popDebugLabel();

        void startPipelineStatistics();

        void stopPipelineStatistics();

        struct SemaphoreSubmit {
            Semaphore* semaphore = nullptr;
            u64 value = 0;
        };
        bool submit(std::span<SemaphoreSubmit> waitSemaphores, std::span<SemaphoreSubmit> signalSemaphores, VkFence fence = VK_NULL_HANDLE);

        bool active() const { return _active; }


        u32 drawCalls() const { return _drawCallCount; }

    private:

        void writeBufferMarker(PipelineStage stage, std::string_view cmd);

        friend Device;

        Device* _device;
        VkCommandBuffer _buffer;
        VkQueue _queue;
        bool _active;

        BufferHandle _indexBuffer;
        const ShaderInterface* _boundInterface;

        struct PipelineKey {
            u32 shaderCount = 0;
            VkPipelineShaderStageCreateInfo shaders[4] = {};
            bool compute = false;
            u32 bindingCount = 0;
            u32 attributeCount = 0;
            VkVertexInputBindingDescription bindings[MAX_VERTEX_INPUT_BINDINGS]{};
            VkVertexInputAttributeDescription attributes[MAX_VERTEX_INPUT_ATTRIBUTES]{};
            Framebuffer* framebuffer = nullptr;
            VkPipelineLayout layout = VK_NULL_HANDLE;
            ViewPort viewPort = {};
            RasterState raster = {};
            DepthState depth = {};
            BlendState blend = {};
            //TODO: add rest of pipeline state to key
        } _pipelineKey;

        bool _pipelineDirty;

        struct PipelineEqual {
            bool operator()(const PipelineKey& lhs, const PipelineKey& rhs) const;
        };

        VkPipeline _currentPipeline;

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
                } images[MAX_BINDING_PER_SET];
            ShaderInterface::BindingType type;

            bool operator==(const DescriptorKey& rhs) const {
                return memcmp(this, &rhs, sizeof(DescriptorKey)) == 0;
            }
        } _descriptorKey[MAX_SET_COUNT] {};

        bool _descriptorDirty;

        //TODO: cull descriptors every now and again
        VkDescriptorSet _currentSets[MAX_SET_COUNT];

        u32 _drawCallCount;

#ifndef NDEBUG
        std::vector<std::string_view> _debugLabels;
#endif

    };

}

#endif //CALA_COMMANDBUFFER_H
