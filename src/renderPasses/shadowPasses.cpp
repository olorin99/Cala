#include "shadowPasses.h"

void shadowPoint(cala::RenderGraph& graph, cala::Engine& engine, cala::Scene& scene) {
    {
        cala::ImageResource pointDepth;
        pointDepth.format = cala::backend::Format::RGBA8_UNORM;
        pointDepth.matchSwapchain = false;
        pointDepth.width = 10;
        pointDepth.height = 10;
        graph.addImageResource("pointDepth", pointDepth);
    }

    auto& pointShadows = graph.addPass("point_shadows", cala::RenderPass::Type::COMPUTE);

    pointShadows.addUniformBufferRead("global", cala::backend::PipelineStage::VERTEX_SHADER | cala::backend::PipelineStage::FRAGMENT_SHADER);
    pointShadows.addStorageImageWrite("pointDepth", cala::backend::PipelineStage::FRAGMENT_SHADER);
    pointShadows.addStorageBufferRead("transforms", cala::backend::PipelineStage::VERTEX_SHADER);
//    pointShadows.addStorageBufferRead("meshData", backend::PipelineStage::VERTEX_SHADER);
    pointShadows.addVertexRead("vertexBuffer");
    pointShadows.addIndexRead("indexBuffer");
    pointShadows.addIndirectRead("shadowDrawCommands");

    pointShadows.setExecuteFunction([&engine, &scene](cala::backend::vulkan::CommandHandle cmd, cala::RenderGraph& graph) {
        auto global = graph.getBuffer("global");
        auto drawCommands = graph.getBuffer("shadowDrawCommands");
        auto drawCount = graph.getBuffer("drawCount");
        u32 shadowIndex = 0;
        for (u32 i = 0; i < scene._lights.size(); i++) {
            auto& light = scene._lights[i].second;
            if (light.shadowing()) {
                switch (light.type()) {
                    case cala::Light::LightType::DIRECTIONAL:
                    {
                        auto directionalShadowMap = engine.getShadowMap(shadowIndex++, false);
                        light.setShadowMap(directionalShadowMap);

                        f32 halfRange = (light.getFar() - light.getNear()) / 2;

                        cala::Transform transform(light.getPosition(), light.getDirection());
                        cala::Camera camera(ende::math::orthographic<f32>(-20, 20, -20, 20, -halfRange, halfRange), transform);
                        camera.updateFrustum();

                        auto frustum = camera.frustum();

                        cmd->clearDescriptors();
                        cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::CULL_POINT));
                        cmd->bindBindings({});
                        cmd->bindAttributes({});
                        cmd->pushConstants(cala::backend::ShaderStage::COMPUTE, frustum);
                        cmd->bindBuffer(1, 0, global);
                        cmd->bindBuffer(2, 0, drawCommands, true);
                        cmd->bindBuffer(2, 1, drawCount, true);
                        cmd->bindPipeline();
                        cmd->bindDescriptors();
                        cmd->dispatchCompute(std::ceil(scene.meshCount() / 16.f), 1, 1);

                        cmd->begin(*engine.getShadowFramebuffer());

                        cmd->clearDescriptors();
                        cmd->bindRasterState({
                            cala::backend::CullMode::FRONT
                        });
                        cmd->bindDepthState({
                            true, true,
                            cala::backend::CompareOp::LESS_EQUAL
                        });

                        cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::SHADOW_DIRECT));

                        struct ShadowData {
                            ende::math::Mat4f viewProjection;
                            ende::math::Vec3f position;
                            f32 near;
                            f32 far;
                        };
                        ShadowData shadowData{
                                camera.viewProjection(),
                                camera.transform().pos(),
                                camera.near(),
                                camera.far()
                        };
                        cmd->pushConstants(cala::backend::ShaderStage::VERTEX, shadowData);

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

                        auto srcBarrier = engine.getShadowTarget()->barrier(cala::backend::PipelineStage::LATE_FRAGMENT,
                                                          cala::backend::PipelineStage::TRANSFER,
                                                          cala::backend::Access::DEPTH_STENCIL_WRITE,
                                                          cala::backend::Access::TRANSFER_READ,
                                                          cala::backend::ImageLayout::TRANSFER_SRC);
                        auto dstBarrier = directionalShadowMap->barrier(cala::backend::PipelineStage::FRAGMENT_SHADER,
                                                             cala::backend::PipelineStage::TRANSFER,
                                                             cala::backend::Access::SHADER_READ,
                                                             cala::backend::Access::TRANSFER_WRITE,
                                                             cala::backend::ImageLayout::TRANSFER_DST);
                        cmd->pipelineBarrier({&srcBarrier, 1});
                        cmd->pipelineBarrier({&dstBarrier, 1});

                        engine.getShadowTarget()->copy(cmd, *directionalShadowMap, 0);
                        srcBarrier = engine.getShadowTarget()->barrier(cala::backend::PipelineStage::TRANSFER,
                                                     cala::backend::PipelineStage::EARLY_FRAGMENT,
                                                     cala::backend::Access::TRANSFER_READ,
                                                     cala::backend::Access::DEPTH_STENCIL_WRITE,
                                                     cala::backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                        dstBarrier = directionalShadowMap->barrier(cala::backend::PipelineStage::TRANSFER,
                                                        cala::backend::PipelineStage::FRAGMENT_SHADER,
                                                        cala::backend::Access::TRANSFER_WRITE,
                                                        cala::backend::Access::SHADER_READ,
                                                        cala::backend::ImageLayout::SHADER_READ_ONLY);
                        cmd->pipelineBarrier({&srcBarrier, 1});
                        cmd->pipelineBarrier({&dstBarrier, 1});
                    }
                        break;
                    case cala::Light::LightType::POINT:
                    {
                        cala::Transform shadowTransform(light.getPosition());
                        cala::Camera shadowCam(ende::math::rad(90.f), engine.getShadowMapSize(), engine.getShadowMapSize(), light.getNear(), light.getFar(), shadowTransform);
                        auto shadowMap = engine.getShadowMap(shadowIndex++, true);
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
                            cmd->pushConstants(cala::backend::ShaderStage::COMPUTE, shadowFrustum);
                            cmd->bindBuffer(1, 0, global);
                            cmd->bindBuffer(2, 0, drawCommands, true);
                            cmd->bindBuffer(2, 1, drawCount, true);
                            cmd->bindPipeline();
                            cmd->bindDescriptors();
                            cmd->dispatchCompute(std::ceil(scene.meshCount() / 16.f), 1, 1);


                            cmd->begin(*engine.getShadowFramebuffer());

                            cmd->clearDescriptors();
                            cmd->bindRasterState({
                                                         cala::backend::CullMode::FRONT
                                                 });
                            cmd->bindDepthState({
                                                        true, true,
                                                        cala::backend::CompareOp::LESS_EQUAL
                                                });

                            cmd->bindProgram(engine.getProgram(cala::Engine::ProgramType::SHADOW_POINT));

                            cmd->bindBuffer(3, 0, scene._lightBuffer[engine.device().frameIndex()],
                                            sizeof(cala::Light::Data) * i, sizeof(cala::Light::Data), true);

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
                                    cala::backend::ShaderStage::VERTEX | cala::backend::ShaderStage::FRAGMENT,
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

                            auto srcBarrier = engine.getShadowTarget()->barrier(cala::backend::PipelineStage::LATE_FRAGMENT,
                                                              cala::backend::PipelineStage::TRANSFER,
                                                              cala::backend::Access::DEPTH_STENCIL_WRITE,
                                                              cala::backend::Access::TRANSFER_READ,
                                                              cala::backend::ImageLayout::TRANSFER_SRC);
                            auto dstBarrier = shadowMap->barrier(cala::backend::PipelineStage::FRAGMENT_SHADER,
                                                                 cala::backend::PipelineStage::TRANSFER,
                                                                 cala::backend::Access::SHADER_READ,
                                                                 cala::backend::Access::TRANSFER_WRITE,
                                                                 cala::backend::ImageLayout::TRANSFER_DST, face);
                            cmd->pipelineBarrier({&srcBarrier, 1});
                            cmd->pipelineBarrier({&dstBarrier, 1});

                            engine.getShadowTarget()->copy(cmd, *shadowMap, 0, face);
                            srcBarrier = engine.getShadowTarget()->barrier(cala::backend::PipelineStage::TRANSFER,
                                                         cala::backend::PipelineStage::EARLY_FRAGMENT,
                                                         cala::backend::Access::TRANSFER_READ,
                                                         cala::backend::Access::DEPTH_STENCIL_WRITE,
                                                         cala::backend::ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                            dstBarrier = shadowMap->barrier(cala::backend::PipelineStage::TRANSFER,
                                                            cala::backend::PipelineStage::FRAGMENT_SHADER,
                                                            cala::backend::Access::TRANSFER_WRITE,
                                                            cala::backend::Access::SHADER_READ,
                                                            cala::backend::ImageLayout::SHADER_READ_ONLY, face);
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