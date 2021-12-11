#ifndef CALA_PIPELINELIST_H
#define CALA_PIPELINELIST_H

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <Ende/util/hash.h>

#include <Cala/backend/vulkan/ShaderProgram.h>
#include <cstring>

namespace cala::backend::vulkan {

    class PipelineList {
    public:

        PipelineList(VkDevice device);

        ~PipelineList();

        void bindProgram(ShaderProgram& program);

        void bindVertexArray(ende::Span<VkVertexInputBindingDescription> bindings, ende::Span<VkVertexInputAttributeDescription> attributes);


        void bindRenderPass(VkRenderPass renderPass);
        //TODO: bind rest of pipeline state

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


        void bindPipeline(VkCommandBuffer cmdBuffer);

        void bindDescriptors(VkCommandBuffer cmdBuffer);

        void bindBuffer(VkBuffer buffer, u32 set, u32 binding, u32 offset, u32 range);

//    private:

        bool get(VkPipeline* pipeline);



        struct Key {
            VkShaderModule shaders[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
            VkVertexInputBindingDescription bindings[10]; //TODO: change from arbitrary values
            VkVertexInputAttributeDescription attributes[10];
            VkRenderPass renderPass = VK_NULL_HANDLE;
            VkDescriptorSetLayout setLayout;
            VkPipelineLayout layout;
            RasterState raster;
            //TODO: add rest of pipeline state to key

            bool operator==(const Key& rhs) const {
                return memcmp(this, &rhs, sizeof(Key)) == 0;
            }
        };

        struct Buffer {
            VkBuffer buffer;
            u32 offset;
            u32 range;
        };

        struct DescriptorKey {
            VkDescriptorSetLayout layout[4] = {};
            Buffer buffers[4][4] = {{}};
        };

        VkDevice _device;

        bool _dirty;
        Key _key;
        DescriptorKey _setKey;
        VkPipeline _current;
        std::unordered_map<Key, VkPipeline, ende::util::MurmurHash<Key>> _pipelines;
//        std::unordered_map<DescriptorKey, VkDescriptorSet, ende::util::MurmurHash<DescriptorKey>> _descriptors;


//        VkDescriptorSetLayout _tmp;

    };

}

#endif //CALA_PIPELINELIST_H
