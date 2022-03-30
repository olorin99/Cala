#ifndef CALA_COMMANDBUFFER_H
#define CALA_COMMANDBUFFER_H

#include <vulkan/vulkan.h>

#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/backend/vulkan/RenderPass.h>
#include <Cala/backend/vulkan/Framebuffer.h>
#include <Cala/backend/vulkan/Buffer.h>
#include <Cala/backend/vulkan/Image.h>
#include <Cala/backend/vulkan/Sampler.h>

#include <unordered_map>
#include <cstring>
#include <Ende/util/hash.h>

namespace cala::backend::vulkan {

    class CommandBuffer {
    public:

        CommandBuffer(VkDevice device, VkQueue queue, VkCommandBuffer buffer);

        ~CommandBuffer();

        // non copyable
//        CommandBuffer(const CommandBuffer& buf) = delete;
        CommandBuffer& operator=(const CommandBuffer& buf) = delete;


        VkCommandBuffer buffer() const { return _buffer; }


        bool begin();

        bool end();

        void begin(RenderPass& renderPass, VkFramebuffer framebuffer, std::pair<u32, u32> extent);
        void begin(Framebuffer& framebuffer);

        void end(RenderPass& renderPass);
        void end(Framebuffer& framebuffer);




        void bindProgram(ShaderProgram& program);

        void bindAttributes(ende::Span<Attribute> attributes);

        void bindBindings(ende::Span<VkVertexInputBindingDescription> bindings);

        void bindAttributeDescriptions(ende::Span<VkVertexInputAttributeDescription> attributes);

        void bindVertexArray(ende::Span<VkVertexInputBindingDescription> bindings, ende::Span<VkVertexInputAttributeDescription> attributes);


        void bindRenderPass(RenderPass& renderPass);


        void bindViewPort(const ViewPort& viewport);

        struct RasterState {
            CullMode cullMode = CullMode::BACK;
            FrontFace frontFace = FrontFace::CCW;
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


        void bindPipeline();


        void bindBuffer(u32 set, u32 slot, VkBuffer buffer, u32 offset, u32 range);
        void bindBuffer(u32 set, u32 slot, Buffer& buffer, u32 offset = 0, u32 range = 0);

        void bindImage(u32 set, u32 slot, Image::View& image, Sampler& sampler, bool storage = false);

        void bindDescriptors();
        void clearDescriptors();

        void bindVertexBuffer(u32 first, VkBuffer buffer, u32 offset = 0);
        void bindVertexBuffers(u32 first, ende::Span<VkBuffer> buffers, ende::Span<VkDeviceSize> offsets);

        void bindVertexBuffer(u32 first, Buffer& buffer, u32 offset);

        void bindIndexBuffer(Buffer& buffer, u32 offset = 0);


        void draw(u32 count, u32 instanceCount, u32 first, u32 firstInstance);

        void dispatchCompute(u32 x, u32 y, u32 z);


        void pipelineBarrier(PipelineStage srcStage, PipelineStage dstStage, VkDependencyFlags dependencyFlags, ende::Span<VkImageMemoryBarrier> imageBarriers);



        bool submit(VkSemaphore wait = VK_NULL_HANDLE, VkFence fence = VK_NULL_HANDLE);

        VkSemaphore signal() const { return _signal; }

        bool active() const { return _active; }

//    private:

        VkPipeline getPipeline();

        VkDescriptorSet getDescriptorSet(u32 set);


        VkCommandBuffer _buffer;
        VkSemaphore _signal;
        VkDevice _device;
        VkQueue _queue;
        bool _active;

        RenderPass* _renderPass;
        Framebuffer* _framebuffer;
        Buffer* _indexBuffer;
        bool _computeBound;

        struct PipelineKey {
            VkPipelineShaderStageCreateInfo shaders[2] = {};
            VkVertexInputBindingDescription bindings[MAX_VERTEX_INPUT_BINDINGS]{};
            VkVertexInputAttributeDescription attributes[MAX_VERTEX_INPUT_ATTRIBUTES]{};
            VkRenderPass renderPass = VK_NULL_HANDLE;
            VkPipelineLayout layout = VK_NULL_HANDLE;
            ViewPort viewPort = {};
            RasterState raster = {};
            DepthState depth = {};
            //TODO: add rest of pipeline state to key

            bool operator==(const PipelineKey& rhs) const {
                return memcmp(this, &rhs, sizeof(PipelineKey)) == 0;
            }
        } _pipelineKey;

        VkPipeline _currentPipeline;
        std::unordered_map<PipelineKey, VkPipeline, ende::util::MurmurHash<PipelineKey>> _pipelines;

//        VkDescriptorSetLayout setLayout[4] {};
        struct /*__attribute__((packed))*/ DescriptorKey {
            VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
            struct {
                VkBuffer buffer = VK_NULL_HANDLE;
                u32 offset = 0;
                u32 range = 0;
            } buffers[MAX_BINDING_PER_SET] {};
            struct {
                VkImageView image = VK_NULL_HANDLE;
                VkSampler sampler = VK_NULL_HANDLE;
                u32 flags = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            } images[MAX_BINDING_PER_SET] {};

            bool operator==(const DescriptorKey& rhs) const {
                return memcmp(this, &rhs, sizeof(DescriptorKey)) == 0;
            }
        } _descriptorKey[MAX_SET_COUNT] {};

        //TODO: cull descriptors every now and again
        VkDescriptorSet _currentSets[MAX_SET_COUNT];
        std::unordered_map<DescriptorKey, VkDescriptorSet, ende::util::MurmurHash<DescriptorKey>> _descriptorSets;

        VkDescriptorPool _descriptorPool;

    };

}

#endif //CALA_COMMANDBUFFER_H
