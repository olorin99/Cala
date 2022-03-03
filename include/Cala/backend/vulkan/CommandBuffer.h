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

    enum class AttribType {
        Vec2f = 2,
        Vec3f = 3,
        Vec4f = 4,
        Mat4f = 16
    };

    struct Attribute {
        u32 location = 0;
        u32 binding = 0;
        AttribType type = AttribType::Vec3f;
    };

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

        struct ViewPort {
            f32 x = 0;
            f32 y = 0;
            f32 width = 0;
            f32 height = 0;
            f32 minDepth = 0.f;
            f32 maxDepth = 1.f;
        };
        void bindViewPort(const ViewPort& viewport);

        struct RasterState {
            bool depthClamp = false;
            bool rasterDiscard = false;
            VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
            f32 lineWidth = 1.f;
            u32 cullMode = VK_CULL_MODE_BACK_BIT;
            VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

        void bindImage(u32 set, u32 slot, VkImageView image, VkSampler sampler, bool storage = false);

        void bindDescriptors();
        void clearDescriptors();


        void bindVertexBuffer(u32 first, VkBuffer buffer, u32 offset = 0);
        void bindVertexBuffers(u32 first, ende::Span<VkBuffer> buffers, ende::Span<VkDeviceSize> offsets);

        void bindIndexBuffer(Buffer& buffer, u32 offset = 0);


        void draw(u32 count, u32 instanceCount, u32 first, u32 firstInstance);

        void dispatchCompute(u32 x, u32 y, u32 z);



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
            VkVertexInputBindingDescription bindings[10]{}; //TODO: change from arbitrary values
            VkVertexInputAttributeDescription attributes[10]{};
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
            } buffers[8] {};
            union {

            };
            struct {
                VkImageView image = VK_NULL_HANDLE;
                VkSampler sampler = VK_NULL_HANDLE;
                u32 flags = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            } images[8] {};

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
