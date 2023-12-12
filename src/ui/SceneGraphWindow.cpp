#include "Cala/ui/SceneGraphWindow.h"
#include <Cala/Material.h>

cala::ui::SceneGraphWindow::SceneGraphWindow(ImGuiContext *context, cala::Scene *scene)
    : Window(context),
    _scene(scene)
{}

void traverseSceneNode(cala::Scene::SceneNode* node, cala::Scene* scene) {

    for (u32 childIndex = 0; childIndex < node->children.size(); childIndex++) {
        auto& child = node->children[childIndex];
        ImGui::PushID(std::hash<const void*>()(child.get()));
        switch (child->type) {
            case cala::Scene::NodeType::NONE:
            {
                if (ImGui::TreeNode(child->name.c_str())) {
                    if (ImGui::Button("Delete")) {
                        scene->removeChildNode(node, childIndex--);
                        ImGui::TreePop();
                        ImGui::PopID();
                        continue;
                    }
                    auto position = child->transform.pos();
                    if (ImGui::DragFloat3("Position", &position[0], 0.1)) {
                        child->transform.setPos(position);
                    }
                    ende::math::Vec3f eulerAngles = child->transform.rot().unit().toEuler();
                    ende::math::Vec3f angleDegrees = { (f32)ende::math::deg(eulerAngles.x()), (f32)ende::math::deg(eulerAngles.y()), (f32)ende::math::deg(eulerAngles.z()) };
                    if (ImGui::DragFloat3("Rotation", &angleDegrees[0], 0.1)) {
                        ende::math::Quaternion rotation(ende::math::rad(angleDegrees.x()), ende::math::rad(angleDegrees.y()), ende::math::rad(angleDegrees.z()));
                        child->transform.setRot(rotation);
                    }
                    auto scale = child->transform.scale();
                    if (ImGui::DragFloat3("Scale", &scale[0], 0.1)) {
                        child->transform.setScale(scale);
                    }
                    traverseSceneNode(child.get(), scene);
                    ImGui::TreePop();
                }
            }
                break;
            case cala::Scene::NodeType::MESH:
            {
                auto meshNode = dynamic_cast<cala::Scene::MeshNode*>(child.get());
                auto label = std::format("Mesh: {}", meshNode->index);
                if (ImGui::TreeNode(label.c_str())) {
                    if (ImGui::Button("Delete")) {
                        scene->removeChildNode(node, childIndex--);
                        ImGui::TreePop();
                        ImGui::PopID();
                        continue;
                    }
                    auto& meshInfo = scene->_meshes[meshNode->index];
                    auto& meshData = scene->_meshData[meshNode->index];

                    bool enabled = meshData.enabled;
                    if (ImGui::Checkbox("Enabled", &enabled))
                        meshData.enabled = enabled;
                    bool castShadows = meshData.castShadows;
                    if (ImGui::Checkbox("Cast Shadows", &castShadows))
                        meshData.castShadows = castShadows;

                    ImGui::Text("First Index: %u", meshInfo.firstIndex);
                    ImGui::Text("Index Count: %u", meshInfo.indexCount);

                    ImGui::Text("Min Extent: (%f, %f, %f)", meshInfo.min.x(), meshInfo.min.y(), meshInfo.min.z());
                    ImGui::Text("Max Extent: (%f, %f, %f)", meshInfo.max.x(), meshInfo.max.y(), meshInfo.max.z());

                    auto position = meshNode->transform.pos();
                    if (ImGui::DragFloat3("Position", &position[0], 0.1)) {
                        meshNode->transform.setPos(position);
                    }
                    ende::math::Vec3f eulerAngles = meshNode->transform.rot().unit().toEuler();
                    ende::math::Vec3f angleDegrees = { (f32)ende::math::deg(eulerAngles.x()), (f32)ende::math::deg(eulerAngles.y()), (f32)ende::math::deg(eulerAngles.z()) };
                    if (ImGui::DragFloat3("Rotation", &angleDegrees[0], 0.1)) {
                        ende::math::Quaternion rotation(ende::math::rad(angleDegrees.x()), ende::math::rad(angleDegrees.y()), ende::math::rad(angleDegrees.z()));
                        meshNode->transform.setRot(rotation);
                    }
                    auto scale = meshNode->transform.scale();
                    if (ImGui::DragFloat3("Scale", &scale[0], 0.1)) {
                        meshNode->transform.setScale(scale);
                    }

                    auto materialInstance = meshInfo.materialInstance;
                    auto& litVariant = materialInstance->material()->getVariant(cala::Material::Variant::LIT);
                    auto parameters = litVariant.interface().getBindingMemberList(2, 0);

                    ImGui::Text("Material Info");
                    for (auto& parameter : parameters) {
                        switch (parameter.type) {
                            case cala::vk::ShaderModuleInterface::MemberType::INT:
                            {
                                auto value = materialInstance->getParameter<i32>(parameter.name);
                                if (ImGui::DragInt(parameter.name.c_str(), &value))
                                    materialInstance->setParameter(parameter.name, value);
                            }
                                break;
                            case cala::vk::ShaderModuleInterface::MemberType::FLOAT:
                            {
                                auto value = materialInstance->getParameter<f32>(parameter.name);
                                if (ImGui::DragFloat(parameter.name.c_str(), &value))
                                    materialInstance->setParameter(parameter.name, value);
                            }
                                break;
                            case cala::vk::ShaderModuleInterface::MemberType::VEC3F:
                            {
                                auto value = materialInstance->getParameter<ende::math::Vec3f>(parameter.name);
                                if (ImGui::DragFloat3(parameter.name.c_str(), &value[0]))
                                    materialInstance->setParameter(parameter.name, value);
                            }
                                break;
                            case cala::vk::ShaderModuleInterface::MemberType::VEC4F:
                            {
                                auto value = materialInstance->getParameter<ende::math::Vec4f>(parameter.name);
                                if (ImGui::DragFloat4(parameter.name.c_str(), &value[0]))
                                    materialInstance->setParameter(parameter.name, value);
                            }
                                break;
                        }
                    }
                    traverseSceneNode(child.get(), scene);
                    ImGui::TreePop();
                }
            }
                break;
            case cala::Scene::NodeType::LIGHT:
            {
                auto lightNode = dynamic_cast<cala::Scene::LightNode*>(child.get());
                auto label = std::format("Light: {}", lightNode->index);
                if (ImGui::TreeNode(label.c_str())) {
                    if (ImGui::Button("Delete")) {
                        scene->removeChildNode(node, childIndex--);
                        ImGui::TreePop();
                        ImGui::PopID();
                        continue;
                    }
                    auto& light = scene->_lights[lightNode->index].second;

                    const char* modes[] = { "DIRECTIONAL", "POINT" };
                    cala::Light::LightType types[] = { cala::Light::LightType::DIRECTIONAL, cala::Light::LightType::POINT };
                    static int modeIndex = 0;
                    if (ImGui::Combo("Type", &modeIndex, modes, 2)) {
                        light.setType(types[modeIndex]);
                    }

                    auto position = lightNode->transform.pos();
                    auto colour = light.getColour();
                    f32 intensity = light.getIntensity();
                    f32 range = light.getFar();
                    bool shadowing = light.shadowing();
                    f32 shadowBias = light.getShadowBias();

                    if (light.type() == cala::Light::POINT) {
                        if (ImGui::DragFloat3("Position", &position[0], 0.1)) {
                            lightNode->transform.setPos(position);
                            light.setPosition(position);
                        }
                    } else if (light.type() == cala::Light::DIRECTIONAL) {
                        ende::math::Vec3f eulerAngles = lightNode->transform.rot().unit().toEuler();
                        ende::math::Vec3f angleDegrees = { (f32)ende::math::deg(eulerAngles.x()), (f32)ende::math::deg(eulerAngles.y()), (f32)ende::math::deg(eulerAngles.z()) };
                        if (ImGui::DragFloat3("Direction", &angleDegrees[0], 0.1)) {
                            ende::math::Quaternion rotation(ende::math::rad(angleDegrees.x()), ende::math::rad(angleDegrees.y()), ende::math::rad(angleDegrees.z()));
                            lightNode->transform.setRot(rotation);
                            light.setDirection(rotation);
                        }
                    }
                    if (ImGui::ColorEdit3("Colour", &colour[0]))
                        light.setColour(colour);
                    if (ImGui::SliderFloat("Intensity", &intensity, 1, 100))
                        light.setIntensity(intensity);
                    if (ImGui::SliderFloat("Range", &range, 0, 100))
                        light.setRange(range);
                    if (ImGui::DragFloat("Shadow Bias", &shadowBias, 0.001, -1, 1))
                        light.setShadowBias(shadowBias);
                    if (ImGui::Checkbox("Shadowing", &shadowing))
                        light.setShadowing(shadowing);

                    traverseSceneNode(child.get(), scene);
                    ImGui::TreePop();
                }
            }
                break;
        }
        ImGui::PopID();
//        childIndex++;
    }
}

void cala::ui::SceneGraphWindow::render() {
    if (ImGui::Begin("SceneGraph Window")) {
        traverseSceneNode(_scene->_root.get(), _scene);
    }
    ImGui::End();
}