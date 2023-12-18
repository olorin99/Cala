#include "shadowPasses.h"

void shadowPoint(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    {
        cala::ImageResource pointDepth;
        pointDepth.format = cala::vk::Format::RGBA8_UNORM;
        pointDepth.matchSwapchain = false;
        pointDepth.width = 10;
        pointDepth.height = 10;
        graph.addImageResource("pointDepth", pointDepth);
    }

    auto& pointShadows = graph.addPass("point_shadows", cala::RenderPass::Type::COMPUTE);

    pointShadows.addUniformBufferRead("global", cala::vk::PipelineStage::VERTEX_SHADER | cala::vk::PipelineStage::FRAGMENT_SHADER);
    pointShadows.addStorageImageWrite("pointDepth", cala::vk::PipelineStage::FRAGMENT_SHADER);
    pointShadows.addStorageBufferRead("transforms", cala::vk::PipelineStage::VERTEX_SHADER);
//    pointShadows.addStorageBufferRead("meshData", vk::PipelineStage::VERTEX_SHADER);
    pointShadows.addVertexRead("vertexBuffer");
    pointShadows.addIndexRead("indexBuffer");
    pointShadows.addIndirectRead("shadowDrawCommands");
    pointShadows.addStorageBufferWrite("shadowDrawCount", cala::vk::PipelineStage::COMPUTE_SHADER);

    pointShadows.setExecuteFunction([&engine, &scene](cala::vk::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("shadowDrawCommands");
        auto drawCount = graph.getBuffer("shadowDrawCount");
        u32 shadowIndex = 0;
        for (u32 i = 0; i < scene._lights.size(); i++) {
            auto& light = scene._lights[i];
            if (light.shadowing()) {
                switch (light.type()) {
                    case cala::Light::LightType::DIRECTIONAL:
                    {
                        if (light.getCameraIndex() < 0)
                            continue;
                        auto directionalShadowMap = engine.getShadowMap(shadowIndex++, false, cmd);
                        light.setShadowMap(directionalShadowMap);
                        for (u32 cascadeIndex = 0; cascadeIndex < light.getCascadeCount(); cascadeIndex++) {
                            light.setCascadeShadowMap(cascadeIndex, directionalShadowMap);
                            cmd->clearDescriptors();
                            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::CULL_DIRECT));
                            cmd->bindBindings({});
                            cmd->bindAttributes({});
                            cmd->pushConstants(cala::vk::ShaderStage::COMPUTE, light.getCameraIndex() + cascadeIndex);
                            cmd->bindBuffer(1, 0, global);
                            cmd->bindBuffer(2, 0, drawCommands, true);
                            cmd->bindBuffer(2, 1, drawCount, true);
                            cmd->bindPipeline();
                            cmd->bindDescriptors();
                            cmd->dispatch(scene.meshCount(), 1, 1);

                            cmd->begin(*engine.getShadowFramebuffer());

                            cmd->clearDescriptors();
                            cmd->bindRasterState({
                                cala::vk::CullMode::FRONT
                            });
                            cmd->bindDepthState({
                                true, true,
                                cala::vk::CompareOp::LESS_EQUAL
                            });

                            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::SHADOW_DIRECT));

                            cmd->pushConstants(cala::vk::ShaderStage::VERTEX, light.getCameraIndex() + cascadeIndex);

                            auto binding = engine.globalBinding();
                            auto attributes = engine.globalVertexAttributes();
                            cmd->bindBindings({ &binding, 1 });
                            cmd->bindAttributes(attributes);

                            cmd->bindBuffer(1, 0, global);

                            cmd->bindPipeline();
                            cmd->bindDescriptors();
                            cmd->bindVertexBuffer(0, engine.vertexBuffer());
                            cmd->bindIndexBuffer(engine.indexBuffer());
                            cmd->drawIndirectCount(drawCommands, 0, drawCount, 0);

                            cmd->end(*engine.getShadowFramebuffer());

                            auto srcBarrier = engine.getShadowTarget()->barrier(cala::vk::PipelineStage::LATE_FRAGMENT,
                                                                                cala::vk::PipelineStage::TRANSFER,
                                                                                cala::vk::Access::DEPTH_STENCIL_WRITE,
                                                                                cala::vk::Access::TRANSFER_READ,
                                                                                cala::vk::ImageLayout::TRANSFER_SRC);
                            auto dstBarrier = directionalShadowMap->barrier(cala::vk::PipelineStage::FRAGMENT_SHADER,
                                                                            cala::vk::PipelineStage::TRANSFER,
                                                                            cala::vk::Access::SHADER_READ,
                                                                            cala::vk::Access::TRANSFER_WRITE,
                                                                            cala::vk::ImageLayout::TRANSFER_DST);
                            cmd->pipelineBarrier({&srcBarrier, 1});
                            cmd->pipelineBarrier({&dstBarrier, 1});

                            engine.getShadowTarget()->copy(cmd, *directionalShadowMap, 0);
                            srcBarrier = engine.getShadowTarget()->barrier(cala::vk::PipelineStage::TRANSFER,
                                                                           cala::vk::PipelineStage::EARLY_FRAGMENT,
                                                                           cala::vk::Access::TRANSFER_READ,
                                                                           cala::vk::Access::DEPTH_STENCIL_WRITE,
                                                                           cala::vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                            dstBarrier = directionalShadowMap->barrier(cala::vk::PipelineStage::TRANSFER,
                                                                       cala::vk::PipelineStage::FRAGMENT_SHADER,
                                                                       cala::vk::Access::TRANSFER_WRITE,
                                                                       cala::vk::Access::SHADER_READ,
                                                                       cala::vk::ImageLayout::SHADER_READ_ONLY);
                            cmd->pipelineBarrier({&srcBarrier, 1});
                            cmd->pipelineBarrier({&dstBarrier, 1});

                            directionalShadowMap = engine.getShadowMap(shadowIndex++, false, cmd);
                        }
                    }
                        break;
                    case cala::Light::LightType::POINT:
                    {
                        cala::Transform shadowTransform(light.getPosition());
                        cala::Camera shadowCam(ende::math::rad(90.f), engine.getShadowMapSize(), engine.getShadowMapSize(), light.getNear(), light.getFar(), &shadowTransform);
                        auto shadowMap = engine.getShadowMap(shadowIndex++, true, cmd);
                        light.setShadowMap(shadowMap);

                        for (u32 face = 0; face < 6; face++) {
                            switch (face) {
                                case 0:
                                    shadowTransform.rotate({0, 1, 0}, ende::math::rad(90));
                                    break;
                                case 1:
                                    shadowTransform.rotate({0, 1, 0}, ende::math::rad(180));
                                    break;
                                case 2:
                                    shadowTransform.rotate({0, 1, 0}, ende::math::rad(90));
                                    shadowTransform.rotate({1, 0, 0}, ende::math::rad(90));
                                    break;
                                case 3:
                                    shadowTransform.rotate({1, 0, 0}, ende::math::rad(180));
                                    break;
                                case 4:
                                    shadowTransform.rotate({1, 0, 0}, ende::math::rad(90));
                                    break;
                                case 5:
                                    shadowTransform.rotate({0, 1, 0}, ende::math::rad(180));
                                    break;
                            }
                            shadowCam.updateFrustum();
                            ende::math::Frustum shadowFrustum = shadowCam.frustum();

                            cmd->clearDescriptors();
                            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::CULL_POINT));
                            cmd->bindBindings({});
                            cmd->bindAttributes({});
                            cmd->pushConstants(cala::vk::ShaderStage::COMPUTE, shadowFrustum);
                            cmd->bindBuffer(1, 0, global);
                            cmd->bindBuffer(2, 0, drawCommands, true);
                            cmd->bindBuffer(2, 1, drawCount, true);
                            cmd->bindPipeline();
                            cmd->bindDescriptors();
                            cmd->dispatch(scene.meshCount(), 1, 1);


                            cmd->begin(*engine.getShadowFramebuffer());

                            cmd->clearDescriptors();
                            cmd->bindRasterState({
                                                         cala::vk::CullMode::FRONT
                                                 });
                            cmd->bindDepthState({
                                                        true, true,
                                                        cala::vk::CompareOp::LESS_EQUAL
                                                });

                            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::SHADOW_POINT));

                            cmd->bindBuffer(3, 0, scene._lightBuffer[engine.device().frameIndex()],
                                            sizeof(GPULight) * i + sizeof(u32), sizeof(GPULight), true);

                            struct ShadowData {
                                ende::math::Mat4f viewProjection;
                                ende::math::Vec3f position;
                                f32 near;
                                f32 far;
                            };
                            ShadowData shadowData{
                                    shadowCam.viewProjection(),
                                    shadowCam.transform().pos(),
                                    shadowCam.near(),
                                    shadowCam.far()
                            };
                            cmd->pushConstants(
                                    cala::vk::ShaderStage::VERTEX | cala::vk::ShaderStage::FRAGMENT,
                                    shadowData);

                            auto binding = engine.globalBinding();
                            auto attributes = engine.globalVertexAttributes();
                            cmd->bindBindings({ &binding, 1 });
                            cmd->bindAttributes(attributes);

                            cmd->bindBuffer(1, 0, global);

                            cmd->bindPipeline();
                            cmd->bindDescriptors();
                            cmd->bindVertexBuffer(0, engine.vertexBuffer());
                            cmd->bindIndexBuffer(engine.indexBuffer());
                            cmd->drawIndirectCount(drawCommands, 0, drawCount, 0);

                            cmd->end(*engine.getShadowFramebuffer());

                            auto srcBarrier = engine.getShadowTarget()->barrier(cala::vk::PipelineStage::LATE_FRAGMENT,
                                                              cala::vk::PipelineStage::TRANSFER,
                                                              cala::vk::Access::DEPTH_STENCIL_WRITE,
                                                              cala::vk::Access::TRANSFER_READ,
                                                              cala::vk::ImageLayout::TRANSFER_SRC);
                            auto dstBarrier = shadowMap->barrier(cala::vk::PipelineStage::FRAGMENT_SHADER,
                                                                 cala::vk::PipelineStage::TRANSFER,
                                                                 cala::vk::Access::SHADER_READ,
                                                                 cala::vk::Access::TRANSFER_WRITE,
                                                                 cala::vk::ImageLayout::TRANSFER_DST, face);
                            cmd->pipelineBarrier({&srcBarrier, 1});
                            cmd->pipelineBarrier({&dstBarrier, 1});

                            engine.getShadowTarget()->copy(cmd, *shadowMap, 0, face);
                            srcBarrier = engine.getShadowTarget()->barrier(cala::vk::PipelineStage::TRANSFER,
                                                         cala::vk::PipelineStage::EARLY_FRAGMENT,
                                                         cala::vk::Access::TRANSFER_READ,
                                                         cala::vk::Access::DEPTH_STENCIL_WRITE,
                                                         cala::vk::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                            dstBarrier = shadowMap->barrier(cala::vk::PipelineStage::TRANSFER,
                                                            cala::vk::PipelineStage::FRAGMENT_SHADER,
                                                            cala::vk::Access::TRANSFER_WRITE,
                                                            cala::vk::Access::SHADER_READ,
                                                            cala::vk::ImageLayout::SHADER_READ_ONLY, face);
                            cmd->pipelineBarrier({&srcBarrier, 1});
                            cmd->pipelineBarrier({&dstBarrier, 1});
                        }
                    }
                        break;
                }
            } else {
                light.setShadowMap({});
            }
        }
    });
}