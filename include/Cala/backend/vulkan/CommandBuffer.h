#ifndef CALA_COMMANDBUFFER_H
#define CALA_COMMANDBUFFER_H

#include <vulkan/vulkan.h>

#include <Cala/backend/vulkan/ShaderProgram.h>

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


        void bindProgram(ShaderProgram& program);

        void bindVertexArray(ende::Span<VkVertexInputBindingDescription> bindings, ende::Span<VkVertexInputAttributeDescription> attributes);


        void bindRenderPass(VkRenderPass renderPass);

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


        void bindPipeline();

        bool submit(VkSemaphore wait, VkFence fence = VK_NULL_HANDLE);

        VkSemaphore signal() const { return _signal; }

//    private:

        // true if requires binding
        VkPipeline getPipeline();


        VkCommandBuffer _buffer;
        VkSemaphore _signal;
        VkDevice _device;
        VkQueue _queue;

        struct PipelineKey {
            VkShaderModule shaders[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
            VkVertexInputBindingDescription bindings[10]; //TODO: change from arbitrary values
            VkVertexInputAttributeDescription attributes[10];
            VkRenderPass renderPass = VK_NULL_HANDLE;
            VkDescriptorSetLayout setLayout;
            VkPipelineLayout layout;
            RasterState raster;
            //TODO: add rest of pipeline state to key

            bool operator==(const PipelineKey& rhs) const {
                return memcmp(this, &rhs, sizeof(PipelineKey)) == 0;
            }
        } _pipelineKey;

        VkPipeline _currentPipeline;
        std::unordered_map<PipelineKey, VkPipeline, ende::util::MurmurHash<PipelineKey>> _pipelines;

    };

}

#endif //CALA_COMMANDBUFFER_H
