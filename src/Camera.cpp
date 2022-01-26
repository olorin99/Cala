#include "Cala/Camera.h"

cala::Camera::Camera(const ende::math::Mat4f &projection, Transform &transform)
    : _projection(projection),
    _transform(transform)
{}

ende::math::Mat4f cala::Camera::view() const {
    ende::math::Mat4f translation = ende::math::translation<4, f32>(_transform.pos() * -1);
    ende::math::Mat4f rotation = _transform.rot().conjugate().unit().toMat();
    return rotation * translation;
}

ende::math::Mat4f cala::Camera::viewProjection() const {
    return _projection * view();
}

cala::Transform &cala::Camera::transform() const {
    return _transform;
}

cala::Camera::Data cala::Camera::data() const {
    return  {
        _projection,
        view(),
        _transform.pos()
    };
}