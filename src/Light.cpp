#include "Cala/Light.h"


cala::Light::Light(LightType type, bool shadows, Transform &transform)
    : _type(type),
      _camera(ende::math::rad(90), 1024, 1024, 0.1, 100, transform),
      _shadowing(shadows),
      _transform(&transform),
      _colour({1, 1, 1}),
      _intensity(1),
      _constant(1),
      _shadowBias(type == POINT ? 0.1 : 0.015),
      _radius(std::sqrt(_intensity / 0.01)),
      _range(_radius),
      _dirty(true)
{
    switch (_type) {
        case DIRECTIONAL:
        {
            _range = 100;
            f32 halfRange = _range / 2;
            _camera = Camera(ende::math::orthographic<f32>(-20, 20, -20, 20, -halfRange, halfRange), *_transform);
        }
            break;
        default:
            _camera = Camera(ende::math::rad(90), 1024, 1024, 0.1, _range, transform);
            break;
    }
}

cala::Light &cala::Light::operator=(const cala::Light &rhs) {
    if (this == &rhs) return *this;
    _transform = rhs._transform;
    _shadowing = rhs._shadowing;
    _colour = rhs._colour;
    _intensity = rhs._intensity;
    _constant = rhs._constant;
    _shadowBias = rhs._shadowBias;
    _radius = rhs._radius;
    _range = rhs._range;
    return *this;
}



cala::Light::Data cala::Light::data() const {
    Data data {
        {0, 0, 0},
        _type,
        _colour,
        _intensity,
        _range,
        _radius,
        _shadowBias,
        _shadowMap.index()
    };

    if (_type == POINT)
        data.position = _transform->pos();
    else if (_type == DIRECTIONAL) {
        data.position = _transform->rot().invertY().back();
    }
    return data;
}

void cala::Light::setDirection(const ende::math::Quaternion &dir) {
    assert(_type == DIRECTIONAL);
    _transform->setRot(dir);
    _dirty = true;
}

void cala::Light::setPosition(const ende::math::Vec3f &pos) {
    assert(_type == POINT);
    _transform->setPos(pos);
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
    if (_type == DIRECTIONAL) {
        f32 halfRange = _range / 2;
        _camera = Camera(ende::math::orthographic<f32>(-20, 20, -20, 20, -halfRange, halfRange), *_transform);
    } else {
        _camera = Camera(ende::math::rad(90), 1024, 1024, 0.1, _range, *_transform);
    }
    _dirty = true;
}

void cala::Light::setShadowBias(f32 bias) {
    _shadowBias = bias;
    _dirty = true;
}

void cala::Light::setShadowMap(backend::vulkan::ImageHandle shadowMap) {
    _shadowMap = shadowMap;
    _dirty = true;
}