#include "Cala/Light.h"


cala::Light::Light(LightType type, bool shadows, Transform &transform)
    : _type(type),
      _camera(ende::math::rad(90), 1024, 1024, 0.1, 100, transform),
      _shadowing(shadows),
      _transform(transform),
      _colour({1, 1, 1}),
      _intensity(1),
      _constant(1),
      _linear(0.045),
      _quadratic(0.0075),
      _radius(80)
{
    switch (_type) {
        case DIRECTIONAL:
            _camera = Camera(ende::math::rad(45.f), 1024, 1024, 0.1, 100, _transform);
            break;
        default:
            break;
    }
}

cala::Light &cala::Light::operator=(const cala::Light &rhs) {
    Transform& tmp = rhs._transform;
    std::swap(_transform, tmp);
    _shadowing = rhs._shadowing;
    _colour = rhs._colour;
    _intensity = rhs._intensity;
    _constant = rhs._constant;
    _linear = rhs._linear;
    _quadratic = rhs._quadratic;
    _radius = rhs._radius;
    return *this;
}



cala::Light::Data cala::Light::data() const {
    Data data {
            {},
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