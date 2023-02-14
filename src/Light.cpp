#include "Cala/Light.h"


cala::Light::Light(LightType type, bool shadows, Transform &transform)
    : _type(type),
      _camera(ende::math::rad(90), 1024, 1024, 0.1, 100, transform),
      _shadowing(shadows),
      _transform(&transform),
      _colour({1, 1, 1}),
      _intensity(1),
      _constant(1),
      _linear(0.045),
      _quadratic(0.0075),
      _radius(80),
      _near(0.1),
      _far(100),
      _dirty(true)
{
    switch (_type) {
        case DIRECTIONAL:
            _camera = Camera(ende::math::rad(45.f), 1024, 1024, 0.1, 100, *_transform);
            break;
        default:
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
    _linear = rhs._linear;
    _quadratic = rhs._quadratic;
    _radius = rhs._radius;
    _near = rhs._near;
    _far = rhs._far;
    return *this;
}



cala::Light::Data cala::Light::data() const {
    Data data {
        {0, 0, 0},
        _type,
        _colour,
        _intensity,
        _far - _near,
        _linear,
        _quadratic,
        0
    };

    if (_type == POINT)
        data.position = _transform->pos();
    else if (_type == DIRECTIONAL) {
        data.position = _transform->rot().back();
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
    _dirty = true;
}

void cala::Light::setNear(f32 near) {
    _near = near;
    _dirty = true;
}

void cala::Light::setFar(f32 far) {
    _far = far;
    _dirty = true;
}