#include "Cala/Light.h"


cala::Light::Light(LightType type, bool shadows)
    : _type(type),
      _shadowing(shadows),
      _colour({1, 1, 1}),
      _intensity(1),
      _constant(1),
      _shadowBias(0.015),
      _radius(std::sqrt(_intensity / 0.01)),
      _range(_radius),
      _dirty(true)
{
    switch (_type) {
        case DIRECTIONAL:
        {
            _range = 100;
        }
            break;
        default:
            break;
    }
}

cala::Light &cala::Light::operator=(const cala::Light &rhs) {
    if (this == &rhs) return *this;
    _shadowing = rhs._shadowing;
    _colour = rhs._colour;
    _intensity = rhs._intensity;
    _constant = rhs._constant;
    _shadowBias = rhs._shadowBias;
    _radius = rhs._radius;
    _range = rhs._range;
    return *this;
}



GPULight cala::Light::data() const {
    GPULight data {
        {0, 0, 0},
        _type,
        _colour,
        _intensity,
        _range,
        _shadowBias,
        _shadowMap.index(),
        -1,
        (u32)_cascadeCount
    };

    for (u32 cascade = 0; cascade < _cascadeCount - 1; cascade++) {
        data.cascades[cascade].shadowMapIndex = _cascadeMaps[cascade].index();
        data.cascades[cascade].distance = _cascadeSplits[cascade];
    }
    if (_cascadeCount > 0) {
        data.cascades[_cascadeCount - 1].shadowMapIndex = _cascadeMaps[_cascadeCount - 1].index();
        data.cascades[_cascadeCount - 1].distance = getFar();
    }

    if (_type == POINT)
        data.position = _position;
    else if (_type == DIRECTIONAL) {
        data.position = _rotation.invertY().back();
    }
    return data;
}

void cala::Light::setDirection(const ende::math::Quaternion &dir) {
    assert(_type == DIRECTIONAL);
    _rotation = dir;
    _dirty = true;
}

void cala::Light::setPosition(const ende::math::Vec3f &pos) {
    assert(_type == POINT);
    _position = pos;
    _dirty = true;
}

void cala::Light::setShadowing(bool shadowing) {
    _shadowing = shadowing;
    _dirty = true;
}

void cala::Light::setColour(const ende::math::Vec3f &colour) {
    _colour = colour;
    _dirty = true;
}

void cala::Light::setIntensity(f32 intensity) {
    _intensity = intensity;
    _radius = std::sqrt(_intensity / 0.01);
    _dirty = true;
}

void cala::Light::setRange(f32 range) {
    _range = range;
    _dirty = true;
}

void cala::Light::setShadowBias(f32 bias) {
    _shadowBias = bias;
    _dirty = true;
}

void cala::Light::setShadowMap(vk::ImageHandle shadowMap) {
    _shadowMap = shadowMap;
    _dirty = true;
}

void cala::Light::setCascadeShadowMap(i32 cascade, vk::ImageHandle shadowMap) {
    _cascadeMaps[cascade] = shadowMap;
    _dirty = true;
}