#include "Cala/Camera.h"

cala::Camera::Camera(f32 fov, f32 width, f32 height, f32 near, f32 far, Transform &transform)
    : _transform(transform),
    _projection(ende::math::perspective(fov, width / height, near, far)),
    _fov(fov),
    _width(width),
    _height(height),
    _near(near),
    _far(far)
{}

cala::Camera::Camera(const ende::math::Mat4f &projection, Transform &transform)
    : _projection(projection),
    _transform(transform)
{}

void cala::Camera::resize(f32 width, f32 height) {
    _width = width;
    _height = height;
    _projection = ende::math::perspective(_fov, _width / _height, _near, _far);
}

ende::math::Mat4f cala::Camera::view() const {
    ende::math::Vec3f pos = _transform.pos();
    //pos = pos * -1;
    //pos[1] *= -1; //inverses all movement on y axis
    ende::math::Mat4f translation = ende::math::translation<4, f32>(pos);
    ende::math::Quaternion rot = _transform.rot().conjugate().unit();
    //rot[0] *= -1;
    ende::math::Mat4f rotation = rot.toMat();
    //ende::math::Mat4f rotation = _transform.rot().conjugate().unit().toMat();
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