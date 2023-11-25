#include "Cala/ui/LightWindow.h"
#include <imgui.h>

cala::ui::LightWindow::LightWindow(Scene *scene)
    : _scene(scene),
    _lightIndex(0)
{}

void cala::ui::LightWindow::render() {
    ImGui::Begin("Lights");
    ImGui::Text("Lights: %lu", _scene->_lights.size());
    ImGui::SliderInt("Light", &_lightIndex, 0, _scene->_lights.size() - 1);

    auto& lightRef = _scene->_lights[_lightIndex].second;

    ende::math::Vec3f position = lightRef.transform().pos();
    ende::math::Vec3f colour = lightRef.getColour();
    f32 intensity = lightRef.getIntensity();
    f32 range = lightRef.getFar();
    bool shadowing = lightRef.shadowing();
    f32 shadowBias = lightRef.getShadowBias();

    if (lightRef.type() == cala::Light::POINT) {
        if (ImGui::DragFloat3("Position", &position[0], 0.1, -10, 10))
            lightRef.setPosition(position);
    } else {
        ende::math::Vec3f eulerAngles = lightRef.transform().rot().unit().toEuler();
        ende::math::Vec3f angleDegrees = { (f32)ende::math::deg(eulerAngles.x()), (f32)ende::math::deg(eulerAngles.y()), (f32)ende::math::deg(eulerAngles.z()) };
        if (ImGui::DragFloat3("Direction", &angleDegrees[0], 0.1)) {
            lightRef.setDirection(ende::math::Quaternion(ende::math::rad(angleDegrees.x()), ende::math::rad(angleDegrees.y()), ende::math::rad(angleDegrees.z())));
//            lightRef.setDirection(ende::math::Quaternion(eulerAngles.x(), eulerAngles.y(), eulerAngles.z()));
        }
//        ende::math::Quaternion direction = lightRef.transform().rot();
//        if (ImGui::DragFloat4("Direction", &direction[0], 0.01, -1, 1))
//            lightRef.setDirection(direction);
    }
    if (ImGui::ColorEdit3("Colour", &colour[0]))
        lightRef.setColour(colour);
    if (ImGui::SliderFloat("Intensity", &intensity, 1, 100))
        lightRef.setIntensity(intensity);
    if (ImGui::SliderFloat("Range", &range, 0, 100))
        lightRef.setRange(range);
    if (ImGui::DragFloat("Shadow Bias", &shadowBias, 0.001, -1, 1))
        lightRef.setShadowBias(shadowBias);
    if (ImGui::Checkbox("Shadowing", &shadowing))
        lightRef.setShadowing(shadowing);
}