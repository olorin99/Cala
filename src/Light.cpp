#include "Cala/Light.h"


cala::Light::Light(LightType type, Transform &transform)
    : _type(type),
      _transform(transform),
      _direction({0, 1, 0}),
      _colour({1, 1, 1}),
      _intensity(1),
      _constant(1),
      _linear(0.045),
      _quadratic(0.0075),
      _radius(80)
{}

cala::Light::Data cala::Light::data() const {
    Data data {
        _direction,
        0,
        _colour,
        _intensity,
        _constant,
        _linear,
        _quadratic,
        _radius
    };

    if (_type == POINT)
        data.position = _transform.pos();
    else if (_type == DIRECTIONAL) {
        data.position = _transform.rot().back();
//        data.position[1] *= -1; // inverse y axis of direction vector
    }
    return data;
}

void cala::Light::setDirection(const ende::math::Quaternion &dir) {
    assert(_type == DIRECTIONAL);
    _transform.setRot(dir);
}

void cala::Light::setPosition(const ende::math::Vec3f &pos) {
    assert(_type == POINT);
    _transform.setPos(pos);
}

void cala::Light::setColour(const ende::math::Vec3f &colour) {
    _colour = colour;
}

void cala::Light::setIntensity(f32 intensity) {
    _intensity = intensity;
}