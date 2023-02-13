#include "Cala/Transform.h"


cala::Transform::Transform(const ende::math::Vec3f &pos, const ende::math::Quaternion &rot, const ende::math::Vec3f &scale)
    : _position(pos),
    _rotation(rot),
    _scale(scale)
{}

ende::math::Mat4f cala::Transform::toMat() const {
    ende::math::Mat4f translation = ende::math::translation<4, f32>(_position);
    ende::math::Mat4f rotation = _rotation.toMat();
    ende::math::Mat4f scale = ende::math::scale<4, f32>(_scale);
    return translation * rotation * scale;
}

cala::Transform &cala::Transform::rotate(const ende::math::Quaternion &rot) {
    _dirty = true;
    _rotation = (rot * _rotation).unit();
    return *this;
}

cala::Transform &cala::Transform::rotate(const ende::math::Vec3f &axis, f32 angle) {
    _dirty = true;
    return rotate(ende::math::Quaternion(axis, angle));
}

cala::Transform &cala::Transform::addPos(const ende::math::Vec3f &vec) {
    _dirty = true;
    _position = _position + vec;
    return *this;
}

cala::Transform &cala::Transform::setPos(const ende::math::Vec3f &pos) {
    _dirty = true;
    _position = pos;
    return *this;
}

cala::Transform &cala::Transform::setRot(const ende::math::Quaternion &rot) {
    _dirty = true;
    _rotation = rot;
    return *this;
}

cala::Transform &cala::Transform::setRot(const ende::math::Vec3f &axis, f32 angle) {
    _dirty = true;
    return setRot(ende::math::Quaternion(axis, angle));
}

cala::Transform &cala::Transform::setScale(const ende::math::Vec3f &scale) {
    _dirty = true;
    _scale = scale;
    return *this;
}

cala::Transform &cala::Transform::setScale(f32 scale) {
    _dirty = true;
    return setScale({ scale, scale, scale });
}