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

        void bindProgram(ShaderProgram& program);
        void bindRenderPass(VkRenderPass renderPass);
        //TODO: bind rest of pipeline state


        void bindPipeline(VkCommandBuffer cmdBuffer);

    private:

        bool get(VkPipeline* pipeline);

        struct Key {
            VkShaderModule shaders[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
            VkRenderPass renderPass = VK_NULL_HANDLE;
            //TODO: add rest of pipeline state to key

            bool operator==(const Key& rhs) const {
                return memcmp(this, &rhs, sizeof(Key)) == 0;
            }
        };

        VkDevice _device;

        bool _dirty;
        Key _key;
        VkPipeline _current;
        std::unordered_map<Key, VkPipeline, ende::util::MurmurHash<Key>> _pipelines;

    };

}

#endif //CALA_PIPELINELIST_H
