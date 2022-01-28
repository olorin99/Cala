#ifndef CALA_COMMANDBUFFER_H
#define CALA_COMMANDBUFFER_H

#include <vulkan/vulkan.h>

#include <Cala/backend/vulkan/ShaderProgram.h>
#include <Cala/backend/vulkan/RenderPass.h>
#include <Cala/backend/vulkan/Framebuffer.h>
#include <Cala/backend/vulkan/Buffer.h>

#include <unordered_map>
#include <cstring>
#include <Ende/util/hash.h>

namespace cala::backend::vulkan {

    constexpr u32 SET_COUNT = 4;

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

        void bindVertexArray(ende::Span<VkVertexInputBindingDescription> bindings, ende::Span<VkVertexInputAttributeDescription> attributes);


        void bindRenderPass(const RenderPass& renderPass);

        struct RasterState {
            bool depthClamp = false;
            bool rasterDiscard = false;
            VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
            f32 lineWidth = 1.f;
            u32 cullMode = VK_CULL_MODE_BACK_BIT;
            VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;
            bool depthBias = false;
        };
        void bindRasterState(RasterState state);

        struct DepthState {
            bool test = false;
            bool write = false;
            VkCompareOp compareOp = VK_COMPARE_OP_LESS;
        };
        void bindDepthState(DepthState state);


        void bindPipeline();


        void bindBuffer(u32 set, u32 slot, VkBuffer buffer, u32 offset, u32 range);
        void bindBuffer(u32 set, u32 slot, Buffer& buffer, u32 offset = 0, u32 range = 0);


        void bindDescriptors();


        void bindVertexBuffer(u32 first, VkBuffer buffer, u32 offset = 0);
        void bindVertexBuffers(u32 first, ende::Span<VkBuffer> buffers, ende::Span<VkDeviceSize> offsets);

        void bindIndexBuffer(Buffer& buffer, u32 offset = 0);


        void draw(u32 count, u32 instanceCount, u32 first, u32 firstInstance);



        bool submit(VkSemaphore wait = VK_NULL_HANDLE, VkFence fence = VK_NULL_HANDLE);

        VkSemaphore signal() const { return _signal; }

        bool active() const { return _active; }

//    private:

        // true if requires binding
        VkPipeline getPipeline();

        VkDescriptorSet getDescriptorSet(u32 set);


        VkCommandBuffer _buffer;
        VkSemaphore _signal;
        VkDevice _device;
        VkQueue _queue;
        bool _active;

        Buffer* _indexBuffer;

        struct PipelineKey {
            VkShaderModule shaders[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
            VkVertexInputBindingDescription bindings[10]{}; //TODO: change from arbitrary values
            VkVertexInputAttributeDescription attributes[10]{};
            VkRenderPass renderPass = VK_NULL_HANDLE;
            VkPipelineLayout layout = VK_NULL_HANDLE;
            RasterState raster = {};
            DepthState depth = {};
            //TODO: add rest of pipeline state to key

            bool operator==(const PipelineKey& rhs) const {
                return memcmp(this, &rhs, sizeof(PipelineKey)) == 0;
            }
        } _pipelineKey;

        VkPipeline _currentPipeline;
        std::unordered_map<PipelineKey, VkPipeline, ende::util::MurmurHash<PipelineKey>> _pipelines;

        VkDescriptorSetLayout setLayout[4] {};
        struct __attribute__((packed)) DescriptorKey {
//            VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;

            struct {
                VkBuffer buffer = VK_NULL_HANDLE;
                u32 offset = 0;
                u32 range = 0;
            } buffers[4] {};

            bool operator==(const DescriptorKey& rhs) const {
                return memcmp(this, &rhs, sizeof(DescriptorKey)) == 0;
            }
        } _descriptorKey[SET_COUNT] {};

        //TODO: cull descriptors every now and again
        VkDescriptorSet _currentSets[SET_COUNT];
        std::unordered_map<DescriptorKey, VkDescriptorSet, ende::util::MurmurHash<DescriptorKey>> _descriptorSets;

        VkDescriptorPool _descriptorPool;

    };

}

#endif //CALA_COMMANDBUFFER_H
