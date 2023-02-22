#ifndef CALA_CAMERA_H
#define CALA_CAMERA_H

#include <Ende/math/Mat.h>
#include <Cala/Transform.h>
#include <Ende/math/Frustum.h>

namespace cala {

    class Camera {
    public:

        Camera(f32 fov, f32 width, f32 height, f32 near, f32 far, Transform& transform);
        Camera(const ende::math::Mat4f& projection, Transform& transform);

        Camera& operator=(const Camera& rhs);

        void resize(f32 width, f32 height);

        inline ende::math::Mat4f projection() const { return _projection; }

        ende::math::Mat4f view() const;

        ende::math::Mat4f viewProjection() const;

        Transform& transform() const;

        const ende::math::Frustum& frustum() const { return _frustum; }

        void updateFrustum();

        void setExposure(f32 exposure);

        f32 getExposure() const { return _exposure; }

        struct Data {
            ende::math::Mat4f projection;
            ende::math::Mat4f view;
            ende::math::Vec3f position;
            f32 near;
            f32 far;
            f32 exposure;
        };

        Data data() const;

        f32 near() const { return _near; }

        f32 far() const { return _far; }

        bool isDirty() const { return _dirty; }

    private:

        ende::math::Frustum _frustum;

        ende::math::Mat4f _projection;
        Transform& _transform;
        f32 _fov;
        f32 _width;
        f32 _height;
        f32 _near;
        f32 _far;
        f32 _exposure;

        bool _dirty;

    };

}

#endif //CALA_CAMERA_H
