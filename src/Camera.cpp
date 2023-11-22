#include "Cala/Camera.h"

cala::Camera::Camera(f32 fov, f32 width, f32 height, f32 near, f32 far, Transform &transform)
    : _transform(transform),
    _projection(ende::math::perspective(fov, width / height, near, far)),
    _frustum(_projection),
    _fov(fov),
    _width(width),
    _height(height),
    _near(near),
    _far(far),
    _exposure(1.0),
    _dirty(true)
{
    auto proj = projection();
    ende::math::Mat4f translation = ende::math::translation<4, f32>(_transform.pos());
    ende::math::Quaternion rot = _transform.rot().conjugate().unit();
    ende::math::Mat4f rotation = rot.toMat(); //inverse rotation of y axis
    auto v = rotation * translation;
    _frustum.update(proj * v);
}

cala::Camera::Camera(const ende::math::Mat4f &projection, Transform &transform)
    : _projection(projection),
      _frustum(projection),
    _transform(transform),
    _dirty(true)
{
    auto proj = this->projection();
    ende::math::Mat4f translation = ende::math::translation<4, f32>(_transform.pos());
    ende::math::Quaternion rot = _transform.rot().conjugate().unit();
    ende::math::Mat4f rotation = rot.toMat(); //inverse rotation of y axis
    auto v = rotation * translation;
    _frustum.update(proj * v);
}

cala::Camera &cala::Camera::operator=(const cala::Camera &rhs) {
    auto& tmp = rhs._transform;
    std::swap(_transform, tmp);
    _projection = rhs._projection;
    _frustum = rhs._frustum;
    _fov = rhs._fov;
    _width = rhs._width;
    _height = rhs._height;
    _near = rhs._near;
    _far = rhs._far;
    _dirty = rhs._dirty;
    return *this;
}

void cala::Camera::resize(f32 width, f32 height) {
    _width = width;
    _height = height;
    _projection = ende::math::perspective(_fov, _width / _height, _near, _far);
    _dirty = true;
}

ende::math::Mat4f cala::Camera::view() const {
    ende::math::Vec3f pos = _transform.pos();
    pos = pos * -1;
    ende::math::Mat4f translation = ende::math::translation<4, f32>(pos);
    ende::math::Quaternion rot = _transform.rot().conjugate().unit();
    ende::math::Mat4f rotation = rot.invertY().toMat();
    return rotation * translation;
}

ende::math::Mat4f cala::Camera::viewProjection() const {
    return _projection * view();
}

cala::Transform &cala::Camera::transform() const {
    return _transform;
}

void cala::Camera::updateFrustum() {
    _frustum.update(viewProjection());
}

std::vector<ende::math::Vec4f> cala::Camera::getFrustumCorners() const {
    const auto inverseViewProjection = viewProjection().inverse();

    std::vector<ende::math::Vec4f> corners;
    for (unsigned int x = 0; x < 2; ++x)
    {
        for (unsigned int y = 0; y < 2; ++y)
        {
            for (unsigned int z = 0; z < 2; ++z)
            {
                const ende::math::Vec4f pt = inverseViewProjection.transform(ende::math::Vec4f{
                        2.0f * x - 1.0f,
                        2.0f * y - 1.0f,
                        2.0f * z - 1.0f,
                        1.0f});
                corners.push_back(pt / pt.w());
            }
        }
    }
    return corners;
}

void cala::Camera::setExposure(f32 exposure) {
    _exposure = exposure;
}

cala::Camera::Data cala::Camera::data() const {
    auto viewPos = _transform.pos();
//    viewPos = viewPos * ende::math::Vec3f({-1.f, 1.f, -1.f});

    return  {
        _projection,
        view(),
        viewPos,
        _near,
        _far,
        _exposure
    };
}